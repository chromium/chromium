// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/passthroughfs/passthrough_fs.h"

#include <errno.h>

#include "nacl_io/kernel_handle.h"
#include "nacl_io/kernel_wrap_real.h"
#include "nacl_io/passthroughfs/real_node.h"

namespace nacl_io {

PassthroughFs::PassthroughFs() {
}

Error PassthroughFs::Init(const FsInitArgs& args) {
  return Filesystem::Init(args);
}

void PassthroughFs::Destroy() {
}

Error PassthroughFs::OpenWithMode(const Path& path, int open_flags,
                                  mode_t mode, ScopedNode* out_node) {
  out_node->reset(NULL);
  int real_fd;
  int error = _real_open(path.Join().c_str(), open_flags, mode, &real_fd);
  if (error)
    return error;

  out_node->reset(new RealNode(this, real_fd, true));
  return 0;
}

Error PassthroughFs::OpenResource(const Path& path, ScopedNode* out_node) {
  int real_fd;
  out_node->reset(NULL);
  int error = _real_open_resource(path.Join().c_str(), &real_fd);
  if (error)
    return error;

  out_node->reset(new RealNode(this, real_fd));
  return 0;
}

Error PassthroughFs::Unlink(const Path& path) {
  // Not implemented by NaCl.
  return ENOSYS;
}

Error PassthroughFs::Mkdir(const Path& path, int perm) {
  return _real_mkdir(path.Join().c_str(), perm);
}

Error PassthroughFs::Rmdir(const Path& path) {
  return _real_rmdir(path.Join().c_str());
}

Error PassthroughFs::Remove(const Path& path) {
  // Not implemented by NaCl.
  return ENOSYS;
}

Error PassthroughFs::Rename(const Path& path, const Path& newpath) {
  // Not implemented by NaCl.
  return ENOSYS;
}

}  // namespace nacl_io
