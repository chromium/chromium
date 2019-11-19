// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/policy_broker.h"

#include <stddef.h>

#include <map>

#include "base/logging.h"
#include "base/win/pe_image.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/policy_target.h"
#include "sandbox/win/src/process_thread_interception.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_nt_types.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/target_process.h"

// This code executes on the broker side, as a callback from the policy on the
// target side (the child).

namespace sandbox {

// This is the list of all imported symbols from ntdll.dll.
SANDBOX_INTERCEPT NtExports g_nt;

#define INIT_GLOBAL_NT(member)                          \
  g_nt.member = reinterpret_cast<Nt##member##Function>( \
      ntdll_image.GetProcAddress("Nt" #member));        \
  if (!g_nt.member)                                     \
  return false

#define INIT_GLOBAL_RTL(member)                                                \
  g_nt.member =                                                                \
      reinterpret_cast<member##Function>(ntdll_image.GetProcAddress(#member)); \
  if (!g_nt.member)                                                            \
  return false

bool InitGlobalNt() {
  HMODULE ntdll = ::GetModuleHandle(kNtdllName);
  base::win::PEImage ntdll_image(ntdll);

  INIT_GLOBAL_NT(AllocateVirtualMemory);
  INIT_GLOBAL_NT(Close);
  INIT_GLOBAL_NT(DuplicateObject);
  INIT_GLOBAL_NT(FreeVirtualMemory);
  INIT_GLOBAL_NT(MapViewOfSection);
  INIT_GLOBAL_NT(ProtectVirtualMemory);
  INIT_GLOBAL_NT(QueryInformationProcess);
  INIT_GLOBAL_NT(QueryObject);
  INIT_GLOBAL_NT(QuerySection);
  INIT_GLOBAL_NT(QueryVirtualMemory);
  INIT_GLOBAL_NT(UnmapViewOfSection);
  INIT_GLOBAL_NT(SignalAndWaitForSingleObject);
  INIT_GLOBAL_NT(WaitForSingleObject);

  INIT_GLOBAL_RTL(RtlAllocateHeap);
  INIT_GLOBAL_RTL(RtlAnsiStringToUnicodeString);
  INIT_GLOBAL_RTL(RtlCompareUnicodeString);
  INIT_GLOBAL_RTL(RtlCreateHeap);
  INIT_GLOBAL_RTL(RtlCreateUserThread);
  INIT_GLOBAL_RTL(RtlDestroyHeap);
  INIT_GLOBAL_RTL(RtlFreeHeap);
  INIT_GLOBAL_RTL(_strnicmp);
  INIT_GLOBAL_RTL(strlen);
  INIT_GLOBAL_RTL(wcslen);
  INIT_GLOBAL_RTL(memcpy);

  return true;
}

bool SetupNtdllImports(TargetProcess* child) {
  if (!InitGlobalNt()) {
    return false;
  }

#ifndef NDEBUG
  // Verify that the structure is fully initialized.
  for (size_t i = 0; i < sizeof(g_nt) / sizeof(void*); i++)
    DCHECK(reinterpret_cast<char**>(&g_nt)[i]);
#endif
  return (SBOX_ALL_OK == child->TransferVariable("g_nt", &g_nt, sizeof(g_nt)));
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
