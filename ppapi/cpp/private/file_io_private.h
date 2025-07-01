// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_FILE_IO_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_FILE_IO_PRIVATE_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/private/pass_file_handle.h"

namespace pp {

class FileIO;

class FileIO_Private : public FileIO {
 public:
  FileIO_Private();
  explicit FileIO_Private(const InstanceHandle& instance);

  int32_t RequestOSFileHandle(
      const CompletionCallbackWithOutput<PassFileHandle>& cc);
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_FILE_IO_PRIVATE_H_
