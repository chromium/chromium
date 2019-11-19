// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sync_policy.h"

#include <stdint.h>

#include <string>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/sandbox_utils.h"
#include "sandbox/win/src/sync_interception.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

// Provides functionality to resolve a symbolic link within the object
// directory passed in.
NTSTATUS ResolveSymbolicLink(const std::wstring& directory_name,
                             const std::wstring& name,
                             std::wstring* target) {
  NtOpenDirectoryObjectFunction NtOpenDirectoryObject = nullptr;
  ResolveNTFunctionPtr("NtOpenDirectoryObject", &NtOpenDirectoryObject);

  NtQuerySymbolicLinkObjectFunction NtQuerySymbolicLinkObject = nullptr;
  ResolveNTFunctionPtr("NtQuerySymbolicLinkObject", &NtQuerySymbolicLinkObject);

  NtOpenSymbolicLinkObjectFunction NtOpenSymbolicLinkObject = nullptr;
  ResolveNTFunctionPtr("NtOpenSymbolicLinkObject", &NtOpenSymbolicLinkObject);

  NtCloseFunction NtClose = nullptr;
  ResolveNTFunctionPtr("NtClose", &NtClose);

  OBJECT_ATTRIBUTES symbolic_link_directory_attributes = {};
  UNICODE_STRING symbolic_link_directory_string = {};
  InitObjectAttribs(directory_name, OBJ_CASE_INSENSITIVE, nullptr,
                    &symbolic_link_directory_attributes,
                    &symbolic_link_directory_string, nullptr);

  HANDLE symbolic_link_directory = nullptr;
  NTSTATUS status =
      NtOpenDirectoryObject(&symbolic_link_directory, DIRECTORY_QUERY,
                            &symbolic_link_directory_attributes);
  if (!NT_SUCCESS(status))
    return status;

  OBJECT_ATTRIBUTES symbolic_link_attributes = {};
  UNICODE_STRING name_string = {};
  InitObjectAttribs(name, OBJ_CASE_INSENSITIVE, symbolic_link_directory,
                    &symbolic_link_attributes, &name_string, nullptr);

  HANDLE symbolic_link = nullptr;
  status = NtOpenSymbolicLinkObject(&symbolic_link, GENERIC_READ,
                                    &symbolic_link_attributes);
  CHECK(NT_SUCCESS(NtClose(symbolic_link_directory)));
  if (!NT_SUCCESS(status))
    return status;

  UNICODE_STRING target_path = {};
  unsigned long target_length = 0;
  status =
      NtQuerySymbolicLinkObject(symbolic_link, &target_path, &target_length);
  if (status != STATUS_BUFFER_TOO_SMALL) {
    CHECK(NT_SUCCESS(NtClose(symbolic_link)));
    return status;
  }

  target_path.Length = 0;
  target_path.MaximumLength = static_cast<USHORT>(target_length);
  target_path.Buffer = new wchar_t[target_path.MaximumLength + 1];
  status =
      NtQuerySymbolicLinkObject(symbolic_link, &target_path, &target_length);
  if (NT_SUCCESS(status))
    target->assign(target_path.Buffer, target_length);

  CHECK(NT_SUCCESS(NtClose(symbolic_link)));
  delete[] target_path.Buffer;
  return status;
}

NTSTATUS GetBaseNamedObjectsDirectory(HANDLE* directory) {
  static HANDLE base_named_objects_handle = nullptr;
  if (base_named_objects_handle) {
    *directory = base_named_objects_handle;
    return STATUS_SUCCESS;
  }

  NtOpenDirectoryObjectFunction NtOpenDirectoryObject = nullptr;
  ResolveNTFunctionPtr("NtOpenDirectoryObject", &NtOpenDirectoryObject);

  DWORD session_id = 0;
  ProcessIdToSessionId(::GetCurrentProcessId(), &session_id);

  std::wstring base_named_objects_path;

  NTSTATUS status = ResolveSymbolicLink(L"\\Sessions\\BNOLINKS",
                                        base::StringPrintf(L"%d", session_id),
                                        &base_named_objects_path);
  if (!NT_SUCCESS(status)) {
    DLOG(ERROR) << "Failed to resolve BaseNamedObjects path. Error: " << status;
    return status;
  }

  UNICODE_STRING directory_name = {};
  OBJECT_ATTRIBUTES object_attributes = {};
  InitObjectAttribs(base_named_objects_path, OBJ_CASE_INSENSITIVE, nullptr,
                    &object_attributes, &directory_name, nullptr);
  status = NtOpenDirectoryObject(&base_named_objects_handle,
                                 DIRECTORY_ALL_ACCESS, &object_attributes);
  if (NT_SUCCESS(status))
    *directory = base_named_objects_handle;
  return status;
}

