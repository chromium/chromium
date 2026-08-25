// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A minimal stub fuzztest used to verify that the fuzztest wrapper
// behaves correctly and does not leak child processes.

#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

static void Stub(int input) {}

FUZZ_TEST(FuzzTestStub, Stub);
