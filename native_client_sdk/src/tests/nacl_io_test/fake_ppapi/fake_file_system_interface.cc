// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_file_system_interface.h"

#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_errors.h>

#include "fake_ppapi/fake_core_interface.h"
#include "fake_ppapi/fake_filesystem.h"
#include "fake_ppapi/fake_util.h"

FakeFileSystemInterface::FakeFileSystemInterface(
    FakeCoreInterface* core_interface)
    : core_interface_(core_interface) {}

PP_Bool FakeFileSystemInterface::IsFileSystem(PP_Resource resource) {
  bool not_found_ok = true;
  FakeFileSystemResource* file_system_resource =
      core_interface_->resource_manager()->Get<FakeFileSystemResource>(
          resource, not_found_ok);
  return file_system_resource != NULL ? PP_TRUE : PP_FALSE;
}

PP_Resource FakeFileSystemInterface::Create(PP_Instance instance,
                                            PP_FileSystemType filesystem_type) {
  FakeHtml5FsResource* instance_resource =
      core_interface_->resource_manager()->Get<FakeHtml5FsResource>(instance);
  if (instance_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  FakeFileSystemResource* file_system_resource = new FakeFileSystemResource;
  file_system_resource->filesystem = new FakeFilesystem(
      *instance_resource->filesystem_template, filesystem_type);

  return CREATE_RESOURCE(core_interface_->resource_manager(),
                         FakeFileSystemResource, file_system_resource);
}

int32_t FakeFileSystemInterface::Open(PP_Resource file_system,
                                      int64_t expected_size,
                                      PP_CompletionCallback callback) {
  FakeFileSystemResource* file_system_resource =
      core_interface_->resource_manager()->Get<FakeFileSystemResource>(
          file_system);
  if (file_system_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  file_system_resource->opened = true;
  return RunCompletionCallback(&callback, PP_OK);
}
