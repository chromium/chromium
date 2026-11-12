// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

// Triggers a memory fault / crash across all configurations (ASan, UBSan,
// etc.). Uses a volatile pointer so the compiler cannot optimize away the
// memory read.
NOINLINE int TriggerCrash() {
  volatile int* p = nullptr;
  return *p;
}

static void FastCrash(int input) {
  TriggerCrash();
}

FUZZ_TEST(FuzzTestCrashing, FastCrash);

// This exercises the CHECK failure path: base logging prints
// "Check failed: ..." and then base::ImmediateCrash() calls abort() (SIGABRT),
// or executes a trap instruction on Windows.
NOINLINE void TriggerCheckFailure() {
  CHECK(false);
}

static void CheckFailure(int input) {
  TriggerCheckFailure();
}

FUZZ_TEST(FuzzTestCrashing, CheckFailure);
