// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_thread_policy.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/free_deleter.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/win_utils.h"

namespace {

// These are the only safe rights that can be given to a sandboxed
// process for the process created by the broker. All others are potential
// vectors of privilege elevation.
const DWORD kProcessRights = SYNCHRONIZE | PROCESS_QUERY_INFORMATION |
                             PROCESS_QUERY_LIMITED_INFORMATION |
                             PROCESS_TERMINATE | PROCESS_SUSPEND_RESUME;

const DWORD kThreadRights = SYNCHRONIZE | THREAD_TERMINATE |
                            THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION |
                            THREAD_QUERY_LIMITED_INFORMATION |
                            THREAD_SET_LIMITED_INFORMATION;

// Creates a child process and duplicates the handles to 'target_process'. The
// remaining parameters are the same as CreateProcess().
bool CreateProcessExWHelper(HANDLE target_process,
                            bool give_full_access,
                            LPCWSTR lpApplicationName,
                            LPWSTR lpCommandLine,
                            LPSECURITY_ATTRIBUTES lpProcessAttributes,
                            LPSECURITY_ATTRIBUTES lpThreadAttributes,
                            bool bInheritHandles,
                            DWORD dwCreationFlags,
                            LPVOID lpEnvironment,
                            LPCWSTR lpCurrentDirectory,
                            LPSTARTUPINFOW lpStartupInfo,
                            LPPROCESS_INFORMATION lpProcessInformation) {
  if (!::CreateProcessW(lpApplicationName, lpCommandLine, lpProcessAttributes,
                        lpThreadAttributes, bInheritHandles, dwCreationFlags,
                        lpEnvironment, lpCurrentDirectory, lpStartupInfo,
                        lpProcessInformation)) {
    return false;
  }

  DWORD process_access = kProcessRights;
  DWORD thread_access = kThreadRights;
  if (give_full_access) {
    process_access = PROCESS_ALL_ACCESS;
    thread_access = THREAD_ALL_ACCESS;
  }
  if (!::DuplicateHandle(::GetCurrentProcess(), lpProcessInformation->hProcess,
                         target_process, &lpProcessInformation->hProcess,
                         process_access, false, DUPLICATE_CLOSE_SOURCE)) {
    ::CloseHandle(lpProcessInformation->hThread);
    return false;
  }
  if (!::DuplicateHandle(::GetCurrentProcess(), lpProcessInformation->hThread,
                         target_process, &lpProcessInformation->hThread,
                         thread_access, false, DUPLICATE_CLOSE_SOURCE)) {
    return false;
  }
  return true;
}

}  // namespace

namespace sandbox {

bool ProcessPolicy::GenerateRules(const wchar_t* name,
                                  TargetPolicy::Semantics semantics,
                                  LowLevelPolicy* policy) {
  std::unique_ptr<PolicyRule> process;
  switch (semantics) {
    case TargetPolicy::PROCESS_MIN_EXEC: {
      process.reset(new PolicyRule(GIVE_READONLY));
      break;
    };
    case TargetPolicy::PROCESS_ALL_EXEC: {
      process.reset(new PolicyRule(GIVE_ALLACCESS));
      break;
    };
    default: { return false; };
  }

  if (!process->AddStringMatch(IF, NameBased::NAME, name, CASE_INSENSITIVE)) {
    return false;
  }
  if (!policy->AddRule(IpcTag::CREATEPROCESSW, process.get())) {
    return false;
  }
  return true;
}

NTSTATUS ProcessPolicy::OpenThreadAction(const ClientInfo& client_info,
                                         uint32_t desired_access,
                                         uint32_t thread_id,
                                         HANDLE* handle) {
  *handle = nullptr;

  NtOpenThreadFunction NtOpenThread = nullptr;
  ResolveNTFunctionPtr("NtOpenThread", &NtOpenThread);

  OBJECT_ATTRIBUTES attributes = {0};
  attributes.Length = sizeof(attributes);
  CLIENT_ID client_id = {0};
  client_id.UniqueProcess =
      reinterpret_cast<PVOID>(static_cast<ULONG_PTR>(client_info.process_id));
  client_id.UniqueThread =
      reinterpret_cast<PVOID>(static_cast<ULONG_PTR>(thread_id));

  HANDLE local_handle = nullptr;
  NTSTATUS status =
      NtOpenThread(&local_handle, desired_access, &attributes, &client_id);
  if (NT_SUCCESS(status)) {
    if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                           client_info.process, handle, 0, false,
                           DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
      return STATUS_ACCESS_DENIED;
    }
  }

  return status;
}

