// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_TYPED_FS_FACTORY_H_
#define LIBRARIES_NACL_IO_TYPED_FS_FACTORY_H_

#include "nacl_io/fs_factory.h"

namespace nacl_io {

template <typename T>
class TypedFsFactory : public FsFactory {
 public:
  virtual Error CreateFilesystem(const FsInitArgs& args,
                                 ScopedFilesystem* out_fs) {
    sdk_util::ScopedRef<T> fs(new T());
    Error error = fs->Init(args);
    if (error)
      return error;

    *out_fs = fs;
    return 0;
  }
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_TYPED_FS_FACTORY_H_
