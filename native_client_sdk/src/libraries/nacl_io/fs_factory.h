// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_FS_FACTORY_H_
#define LIBRARIES_NACL_IO_FS_FACTORY_H_

#include <errno.h>

#include "nacl_io/error.h"
#include "nacl_io/filesystem.h"
#include "sdk_util/scoped_ref.h"

namespace nacl_io {

class FsFactory {
 public:
  virtual ~FsFactory() {}
  virtual Error CreateFilesystem(const FsInitArgs& args,
                                 ScopedFilesystem* out_fs) = 0;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_FS_FACTORY_H_
