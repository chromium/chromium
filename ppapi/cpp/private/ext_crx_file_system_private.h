// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_EXT_CRX_FILE_SYSTEM_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_EXT_CRX_FILE_SYSTEM_PRIVATE_H_

#include <stdint.h>

#include "ppapi/c/pp_instance.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance_handle.h"

namespace pp {

class ExtCrxFileSystemPrivate {
 public:
  ExtCrxFileSystemPrivate();
  explicit ExtCrxFileSystemPrivate(const InstanceHandle& instance);
  virtual ~ExtCrxFileSystemPrivate();

  int32_t Open(const CompletionCallbackWithOutput<pp::FileSystem>& cc);

 private:
  PP_Instance instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_EXT_CRX_FILE_SYSTEM_PRIVATE_H_
