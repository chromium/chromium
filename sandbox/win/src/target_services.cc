// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/target_services.h"

#include <new>

#include <process.h>
#include <stdint.h>

#include "base/win/windows_version.h"
#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/handle_closer_agent.h"
#include "sandbox/win/src/heap_helper.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/restricted_token_utils.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"

namespace sandbox {
namespace {

// Flushing a cached key is triggered by just opening the key and closing the
// resulting handle. RegDisablePredefinedCache() is the documented way to flush
// HKCU so do not use it with this function.
bool FlushRegKey(HKEY root) {
  HKEY key;
  if (ERROR_SUCCESS ==
      ::RegOpenKeyExW(root, nullptr, 0, MAXIMUM_ALLOWED, &key)) {
    if (ERROR_SUCCESS != ::RegCloseKey(key))
      return false;
  }
  return true;
}

// This function forces advapi32.dll to release some internally cached handles
// that were made during calls to RegOpenkey and RegOpenKeyEx if it is called
// with a more restrictive token. Returns true if the flushing is succesful
// although this behavior is undocumented and there is no guarantee that in
// fact this will happen in future versions of windows.
bool FlushCachedRegHandles() {
  return (FlushRegKey(HKEY_LOCAL_MACHINE) && FlushRegKey(HKEY_CLASSES_ROOT) &&
          FlushRegKey(HKEY_USERS));
}

// Cleans up this process if CSRSS will be disconnected, as this disconnection
// is not supported Windows behavior.
// Currently, this step requires closing a heap that this shared with csrss.exe.
// Closing the ALPC Port handle to csrss.exe leaves this heap in an invalid
// state. This causes problems if anyone enumerates the heap.
bool CsrssDisconnectCleanup() {
  HANDLE csr_port_heap = FindCsrPortHeap();
  if (!csr_port_heap) {
    DLOG(ERROR) << "Failed to find CSR Port heap handle";
    return false;
  }
  HeapDestroy(csr_port_heap);
  return true;
}

// Used by EnumSystemLocales for warming up.
static BOOL CALLBACK EnumLocalesProcEx(LPWSTR lpLocaleString,
                                       DWORD dwFlags,
                                       LPARAM lParam) {
  return TRUE;
}

// Additional warmup done just when CSRSS is being disconnected.
bool CsrssDisconnectWarmup() {
  return ::EnumSystemLocalesEx(EnumLocalesProcEx, LOCALE_WINDOWS, 0, 0);
}

// Checks if we have handle entries pending and runs the closer.
// Updates is_csrss_connected based on which handle types are closed.
bool CloseOpenHandles(bool* is_csrss_connected) {
  if (HandleCloserAgent::NeedsHandlesClosed()) {
    HandleCloserAgent handle_closer;
    handle_closer.InitializeHandlesToClose(is_csrss_connected);
    if (!*is_csrss_connected) {
      if (!CsrssDisconnectWarmup() || !CsrssDisconnectCleanup()) {
        return false;
      }
    }
    if (!handle_closer.CloseHandles())
      return false;
  }
  return true;
}

// Warm up language subsystems before the sandbox is turned on.
// Tested on Win8.1 x64:
// This needs to happen after RevertToSelf() is called, because (at least) in
// the case of GetUserDefaultLCID() it checks the TEB to see if the process is
// impersonating (TEB!IsImpersonating). If it is, the cached locale information
// is not used, nor is it set. Therefore, calls after RevertToSelf() will not
// have warmed-up values to use.
bool WarmupWindowsLocales() {
  // NOTE(liamjm): When last checked (Win 8.1 x64) it wasn't necessary to
  // warmup all of these functions, but let's not assume that.
  ::GetUserDefaultLangID();
  ::GetUserDefaultLCID();
  wchar_t localeName[LOCALE_NAME_MAX_LENGTH] = {0};
  return (0 != ::GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH));
}

// Used as storage for g_target_services, because other allocation facilities
// are not available early. We can't use a regular function static because on
// VS2015, because the CRT tries to acquire a lock to guard initialization, but
// this code runs before the CRT is initialized.
char g_target_services_memory[sizeof(TargetServicesBase)];
TargetServicesBase* g_target_services = nullptr;

}  // namespace

SANDBOX_INTERCEPT IntegrityLevel g_shared_delayed_integrity_level =
    INTEGRITY_LEVEL_LAST;
SANDBOX_INTERCEPT MitigationFlags g_shared_delayed_mitigations = 0;

TargetServicesBase::TargetServicesBase() {}

ResultCode TargetServicesBase::Init() {
  process_state_.SetInitCalled();
  return SBOX_ALL_OK;
}

