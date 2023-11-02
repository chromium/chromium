// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_FUSE_H_
#define LIBRARIES_NACL_IO_FUSE_H_

#include "osinttypes.h"
#include "ostypes.h"

// These interfaces are copied from the FUSE library.
//
// FUSE has two interfaces that can be implemented: low-level and high-level.
// In nacl_io, we only support the high-level interface.
//
// See http://fuse.sourceforge.net/ for more information.

// This struct is typically passed to functions that would normally use return
// or receive an fd; that is, operations to open/create a node, or operations
// that act on an already opened node.
struct fuse_file_info {
  // This is filled with the flags passed to open()
  int flags;
  // Deprecated in FUSE. Use fh instead.
  unsigned long fh_old;
  int writepage;
  // Currently unsupported
  unsigned int direct_io : 1;
  // Currently unsupported
  unsigned int keep_cache : 1;
  // Currently unsupported
  unsigned int flush : 1;
  // Currently unsupported
  unsigned int nonseekable : 1;
  // Currently unsupported
  unsigned int padding : 27;
  // This value is not used by nacl_io. It can be filled by the developer when
  // open() is called, and reused for subsequent calls on the same node.
  uint64_t fh;
  // Currently unsupported
  uint64_t lock_owner;
  // Currently unsupported
  uint32_t poll_events;
};

// A dummy structure that currently exists only to match the FUSE interface.
struct fuse_conn_info {};

// A function of this type will be passed to readdir (see below). The developer
// should call this function once for each directory entry.
//
// See the documentation for readdir() below for more information on how to use
// this function.
typedef int (*fuse_fill_dir_t)(void* buf,
                               const char* name,
                               const struct stat* stbuf,
                               off_t off);

// This structure defines the interface to create a user filesystem. Pass this
// to
// nacl_io_register_fs_type(). (see nacl_io.h)
//
// Example:
//
//     struct fuse_operations g_my_fuse_operations = { ... };
//     ...
//     nacl_io_register_fs_type("myfusefs", &g_my_fuse_operations);
//     ...
//     mount("", "/fs/fuse", "myfusefs", 0, NULL);
//
// It is not necessary to implement every function -- nacl_io will first check
// if the function pointer is NULL before calling it. If it is NULL and
// required by the current operation, the call will fail and return ENOSYS in
// errno.
//
// Except where specified below, each function should return one of the
// following values:
// == 0: operation completed successfully.
// <  0: operation failed. The error is a negative errno. e.g. -EACCES, -EPERM,
//       etc. The sign will be flipped when the error is actually set.
//
// Some functions (e.g. read, write) also return a positive count, which is the
// number of bytes read/written.
//
struct fuse_operations {
  // Currently unsupported
  unsigned int flag_nopath : 1;
  unsigned int flag_reserved : 31;

