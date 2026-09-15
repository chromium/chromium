// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Sink for APIs that must never be called in the basic process. It always logs
// a stack before crashing so audit runs can identify the caller even when
// call-site logging is disabled.

#include "sandbox/win/tests/basic_process/shim/shim_runtime.h"

extern "C" {

BASIC_STUB_EXPORT void WINAPI ApifwNotReached() {
  basic_process::LogStackFn(__FUNCTION__);
  CAPTURE_CALLSITE();
  __debugbreak();
  __builtin_unreachable();
}

}  // extern "C"
