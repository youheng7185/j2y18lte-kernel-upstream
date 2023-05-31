#ifndef USB_F_MASS_STORAGE_H
#define USB_F_MASS_STORAGE_H

#include <linux/usb/composite.h>
#include "storage_common.h"

struct fsg_module_parameters {
	char		*file[FSG_MAX_LUNS];
	bool		ro[FSG_MAX_LUNS];
	bool		removable[FSG_MAX_LUNS];
	bool		cdrom[FSG_MAX_LUNS];
	bool		nofua[FSG_MAX_LUNS];

	unsigned int	file_count, ro_count, removable_count, cdrom_count;
	unsigned int	nofua_count;
	unsigned int	luns;	/* nluns */
	bool		stall;	/* can_stall */
};

#define _FSG_MODULE_PARAM_ARRAY(prefix, params, name, type, desc)	\
	module_param_array_named(prefix ## name, params.name, type,	\
				 &prefix ## params.name ## _count,	\
				 S_IRUGO);				\
	MODULE_PARM_DESC(prefix ## name, desc)

#define _FSG_MODULE_PARAM(prefix, params, name, type, desc)		\
	module_param_named(prefix ## name, params.name, type,		\
			   S_IRUGO);					\
	MODULE_PARM_DESC(prefix ## name, desc)

#define __FSG_MODULE_PARAMETERS(prefix, params)				\
	_FSG_MODULE_PARAM_ARRAY(prefix, params, file, charp,		\
				"names of backing files or devices");	\
	_FSG_MODULE_PARAM_ARRAY(prefix, params, ro, bool,		\
				"true to force read-only");		\
	_FSG_MODULE_PARAM_ARRAY(prefix, params, removable, bool,	\
				"true to simulate removable media");	\
	_FSG_MODULE_PARAM_ARRAY(prefix, params, cdrom, bool,		\
				"true to simulate CD-ROM instead of disk"); \
	_FSG_MODULE_PARAM_ARRAY(prefix, params, nofua, bool,		\
				"true to ignore SCSI WRITE(10,12) FUA bit"); \
	_FSG_MODULE_PARAM(prefix, params, luns, uint,			\
			  "number of LUNs");				\
	_FSG_MODULE_PARAM(prefix, params, stall, bool,			\
			  "false to prevent bulk stalls")

#ifdef CONFIG_USB_GADGET_DEBUG_FILES

#define FSG_MODULE_PARAMETERS(prefix, params)				\
	__FSG_MODULE_PARAMETERS(prefix, params);			\
	module_param_named(num_buffers, fsg_num_buffers, uint, S_IRUGO);\
	MODULE_PARM_DESC(num_buffers, "Number of pipeline buffers")
#else

#define FSG_MODULE_PARAMETERS(prefix, params)				\
	__FSG_MODULE_PARAMETERS(prefix, params)

#endif

struct fsg_common;

/* FSF callback functions */
struct fsg_operations {
	/*
	 * Callback function to call when thread exits.  If no
	 * callback is set or it returns value lower then zero MSF
	 * will force eject all LUNs it operates on (including those
	 * marked as non-removable or with prevent_medium_removal flag
	 * set).
	 */
	int (*thread_exits)(struct fsg_common *common);
};

struct fsg_lun_opts {
	struct config_group group;
	struct fsg_lun *lun;
	int lun_id;
};

struct fsg_opts {
	struct fsg_common *common;
	struct usb_function_instance func_inst;
	struct fsg_lun_opts lun0;
	struct config_group *default_groups[2];
	bool no_configfs; /* for legacy gadgets */

	/*
	 * Read/write access to configfs attributes is handled by configfs.
	 *
	 * This is to protect the data from concurrent access by read/write
	 * and create symlink/remove symlink.
	 */
	struct mutex			lock;
	int				refcnt;
};

struct fsg_lun_config {
	const char *filename;
	char ro;
	char removable;
	char cdrom;
	char nofua;
};

struct fsg_config {
	unsigned nluns;
	struct fsg_lun_config luns[FSG_MAX_LUNS];

	/* Callback functions. */
	const struct fsg_operations	*ops;
	/* Gadget's private data. */
	void			*private_data;

	const char *vendor_name;		/*  8 characters or less */
	const char *product_name;		/* 16 characters or less */

	char			can_stall;
	unsigned int		fsg_num_buffers;
};

#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
/* Data shared by all the FSG instances. */
struct fsg_common {
	struct usb_gadget	*gadget;
	struct usb_composite_dev *cdev;
	struct fsg_dev		*fsg, *new_fsg;
	wait_queue_head_t	fsg_wait;

	/* filesem protects: backing files in use */
	struct rw_semaphore	filesem;

	/* lock protects: state, all the req_busy's */
	spinlock_t		lock;

	struct usb_ep		*ep0;		/* Copy of gadget->ep0 */
	struct usb_request	*ep0req;	/* Copy of cdev->req */
	unsigned int		ep0_req_tag;

	struct fsg_buffhd	*next_buffhd_to_fill;
	struct fsg_buffhd	*next_buffhd_to_drain;
	struct fsg_buffhd	*buffhds;
	unsigned int		fsg_num_buffers;
	int			cmnd_size;
	u8			cmnd[MAX_COMMAND_SIZE];

	unsigned int		nluns;
	unsigned int		lun;
	struct fsg_lun		**luns;
	struct fsg_lun		*curlun;

	unsigned int		bulk_out_maxpacket;
	enum fsg_state		state;		/* For exception handling */
	unsigned int		exception_req_tag;

	enum data_direction	data_dir;
	u32			data_size;
	u32			data_size_from_cmnd;
	u32			tag;
	u32			residue;
	u32			usb_amount_left;

	unsigned int		can_stall:1;
	unsigned int		free_storage_on_release:1;
	unsigned int		phase_error:1;
	unsigned int		short_packet_received:1;
	unsigned int		bad_lun_okay:1;
	unsigned int		running:1;
	unsigned int		sysfs:1;

	int			thread_wakeup_needed;
	struct completion	thread_notifier;
	struct task_struct	*thread_task;

	/* Callback functions. */
	const struct fsg_operations	*ops;
	/* Gadget's private data. */
	void			*private_data;
#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
	char vendor_string[8 + 1];
	char product_string[16 + 1];
	/* Additional image version info for SUA */
	char version_string[100 + 1];
#endif
	char inquiry_string[INQUIRY_MAX_LEN];
	/* LUN name for sysfs purpose */
	char name[FSG_MAX_LUNS][LUN_NAME_LEN];
	struct kref		ref;
	struct timer_list	vfs_timer;
};


#endif

static inline struct fsg_opts *
fsg_opts_from_func_inst(const struct usb_function_instance *fi)
{
	return container_of(fi, struct fsg_opts, func_inst);
}

void fsg_common_get(struct fsg_common *common);

void fsg_common_put(struct fsg_common *common);

void fsg_common_set_sysfs(struct fsg_common *common, bool sysfs);

int fsg_common_set_num_buffers(struct fsg_common *common, unsigned int n);

void fsg_common_free_buffers(struct fsg_common *common);

int fsg_common_set_cdev(struct fsg_common *common,
			struct usb_composite_dev *cdev, bool can_stall);

void fsg_common_remove_lun(struct fsg_lun *lun, bool sysfs);

void fsg_common_remove_luns(struct fsg_common *common);

void fsg_common_free_luns(struct fsg_common *common);

int fsg_common_set_nluns(struct fsg_common *common, int nluns);

void fsg_common_set_ops(struct fsg_common *common,
			const struct fsg_operations *ops);

int fsg_common_create_lun(struct fsg_common *common, struct fsg_lun_config *cfg,
			  unsigned int id, const char *name,
			  const char **name_pfx);

int fsg_common_create_luns(struct fsg_common *common, struct fsg_config *cfg);

void fsg_common_set_inquiry_string(struct fsg_common *common, const char *vn,
				   const char *pn);

int fsg_common_run_thread(struct fsg_common *common);

void fsg_config_from_params(struct fsg_config *cfg,
			    const struct fsg_module_parameters *params,
			    unsigned int fsg_num_buffers);
int fsg_sysfs_update(struct fsg_common *common, struct device *dev,
				bool create);
#endif /* USB_F_MASS_STORAGE_H */