// Failure here is a breach of security so the process is terminated.
void TargetServicesBase::LowerToken() {
  if (ERROR_SUCCESS !=
      SetProcessIntegrityLevel(g_shared_delayed_integrity_level))
    ::TerminateProcess(::GetCurrentProcess(), SBOX_FATAL_INTEGRITY);
  process_state_.SetRevertedToSelf();
  // If the client code as called RegOpenKey, advapi32.dll has cached some
  // handles. The following code gets rid of them.
  if (!::RevertToSelf())
    ::TerminateProcess(::GetCurrentProcess(), SBOX_FATAL_DROPTOKEN);
  if (!FlushCachedRegHandles())
    ::TerminateProcess(::GetCurrentProcess(), SBOX_FATAL_FLUSHANDLES);
  if (ERROR_SUCCESS != ::RegDisablePredefinedCache())
    ::TerminateProcess(::GetCurrentProcess(), SBOX_FATAL_CACHEDISABLE);
  if (!WarmupWindowsLocales())
    ::TerminateProcess(::GetCurrentProcess(), SBOX_FATAL_WARMUP);
  bool is_csrss_connected = true;
  if (!CloseOpenHandles(&is_csrss_connected))
    ::TerminateProcess(::GetCurrentProcess(), SBOX_FATAL_CLOSEHANDLES);
  process_state_.SetCsrssConnected(is_csrss_connected);
  // Enabling mitigations must happen last otherwise handle closing breaks
  if (g_shared_delayed_mitigations &&
      !ApplyProcessMitigationsToCurrentProcess(g_shared_delayed_mitigations))
    ::TerminateProcess(::GetCurrentProcess(), SBOX_FATAL_MITIGATION);
}

ProcessState* TargetServicesBase::GetState() {
  return &process_state_;
}

TargetServicesBase* TargetServicesBase::GetInstance() {
  // Leak on purpose TargetServicesBase.
  if (!g_target_services)
    g_target_services = new (g_target_services_memory) TargetServicesBase;
  return g_target_services;
}

// The broker services a 'test' IPC service with the PING tag.
bool TargetServicesBase::TestIPCPing(int version) {
  void* memory = GetGlobalIPCMemory();
  if (!memory)
    return false;
  SharedMemIPCClient ipc(memory);
  CrossCallReturn answer = {0};

  if (1 == version) {
    uint32_t tick1 = ::GetTickCount();
    uint32_t cookie = 717115;
    ResultCode code = CrossCall(ipc, IpcTag::PING1, cookie, &answer);

    if (SBOX_ALL_OK != code) {
      return false;
    }
    // We should get two extended returns values from the IPC, one is the
    // tick count on the broker and the other is the cookie times two.
    if ((answer.extended_count != 2)) {
      return false;
    }
    // We test the first extended answer to be within the bounds of the tick
    // count only if there was no tick count wraparound.
    uint32_t tick2 = ::GetTickCount();
    if (tick2 >= tick1) {
      if ((answer.extended[0].unsigned_int < tick1) ||
          (answer.extended[0].unsigned_int > tick2)) {
        return false;
      }
    }

    if (answer.extended[1].unsigned_int != cookie * 2) {
      return false;
    }
  } else if (2 == version) {
    uint32_t cookie = 717111;
    InOutCountedBuffer counted_buffer(&cookie, sizeof(cookie));
    ResultCode code = CrossCall(ipc, IpcTag::PING2, counted_buffer, &answer);

    if (SBOX_ALL_OK != code) {
      return false;
    }
    if (cookie != 717111 * 3) {
      return false;
    }
  } else {
    return false;
  }
  return true;
}

ProcessState::ProcessState()
    : process_state_(ProcessStateInternal::NONE), csrss_connected_(true) {}

bool ProcessState::InitCalled() const {
  return process_state_ >= ProcessStateInternal::INIT_CALLED;
}

bool ProcessState::RevertedToSelf() const {
  return process_state_ >= ProcessStateInternal::REVERTED_TO_SELF;
}

bool ProcessState::IsCsrssConnected() const {
  return csrss_connected_;
}

void ProcessState::SetInitCalled() {
  if (process_state_ < ProcessStateInternal::INIT_CALLED)
    process_state_ = ProcessStateInternal::INIT_CALLED;
}

void ProcessState::SetRevertedToSelf() {
  if (process_state_ < ProcessStateInternal::REVERTED_TO_SELF)
    process_state_ = ProcessStateInternal::REVERTED_TO_SELF;
}

void ProcessState::SetCsrssConnected(bool csrss_connected) {
  csrss_connected_ = csrss_connected;
}

}  // namespace sandbox
