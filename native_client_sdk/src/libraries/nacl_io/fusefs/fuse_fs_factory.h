// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_FUSEFS_FUSE_FS_FACTORY_H_
#define LIBRARIES_NACL_IO_FUSEFS_FUSE_FS_FACTORY_H_

#include "nacl_io/filesystem.h"
#include "nacl_io/fs_factory.h"

struct fuse_operations;

namespace nacl_io {

class FuseFsFactory : public FsFactory {
 public:
  explicit FuseFsFactory(fuse_operations* fuse_ops);
  virtual Error CreateFilesystem(const FsInitArgs& args,
                                 ScopedFilesystem* out_fs);

 private:
  fuse_operations* fuse_ops_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_FUSEFS_FUSE_FS_FACTORY_H_
