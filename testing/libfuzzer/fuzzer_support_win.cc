// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <dbghelp.h>

namespace {

// In SDK 10.0.28000+, dbghelp.dll dynamically loads msdia140.dll at runtime,
// and symbolization calls WinVerifyTrust which creates threads in crypt32.
// If dbghelp is initialized during ASAN symbolization of a crash, this
// causes reentrancy or deadlocks inside AddressSanitizer's thread registry
// or malloc handlers (see crbug.com/548509159). Therefore, we preload these
// DLLs and initialize DbgHelp before fuzzing starts.
struct WinDllPreloader {
  WinDllPreloader() {
    ::LoadLibraryW(L"dbghelp.dll");
    ::LoadLibraryW(L"msdia140.dll");
    if (::SymInitialize(::GetCurrentProcess(), nullptr, TRUE)) {
      ::SymCleanup(::GetCurrentProcess());
    }
  }
} g_win_dll_preloader;

}  // namespace
