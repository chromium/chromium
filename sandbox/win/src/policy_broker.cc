// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/policy_broker.h"

#include <stddef.h>

#include <map>

#include "base/check.h"
#include "base/win/pe_image.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/policy_target.h"
#include "sandbox/win/src/process_thread_interception.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_nt_types.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/target_process.h"

// This code executes on the broker side, as a callback from the policy on the
// target side (the child).

namespace sandbox {

bool SetupNtdllImports(TargetProcess& child) {
  return (SBOX_ALL_OK ==
          child.TransferVariable("g_nt", GetNtExports(),
                                 const_cast<NtExports*>(GetNtExports()),
                                 sizeof(NtExports)));
}

#undef INIT_GLOBAL_NT
#undef INIT_GLOBAL_RTL

bool SetupBasicInterceptions(InterceptionManager* manager,
                             bool is_csrss_connected) {
  // Interceptions provided by process_thread_policy, without actual policy.
  if (!INTERCEPT_NT(manager, NtOpenThread, OPEN_THREAD_ID, 20) ||
      !INTERCEPT_NT(manager, NtOpenProcess, OPEN_PROCESS_ID, 20) ||
      !INTERCEPT_NT(manager, NtOpenProcessToken, OPEN_PROCESS_TOKEN_ID, 16))
    return false;

  // Interceptions with neither policy nor IPC.
  if (!INTERCEPT_NT(manager, NtSetInformationThread, SET_INFORMATION_THREAD_ID,
                    20) ||
      !INTERCEPT_NT(manager, NtOpenThreadToken, OPEN_THREAD_TOKEN_ID, 20))
    return false;

  // This one is also provided by process_thread_policy.
  if (!INTERCEPT_NT(manager, NtOpenProcessTokenEx, OPEN_PROCESS_TOKEN_EX_ID,
                    20))
    return false;

  if (!INTERCEPT_NT(manager, NtOpenThreadTokenEx, OPEN_THREAD_TOKEN_EX_ID, 24))
    return false;

  if (!is_csrss_connected) {
    if (!INTERCEPT_EAT(manager, kKerneldllName, CreateThread, CREATE_THREAD_ID,
                       28))
      return false;
  }

  return true;
}

}  // namespace sandbox
