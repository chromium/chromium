// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/test_hook.h"

namespace device {

namespace {
ServiceTestHook::InitializeOpenXrMockTrampolineFn g_init_trampoline_fn =
    nullptr;
}  // namespace

// static
void ServiceTestHook::RegisterInitializeOpenXrMockTrampolineFn(
    InitializeOpenXrMockTrampolineFn fn) {
  g_init_trampoline_fn = fn;
}

// static
void ServiceTestHook::MaybeInitializeOpenXrMockTrampoline() {
  if (g_init_trampoline_fn) {
    g_init_trampoline_fn();
  }
}

}  // namespace device
