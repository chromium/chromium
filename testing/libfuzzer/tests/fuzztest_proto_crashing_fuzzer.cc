// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "testing/libfuzzer/proto/string_compare_fuzzable.pb.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

// Triggers a memory fault / crash across all configurations (ASan, UBSan,
// etc.). Uses a volatile pointer so the compiler cannot optimize away the
// memory read.
NOINLINE int TriggerCrash() {
  volatile int* p = nullptr;
  return *p;
}

static void FastCrash(const fuzzable::string_compare::StringCompare& input) {
  TriggerCrash();
}

FUZZ_TEST(FuzzTestProtoCrashing, FastCrash);
