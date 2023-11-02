// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/fusefs/fuse_fs_factory.h"

#include "nacl_io/fusefs/fuse_fs.h"

namespace nacl_io {

FuseFsFactory::FuseFsFactory(fuse_operations* fuse_ops) : fuse_ops_(fuse_ops) {
}

Error FuseFsFactory::CreateFilesystem(const FsInitArgs& args,
                                      ScopedFilesystem* out_fs) {
  FsInitArgs args_copy(args);
  args_copy.fuse_ops = fuse_ops_;

  sdk_util::ScopedRef<FuseFs> fs(new FuseFs());
  Error error = fs->Init(args_copy);
  if (error)
    return error;

  *out_fs = fs;
  return 0;
}

}  // namespace nacl_io
