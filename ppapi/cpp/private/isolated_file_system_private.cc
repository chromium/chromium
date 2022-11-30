// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/isolated_file_system_private.h"

#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_IsolatedFileSystem_Private_0_2>() {
  return PPB_ISOLATEDFILESYSTEM_PRIVATE_INTERFACE_0_2;
}

}  // namespace

IsolatedFileSystemPrivate::IsolatedFileSystemPrivate()
    : instance_(0), type_(PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_INVALID) {
}

IsolatedFileSystemPrivate::IsolatedFileSystemPrivate(
    const InstanceHandle& instance,
    PP_IsolatedFileSystemType_Private type)
    : instance_(instance.pp_instance()), type_(type) {
}

IsolatedFileSystemPrivate::~IsolatedFileSystemPrivate() {
}

int32_t IsolatedFileSystemPrivate::Open(
    const CompletionCallbackWithOutput<pp::FileSystem>& cc) {
  if (!has_interface<PPB_IsolatedFileSystem_Private_0_2>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_IsolatedFileSystem_Private_0_2>()->
      Open(instance_, type_, cc.output(), cc.pp_completion_callback());
}

}  // namespace pp
