// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/signed_interception.h"

#include <stdint.h>

#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/policy_target.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"
#include "sandbox/win/src/target_services.h"

namespace sandbox {

NTSTATUS WINAPI
TargetNtCreateSection(NtCreateSectionFunction orig_CreateSection,
                      PHANDLE section_handle,
                      ACCESS_MASK desired_access,
                      POBJECT_ATTRIBUTES object_attributes,
                      PLARGE_INTEGER maximum_size,
                      ULONG section_page_protection,
                      ULONG allocation_attributes,
                      HANDLE file_handle) {
  do {
    // The section only needs to have SECTION_MAP_EXECUTE, but the permissions
    // vary depending on the OS. Windows 1903 and higher requests (SECTION_QUERY
    // | SECTION_MAP_READ | SECTION_MAP_EXECUTE) while previous OS versions also
    // request SECTION_MAP_WRITE. Just check for EXECUTE.
    if (!(desired_access & SECTION_MAP_EXECUTE))
      break;
    if (object_attributes)
      break;
    if (maximum_size)
      break;
    if (section_page_protection != PAGE_EXECUTE)
      break;
    if (allocation_attributes != SEC_IMAGE)
      break;

    // IPC must be fully started.
    void* memory = GetGlobalIPCMemory();
    if (!memory)
      break;

    std::unique_ptr<wchar_t, NtAllocDeleter> path;

    if (!NtGetPathFromHandle(file_handle, &path))
      break;

    const wchar_t* const_name = path.get();

    CountedParameterSet<NameBased> params;
    params[NameBased::NAME] = ParamPickerMake(const_name);

    // Check if this will be sent to the broker.
    if (!QueryBroker(IpcTag::NTCREATESECTION, params.GetBase()))
      break;

    if (!ValidParameter(section_handle, sizeof(HANDLE), WRITE))
      break;

    CrossCallReturn answer = {0};
    answer.nt_status = STATUS_INVALID_IMAGE_HASH;
    SharedMemIPCClient ipc(memory);
    ResultCode code =
        CrossCall(ipc, IpcTag::NTCREATESECTION, file_handle, &answer);

    if (code != SBOX_ALL_OK)
      break;

    if (!NT_SUCCESS(answer.nt_status))
      break;

    __try {
      *section_handle = answer.handle;
      return answer.nt_status;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      break;
    }
  } while (false);

  // Fall back to the original API in all failure cases.
  return orig_CreateSection(section_handle, desired_access, object_attributes,
                            maximum_size, section_page_protection,
                            allocation_attributes, file_handle);
}

}  // namespace sandbox
