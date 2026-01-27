// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/signed_interception.h"

#include <ntstatus.h>

#include "base/win/win_util.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/policy_target.h"
#include "sandbox/win/src/sandbox_nt_util.h"

namespace sandbox {

namespace {

// Note that this shim may be called before the heap is available, we must get
// as far as `ReturnConst` without using the heap, for example when AppVerifier
// is enabled. Returns true if a constant handle was found and the handle was
// successfully duplicated.
bool CheckForConstantHandle(PHANDLE section_handle,
                            ACCESS_MASK desired_access,
                            POBJECT_ATTRIBUTES object_attributes,
                            PLARGE_INTEGER maximum_size,
                            ULONG section_page_protection,
                            ULONG allocation_attributes,
                            HANDLE file_handle) {
  // The section only needs to have SECTION_MAP_EXECUTE, but the permissions
  // vary depending on the OS. Windows 1903 and higher requests (SECTION_QUERY
  // | SECTION_MAP_READ | SECTION_MAP_EXECUTE) while previous OS versions also
  // request SECTION_MAP_WRITE. Just check for EXECUTE.
  if (!(desired_access & SECTION_MAP_EXECUTE)) {
    return false;
  }
  if (object_attributes) {
    return false;
  }
  if (maximum_size) {
    return false;
  }
  if (section_page_protection != PAGE_EXECUTE) {
    return false;
  }
  if (allocation_attributes != SEC_IMAGE) {
    return false;
  }
  // Policy memory must be initialized.
  if (!GetGlobalIPCMemory()) {
    return false;
  }
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
    return false;
  }

  CountedParameterSet<NameBased> params;
  std::wstring_view object_name =
      base::win::UnicodeStringToView(path->ObjectName);
  params[NameBased::NAME] = ParamPickerMake(object_name);

  auto handle = QueryReturnConst(IpcTag::NTCREATESECTION, params.GetBase());
  if (!handle) {
    return false;
  }

  status = GetNtExports()->DuplicateObject(
      CURRENT_PROCESS, reinterpret_cast<HANDLE>(*handle), CURRENT_PROCESS,
      section_handle, 0, 0, DUPLICATE_SAME_ACCESS);
  return NT_SUCCESS(status);
}

}  // namespace

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
  if (CheckForConstantHandle(section_handle, desired_access, object_attributes,
                             maximum_size, section_page_protection,
                             allocation_attributes, file_handle)) {
    return STATUS_SUCCESS;
  }

  // Fall back to the original API in all failure cases.
  return orig_CreateSection(section_handle, desired_access, object_attributes,
                            maximum_size, section_page_protection,
                            allocation_attributes, file_handle);
}

}  // namespace sandbox
