// Copyright 2006-2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_PROCESS_THREAD_POLICY_H_
#define SANDBOX_WIN_SRC_PROCESS_THREAD_POLICY_H_

#include <stdint.h>

#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace sandbox {

// This class centralizes most of the knowledge related to process execution.
class ProcessPolicy {
 public:
  // Opens a thread from the child process and returns the handle.
  // client_info contains the information about the child process,
  // desired_access is the access requested by the child and thread_id
  // is the thread_id to be opened.
  // The function returns the return value of NtOpenThread.
  static NTSTATUS OpenThreadAction(const ClientInfo& client_info,
                                   uint32_t desired_access,
                                   uint32_t thread_id,
                                   HANDLE* handle);

  // Opens the token associated with the process and returns the duplicated
  // handle to the child. We only allow the child processes to open its own
  // token (using ::GetCurrentProcess()).
  static NTSTATUS OpenProcessTokenExAction(const ClientInfo& client_info,
                                           HANDLE process,
                                           uint32_t desired_access,
                                           uint32_t attributes,
                                           HANDLE* handle);

  // Processes a 'CreateThread()' request from the target.
  // 'client_info' : the target process that is making the request.
  static DWORD CreateThreadAction(const ClientInfo& client_info,
                                  SIZE_T stack_size,
                                  LPTHREAD_START_ROUTINE start_address,
                                  PVOID parameter,
                                  DWORD creation_flags,
                                  HANDLE* handle);
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_PROCESS_THREAD_POLICY_H_