NTSTATUS ProcessPolicy::OpenProcessAction(const ClientInfo& client_info,
                                          uint32_t desired_access,
                                          uint32_t process_id,
                                          HANDLE* handle) {
  *handle = nullptr;

  NtOpenProcessFunction NtOpenProcess = nullptr;
  ResolveNTFunctionPtr("NtOpenProcess", &NtOpenProcess);

  if (client_info.process_id != process_id)
    return STATUS_ACCESS_DENIED;

  OBJECT_ATTRIBUTES attributes = {0};
  attributes.Length = sizeof(attributes);
  CLIENT_ID client_id = {0};
  client_id.UniqueProcess =
      reinterpret_cast<PVOID>(static_cast<ULONG_PTR>(client_info.process_id));
  HANDLE local_handle = nullptr;
  NTSTATUS status =
      NtOpenProcess(&local_handle, desired_access, &attributes, &client_id);
  if (NT_SUCCESS(status)) {
    if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                           client_info.process, handle, 0, false,
                           DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
      return STATUS_ACCESS_DENIED;
    }
  }

  return status;
}

NTSTATUS ProcessPolicy::OpenProcessTokenAction(const ClientInfo& client_info,
                                               HANDLE process,
                                               uint32_t desired_access,
                                               HANDLE* handle) {
  *handle = nullptr;
  NtOpenProcessTokenFunction NtOpenProcessToken = nullptr;
  ResolveNTFunctionPtr("NtOpenProcessToken", &NtOpenProcessToken);

  if (CURRENT_PROCESS != process)
    return STATUS_ACCESS_DENIED;

  HANDLE local_handle = nullptr;
  NTSTATUS status =
      NtOpenProcessToken(client_info.process, desired_access, &local_handle);
  if (NT_SUCCESS(status)) {
    if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                           client_info.process, handle, 0, false,
                           DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
      return STATUS_ACCESS_DENIED;
    }
  }
  return status;
}

NTSTATUS ProcessPolicy::OpenProcessTokenExAction(const ClientInfo& client_info,
                                                 HANDLE process,
                                                 uint32_t desired_access,
                                                 uint32_t attributes,
                                                 HANDLE* handle) {
  *handle = nullptr;
  NtOpenProcessTokenExFunction NtOpenProcessTokenEx = nullptr;
  ResolveNTFunctionPtr("NtOpenProcessTokenEx", &NtOpenProcessTokenEx);

  if (CURRENT_PROCESS != process)
    return STATUS_ACCESS_DENIED;

  HANDLE local_handle = nullptr;
  NTSTATUS status = NtOpenProcessTokenEx(client_info.process, desired_access,
                                         attributes, &local_handle);
  if (NT_SUCCESS(status)) {
    if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                           client_info.process, handle, 0, false,
                           DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
      return STATUS_ACCESS_DENIED;
    }
  }
  return status;
}

DWORD ProcessPolicy::CreateProcessWAction(EvalResult eval_result,
                                          const ClientInfo& client_info,
                                          const std::wstring& app_name,
                                          const std::wstring& command_line,
                                          const std::wstring& current_dir,
                                          PROCESS_INFORMATION* process_info) {
  // The only action supported is ASK_BROKER which means create the process.
  if (GIVE_ALLACCESS != eval_result && GIVE_READONLY != eval_result) {
    return ERROR_ACCESS_DENIED;
  }

  STARTUPINFO startup_info = {0};
  startup_info.cb = sizeof(startup_info);
  std::unique_ptr<wchar_t, base::FreeDeleter> cmd_line(
      _wcsdup(command_line.c_str()));

  bool should_give_full_access = (GIVE_ALLACCESS == eval_result);

  const wchar_t* cwd = current_dir.c_str();
  if (current_dir.empty())
    cwd = nullptr;

  if (!CreateProcessExWHelper(client_info.process, should_give_full_access,
                              app_name.c_str(), cmd_line.get(), nullptr,
                              nullptr, false, 0, nullptr, cwd, &startup_info,
                              process_info)) {
    return ERROR_ACCESS_DENIED;
  }
  return ERROR_SUCCESS;
}

DWORD ProcessPolicy::CreateThreadAction(
    const ClientInfo& client_info,
    const SIZE_T stack_size,
    const LPTHREAD_START_ROUTINE start_address,
    const LPVOID parameter,
    const DWORD creation_flags,
    LPDWORD thread_id,
    HANDLE* handle) {
  *handle = nullptr;
  HANDLE local_handle =
      ::CreateRemoteThread(client_info.process, nullptr, stack_size,
                           start_address, parameter, creation_flags, thread_id);
  if (!local_handle) {
    return ::GetLastError();
  }
  if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                         client_info.process, handle, 0, false,
                         DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    return ERROR_ACCESS_DENIED;
  }
  return ERROR_SUCCESS;
}

}  // namespace sandbox
