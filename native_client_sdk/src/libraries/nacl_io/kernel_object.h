// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_KERNEL_OBJECT_H_
#define LIBRARIES_NACL_IO_KERNEL_OBJECT_H_

#include <pthread.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "nacl_io/error.h"
#include "nacl_io/filesystem.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/node.h"
#include "nacl_io/path.h"

#include "sdk_util/macros.h"
#include "sdk_util/simple_lock.h"

namespace nacl_io {

// KernelObject provides basic functionality for threadsafe access to kernel
// objects such as the CWD, mount points, file descriptors and file handles.
// All Kernel locks are private to ensure the lock order.
//
// All calls are assumed to be a relative path.
class KernelObject {
 public:
  struct Descriptor_t {
    Descriptor_t() : flags(0) {}
    explicit Descriptor_t(const ScopedKernelHandle& h,
                          const std::string& open_path)
        : handle(h), flags(0), path(open_path) {}

    ScopedKernelHandle handle;
    int flags;
    std::string path;
  };
  typedef std::vector<Descriptor_t> HandleMap_t;
  typedef std::map<std::string, ScopedFilesystem> FsMap_t;

  KernelObject();

  KernelObject(const KernelObject&) = delete;
  KernelObject& operator=(const KernelObject&) = delete;

  virtual ~KernelObject();

  // Attach the given Filesystem object at the specified path.
  Error AttachFsAtPath(const ScopedFilesystem& fs, const std::string& path);

  // Unmap the Filesystem object from the specified path and release it.
  Error DetachFsAtPath(const std::string& path, ScopedFilesystem* out_fs);

  // Find the filesystem for the given path, and acquires it and return a
  // path relative to the filesystem.
  // Assumes |out_fs| and |rel_path| are non-NULL.
  Error AcquireFsAndRelPath(const std::string& path,
                            ScopedFilesystem* out_fs,
                            Path* rel_path);

  // Find the filesystem and node for the given path, acquiring/creating it as
  // specified by the |oflags|.
  // Assumes |out_fs| and |out_node| are non-NULL.
  Error AcquireFsAndNode(const std::string& path,
                         int oflags, mode_t mflags,
                         ScopedFilesystem* out_fs,
                         ScopedNode* out_node);

  // Get FD-specific flags (currently only FD_CLOEXEC is supported).
  Error GetFDFlags(int fd, int* out_flags);
  // Set FD-specific flags (currently only FD_CLOEXEC is supported).
  Error SetFDFlags(int fd, int flags);

  // Convert from FD to KernelHandle, and acquire the handle.
  // Assumes |out_handle| is non-NULL.
  Error AcquireHandle(int fd, ScopedKernelHandle* out_handle);
  Error AcquireHandleAndPath(int fd,
                             ScopedKernelHandle* out_handle,
                             std::string* out_path);

  // Allocate a new fd and assign the handle to it, while
  // ref counting the handle and associated filesystem.
  // Assumes |handle| is non-NULL;
  int AllocateFD(const ScopedKernelHandle& handle,
                 const std::string& path = std::string());

  // Assumes |handle| is non-NULL;
  void FreeAndReassignFD(int fd,
                         const ScopedKernelHandle& handle,
                         const std::string& path);
  void FreeFD(int fd);

  // Returns or sets CWD
  std::string GetCWD();
  Error SetCWD(const std::string& path);

  mode_t GetUmask();
  // Also returns current umask (like POSIX's umask(2))
  mode_t SetUmask(mode_t);

  // Returns parts of the absolute path for the given relative path
  Path GetAbsParts(const std::string& path);

 private:
  std::string cwd_;
  mode_t umask_;
  std::set<int> free_fds_;
  HandleMap_t handle_map_;
  FsMap_t filesystems_;

  // Lock to protect free_fds_ and handle_map_.
  sdk_util::SimpleLock handle_lock_;

  // Lock to protect filesystems_.
  sdk_util::SimpleLock fs_lock_;

  // Lock to protect cwd_.
  sdk_util::SimpleLock cwd_lock_;
  // Lock to protect umask_.
  sdk_util::SimpleLock umask_lock_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_KERNEL_OBJECT_H_
