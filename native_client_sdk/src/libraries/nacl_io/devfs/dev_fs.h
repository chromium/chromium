// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_DEVFS_DEV_FS_H_
#define LIBRARIES_NACL_IO_DEVFS_DEV_FS_H_

#include "nacl_io/filesystem.h"
#include "nacl_io/typed_fs_factory.h"

namespace nacl_io {

class DevFs : public Filesystem {
 public:
  DevFs(const DevFs&) = delete;
  DevFs& operator=(const DevFs&) = delete;

  virtual Error OpenWithMode(const Path& path, int open_flags, mode_t mode,
                             ScopedNode* out_node);
  virtual Error Unlink(const Path& path);
  virtual Error Mkdir(const Path& path, int permissions);
  virtual Error Rmdir(const Path& path);
  virtual Error Remove(const Path& path);
  virtual Error Rename(const Path& path, const Path& newpath);

  Error CreateFsNode(Filesystem* fs);
  Error DestroyFsNode(Filesystem* fs);

 protected:
  DevFs();

  virtual Error Init(const FsInitArgs& args);

 private:
  ScopedNode root_;
  ScopedNode fs_dir_;

  friend class TypedFsFactory<DevFs>;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_DEVFS_DEV_FS_H_