  // Called by stat()/fstat(), but only when fuse_operations.fgetattr is NULL.
  // Also called by open() to determine if the path is a directory or a regular
  // file.
  int (*getattr)(const char* path, struct stat*);
  // Not called currently.
  int (*readlink)(const char*, char*, size_t);
  // Called when O_CREAT is passed to open(), but only if fuse_operations.create
  // is non-NULL.
  int (*mknod)(const char* path, mode_t, dev_t);
  // Called by mkdir()
  int (*mkdir)(const char* path, mode_t);
  // Called by unlink()
  int (*unlink)(const char* path);
  // Called by rmdir()
  int (*rmdir)(const char* path);
  // Not called currently.
  int (*symlink)(const char*, const char*);
  // Called by rename()
  int (*rename)(const char* path, const char* new_path);
  // Not called currently.
  int (*link)(const char*, const char*);
  // Called by chmod()/fchmod()
  int (*chmod)(const char*, mode_t);
  // Not called currently.
  int (*chown)(const char*, uid_t, gid_t);
  // Called by truncate(), as well as open() when O_TRUNC is passed.
  int (*truncate)(const char* path, off_t);
  // Called by open()
  int (*open)(const char* path, struct fuse_file_info*);
  // Called by read(). Note that FUSE specifies that all reads will fill the
  // entire requested buffer. If this function returns less than that, the
  // remainder of the buffer is zeroed.
  int (*read)(const char* path,
              char* buf,
              size_t count,
              off_t,
              struct fuse_file_info*);
  // Called by write(). Note that FUSE specifies that a write should always
  // return the full count, unless an error occurs.
  int (*write)(const char* path,
               const char* buf,
               size_t count,
               off_t,
               struct fuse_file_info*);
  // Not called currently.
  int (*statfs)(const char*, struct statvfs*);
  // Not called currently.
  int (*flush)(const char*, struct fuse_file_info*);
  // Called when the last reference to this node is released. This is only
  // called for regular files. For directories, fuse_operations.releasedir is
  // called instead.
  int (*release)(const char* path, struct fuse_file_info*);
  // Called by fsync(). The datasync paramater is not currently supported.
  int (*fsync)(const char* path, int datasync, struct fuse_file_info*);
  // Not called currently.
  int (*setxattr)(const char*, const char*, const char*, size_t, int);
  // Not called currently.
  int (*getxattr)(const char*, const char*, char*, size_t);
  // Not called currently.
  int (*listxattr)(const char*, char*, size_t);
  // Not called currently.
  int (*removexattr)(const char*, const char*);
  // Called by getdents(), which is called by the more standard functions
  // opendir()/readdir().
  int (*opendir)(const char* path, struct fuse_file_info*);
  // Called by getdents(), which is called by the more standard function
  // readdir().
  //
  // NOTE: it is the responsibility of this function to add the "." and ".."
  // entries.
  //
  // This function can be implemented one of two ways:
  // 1) Ignore the offset, and always write every entry in a given directory.
  //    In this case, you should always call filler() with an offset of 0. You
  //    can ignore the return value of the filler.
  //
  //   int my_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
  //                  off_t offset, struct fuse_file_info*) {
  //     ...
  //     filler(buf, ".", NULL, 0);
  //     filler(buf, "..", NULL, 0);
  //     filler(buf, "file1", &file1stat, 0);
  //     filler(buf, "file2", &file2stat, 0);
  //     return 0;
  //   }
  //
  // 2) Only write entries starting from offset. Always pass the correct offset
  //    to the filler function. When the filler function returns 1, the buffer
  //    is full so you can exit readdir.
  //
  //   int my_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
  //                  off_t offset, struct fuse_file_info*) {
  //     ...
  //     size_t kNumEntries = 4;
  //     const char* my_entries[] = { ".", "..", "file1", "file2" };
  //     int entry_index = offset / sizeof(dirent);
  //     offset = entry_index * sizeof(dirent);
  //     while (entry_index < kNumEntries) {
  //       int result = filler(buf, my_entries[entry_index], NULL, offset);
  //       if (filler == 1) {
  //         // buffer filled, we're done.
  //         return 0;
  //       }
  //       offset += sizeof(dirent);
  //       entry_index++;
  //     }
  //
  //     // No more entries, we're done.
  //     return 0;
  //   }
  //
  int (*readdir)(const char* path,
                 void* buf,
                 fuse_fill_dir_t filldir,
                 off_t,
                 struct fuse_file_info*);
  // Called when the last reference to this node is released. This is only
  // called for directories. For regular files, fuse_operations.release is
  // called instead.
  int (*releasedir)(const char* path, struct fuse_file_info*);
  // Not called currently.
  int (*fsyncdir)(const char*, int, struct fuse_file_info*);
  // Called when a filesystem of this type is initialized.
  void* (*init)(struct fuse_conn_info* conn);
  // Called when a filesystem of this type is unmounted.
  void (*destroy)(void*);
  // Called by access()
  int (*access)(const char* path, int mode);
  // Called when O_CREAT is passed to open()
  int (*create)(const char* path, mode_t mode, struct fuse_file_info*);
  // Called by ftruncate()
  int (*ftruncate)(const char* path, off_t, struct fuse_file_info*);
  // Called by stat()/fstat(). If this function pointer is non-NULL, it is
  // called, otherwise fuse_operations.getattr will be called.
  int (*fgetattr)(const char* path, struct stat*, struct fuse_file_info*);
  // Not called currently.
  int (*lock)(const char*, struct fuse_file_info*, int cmd, struct flock*);
  // Called by utime()/utimes()/futimes()/futimens() etc.
  int (*utimens)(const char*, const struct timespec tv[2]);
  // Not called currently.
  int (*bmap)(const char*, size_t blocksize, uint64_t* idx);
  // Not called currently.
  int (*ioctl)(const char*,
               int cmd,
               void* arg,
               struct fuse_file_info*,
               unsigned int flags,
               void* data);
  // Not called currently.
  int (*poll)(const char*,
              struct fuse_file_info*,
              struct fuse_pollhandle* ph,
              unsigned* reventsp);
  // Not called currently.
  int (*write_buf)(const char*,
                   struct fuse_bufvec* buf,
                   off_t off,
                   struct fuse_file_info*);
  // Not called currently.
  int (*read_buf)(const char*,
                  struct fuse_bufvec** bufp,
                  size_t size,
                  off_t off,
                  struct fuse_file_info*);
  // Not called currently.
  int (*flock)(const char*, struct fuse_file_info*, int op);
  // Not called currently.
  int (*fallocate)(const char*, int, off_t, off_t, struct fuse_file_info*);
};

#endif  // LIBRARIES_NACL_IO_FUSE_H_
