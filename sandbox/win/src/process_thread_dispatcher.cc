// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_thread_dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include "base/notreached.h"
#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_broker.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/process_thread_interception.h"
#include "sandbox/win/src/process_thread_policy.h"
#include "sandbox/win/src/sandbox.h"

namespace {

// Extracts the application name from a command line.
//
// The application name is the first element of the command line. If
// there is no quotes, the first element is delimited by the first space.
// If there are quotes, the first element is delimited by the quotes.
//
// The create process call is smarter than us. It tries really hard to launch
// the process even if the command line is wrong. For example:
// "c:\program files\test param" will first try to launch c:\program.exe then
// c:\program files\test.exe. We don't do that, we stop after at the first
// space when there is no quotes.
std::wstring GetPathFromCmdLine(const std::wstring& cmd_line) {
  std::wstring exe_name;
  // Check if it starts with '"'.
  if (cmd_line[0] == L'\"') {
    // Find the position of the second '"', this terminates the path.
    std::wstring::size_type pos = cmd_line.find(L'\"', 1);
    if (std::wstring::npos == pos)
      return cmd_line;
    exe_name = cmd_line.substr(1, pos - 1);
  } else {
    // There is no '"', that means that the appname is terminated at the
    // first space.
    std::wstring::size_type pos = cmd_line.find(L' ');
    if (std::wstring::npos == pos) {
      // There is no space, the cmd_line contains only the app_name
      exe_name = cmd_line;
    } else {
      exe_name = cmd_line.substr(0, pos);
    }
  }

  return exe_name;
}

// Returns true is the path in parameter is relative. False if it's
// absolute.
bool IsPathRelative(const std::wstring& path) {
  // A path is Relative if it's not a UNC path beginnning with \\ or a
  // path beginning with a drive. (i.e. X:\)
  if (path.find(L"\\\\") == 0 || path.find(L":\\") == 1)
    return false;
  return true;
}

// Converts a relative path to an absolute path.
bool ConvertToAbsolutePath(const std::wstring& child_current_directory,
                           bool use_env_path,
                           std::wstring* path) {
  wchar_t file_buffer[MAX_PATH];
  wchar_t* file_part = nullptr;

  // Here we should start by looking at the path where the child application was
  // started. We don't have this information yet.
  DWORD result = 0;
  if (use_env_path) {
    // Try with the complete path
    result = ::SearchPath(nullptr, path->c_str(), nullptr, MAX_PATH,
                          file_buffer, &file_part);
  }

  if (0 == result) {
    // Try with the current directory of the child
    result = ::SearchPath(child_current_directory.c_str(), path->c_str(),
                          nullptr, MAX_PATH, file_buffer, &file_part);
  }

  if (0 == result || result >= MAX_PATH)
    return false;

  *path = file_buffer;
  return true;
}

}  // namespace
namespace sandbox {

ThreadProcessDispatcher::ThreadProcessDispatcher(PolicyBase* policy_base)
    : policy_base_(policy_base) {
  static const IPCCall open_thread = {
      {IpcTag::NTOPENTHREAD, {UINT32_TYPE, UINT32_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ThreadProcessDispatcher::NtOpenThread)};

  static const IPCCall open_process = {
      {IpcTag::NTOPENPROCESS, {UINT32_TYPE, UINT32_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ThreadProcessDispatcher::NtOpenProcess)};

  static const IPCCall process_token = {
      {IpcTag::NTOPENPROCESSTOKEN, {VOIDPTR_TYPE, UINT32_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ThreadProcessDispatcher::NtOpenProcessToken)};

  static const IPCCall process_tokenex = {
      {IpcTag::NTOPENPROCESSTOKENEX, {VOIDPTR_TYPE, UINT32_TYPE, UINT32_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ThreadProcessDispatcher::NtOpenProcessTokenEx)};

  static const IPCCall create_params = {
      {IpcTag::CREATEPROCESSW,
       {WCHAR_TYPE, WCHAR_TYPE, WCHAR_TYPE, WCHAR_TYPE, INOUTPTR_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ThreadProcessDispatcher::CreateProcessW)};

  // NOTE(liamjm): 2nd param is size_t: Using VOIDPTR_TYPE as they are
  // the same size on windows.
  static_assert(sizeof(size_t) == sizeof(void*),
                "VOIDPTR_TYPE not same size as size_t");
  static const IPCCall create_thread_params = {
      {IpcTag::CREATETHREAD,
       {VOIDPTR_TYPE, VOIDPTR_TYPE, VOIDPTR_TYPE, UINT32_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ThreadProcessDispatcher::CreateThread)};

  ipc_calls_.push_back(open_thread);
  ipc_calls_.push_back(open_process);
  ipc_calls_.push_back(process_token);
  ipc_calls_.push_back(process_tokenex);
  ipc_calls_.push_back(create_params);
  ipc_calls_.push_back(create_thread_params);
}

bool ThreadProcessDispatcher::SetupService(InterceptionManager* manager,
                                           IpcTag service) {
  switch (service) {
    case IpcTag::NTOPENTHREAD:
    case IpcTag::NTOPENPROCESS:
    case IpcTag::NTOPENPROCESSTOKEN:
    case IpcTag::NTOPENPROCESSTOKENEX:
    case IpcTag::CREATETHREAD:
      // There is no explicit policy for these services.
      NOTREACHED();
      return false;

    case IpcTag::CREATEPROCESSW:
      return INTERCEPT_EAT(manager, kKerneldllName, CreateProcessW,
                           CREATE_PROCESSW_ID, 44) &&
             INTERCEPT_EAT(manager, L"kernel32.dll", CreateProcessA,
                           CREATE_PROCESSA_ID, 44);

    default:
      return false;
  }
}

bool ThreadProcessDispatcher::NtOpenThread(IPCInfo* ipc,
                                           uint32_t desired_access,
                                           uint32_t thread_id) {
  HANDLE handle;
  NTSTATUS ret = ProcessPolicy::OpenThreadAction(
      *ipc->client_info, desired_access, thread_id, &handle);
  ipc->return_info.nt_status = ret;
  ipc->return_info.handle = handle;
  return true;
}

bool ThreadProcessDispatcher::NtOpenProcess(IPCInfo* ipc,
                                            uint32_t desired_access,
                                            uint32_t process_id) {
  HANDLE handle;
  NTSTATUS ret = ProcessPolicy::OpenProcessAction(
      *ipc->client_info, desired_access, process_id, &handle);
  ipc->return_info.nt_status = ret;
  ipc->return_info.handle = handle;
  return true;
}

bool ThreadProcessDispatcher::NtOpenProcessToken(IPCInfo* ipc,
                                                 HANDLE process,
                                                 uint32_t desired_access) {
  HANDLE handle;
  NTSTATUS ret = ProcessPolicy::OpenProcessTokenAction(
      *ipc->client_info, process, desired_access, &handle);
  ipc->return_info.nt_status = ret;
  ipc->return_info.handle = handle;
  return true;
}

bool ThreadProcessDispatcher::NtOpenProcessTokenEx(IPCInfo* ipc,
                                                   HANDLE process,
                                                   uint32_t desired_access,
                                                   uint32_t attributes) {
  HANDLE handle;
  NTSTATUS ret = ProcessPolicy::OpenProcessTokenExAction(
      *ipc->client_info, process, desired_access, attributes, &handle);
  ipc->return_info.nt_status = ret;
  ipc->return_info.handle = handle;
  return true;
}

bool ThreadProcessDispatcher::CreateProcessW(IPCInfo* ipc,
                                             std::wstring* name,
                                             std::wstring* cmd_line,
                                             std::wstring* cur_dir,
                                             std::wstring* target_cur_dir,
                                             CountedBuffer* info) {
  if (sizeof(PROCESS_INFORMATION) != info->Size())
    return false;

  // Check if there is an application name.
  std::wstring exe_name;
  if (!name->empty())
    exe_name = *name;
  else
    exe_name = GetPathFromCmdLine(*cmd_line);

  if (IsPathRelative(exe_name)) {
    if (!ConvertToAbsolutePath(*cur_dir, name->empty(), &exe_name)) {
      // Cannot find the path. Maybe the file does not exist.
      ipc->return_info.win32_result = ERROR_FILE_NOT_FOUND;
      return true;
    }
  }

  const wchar_t* const_exe_name = exe_name.c_str();
  CountedParameterSet<NameBased> params;
  params[NameBased::NAME] = ParamPickerMake(const_exe_name);

  EvalResult eval =
      policy_base_->EvalPolicy(IpcTag::CREATEPROCESSW, params.GetBase());

  PROCESS_INFORMATION* proc_info =
      reinterpret_cast<PROCESS_INFORMATION*>(info->Buffer());
  // Here we force the app_name to be the one we used for the policy lookup.
  // If our logic was wrong, at least we wont allow create a random process.
  DWORD ret = ProcessPolicy::CreateProcessWAction(
      eval, *ipc->client_info, exe_name, *cmd_line, *target_cur_dir, proc_info);

  ipc->return_info.win32_result = ret;
  return true;
}

bool ThreadProcessDispatcher::CreateThread(IPCInfo* ipc,
                                           SIZE_T stack_size,
                                           LPTHREAD_START_ROUTINE start_address,
                                           LPVOID parameter,
                                           DWORD creation_flags) {
  if (!start_address) {
    return false;
  }

  HANDLE handle;
  DWORD ret = ProcessPolicy::CreateThreadAction(
      *ipc->client_info, stack_size, start_address, parameter, creation_flags,
      nullptr, &handle);

  ipc->return_info.nt_status = ret;
  ipc->return_info.handle = handle;
  return true;
}

}  // namespace sandbox
