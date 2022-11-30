// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/private/ppb_file_ref_private.h"
#include "ppapi/shared_impl/file_ref_create_info.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_ref_api.h"
#include "ppapi/thunk/ppb_file_system_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_FileRef_API> EnterFileRef;

PP_Resource Create(PP_Resource file_system, const char* path) {
  VLOG(4) << "PPB_FileRef::Create()";
  ppapi::ProxyAutoLock lock;
  EnterResourceNoLock<PPB_FileSystem_API> enter_file_system(file_system, true);
  if (enter_file_system.failed())
    return 0;
  PP_Instance instance = enter_file_system.resource()->pp_instance();
  EnterResourceCreationNoLock enter(instance);
  if (enter.failed())
    return 0;
  FileRefCreateInfo info;
  info.file_system_type = enter_file_system.object()->GetType();
  info.internal_path = std::string(path);
  info.browser_pending_host_resource_id = 0;
  info.renderer_pending_host_resource_id = 0;
  info.file_system_plugin_resource = file_system;
  return enter.functions()->CreateFileRef(instance, info);
}

PP_Bool IsFileRef(PP_Resource resource) {
  VLOG(4) << "PPB_FileRef::IsFileRef()";
  EnterFileRef enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_FileSystemType GetFileSystemType(PP_Resource file_ref) {
  VLOG(4) << "PPB_FileRef::GetFileSystemType()";
  EnterFileRef enter(file_ref, true);
  if (enter.failed())
    return PP_FILESYSTEMTYPE_INVALID;
  return enter.object()->GetFileSystemType();
}

PP_Var GetName(PP_Resource file_ref) {
  VLOG(4) << "PPB_FileRef::GetName()";
  EnterFileRef enter(file_ref, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetName();
}

PP_Var GetPath(PP_Resource file_ref) {
  VLOG(4) << "PPB_FileRef::GetPath()";
  EnterFileRef enter(file_ref, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetPath();
}

PP_Resource GetParent(PP_Resource file_ref) {
  VLOG(4) << "PPB_FileRef::GetParent()";
  EnterFileRef enter(file_ref, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetParent();
}

int32_t MakeDirectory(PP_Resource directory_ref,
                      PP_Bool make_ancestors,
                      PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileRef::MakeDirectory()";
  EnterFileRef enter(directory_ref, callback, true);
  if (enter.failed())
    return enter.retval();
  int32_t flag = make_ancestors ? PP_MAKEDIRECTORYFLAG_WITH_ANCESTORS
                                : PP_MAKEDIRECTORYFLAG_NONE;
  return enter.SetResult(enter.object()->MakeDirectory(
      flag, enter.callback()));
}

int32_t MakeDirectory_1_2(PP_Resource directory_ref,
                          int32_t make_directory_flags,
                          PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileRef::MakeDirectory()";
  EnterFileRef enter(directory_ref, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->MakeDirectory(
      make_directory_flags, enter.callback()));
}

int32_t Touch(PP_Resource file_ref,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileRef::Touch()";
  EnterFileRef enter(file_ref, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Touch(
      last_access_time, last_modified_time, enter.callback()));
}

int32_t Delete(PP_Resource file_ref,
               PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileRef::Delete()";
  EnterFileRef enter(file_ref, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Delete(enter.callback()));
}

int32_t Rename(PP_Resource file_ref,
               PP_Resource new_file_ref,
               PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileRef::Rename()";
  EnterFileRef enter(file_ref, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Rename(new_file_ref,
                                                enter.callback()));
}

int32_t Query(PP_Resource file_ref,
              PP_FileInfo* info,
              PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileRef::Query()";
  EnterFileRef enter(file_ref, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Query(info,
                                               enter.callback()));
}

int32_t ReadDirectoryEntries(PP_Resource file_ref,
                             PP_ArrayOutput output,
                             PP_CompletionCallback callback) {
  EnterFileRef enter(file_ref, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->ReadDirectoryEntries(
      output, enter.callback()));
}

PP_Var GetAbsolutePath(PP_Resource file_ref) {
  VLOG(4) << "PPB_FileRef::GetAbsolutePath";
  EnterFileRef enter(file_ref, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetAbsolutePath();
}

const PPB_FileRef_1_0 g_ppb_file_ref_thunk_1_0 = {
  &Create,
  &IsFileRef,
  &GetFileSystemType,
  &GetName,
  &GetPath,
  &GetParent,
  &MakeDirectory,
  &Touch,
  &Delete,
  &Rename
};

const PPB_FileRef_1_1 g_ppb_file_ref_thunk_1_1 = {
  &Create,
  &IsFileRef,
  &GetFileSystemType,
  &GetName,
  &GetPath,
  &GetParent,
  &MakeDirectory,
  &Touch,
  &Delete,
  &Rename,
  &Query,
  &ReadDirectoryEntries
};

const PPB_FileRef_1_2 g_ppb_file_ref_thunk_1_2 = {
  &Create,
  &IsFileRef,
  &GetFileSystemType,
  &GetName,
  &GetPath,
  &GetParent,
  &MakeDirectory_1_2,
  &Touch,
  &Delete,
  &Rename,
  &Query,
  &ReadDirectoryEntries
};

const PPB_FileRefPrivate g_ppb_file_ref_private_thunk = {
  &GetAbsolutePath
};

}  // namespace

const PPB_FileRef_1_0* GetPPB_FileRef_1_0_Thunk() {
  return &g_ppb_file_ref_thunk_1_0;
}

const PPB_FileRef_1_1* GetPPB_FileRef_1_1_Thunk() {
  return &g_ppb_file_ref_thunk_1_1;
}

const PPB_FileRef_1_2* GetPPB_FileRef_1_2_Thunk() {
  return &g_ppb_file_ref_thunk_1_2;
}

const PPB_FileRefPrivate_0_1* GetPPB_FileRefPrivate_0_1_Thunk() {
  return &g_ppb_file_ref_private_thunk;
}

}  // namespace thunk
}  // namespace ppapi