bool SyncPolicy::GenerateRules(const wchar_t* name,
                               TargetPolicy::Semantics semantics,
                               LowLevelPolicy* policy) {
  std::wstring mod_name(name);
  if (mod_name.empty()) {
    return false;
  }

  if (TargetPolicy::EVENTS_ALLOW_ANY != semantics &&
      TargetPolicy::EVENTS_ALLOW_READONLY != semantics) {
    // Other flags are not valid for sync policy yet.
    NOTREACHED();
    return false;
  }

  // Add the open rule.
  EvalResult result = ASK_BROKER;
  PolicyRule open(result);

  if (!open.AddStringMatch(IF, OpenEventParams::NAME, name, CASE_INSENSITIVE))
    return false;

  if (TargetPolicy::EVENTS_ALLOW_READONLY == semantics) {
    // We consider all flags that are not known to be readonly as potentially
    // used for write.
    uint32_t allowed_flags = SYNCHRONIZE | GENERIC_READ | READ_CONTROL;
    uint32_t restricted_flags = ~allowed_flags;
    open.AddNumberMatch(IF_NOT, OpenEventParams::ACCESS, restricted_flags, AND);
  }

  if (!policy->AddRule(IpcTag::OPENEVENT, &open))
    return false;

  // If it's not a read only, add the create rule.
  if (TargetPolicy::EVENTS_ALLOW_READONLY != semantics) {
    PolicyRule create(result);
    if (!create.AddStringMatch(IF, NameBased::NAME, name, CASE_INSENSITIVE))
      return false;

    if (!policy->AddRule(IpcTag::CREATEEVENT, &create))
      return false;
  }

  return true;
}

NTSTATUS SyncPolicy::CreateEventAction(EvalResult eval_result,
                                       const ClientInfo& client_info,
                                       const std::wstring& event_name,
                                       uint32_t event_type,
                                       uint32_t initial_state,
                                       HANDLE* handle) {
  NtCreateEventFunction NtCreateEvent = nullptr;
  ResolveNTFunctionPtr("NtCreateEvent", &NtCreateEvent);

  // The only action supported is ASK_BROKER which means create the requested
  // file as specified.
  if (ASK_BROKER != eval_result)
    return false;

  HANDLE object_directory = nullptr;
  NTSTATUS status = GetBaseNamedObjectsDirectory(&object_directory);
  if (status != STATUS_SUCCESS)
    return status;

  UNICODE_STRING unicode_event_name = {};
  OBJECT_ATTRIBUTES object_attributes = {};
  InitObjectAttribs(event_name, OBJ_CASE_INSENSITIVE, object_directory,
                    &object_attributes, &unicode_event_name, nullptr);

  HANDLE local_handle = nullptr;
  status = NtCreateEvent(&local_handle, EVENT_ALL_ACCESS, &object_attributes,
                         static_cast<EVENT_TYPE>(event_type),
                         static_cast<BOOLEAN>(initial_state != 0));
  if (!local_handle)
    return status;

  if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                         client_info.process, handle, 0, false,
                         DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    return STATUS_ACCESS_DENIED;
  }
  return status;
}

NTSTATUS SyncPolicy::OpenEventAction(EvalResult eval_result,
                                     const ClientInfo& client_info,
                                     const std::wstring& event_name,
                                     uint32_t desired_access,
                                     HANDLE* handle) {
  NtOpenEventFunction NtOpenEvent = nullptr;
  ResolveNTFunctionPtr("NtOpenEvent", &NtOpenEvent);

  // The only action supported is ASK_BROKER which means create the requested
  // event as specified.
  if (ASK_BROKER != eval_result)
    return false;

  HANDLE object_directory = nullptr;
  NTSTATUS status = GetBaseNamedObjectsDirectory(&object_directory);
  if (status != STATUS_SUCCESS)
    return status;

  UNICODE_STRING unicode_event_name = {};
  OBJECT_ATTRIBUTES object_attributes = {};
  InitObjectAttribs(event_name, OBJ_CASE_INSENSITIVE, object_directory,
                    &object_attributes, &unicode_event_name, nullptr);

  HANDLE local_handle = nullptr;
  status = NtOpenEvent(&local_handle, desired_access, &object_attributes);
  if (!local_handle)
    return status;

  if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                         client_info.process, handle, 0, false,
                         DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    return STATUS_ACCESS_DENIED;
  }
  return status;
}

}  // namespace sandbox
