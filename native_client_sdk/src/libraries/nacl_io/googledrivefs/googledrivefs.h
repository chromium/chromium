// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_GOOGLEDRIVEFS_GOOGLEDRIVEFS_H_
#define LIBRARIES_NACL_IO_GOOGLEDRIVEFS_GOOGLEDRIVEFS_H_

#include "nacl_io/filesystem.h"
#include "nacl_io/node.h"
#include "nacl_io/path.h"
#include "nacl_io/typed_fs_factory.h"

namespace nacl_io {

// This is not further implemented.
// PNaCl is on a path to deprecation, and WebAssembly is
// the focused technology.

class GoogleDriveFs : public Filesystem {
 protected:
  GoogleDriveFs();

  Error Init(const FsInitArgs& args);

 public:
  Error OpenWithMode(const Path& path,
                     int open_flags,
                     mode_t mode,
                     ScopedNode* out_node);
  Error Unlink(const Path& path);
  Error Mkdir(const Path& path, int permissions);
  Error Rmdir(const Path& path);
  Error Remove(const Path& path);
  Error Rename(const Path& path, const Path& newPath);

 private:
  friend class TypedFsFactory<GoogleDriveFs>;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_GOOGLEDRIVEFS_GOOGLEDRIVEFS_H_
