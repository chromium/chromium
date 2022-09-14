// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_file_io_interface.h"

#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_errors.h>

#include "gtest/gtest.h"

#include "fake_ppapi/fake_core_interface.h"
#include "fake_ppapi/fake_filesystem.h"
#include "fake_ppapi/fake_node.h"
#include "fake_ppapi/fake_util.h"

namespace {

class FakeFileIoResource : public FakeResource {
 public:
  FakeFileIoResource() : node(NULL), open_flags(0) {}
  static const char* classname() { return "FakeFileIoResource"; }

  FakeNode* node;  // Weak reference.
  int32_t open_flags;
};

}  // namespace

FakeFileIoInterface::FakeFileIoInterface(FakeCoreInterface* core_interface)
    : core_interface_(core_interface) {}

PP_Resource FakeFileIoInterface::Create(PP_Resource) {
  return CREATE_RESOURCE(core_interface_->resource_manager(),
                         FakeFileIoResource, new FakeFileIoResource);
}

int32_t FakeFileIoInterface::Open(PP_Resource file_io,
                                  PP_Resource file_ref,
                                  int32_t open_flags,
                                  PP_CompletionCallback callback) {
  FakeFileIoResource* file_io_resource =
      core_interface_->resource_manager()->Get<FakeFileIoResource>(file_io);
  if (file_io_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  bool flag_write = !!(open_flags & PP_FILEOPENFLAG_WRITE);
  bool flag_create = !!(open_flags & PP_FILEOPENFLAG_CREATE);
  bool flag_truncate = !!(open_flags & PP_FILEOPENFLAG_TRUNCATE);
  bool flag_exclusive = !!(open_flags & PP_FILEOPENFLAG_EXCLUSIVE);
  bool flag_append = !!(open_flags & PP_FILEOPENFLAG_APPEND);

  if ((flag_append && flag_write) || (flag_truncate && !flag_write))
    return PP_ERROR_BADARGUMENT;

  FakeFileRefResource* file_ref_resource =
      core_interface_->resource_manager()->Get<FakeFileRefResource>(file_ref);

  if (file_ref_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  const FakeFilesystem::Path& path = file_ref_resource->path;
  FakeFilesystem* filesystem = file_ref_resource->filesystem;
  FakeNode* node = filesystem->GetNode(path);
  bool node_exists = node != NULL;

  if (!node_exists) {
    if (!flag_create)
      return RunCompletionCallback(&callback, PP_ERROR_FILENOTFOUND);

    bool result = filesystem->AddEmptyFile(path, &node);
    EXPECT_EQ(true, result);
  } else {
    if (flag_create && flag_exclusive)
      return RunCompletionCallback(&callback, PP_ERROR_FILEEXISTS);
  }

  file_io_resource->node = node;
  file_io_resource->open_flags = open_flags;

  if (flag_truncate)
    return RunCompletionCallback(&callback, node->SetLength(0));

  return RunCompletionCallback(&callback, PP_OK);
}

int32_t FakeFileIoInterface::Query(PP_Resource file_io,
                                   PP_FileInfo* info,
                                   PP_CompletionCallback callback) {
  FakeFileIoResource* file_io_resource =
      core_interface_->resource_manager()->Get<FakeFileIoResource>(file_io);
  if (file_io_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  if (!file_io_resource->node)
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  file_io_resource->node->GetInfo(info);
  return RunCompletionCallback(&callback, PP_OK);
}

int32_t FakeFileIoInterface::Read(PP_Resource file_io,
                                  int64_t offset,
                                  char* buffer,
                                  int32_t bytes_to_read,
                                  PP_CompletionCallback callback) {
  FakeFileIoResource* file_io_resource =
      core_interface_->resource_manager()->Get<FakeFileIoResource>(file_io);

  if (file_io_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  if (bytes_to_read < 0)
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  if ((file_io_resource->open_flags & PP_FILEOPENFLAG_READ) !=
      PP_FILEOPENFLAG_READ) {
    return RunCompletionCallback(&callback, PP_ERROR_NOACCESS);
  }

  if (!file_io_resource->node)
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  int32_t result = file_io_resource->node->Read(offset, buffer, bytes_to_read);

  return RunCompletionCallback(&callback, result);
}

int32_t FakeFileIoInterface::Write(PP_Resource file_io,
                                   int64_t offset,
                                   const char* buffer,
                                   int32_t bytes_to_write,
                                   PP_CompletionCallback callback) {
  FakeFileIoResource* file_io_resource =
      core_interface_->resource_manager()->Get<FakeFileIoResource>(file_io);
  if (file_io_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  if ((file_io_resource->open_flags & PP_FILEOPENFLAG_WRITE) !=
      PP_FILEOPENFLAG_WRITE) {
    return RunCompletionCallback(&callback, PP_ERROR_NOACCESS);
  }

  if (!file_io_resource->node)
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  int32_t result;
  if ((file_io_resource->open_flags & PP_FILEOPENFLAG_APPEND) ==
      PP_FILEOPENFLAG_APPEND) {
    result = file_io_resource->node->Append(buffer, bytes_to_write);
  } else {
    result = file_io_resource->node->Write(offset, buffer, bytes_to_write);
  }

  return RunCompletionCallback(&callback, result);
}

int32_t FakeFileIoInterface::SetLength(PP_Resource file_io,
                                       int64_t length,
                                       PP_CompletionCallback callback) {
  FakeFileIoResource* file_io_resource =
      core_interface_->resource_manager()->Get<FakeFileIoResource>(file_io);
  if (file_io_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  if ((file_io_resource->open_flags & PP_FILEOPENFLAG_WRITE) !=
      PP_FILEOPENFLAG_WRITE) {
    return RunCompletionCallback(&callback, PP_ERROR_NOACCESS);
  }

  if (!file_io_resource->node)
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  int32_t result = file_io_resource->node->SetLength(length);
  return RunCompletionCallback(&callback, result);
}

int32_t FakeFileIoInterface::Flush(PP_Resource file_io,
                                   PP_CompletionCallback callback) {
  FakeFileIoResource* file_io_resource =
      core_interface_->resource_manager()->Get<FakeFileIoResource>(file_io);
  if (file_io_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  if (!file_io_resource->node)
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  return RunCompletionCallback(&callback, PP_OK);
}

void FakeFileIoInterface::Close(PP_Resource file_io) {
  FakeFileIoResource* file_io_resource =
      core_interface_->resource_manager()->Get<FakeFileIoResource>(file_io);

  if (file_io_resource == NULL)
    return;

  file_io_resource->node = NULL;
  file_io_resource->open_flags = 0;
}
