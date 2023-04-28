// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a small file to make a loadable dll for the echo service tests.
#include <windows.h>

#define SECURITY_WIN32
#include <security.h>

extern "C" {
BOOL WINAPI DllMain(PVOID h, DWORD reason, PVOID reserved) {
  return true;
}

BOOL FnCallsDelayloadFn() {
  // Calls xyz which is delayloaded but in a library already in
  // utility processes.
  ULONG sz = 0;
  wchar_t buf[1];
  // This call should happen, we don't actually care about the return.
  if (::GetUserNameExW(NameSamCompatible, buf, &sz)) {
    return false;
  }
  return true;
}
}  // extern "C"
