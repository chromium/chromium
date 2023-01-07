// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/googledrivefs/googledrivefs_node.h"

#include <sys/stat.h>

#include "nacl_io/googledrivefs/googledrivefs.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/node.h"
#include "nacl_io/osdirent.h"

namespace nacl_io {

// This is not further implemented.
// PNaCl is on a path to deprecation, and WebAssembly is
// the focused technology.

GoogleDriveFsNode::GoogleDriveFsNode(GoogleDriveFs* googledrivefs)
    : Node(googledrivefs) {}

Error GoogleDriveFsNode::GetDents(size_t offs,
                                  struct dirent* pdir,
                                  size_t size,
                                  int* out_bytes) {
  // TODO: support getdents
  LOG_ERROR("getdents not supported.");
  return EPERM;
}

Error GoogleDriveFsNode::GetStat(struct stat* pstat) {
  // TODO: support getstat
  LOG_ERROR("getstat not supported.");
  return EPERM;
}

Error GoogleDriveFsNode::Write(const HandleAttr& attr,
                               const void* buf,
                               size_t count,
                               int* out_bytes) {
  // TODO: support write
  LOG_ERROR("write not supported.");
  return EPERM;
}

Error GoogleDriveFsNode::FTruncate(off_t length) {
  // TODO: support ftruncate
  LOG_ERROR("ftruncate not supported.");
  return EPERM;
}

Error GoogleDriveFsNode::Read(const HandleAttr& attr,
                              void* buf,
                              size_t count,
                              int* out_bytes) {
  // TODO: support read
  LOG_ERROR("read not supported.");
  return EPERM;
}

Error GoogleDriveFsNode::GetSize(off_t* out_size) {
  // TODO: support getsize
  LOG_ERROR("getsize not supported.");
  return EPERM;
}

Error GoogleDriveFsNode::Init(int open_flags) {
  // TODO: support init
  LOG_ERROR("init not supported.");
  return EPERM;
}

}  // namespace nacl_io
