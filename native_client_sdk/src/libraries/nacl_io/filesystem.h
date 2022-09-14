// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_FILESYSTEM_H_
#define LIBRARIES_NACL_IO_FILESYSTEM_H_

#include <map>
#include <string>

#include "nacl_io/error.h"
#include "nacl_io/inode_pool.h"
#include "nacl_io/node.h"
#include "nacl_io/path.h"

#include "sdk_util/macros.h"
#include "sdk_util/ref_object.h"
#include "sdk_util/scoped_ref.h"

struct fuse_operations;

namespace nacl_io {

class Filesystem;
class Node;
class PepperInterface;

typedef sdk_util::ScopedRef<Filesystem> ScopedFilesystem;
typedef std::map<std::string, std::string> StringMap_t;

// This structure is passed to all filesystems via the Filesystem::Init virtual
// function.  With it, we can add or remove initialization values without
// changing the function signature.
struct FsInitArgs {
  FsInitArgs() : dev(0), ppapi(NULL), fuse_ops(NULL) {}
  explicit FsInitArgs(int dev) : dev(dev), ppapi(NULL), fuse_ops(NULL) {}

  // Device number of the new filesystem.
  int dev;
  StringMap_t string_map;
  PepperInterface* ppapi;
  fuse_operations* fuse_ops;
};

// NOTE: The KernelProxy is the only class that should be setting errno. All
// other classes should return Error (as defined by nacl_io/error.h).
class Filesystem : public sdk_util::RefObject {
 public:
  Filesystem(const Filesystem&) = delete;
  Filesystem& operator=(const Filesystem&) = delete;

 protected:
  // The protected functions are only used internally and will not
  // acquire or release the filesystem's lock.
  Filesystem();
  virtual ~Filesystem();

  // Init must be called by the factory before the filesystem is used.
  // |ppapi| can be NULL. If so, this filesystem cannot make any pepper calls.
  virtual Error Init(const FsInitArgs& args);
  virtual void Destroy();

 public:
  PepperInterface* ppapi() { return ppapi_; }
  int dev() { return dev_; }

  // All paths in functions below are expected to containing a leading "/".

  // Open a node at |path| with the specified open and modeflags. The resulting
  // Node is created with a ref count of 1.
  // Assumes that |out_node| is non-NULL.
  virtual Error OpenWithMode(const Path& path,
                             int open_flags,
                             mode_t mode,
                             ScopedNode* out_node) = 0;

  // Open a node at |path| with the specified open flags. The resulting
  // Node is created with a ref count of 1.
  // Assumes that |out_node| is non-NULL.
  Error Open(const Path& path,
             int open_flags,
             ScopedNode* out_node);

  // OpenResource is only used to read files from the NaCl NMF file. No
  // filesystem except PassthroughFs should implement it.
  // Assumes that |out_node| is non-NULL.
  virtual Error OpenResource(const Path& path, ScopedNode* out_node);

  // Unlink, Mkdir, Rmdir will affect the both the RefCount
  // and the nlink number in the stat object.
  virtual Error Unlink(const Path& path) = 0;
  virtual Error Mkdir(const Path& path, int permissions) = 0;
  virtual Error Rmdir(const Path& path) = 0;
  virtual Error Remove(const Path& path) = 0;
  virtual Error Rename(const Path& path, const Path& newpath) = 0;
  virtual Error Filesystem_VIoctl(int request, va_list args);

  // Helper function that forwards to Filesystem_VIoctl.
  Error Filesystem_Ioctl(int request, ...);

  // Assumes that |node| is non-NULL.
  virtual void OnNodeCreated(Node* node);

  // Assumes that |node| is non-NULL.
  virtual void OnNodeDestroyed(Node* node);

 protected:
  // Device number for the filesystem.
  int dev_;
  PepperInterface* ppapi_;  // Weak reference.
  INodePool inode_pool_;

 private:
  // May only be called by the KernelProxy when the Kernel's
  // lock is held, so we make it private.
  friend class KernelObject;
  friend class KernelProxy;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_FILESYSTEM_H_
