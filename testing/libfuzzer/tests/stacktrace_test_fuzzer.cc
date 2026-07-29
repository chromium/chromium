// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

// Tries to use a dangling pointer, triggers a UaF crash under ASAN.
NOINLINE int TriggerUAF() {
  auto ptr = std::make_unique<int>(0);
  int* dangling = ptr.get();
  ptr = nullptr;
  return *dangling;
}

NOINLINE int TriggerCheck() {
  CHECK(false);
}

#if defined(_WIN32)
#include <windows.h>

// We need to preload these dlls, because if they get loaded during
// symbolization it can cause reentrancy into asan's malloc handler.
struct WinDllPreloader {
  WinDllPreloader() {
    ::LoadLibraryW(L"dbghelp.dll");
    ::LoadLibraryW(L"msdia140.dll");
  }
};
#endif

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> bytes) {
#if defined(_WIN32)
  static WinDllPreloader win_dll_preloader;
#endif
  auto str = base::as_string_view(bytes);

  if (str == "uaf") {
    return TriggerUAF();
  }
  if (str == "check") {
    return TriggerCheck();
  }
  return 0;
}
