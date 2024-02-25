// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/signed_interception.h"

#include <ntstatus.h>
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

// Note that this shim may be called before the heap is available, we must get
// as far as |QueryBroker| without using the heap, for example when AppVerifier
// is enabled.
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

    // As mentioned at the top of the function, we need to use the stack here
    // because the heap may not be available.
    constexpr ULONG path_buffer_size =
        (MAX_PATH * sizeof(wchar_t)) + sizeof(OBJECT_NAME_INFORMATION);
    // Avoid memset inserted by -ftrivial-auto-var-init=pattern.
    STACK_UNINITIALIZED char path_buffer[path_buffer_size];
    OBJECT_NAME_INFORMATION* path =
        reinterpret_cast<OBJECT_NAME_INFORMATION*>(path_buffer);
    ULONG out_buffer_size = 0;
    NTSTATUS status =
        GetNtExports()->QueryObject(file_handle, ObjectNameInformation, path,
                                    path_buffer_size, &out_buffer_size);

    if (!NT_SUCCESS(status)) {
      break;
    }

    CountedParameterSet<NameBased> params;
    params[NameBased::NAME] = ParamPickerMake(path->ObjectName.Buffer);

    // Check if this will be sent to the broker.
    if (!QueryBroker(IpcTag::NTCREATESECTION, params.GetBase()))
      break;

    if (!ValidParameter(section_handle, sizeof(HANDLE), WRITE))
      break;

    // Avoid memset inserted by -ftrivial-auto-var-init=pattern on debug builds.
    STACK_UNINITIALIZED CrossCallReturn answer;
    Memset(&answer, 0, sizeof(answer));

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
