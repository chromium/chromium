/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_NACL_IO_H_
#define LIBRARIES_NACL_IO_NACL_IO_H_

#include <ppapi/c/pp_instance.h>
#include <ppapi/c/ppb.h>

#include "nacl_io/ostypes.h"
#include "sdk_util/macros.h"

EXTERN_C_BEGIN

typedef void (*nacl_io_exit_callback_t)(int status, void* user_data);

typedef void (*nacl_io_mount_callback_t)(const char* source,
                                         const char* target,
                                         const char* filesystemtype,
                                         unsigned long mountflags,
                                         const void* data,
                                         dev_t dev,
                                         void* user_data);

/**
 * Initialize nacl_io.
 *
 * NOTE: If you initialize nacl_io with this constructor, you cannot
 * use any filesystems that require PPAPI; e.g. persistent storage, etc.
 */
int nacl_io_init(void);

/**
 * Initialize nacl_io with PPAPI support.
 *
 * Usage:
 *   PP_Instance instance;
 *   PPB_GetInterface get_interface;
 *   nacl_io_init(instance, get_interface);
 *
 * If you are using the PPAPI C interface:
 *   |instance| is passed to your instance in the DidCreate function.
 *   |get_interface| is passed to your module in the PPP_InitializeModule
 *   function.
 *
 * If you are using the PPAPI C++ interface:
 *   |instance| can be retrieved via the pp::Instance::pp_instance() method.
 *   |get_interface| can be retrieved via
 *       pp::Module::Get()->get_browser_interface()
 */
int nacl_io_init_ppapi(PP_Instance instance, PPB_GetInterface get_interface);

/**
 * Uninitialize nacl_io.
 *
 * This removes interception for POSIX C-library function and releases
 * any associated resources.
 */
int nacl_io_uninit(void);

void nacl_io_set_exit_callback(nacl_io_exit_callback_t exit_callback,
                               void* user_data);

/**
 * Mount a new filesystem type.
 *
 * This function is declared in <sys/mount.h>, but we document it here
 * because nacl_io is controlled primarily through mount(2)/umount(2).
 *
 * Some parameters are dependent on the filesystem type being mounted.
 *
 * The |data| parameter, if used, is always parsed as a string of comma
 * separated key-value pairs:
 *   e.g. "key1=param1,key2=param2"
 *
 *
 * filesystem types:
 *   "memfs": An in-memory filesystem.
 *     source: Unused.
 *     data: Unused.
 *
 *   "dev": A filesystem with various utility nodes. Some examples:
 *          "null": equivalent to /dev/null.
 *          "zero": equivalent to /dev/zero.
 *          "urandom": equivalent to /dev/urandom.
 *          "console[0-3]": logs to the JavaScript console with varying log
 *              levels.
 *          "tty": Posts a message to JavaScript, which will send a "message"
 *              event from this module's embed element.
 *     source: Unused.
 *     data: Unused.
 *
 *   "html5fs": A filesystem that uses PPAPI FileSystem interface, which can be
 *              read in JavaScript via the HTML5 FileSystem API. This filesystem
 *              provides the use of persistent storage. Please read the
 *              documentation in ppapi/c/ppb_file_system.h for more information.
 *     source: Used to mount a subtree of the filesystem. Necessary when
 *             mounting non-sandboxed filesystems provided from javascript (e.g.
 *             via chrome.fileSystem in a chrome app). This should be a path
 *             which will be transparently prepended to all paths when
 *             performing the underlying file operations.
 *     data: A string of parameters:
 *       "type": Which type of filesystem to mount. Valid values are
 *           "PERSISTENT" and "TEMPORARY". The default is "PERSISTENT".
 *       "expected_size": The expected file-system size. Note that this does
 *           not request quota -- you must do that from JavaScript.
 *       "filesystem_resource": If specified, this is a string that contains
 *           the integer ID of the Filesystem resource to use instead of
 *           creating a new one. The "type" and "expected_size" parameters are
 *           ignored in this case. This parameter is useful when you pass a
 *           Filesystem resource from JavaScript, but still want to be able to
 *           call open/read/write/etc.
 *
 *   "httpfs": A filesystem that reads from a URL via HTTP.
 *     source: The root URL to read from. All paths read from this filesystem
 *             will be appended to this root.
 *             e.g. If source == "http://example.com/path", reading from
 *             "foo/bar.txt" will attempt to read from the URL
 *             "http://example.com/path/foo/bar.txt".
 *     data: A string of parameters:
 *       "allow_cross_origin_requests": If "true", then reads from this
 *           filesystem will follow the CORS standard for cross-origin requests.
 *           See http://www.w3.org/TR/access-control.
 *       "allow_credentials": If "true", credentials are sent with cross-origin
 *           requests. If false, no credentials are sent with the request and
 *           cookies are ignored in the response.
 *       All other key/value pairs are assumed to be headers to use with
 *       HTTP requests.
 *
 *   "passthroughfs": A filesystem that passes all requests through to the
 *                    underlying NaCl calls. The primary use of this filesystem
 *                    is to allow reading NMF resources.
 *     source: Unused.
 *     data: Unused.
 *
 *
 * @param[in] source Depends on the filesystem type. See above.
 * @param[in] target The absolute path to mount the filesystem.
 * @param[in] filesystemtype The name of the filesystem type to mount. See
 *     above for examples.
 * @param[in] mountflags Unused.
 * @param[in] data Depends on the filesystem type. See above.
 * @return 0 on success, -1 on failure (with errno set).
 *
 * int mount(const char* source, const char* target, const char* filesystemtype,
 *         unsigned long mountflags, const void *data) NOTHROW;
 */

