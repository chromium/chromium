// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/filesystem_interception.h"

#include <ntstatus.h>
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
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

namespace {
// This checks for three conditions on whether to ask the broker.
// - The path looks like a DOS device path (namely \??\something).
// - The path looks like a short-name path.
// - Whether the details match the policy.
bool ShouldAskBroker(IpcTag ipc_tag,
                     std::wstring_view name,
                     uint32_t desired_access = 0,
                     bool open_only = true) {
  if (name.size() >= 4 && name[0] == L'\\' && name[1] == L'?' &&
      name[2] == L'?' && name[3] == L'\\') {
    return true;
  }

  if (name.find(L'~') != std::wstring_view::npos) {
    return true;
  }

  CountedParameterSet<OpenFile> params;
  params[OpenFile::NAME] = ParamPickerMake(name);
  params[OpenFile::ACCESS] = ParamPickerMake(desired_access);
  uint32_t open_only_int = open_only;
  params[OpenFile::OPENONLY] = ParamPickerMake(open_only_int);
  return QueryBroker(ipc_tag, params.GetBase());
}

bool ValidateObjectAttributes(const OBJECT_ATTRIBUTES* in_object,
                              std::wstring_view& name,
                              uint32_t& attributes) {
  __try {
    if (in_object->RootDirectory != nullptr || !in_object->ObjectName ||
        !in_object->ObjectName->Buffer) {
      return false;
    }

    name = {in_object->ObjectName->Buffer,
            in_object->ObjectName->Length / sizeof(wchar_t)};
    // We don't support embedded NUL characters. This also acts as a test for
    // the string buffer memory being valid.
    if (ContainsNulCharacter(name)) {
      return false;
    }
    attributes = in_object->Attributes;
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
  return false;
}
}  // namespace

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
  if (STATUS_ACCESS_DENIED != status) {
    return status;
  }

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled()) {
    return status;
  }

  do {
    if (!ValidParameter(file, sizeof(HANDLE), WRITE)) {
      break;
    }
    if (!ValidParameter(io_status, sizeof(IO_STATUS_BLOCK), WRITE)) {
      break;
    }
    // From around 22H2 19045.2846 CopyFileExW() uses the ea_buffer to pass
    // extra FILE_CONTAINS_EXTENDED_CREATE_INFORMATION flags. No sandboxed
    // processes need to have CopyFile succeed so we do not broker these calls.
    if ((options & FILE_VALID_OPTION_FLAGS) != options) {
      break;
    }

    void* memory = GetGlobalIPCMemory();
    if (!memory) {
      break;
    }

    std::wstring_view name;
    uint32_t attributes;

    if (!ValidateObjectAttributes(object_attributes, name, attributes)) {
      break;
    }
    if (!ShouldAskBroker(IpcTag::NTCREATEFILE, name, desired_access,
                         disposition == FILE_OPEN)) {
      break;
    }

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    // The following call must match in the parameters with
    // FilesystemDispatcher::ProcessNtCreateFile.
    ResultCode code =
        CrossCall(ipc, IpcTag::NTCREATEFILE, name, attributes, desired_access,
                  file_attributes, sharing, disposition, options, &answer);
    if (SBOX_ALL_OK != code) {
      break;
    }

    status = answer.nt_status;

    if (!NT_SUCCESS(answer.nt_status)) {
      break;
    }

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

    std::wstring_view name;
    uint32_t attributes;
    if (!ValidateObjectAttributes(object_attributes, name, attributes)) {
      break;
    }
    if (!ShouldAskBroker(IpcTag::NTOPENFILE, name, desired_access, true)) {
      break;
    }

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IpcTag::NTOPENFILE, name, attributes,
                                desired_access, sharing, options, &answer);
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

    std::wstring_view name;
    uint32_t attributes;
    if (!ValidateObjectAttributes(object_attributes, name, attributes)) {
      break;
    }
    if (!ShouldAskBroker(IpcTag::NTQUERYATTRIBUTESFILE, name)) {
      break;
    }

    InOutCountedBuffer file_info(file_attributes,
                                 sizeof(FILE_BASIC_INFORMATION));
    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IpcTag::NTQUERYATTRIBUTESFILE, name,
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

    std::wstring_view name;
    uint32_t attributes;
    if (!ValidateObjectAttributes(object_attributes, name, attributes)) {
      break;
    }
    if (!ShouldAskBroker(IpcTag::NTQUERYFULLATTRIBUTESFILE, name)) {
      break;
    }

    InOutCountedBuffer file_info(file_attributes,
                                 sizeof(FILE_NETWORK_OPEN_INFORMATION));
    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IpcTag::NTQUERYFULLATTRIBUTESFILE, name,
                                attributes, file_info, &answer);

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
    std::wstring_view name;
    __try {
      if (!IsSupportedRenameCall(file_rename_info, length, file_info_class))
        break;

      name = {file_rename_info->FileName,
              file_rename_info->FileNameLength / sizeof(wchar_t)};
      if (ContainsNulCharacter(name)) {
        break;
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      break;
    }

    if (!ShouldAskBroker(IpcTag::NTSETINFO_RENAME, name)) {
      break;
    }

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
