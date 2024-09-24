// Copyright 2013 The Chromium Authors
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

namespace sandbox {

ThreadProcessDispatcher::ThreadProcessDispatcher() {
  static const IPCCall open_thread = {
      {IpcTag::NTOPENTHREAD, {UINT32_TYPE, UINT32_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ThreadProcessDispatcher::NtOpenThread)};

  static const IPCCall process_tokenex = {
      {IpcTag::NTOPENPROCESSTOKENEX, {VOIDPTR_TYPE, UINT32_TYPE, UINT32_TYPE}},
      reinterpret_cast<CallbackGeneric>(
          &ThreadProcessDispatcher::NtOpenProcessTokenEx)};

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
  ipc_calls_.push_back(process_tokenex);
  ipc_calls_.push_back(create_thread_params);
}

bool ThreadProcessDispatcher::SetupService(InterceptionManager* manager,
                                           IpcTag service) {
  switch (service) {
    case IpcTag::NTOPENTHREAD:
    case IpcTag::NTOPENPROCESSTOKENEX:
    case IpcTag::CREATETHREAD:
      // There is no explicit policy for these services.
      // Intercepts are set up in SetupBasicInterceptions(), not here.
      NOTREACHED();

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

bool ThreadProcessDispatcher::CreateThread(IPCInfo* ipc,
                                           SIZE_T stack_size,
                                           LPTHREAD_START_ROUTINE start_address,
                                           LPVOID parameter,
                                           DWORD creation_flags) {
  if (!start_address) {
    return false;
  }

  HANDLE handle;
  DWORD ret = ProcessPolicy::CreateThreadAction(*ipc->client_info, stack_size,
                                                start_address, parameter,
                                                creation_flags, &handle);

  ipc->return_info.win32_result = ret;
  ipc->return_info.handle = handle;
  return true;
}

}  // namespace sandbox
