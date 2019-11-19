// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/filesystem_dispatcher.h"

#include <stdint.h>

#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/filesystem_interception.h"
#include "sandbox/win/src/filesystem_policy.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_broker.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_nt_util.h"

namespace sandbox {

FilesystemDispatcher::FilesystemDispatcher(PolicyBase* policy_base)
    : policy_base_(policy_base) {
  static const IPCCall create_params = {
      {IpcTag::NTCREATEFILE,
       {WCHAR_TYPE, UINT32_TYPE, UINT32_TYPE, UINT32_TYPE, UINT32_TYPE,
        UINT32_TYPE, UINT32_TYPE}},
      reinterpret_cast<CallbackGeneric>(&FilesystemDispatcher::NtCreateFile)};

  static const IPCCall open_file = {
      {IpcTag::NTOPENFILE,
       {WCHAR_TYPE, UINT32_TYPE, UINT32_TYPE, UINT32_TYPE, UINT32_TYPE}},
      reinterpret_cast<CallbackGeneric>(&FilesystemDispatcher::NtOpenFile)};

  static const IPCCall attribs = {
      {IpcTag::NTQUERYATTRIBUTESFILE, {WCHAR_TYPE, UINT32_TYPE, INOUTPTR_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &FilesystemDispatcher::NtQueryAttributesFile)};

  static const IPCCall full_attribs = {
      {IpcTag::NTQUERYFULLATTRIBUTESFILE,
       {WCHAR_TYPE, UINT32_TYPE, INOUTPTR_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &FilesystemDispatcher::NtQueryFullAttributesFile)};

  static const IPCCall set_info = {
      {IpcTag::NTSETINFO_RENAME,
       {VOIDPTR_TYPE, INOUTPTR_TYPE, INOUTPTR_TYPE, UINT32_TYPE, UINT32_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &FilesystemDispatcher::NtSetInformationFile)};

  ipc_calls_.push_back(create_params);
  ipc_calls_.push_back(open_file);
  ipc_calls_.push_back(attribs);
  ipc_calls_.push_back(full_attribs);
  ipc_calls_.push_back(set_info);
}

bool FilesystemDispatcher::SetupService(InterceptionManager* manager,
                                        IpcTag service) {
  switch (service) {
    case IpcTag::NTCREATEFILE:
      return INTERCEPT_NT(manager, NtCreateFile, CREATE_FILE_ID, 48);

    case IpcTag::NTOPENFILE:
      return INTERCEPT_NT(manager, NtOpenFile, OPEN_FILE_ID, 28);

    case IpcTag::NTQUERYATTRIBUTESFILE:
      return INTERCEPT_NT(manager, NtQueryAttributesFile, QUERY_ATTRIB_FILE_ID,
                          12);

    case IpcTag::NTQUERYFULLATTRIBUTESFILE:
      return INTERCEPT_NT(manager, NtQueryFullAttributesFile,
                          QUERY_FULL_ATTRIB_FILE_ID, 12);

    case IpcTag::NTSETINFO_RENAME:
      return INTERCEPT_NT(manager, NtSetInformationFile, SET_INFO_FILE_ID, 24);

    default:
      return false;
  }
}

bool FilesystemDispatcher::NtCreateFile(IPCInfo* ipc,
                                        std::wstring* name,
                                        uint32_t attributes,
                                        uint32_t desired_access,
                                        uint32_t file_attributes,
                                        uint32_t share_access,
                                        uint32_t create_disposition,
                                        uint32_t create_options) {
  if (!PreProcessName(name)) {
    // The path requested might contain a reparse point.
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }

  const wchar_t* filename = name->c_str();

  uint32_t broker = BROKER_TRUE;
  CountedParameterSet<OpenFile> params;
  params[OpenFile::NAME] = ParamPickerMake(filename);
  params[OpenFile::ACCESS] = ParamPickerMake(desired_access);
  params[OpenFile::DISPOSITION] = ParamPickerMake(create_disposition);
  params[OpenFile::OPTIONS] = ParamPickerMake(create_options);
  params[OpenFile::BROKER] = ParamPickerMake(broker);

  // To evaluate the policy we need to call back to the policy object. We
  // are just middlemen in the operation since is the FileSystemPolicy which
  // knows what to do.
  EvalResult result =
      policy_base_->EvalPolicy(IpcTag::NTCREATEFILE, params.GetBase());
  HANDLE handle;
  ULONG_PTR io_information = 0;
  NTSTATUS nt_status;
  if (!FileSystemPolicy::CreateFileAction(
          result, *ipc->client_info, *name, attributes, desired_access,
          file_attributes, share_access, create_disposition, create_options,
          &handle, &nt_status, &io_information)) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  // Return operation status on the IPC.
  ipc->return_info.extended[0].ulong_ptr = io_information;
  ipc->return_info.nt_status = nt_status;
  ipc->return_info.handle = handle;
  return true;
}

bool FilesystemDispatcher::NtOpenFile(IPCInfo* ipc,
                                      std::wstring* name,
                                      uint32_t attributes,
                                      uint32_t desired_access,
                                      uint32_t share_access,
                                      uint32_t open_options) {
  if (!PreProcessName(name)) {
    // The path requested might contain a reparse point.
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }

  const wchar_t* filename = name->c_str();

  uint32_t broker = BROKER_TRUE;
  uint32_t create_disposition = FILE_OPEN;
  CountedParameterSet<OpenFile> params;
  params[OpenFile::NAME] = ParamPickerMake(filename);
  params[OpenFile::ACCESS] = ParamPickerMake(desired_access);
  params[OpenFile::DISPOSITION] = ParamPickerMake(create_disposition);
  params[OpenFile::OPTIONS] = ParamPickerMake(open_options);
  params[OpenFile::BROKER] = ParamPickerMake(broker);

  // To evaluate the policy we need to call back to the policy object. We
  // are just middlemen in the operation since is the FileSystemPolicy which
  // knows what to do.
  EvalResult result =
      policy_base_->EvalPolicy(IpcTag::NTOPENFILE, params.GetBase());
  HANDLE handle;
  ULONG_PTR io_information = 0;
  NTSTATUS nt_status;
  if (!FileSystemPolicy::OpenFileAction(
          result, *ipc->client_info, *name, attributes, desired_access,
          share_access, open_options, &handle, &nt_status, &io_information)) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  // Return operation status on the IPC.
  ipc->return_info.extended[0].ulong_ptr = io_information;
  ipc->return_info.nt_status = nt_status;
  ipc->return_info.handle = handle;
  return true;
}

bool FilesystemDispatcher::NtQueryAttributesFile(IPCInfo* ipc,
                                                 std::wstring* name,
                                                 uint32_t attributes,
                                                 CountedBuffer* info) {
  if (sizeof(FILE_BASIC_INFORMATION) != info->Size())
    return false;

  if (!PreProcessName(name)) {
    // The path requested might contain a reparse point.
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }

  uint32_t broker = BROKER_TRUE;
  const wchar_t* filename = name->c_str();
  CountedParameterSet<FileName> params;
  params[FileName::NAME] = ParamPickerMake(filename);
  params[FileName::BROKER] = ParamPickerMake(broker);

  // To evaluate the policy we need to call back to the policy object. We
  // are just middlemen in the operation since is the FileSystemPolicy which
  // knows what to do.
  EvalResult result =
      policy_base_->EvalPolicy(IpcTag::NTQUERYATTRIBUTESFILE, params.GetBase());

  FILE_BASIC_INFORMATION* information =
      reinterpret_cast<FILE_BASIC_INFORMATION*>(info->Buffer());
  NTSTATUS nt_status;
  if (!FileSystemPolicy::QueryAttributesFileAction(result, *ipc->client_info,
                                                   *name, attributes,
                                                   information, &nt_status)) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }

  // Return operation status on the IPC.
  ipc->return_info.nt_status = nt_status;
  return true;
}

bool FilesystemDispatcher::NtQueryFullAttributesFile(IPCInfo* ipc,
                                                     std::wstring* name,
                                                     uint32_t attributes,
                                                     CountedBuffer* info) {
  if (sizeof(FILE_NETWORK_OPEN_INFORMATION) != info->Size())
    return false;

  if (!PreProcessName(name)) {
    // The path requested might contain a reparse point.
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }

  uint32_t broker = BROKER_TRUE;
  const wchar_t* filename = name->c_str();
  CountedParameterSet<FileName> params;
  params[FileName::NAME] = ParamPickerMake(filename);
  params[FileName::BROKER] = ParamPickerMake(broker);

  // To evaluate the policy we need to call back to the policy object. We
  // are just middlemen in the operation since is the FileSystemPolicy which
  // knows what to do.
  EvalResult result = policy_base_->EvalPolicy(
      IpcTag::NTQUERYFULLATTRIBUTESFILE, params.GetBase());

  FILE_NETWORK_OPEN_INFORMATION* information =
      reinterpret_cast<FILE_NETWORK_OPEN_INFORMATION*>(info->Buffer());
  NTSTATUS nt_status;
  if (!FileSystemPolicy::QueryFullAttributesFileAction(
          result, *ipc->client_info, *name, attributes, information,
          &nt_status)) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }

  // Return operation status on the IPC.
  ipc->return_info.nt_status = nt_status;
  return true;
}

bool FilesystemDispatcher::NtSetInformationFile(IPCInfo* ipc,
                                                HANDLE handle,
                                                CountedBuffer* status,
                                                CountedBuffer* info,
                                                uint32_t length,
                                                uint32_t info_class) {
  if (sizeof(IO_STATUS_BLOCK) != status->Size())
    return false;
  if (length != info->Size())
    return false;

  FILE_RENAME_INFORMATION* rename_info =
      reinterpret_cast<FILE_RENAME_INFORMATION*>(info->Buffer());

  if (!IsSupportedRenameCall(rename_info, length, info_class))
    return false;

  std::wstring name;
  name.assign(rename_info->FileName,
              rename_info->FileNameLength / sizeof(rename_info->FileName[0]));
  if (!PreProcessName(&name)) {
    // The path requested might contain a reparse point.
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }

  uint32_t broker = BROKER_TRUE;
  const wchar_t* filename = name.c_str();
  CountedParameterSet<FileName> params;
  params[FileName::NAME] = ParamPickerMake(filename);
  params[FileName::BROKER] = ParamPickerMake(broker);

  // To evaluate the policy we need to call back to the policy object. We
  // are just middlemen in the operation since is the FileSystemPolicy which
  // knows what to do.
  EvalResult result =
      policy_base_->EvalPolicy(IpcTag::NTSETINFO_RENAME, params.GetBase());

  IO_STATUS_BLOCK* io_status =
      reinterpret_cast<IO_STATUS_BLOCK*>(status->Buffer());
  NTSTATUS nt_status;
  if (!FileSystemPolicy::SetInformationFileAction(
          result, *ipc->client_info, handle, rename_info, length, info_class,
          io_status, &nt_status)) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }

  // Return operation status on the IPC.
  ipc->return_info.nt_status = nt_status;
  return true;
}

}  // namespace sandbox
