// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/file_system.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_FileSystem_1_0>() {
  return PPB_FILESYSTEM_INTERFACE_1_0;
}

}  // namespace

FileSystem::FileSystem() {
}

FileSystem::FileSystem(const FileSystem& other) : Resource(other) {
}

FileSystem::FileSystem(const Resource& resource) : Resource(resource) {
  if (!IsFileSystem(resource)) {
    PP_NOTREACHED();

    // On release builds, set this to null.
    Clear();
  }
}

FileSystem::FileSystem(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

FileSystem::FileSystem(const InstanceHandle& instance,
                       PP_FileSystemType type) {
  if (!has_interface<PPB_FileSystem_1_0>())
    return;
  PassRefFromConstructor(get_interface<PPB_FileSystem_1_0>()->Create(
      instance.pp_instance(), type));
}

int32_t FileSystem::Open(int64_t expected_size,
                         const CompletionCallback& cc) {
  if (!has_interface<PPB_FileSystem_1_0>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileSystem_1_0>()->Open(
      pp_resource(), expected_size, cc.pp_completion_callback());
}

// static
bool FileSystem::IsFileSystem(const Resource& resource) {
  if (!has_interface<PPB_FileSystem_1_0>())
    return false;
  return get_interface<PPB_FileSystem_1_0>()->IsFileSystem(
      resource.pp_resource()) == PP_TRUE;
}

}  // namespace pp
