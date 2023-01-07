// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/ext_crx_file_system_private.h"

#include "ppapi/c/private/ppb_ext_crx_file_system_private.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Ext_CrxFileSystem_Private_0_1>() {
  return PPB_EXT_CRXFILESYSTEM_PRIVATE_INTERFACE_0_1;
}

}  // namespace

ExtCrxFileSystemPrivate::ExtCrxFileSystemPrivate() {
}

ExtCrxFileSystemPrivate::ExtCrxFileSystemPrivate(
    const InstanceHandle& instance) : instance_(instance.pp_instance()) {
}

ExtCrxFileSystemPrivate::~ExtCrxFileSystemPrivate() {
}

int32_t ExtCrxFileSystemPrivate::Open(
    const CompletionCallbackWithOutput<pp::FileSystem>& cc) {
  if (!has_interface<PPB_Ext_CrxFileSystem_Private_0_1>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_Ext_CrxFileSystem_Private_0_1>()->
      Open(instance_, cc.output(), cc.pp_completion_callback());
}

}  // namespace pp
