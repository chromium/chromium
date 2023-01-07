// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/googledrivefs/googledrivefs.h"

#include "nacl_io/filesystem.h"
#include "nacl_io/node.h"
#include "nacl_io/path.h"

namespace nacl_io {

// This is not further implemented.
// PNaCl is on a path to deprecation, and WebAssembly is
// the focused technology.

GoogleDriveFs::GoogleDriveFs() {}

Error GoogleDriveFs::Init(const FsInitArgs& args) {
  // TODO: support init
  LOG_ERROR("init not supported.");
  return EPERM;
}

Error GoogleDriveFs::OpenWithMode(const Path& path,
                                  int open_flags,
                                  mode_t mode,
                                  ScopedNode* out_node) {
  // TODO: support openwithmode
  LOG_ERROR("openwithmode not supported.");
  return EPERM;
}

Error GoogleDriveFs::Mkdir(const Path& path, int permissions) {
  // TODO: support mkdir
  LOG_ERROR("mkdir not supported.");
  return EPERM;
}

Error GoogleDriveFs::Rmdir(const Path& path) {
  // TODO: support rmdir
  LOG_ERROR("rmdir not supported.");
  return EPERM;
}

Error GoogleDriveFs::Rename(const Path& path, const Path& newPath) {
  // TODO: support rename
  LOG_ERROR("rename not supported.");
  return EPERM;
}

Error GoogleDriveFs::Unlink(const Path& path) {
  // TODO: support unlink
  LOG_ERROR("unlink not supported.");
  return EPERM;
}

Error GoogleDriveFs::Remove(const Path& path) {
  // TODO: support remove
  LOG_ERROR("remove not supported.");
  return EPERM;
}

}  // namespace nacl_io
