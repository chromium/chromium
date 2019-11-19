// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/filesystem_interception.h"

#include <stdint.h>

#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/filesystem_policy.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/policy_target.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"
#include "sandbox/win/src/target_services.h"

namespace sandbox {

NTSTATUS WINAPI TargetNtCreateFile(NtCreateFileFunction orig_CreateFile,
                                   PHANDLE file,
                                   ACCESS_MASK desired_access,
                                   POBJECT_ATTRIBUTES object_attributes,
                                   PIO_STATUS_BLOCK io_status,
                                   PLARGE_INTEGER allocation_size,
                                   ULONG file_attributes,
                                   ULONG sharing,
                                   ULONG disposition,
                                   ULONG options,
                                   PVOID ea_buffer,
                                   ULONG ea_length) {
  // Check if the process can open it first.
  NTSTATUS status = orig_CreateFile(
      file, desired_access, object_attributes, io_status, allocation_size,
      file_attributes, sharing, disposition, options, ea_buffer, ea_length);
  if (STATUS_ACCESS_DENIED != status)
    return status;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return status;

  do {
    if (!ValidParameter(file, sizeof(HANDLE), WRITE))
      break;
    if (!ValidParameter(io_status, sizeof(IO_STATUS_BLOCK), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (!memory)
      break;

    std::unique_ptr<wchar_t, NtAllocDeleter> name;
    uint32_t attributes = 0;
    NTSTATUS ret =
        AllocAndCopyName(object_attributes, &name, &attributes, nullptr);
    if (!NT_SUCCESS(ret) || !name)
      break;

    uint32_t desired_access_uint32 = desired_access;
    uint32_t options_uint32 = options;
    uint32_t disposition_uint32 = disposition;
    uint32_t broker = BROKER_FALSE;
    CountedParameterSet<OpenFile> params;
    const wchar_t* name_ptr = name.get();
    params[OpenFile::NAME] = ParamPickerMake(name_ptr);
    params[OpenFile::ACCESS] = ParamPickerMake(desired_access_uint32);
    params[OpenFile::DISPOSITION] = ParamPickerMake(disposition_uint32);
    params[OpenFile::OPTIONS] = ParamPickerMake(options_uint32);
    params[OpenFile::BROKER] = ParamPickerMake(broker);

    if (!QueryBroker(IpcTag::NTCREATEFILE, params.GetBase()))
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    // The following call must match in the parameters with
    // FilesystemDispatcher::ProcessNtCreateFile.
    ResultCode code =
        CrossCall(ipc, IpcTag::NTCREATEFILE, name.get(), attributes,
                  desired_access_uint32, file_attributes, sharing, disposition,
                  options_uint32, &answer);
    if (SBOX_ALL_OK != code)
      break;

    status = answer.nt_status;

    if (!NT_SUCCESS(answer.nt_status))
      break;

    __try {
      *file = answer.handle;
      io_status->Status = answer.nt_status;
      io_status->Information = answer.extended[0].ulong_ptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      break;
    }
  } while (false);

  return status;
}

NTSTATUS WINAPI TargetNtOpenFile(NtOpenFileFunction orig_OpenFile,
                                 PHANDLE file,
                                 ACCESS_MASK desired_access,
                                 POBJECT_ATTRIBUTES object_attributes,
                                 PIO_STATUS_BLOCK io_status,
                                 ULONG sharing,
                                 ULONG options) {
  // Check if the process can open it first.
  NTSTATUS status = orig_OpenFile(file, desired_access, object_attributes,
                                  io_status, sharing, options);
  if (STATUS_ACCESS_DENIED != status)
    return status;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return status;

  do {
    if (!ValidParameter(file, sizeof(HANDLE), WRITE))
      break;
    if (!ValidParameter(io_status, sizeof(IO_STATUS_BLOCK), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (!memory)
      break;

    std::unique_ptr<wchar_t, NtAllocDeleter> name;
    uint32_t attributes;
    NTSTATUS ret =
        AllocAndCopyName(object_attributes, &name, &attributes, nullptr);
    if (!NT_SUCCESS(ret) || !name)
      break;

    uint32_t desired_access_uint32 = desired_access;
    uint32_t options_uint32 = options;
    uint32_t disposition_uint32 = FILE_OPEN;
    uint32_t broker = BROKER_FALSE;
    const wchar_t* name_ptr = name.get();
    CountedParameterSet<OpenFile> params;
    params[OpenFile::NAME] = ParamPickerMake(name_ptr);
    params[OpenFile::ACCESS] = ParamPickerMake(desired_access_uint32);
    params[OpenFile::DISPOSITION] = ParamPickerMake(disposition_uint32);
    params[OpenFile::OPTIONS] = ParamPickerMake(options_uint32);
    params[OpenFile::BROKER] = ParamPickerMake(broker);

    if (!QueryBroker(IpcTag::NTOPENFILE, params.GetBase()))
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code =
        CrossCall(ipc, IpcTag::NTOPENFILE, name.get(), attributes,
                  desired_access_uint32, sharing, options_uint32, &answer);
    if (SBOX_ALL_OK != code)
      break;

    status = answer.nt_status;

    if (!NT_SUCCESS(answer.nt_status))
      break;

    __try {
      *file = answer.handle;
      io_status->Status = answer.nt_status;
      io_status->Information = answer.extended[0].ulong_ptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      break;
    }
  } while (false);

  return status;
}

NTSTATUS WINAPI
TargetNtQueryAttributesFile(NtQueryAttributesFileFunction orig_QueryAttributes,
                            POBJECT_ATTRIBUTES object_attributes,
                            PFILE_BASIC_INFORMATION file_attributes) {
  // Check if the process can query it first.
  NTSTATUS status = orig_QueryAttributes(object_attributes, file_attributes);
  if (STATUS_ACCESS_DENIED != status)
    return status;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return status;

  do {
    if (!ValidParameter(file_attributes, sizeof(FILE_BASIC_INFORMATION), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (!memory)
      break;

    std::unique_ptr<wchar_t, NtAllocDeleter> name;
    uint32_t attributes = 0;
    NTSTATUS ret =
        AllocAndCopyName(object_attributes, &name, &attributes, nullptr);
    if (!NT_SUCCESS(ret) || !name)
      break;

    InOutCountedBuffer file_info(file_attributes,
                                 sizeof(FILE_BASIC_INFORMATION));

    uint32_t broker = BROKER_FALSE;
    CountedParameterSet<FileName> params;
    const wchar_t* name_ptr = name.get();
    params[FileName::NAME] = ParamPickerMake(name_ptr);
    params[FileName::BROKER] = ParamPickerMake(broker);

    if (!QueryBroker(IpcTag::NTQUERYATTRIBUTESFILE, params.GetBase()))
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IpcTag::NTQUERYATTRIBUTESFILE, name.get(),
                                attributes, file_info, &answer);

    if (SBOX_ALL_OK != code)
      break;

    status = answer.nt_status;

  } while (false);

  return status;
}

NTSTATUS WINAPI TargetNtQueryFullAttributesFile(
    NtQueryFullAttributesFileFunction orig_QueryFullAttributes,
    POBJECT_ATTRIBUTES object_attributes,
    PFILE_NETWORK_OPEN_INFORMATION file_attributes) {
  // Check if the process can query it first.
  NTSTATUS status =
      orig_QueryFullAttributes(object_attributes, file_attributes);
  if (STATUS_ACCESS_DENIED != status)
    return status;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return status;

  do {
    if (!ValidParameter(file_attributes, sizeof(FILE_NETWORK_OPEN_INFORMATION),
                        WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (!memory)
      break;

    std::unique_ptr<wchar_t, NtAllocDeleter> name;
    uint32_t attributes = 0;
    NTSTATUS ret =
        AllocAndCopyName(object_attributes, &name, &attributes, nullptr);
    if (!NT_SUCCESS(ret) || !name)
      break;

    InOutCountedBuffer file_info(file_attributes,
                                 sizeof(FILE_NETWORK_OPEN_INFORMATION));

    uint32_t broker = BROKER_FALSE;
    CountedParameterSet<FileName> params;
    const wchar_t* name_ptr = name.get();
    params[FileName::NAME] = ParamPickerMake(name_ptr);
    params[FileName::BROKER] = ParamPickerMake(broker);

    if (!QueryBroker(IpcTag::NTQUERYFULLATTRIBUTESFILE, params.GetBase()))
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IpcTag::NTQUERYFULLATTRIBUTESFILE,
                                name.get(), attributes, file_info, &answer);

    if (SBOX_ALL_OK != code)
      break;

    status = answer.nt_status;
  } while (false);

  return status;
}

NTSTATUS WINAPI
TargetNtSetInformationFile(NtSetInformationFileFunction orig_SetInformationFile,
                           HANDLE file,
                           PIO_STATUS_BLOCK io_status,
                           PVOID file_info,
                           ULONG length,
                           FILE_INFORMATION_CLASS file_info_class) {
  // Check if the process can open it first.
  NTSTATUS status = orig_SetInformationFile(file, io_status, file_info, length,
                                            file_info_class);
  if (STATUS_ACCESS_DENIED != status)
    return status;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return status;

  do {
    void* memory = GetGlobalIPCMemory();
    if (!memory)
      break;

    if (!ValidParameter(io_status, sizeof(IO_STATUS_BLOCK), WRITE))
      break;

    if (!ValidParameter(file_info, length, READ))
      break;

    FILE_RENAME_INFORMATION* file_rename_info =
        reinterpret_cast<FILE_RENAME_INFORMATION*>(file_info);
    OBJECT_ATTRIBUTES object_attributes;
    UNICODE_STRING object_name;
    InitializeObjectAttributes(&object_attributes, &object_name, 0, nullptr,
                               nullptr);

    __try {
      if (!IsSupportedRenameCall(file_rename_info, length, file_info_class))
        break;

      object_attributes.RootDirectory = file_rename_info->RootDirectory;
      object_name.Buffer = file_rename_info->FileName;
      object_name.Length = object_name.MaximumLength =
          static_cast<USHORT>(file_rename_info->FileNameLength);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      break;
    }

    std::unique_ptr<wchar_t, NtAllocDeleter> name;
    NTSTATUS ret =
        AllocAndCopyName(&object_attributes, &name, nullptr, nullptr);
    if (!NT_SUCCESS(ret) || !name)
      break;

    uint32_t broker = BROKER_FALSE;
    CountedParameterSet<FileName> params;
    const wchar_t* name_ptr = name.get();
    params[FileName::NAME] = ParamPickerMake(name_ptr);
    params[FileName::BROKER] = ParamPickerMake(broker);

    if (!QueryBroker(IpcTag::NTSETINFO_RENAME, params.GetBase()))
      break;

    InOutCountedBuffer io_status_buffer(io_status, sizeof(IO_STATUS_BLOCK));
    // This is actually not an InOut buffer, only In, but using InOut facility
    // really helps to simplify the code.
    InOutCountedBuffer file_info_buffer(file_info, length);

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code =
        CrossCall(ipc, IpcTag::NTSETINFO_RENAME, file, io_status_buffer,
                  file_info_buffer, length, file_info_class, &answer);

    if (SBOX_ALL_OK != code)
      break;

    status = answer.nt_status;
  } while (false);

  return status;
}

}  // namespace sandbox
