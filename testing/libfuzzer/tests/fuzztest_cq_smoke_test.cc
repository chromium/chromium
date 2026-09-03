// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The most basic smoke test for FUZZ_TEST macros.
//
// The main purpose is to ensure that FUZZ_TEST compiles, registers with
// GoogleTest, and runs as a standard unit test across all build configurations:
// * On regular builders, this produces a standard GoogleTest unit test
// executable.
// * On ASAN builders, this produces a unit test executable with coverage
//   instrumentation supporting `--fuzz=` arguments.
// * On LibFuzzer and Centipede builders, this enables engine-specific options.
//
// NOTE: This test suite only verifies unit-test mode. It does NOT cover how
// fuzzing engines execute a FUZZ_TEST in ClusterFuzz (which occurs through
// generated wrapper binaries and is tested in `fuzzer_smoke_test.cc`).

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

static void StringsAlwaysOccupyPositiveSpace(std::string input) {
  EXPECT_TRUE(input.size() >= 0);
}

FUZZ_TEST(FuzzTestCqSmokeTest, StringsAlwaysOccupyPositiveSpace);

// Define an empty test so that on platforms where `FUZZ_TEST` is compiled away,
// there exists at least one test to run. Otherwise, testing infrastructure
// may complain that there are no tests to run in the binary.
TEST(FuzzTestCqSmokeTest, Nop) {}
