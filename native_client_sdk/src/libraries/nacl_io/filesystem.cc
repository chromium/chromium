// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/filesystem.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>

#include <string>

#include "nacl_io/dir_node.h"
#include "nacl_io/log.h"
#include "nacl_io/node.h"
#include "nacl_io/osstat.h"
#include "nacl_io/path.h"
#include "sdk_util/auto_lock.h"
#include "sdk_util/ref_object.h"

#if defined(WIN32)
#include <windows.h>
#endif

namespace nacl_io {

Filesystem::Filesystem() : dev_(0) {
}

Filesystem::~Filesystem() {
}

Error Filesystem::Init(const FsInitArgs& args) {
  dev_ = args.dev;
  ppapi_ = args.ppapi;
  return 0;
}

void Filesystem::Destroy() {
}

Error Filesystem::Open(const Path& path,
                       int open_flags,
                       ScopedNode* out_node) {
  return OpenWithMode(path, open_flags, 0666, out_node);
}

Error Filesystem::OpenResource(const Path& path, ScopedNode* out_node) {
  out_node->reset(NULL);
  LOG_TRACE("Can't open resource: %s", path.Join().c_str());
  return EINVAL;
}

void Filesystem::OnNodeCreated(Node* node) {
  node->stat_.st_ino = inode_pool_.Acquire();
  node->stat_.st_dev = dev_;
}

void Filesystem::OnNodeDestroyed(Node* node) {
  if (node->stat_.st_ino)
    inode_pool_.Release(node->stat_.st_ino);
}

Error Filesystem::Filesystem_VIoctl(int request, va_list args) {
  LOG_ERROR("Unsupported ioctl: %#x", request);
  return EINVAL;
}

Error Filesystem::Filesystem_Ioctl(int request, ...) {
  va_list args;
  va_start(args, request);
  Error error = Filesystem_VIoctl(request, args);
  va_end(args);
  return error;
}

}  // namespace nacl_io
