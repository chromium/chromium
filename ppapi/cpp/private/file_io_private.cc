// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/file_io_private.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_file_io_private.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_FileIO_Private>() {
  return PPB_FILEIO_PRIVATE_INTERFACE_0_1;
}

}  // namespace

FileIO_Private::FileIO_Private()
    : FileIO() {
}

FileIO_Private::FileIO_Private(const InstanceHandle& instance)
    : FileIO(instance) {
}

int32_t FileIO_Private::RequestOSFileHandle(
    const CompletionCallbackWithOutput<PassFileHandle>& cc) {
  if (has_interface<PPB_FileIO_Private>())
    return get_interface<PPB_FileIO_Private>()->RequestOSFileHandle(
        pp_resource(),
        cc.output(),
        cc.pp_completion_callback());
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

}  // namespace pp