/**
 * Register a new filesystem type, using a FUSE interface to implement it.
 *
 * Example:
 *   int my_open(const char* path, struct fuse_file_info*) {
 *     ...
 *   }
 *
 *   int my_read(const char* path, char* buf, size_t count, off_t offset, struct
 *               fuse_file_info* info) {
 *     ...
 *   }
 *
 *   struct fuse_operations my_fuse_ops = {
 *     ...
 *     my_open,
 *     NULL,  // opendir() not implemented.
 *     my_read,
 *     ...
 *   };
 *
 *   ...
 *
 *   const char fs_type[] = "my_fs";
 *   int result = nacl_io_register_fs_type(fs_type, &my_fuse_ops);
 *   if (!result) {
 *     fprintf(stderr, "Error registering filesystem type %s.\n", fs_type);
 *     exit(1);
 *   }
 *
 *   ...
 *
 *   int result = mount("", "/fs/foo", fs_type, 0, NULL);
 *   if (!result) {
 *     fprintf(stderr, "Error mounting %s.\n", fs_type);
 *     exit(1);
 *   }
 *
 * See fuse.h for more information about the FUSE interface.
 * Also see fuse.sourceforge.net for more information about FUSE in general.
 *
 * @param[in] fs_type The name of the new filesystem type.
 * @param[in] fuse_ops A pointer to the FUSE interface that will be used to
 *     implement this filesystem type. This pointer must be valid for the
 *     lifetime of all filesystems and nodes that are created with it.
 * @return 0 on success, -1 on failure (with errno set).
 */
struct fuse_operations;
int nacl_io_register_fs_type(const char* fs_type,
                             struct fuse_operations* fuse_ops);

/**
 * Unregister a filesystem type, previously registered by
 * nacl_io_register_fs_type().
 *
 * @param[in] fs_type The name of the filesystem type; the same identifier that
 *     was passed to nacl_io_register_fs_type().
 * @return 0 on success, -1 on failure (with errno set).
 */
int nacl_io_unregister_fs_type(const char* fs_type);

/**
 * Set a mount callback.
 *
 * This callback is called whenever mount() succeeds. This callback can be used
 * to get the dev number of the newly-mounted filesystem.
 *
 * @param[in] callback The callback to set, or NULL.
 * @param[in] user_data User data that will be passed to the callback.
 * @return 0 on success, -1 on failure.
 */
void nacl_io_set_mount_callback(nacl_io_mount_callback_t callback,
                                void* user_data);

EXTERN_C_END

#endif  // LIBRARIES_NACL_IO_NACL_IO_H_
