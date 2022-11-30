// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_file_ref_interface.h"

#include <vector>

#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_directory_entry.h>
#include <ppapi/c/pp_errors.h>

#include "gtest/gtest.h"

#include "fake_ppapi/fake_core_interface.h"
#include "fake_ppapi/fake_filesystem.h"
#include "fake_ppapi/fake_node.h"
#include "fake_ppapi/fake_util.h"
#include "fake_ppapi/fake_var_interface.h"

FakeFileRefInterface::FakeFileRefInterface(FakeCoreInterface* core_interface,
                                           FakeVarInterface* var_interface)
    : core_interface_(core_interface), var_interface_(var_interface) {}

PP_Resource FakeFileRefInterface::Create(PP_Resource file_system,
                                         const char* path) {
  FakeFileSystemResource* file_system_resource =
      core_interface_->resource_manager()->Get<FakeFileSystemResource>(
          file_system);
  if (file_system_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  if (!file_system_resource->opened)
    return PP_ERROR_FAILED;

  if (path == NULL)
    return PP_ERROR_FAILED;

  size_t path_len = strlen(path);
  if (path_len == 0)
    return PP_ERROR_FAILED;

  FakeFileRefResource* file_ref_resource = new FakeFileRefResource;
  file_ref_resource->filesystem = file_system_resource->filesystem;
  file_ref_resource->path = path;

  // Remove a trailing slash from the path, unless it is the root path.
  if (path_len > 1 && file_ref_resource->path[path_len - 1] == '/')
    file_ref_resource->path.erase(path_len - 1);

  return CREATE_RESOURCE(core_interface_->resource_manager(),
                         FakeFileRefResource, file_ref_resource);
}

PP_Var FakeFileRefInterface::GetName(PP_Resource file_ref) {
  FakeFileRefResource* file_ref_resource =
      core_interface_->resource_manager()->Get<FakeFileRefResource>(file_ref);
  if (file_ref_resource == NULL)
    return PP_MakeUndefined();

  return var_interface_->VarFromUtf8(file_ref_resource->path.c_str(),
                                     file_ref_resource->path.size());
}

int32_t FakeFileRefInterface::MakeDirectory(PP_Resource directory_ref,
                                            PP_Bool make_parents,
                                            PP_CompletionCallback callback) {
  FakeFileRefResource* directory_ref_resource =
      core_interface_->resource_manager()->Get<FakeFileRefResource>(
          directory_ref);
  if (directory_ref_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  // TODO(binji): We don't currently use make_parents in nacl_io, so
  // I won't bother handling it yet.
  if (make_parents == PP_TRUE)
    return PP_ERROR_FAILED;

  FakeFilesystem* filesystem = directory_ref_resource->filesystem;
  FakeFilesystem::Path path = directory_ref_resource->path;

  // Pepper returns PP_ERROR_NOACCESS when trying to create the root directory,
  // not PP_ERROR_FILEEXISTS, as you might expect.
  if (path == "/")
    return RunCompletionCallback(&callback, PP_ERROR_NOACCESS);

  FakeNode* node = filesystem->GetNode(path);
  if (node != NULL)
    return RunCompletionCallback(&callback, PP_ERROR_FILEEXISTS);

  FakeFilesystem::Path parent_path = filesystem->GetParentPath(path);
  FakeNode* parent_node = filesystem->GetNode(parent_path);
  if (parent_node == NULL)
    return RunCompletionCallback(&callback, PP_ERROR_FILENOTFOUND);

  if (!parent_node->IsDirectory())
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  bool result = filesystem->AddDirectory(directory_ref_resource->path, NULL);
  EXPECT_EQ(true, result);
  return RunCompletionCallback(&callback, PP_OK);
}

int32_t FakeFileRefInterface::Delete(PP_Resource file_ref,
                                     PP_CompletionCallback callback) {
  FakeFileRefResource* file_ref_resource =
      core_interface_->resource_manager()->Get<FakeFileRefResource>(file_ref);
  if (file_ref_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  FakeFilesystem* filesystem = file_ref_resource->filesystem;
  FakeFilesystem::Path path = file_ref_resource->path;
  FakeNode* node = filesystem->GetNode(path);
  if (node == NULL)
    return RunCompletionCallback(&callback, PP_ERROR_FILENOTFOUND);

  filesystem->RemoveNode(path);
  return RunCompletionCallback(&callback, PP_OK);
}

int32_t FakeFileRefInterface::Query(PP_Resource file_ref,
                                    PP_FileInfo* info,
                                    PP_CompletionCallback callback) {
  FakeFileRefResource* file_ref_resource =
      core_interface_->resource_manager()->Get<FakeFileRefResource>(file_ref);
  if (file_ref_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  FakeFilesystem* filesystem = file_ref_resource->filesystem;
  FakeFilesystem::Path path = file_ref_resource->path;
  FakeNode* node = filesystem->GetNode(path);
  if (node == NULL)
    return RunCompletionCallback(&callback, PP_ERROR_FILENOTFOUND);

  node->GetInfo(info);
  return RunCompletionCallback(&callback, PP_OK);
}

int32_t FakeFileRefInterface::ReadDirectoryEntries(
    PP_Resource directory_ref,
    const PP_ArrayOutput& output,
    PP_CompletionCallback callback) {
  FakeFileRefResource* directory_ref_resource =
      core_interface_->resource_manager()->Get<FakeFileRefResource>(
          directory_ref);
  if (directory_ref_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  FakeFilesystem* filesystem = directory_ref_resource->filesystem;
  FakeFilesystem::Path path = directory_ref_resource->path;
  FakeNode* node = filesystem->GetNode(path);
  if (node == NULL)
    return RunCompletionCallback(&callback, PP_ERROR_FILENOTFOUND);

  if (!node->IsDirectory())
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  FakeFilesystem::DirectoryEntries fake_dir_entries;
  filesystem->GetDirectoryEntries(path, &fake_dir_entries);

  uint32_t element_count = fake_dir_entries.size();
  uint32_t element_size = sizeof(fake_dir_entries[0]);
  void* data_buffer =
      (*output.GetDataBuffer)(output.user_data, element_count, element_size);

  if (data_buffer == NULL)
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  PP_DirectoryEntry* dir_entries = static_cast<PP_DirectoryEntry*>(data_buffer);
  for (uint32_t i = 0; i < element_count; ++i) {
    const FakeFilesystem::DirectoryEntry& fake_dir_entry = fake_dir_entries[i];

    FakeFileRefResource* file_ref_resource = new FakeFileRefResource;
    file_ref_resource->filesystem = directory_ref_resource->filesystem;
    file_ref_resource->path = fake_dir_entry.path;
    PP_Resource file_ref =
        CREATE_RESOURCE(core_interface_->resource_manager(),
                        FakeFileRefResource, file_ref_resource);

    dir_entries[i].file_ref = file_ref;
    dir_entries[i].file_type = fake_dir_entry.node->file_type();
  }

  return RunCompletionCallback(&callback, PP_OK);
}

int32_t FakeFileRefInterface::Rename(PP_Resource file_ref,
                                     PP_Resource new_file_ref,
                                     PP_CompletionCallback callback) {
  FakeFileRefResource* file_ref_resource =
      core_interface_->resource_manager()->Get<FakeFileRefResource>(file_ref);
  if (file_ref_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  FakeFileRefResource* new_file_ref_resource =
      core_interface_->resource_manager()->Get<FakeFileRefResource>(
          new_file_ref);
  if (new_file_ref_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  FakeFilesystem* filesystem = file_ref_resource->filesystem;
  FakeFilesystem::Path path = file_ref_resource->path;
  FakeFilesystem::Path newpath = new_file_ref_resource->path;
  FakeNode* node = filesystem->GetNode(path);
  if (node == NULL)
    return RunCompletionCallback(&callback, PP_ERROR_FILENOTFOUND);
  // FakeFileRefResource does not support directory rename.
  if (!node->IsRegular())
    return RunCompletionCallback(&callback, PP_ERROR_NOTAFILE);

  // Remove the destination if it exists.
  filesystem->RemoveNode(newpath);
  const std::vector<uint8_t> contents = node->contents();
  EXPECT_TRUE(filesystem->AddFile(newpath, contents, NULL));
  EXPECT_TRUE(filesystem->RemoveNode(path));
  return RunCompletionCallback(&callback, PP_OK);
}
