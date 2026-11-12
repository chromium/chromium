// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/check.h"
#include "base/compiler_specific.h"

// This exercises the CHECK failure path: base logging prints
// "Check failed: ..." and then base::ImmediateCrash() calls abort() (SIGABRT),
// or executes a trap instruction on Windows.
NOINLINE int TriggerCheckFailure() {
  CHECK(false);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return TriggerCheckFailure();
}
