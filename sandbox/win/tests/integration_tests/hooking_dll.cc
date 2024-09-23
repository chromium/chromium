// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <stdio.h>

#define BUILDING_DLL
#include "hooking_dll.h"

// This data section creates a common area that is accessible
// to all instances of the DLL (in every process).  They map to
// the same physical memory location.
// **Note that each instance of this DLL runs in the context of
// the process it's injected into, so things like pointers and
// addresses won't work.
// **Note that any variables must be initialized to put them in
// the specified segment, otherwise they will end up in the
// default data segment.
#pragma data_seg(".hook")
HHOOK hook = NULL;
bool hook_called = false;
#pragma data_seg()
#pragma comment(linker, "/SECTION:.hook,RWS")

namespace {
HANDLE event = NULL;
}

namespace hooking_dll {

void SetHook(HHOOK hook_handle) {
  hook = hook_handle;

  return;
}

bool WasHookCalled() {
  return hook_called;
}

LRESULT HookProc(int code, WPARAM w_param, LPARAM l_param) {
  hook_called = true;
  if (event)
    ::SetEvent(event);

  // Recent versions of Windows do not require the HHOOK to be passed along,
  // but I'm doing it here to show the shared use of the HHOOK variable in
  // the shared data segment.  It is set by the instance of the DLL in the
  // main test process.
  return CallNextHookEx(hook, code, w_param, l_param);
}

}  // namespace hooking_dll

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    // The testing process should have set up this named event already
    // (if the test needs this event to be signaled).
    event = ::OpenEventW(EVENT_MODIFY_STATE, FALSE, hooking_dll::g_hook_event);
  }

  if (reason == DLL_PROCESS_DETACH && event != nullptr)
    ::CloseHandle(event);

  return TRUE;
}
