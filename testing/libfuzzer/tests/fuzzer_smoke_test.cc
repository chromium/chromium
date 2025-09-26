// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sanitizer_buildflags.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/libfuzzer/tests/fuzz_target.h"

namespace fuzzing {
namespace {

using testing::ContainsRegex;
using testing::ElementsAre;
using testing::IsEmpty;

TEST(FuzzerSmokeTest, EmptyFuzzerFindsNoCrashes) {
  auto target = FuzzTarget::Make("empty_fuzzer");
  ASSERT_TRUE(target);

  EXPECT_TRUE(target->Fuzz({.timeout_secs = 5})) << target->output();

  EXPECT_THAT(target->GetCrashingInputs(), IsEmpty());
}

// TODO(https://crbug.com/445826636): Fix and re-enable.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_UBSAN) || BUILDFLAG(IS_UBSAN_SECURITY)
#define MAYBE_FuzzerSolvesStringComparison DISABLED_FuzzerSolvesStringComparison
#else
#define MAYBE_FuzzerSolvesStringComparison FuzzerSolvesStringComparison
#endif
TEST(FuzzerSmokeTest, MAYBE_FuzzerSolvesStringComparison) {
  auto target = FuzzTarget::Make("string_compare_fuzzer");
  ASSERT_TRUE(target);

  target->Fuzz({.timeout_secs = 5});

  EXPECT_THAT(target->GetCrashingInputs(), ElementsAre("fish"))
      << target->output();
}

// TODO(https://crbug.com/445826636): Fix and re-enable.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_UBSAN) || BUILDFLAG(IS_UBSAN_SECURITY)
#define MAYBE_FuzzerSolvesProtoStringComparison \
  DISABLED_FuzzerSolvesProtoStringComparison
#else
#define MAYBE_FuzzerSolvesProtoStringComparison \
  FuzzerSolvesProtoStringComparison
#endif
TEST(FuzzerSmokeTest, MAYBE_FuzzerSolvesProtoStringComparison) {
  auto target = FuzzTarget::Make("string_compare_proto_fuzzer");
  ASSERT_TRUE(target);

  target->Fuzz({.timeout_secs = 5});

  EXPECT_THAT(target->GetCrashingInputs(), ElementsAre("\012\004fish"))
      << target->output();
}

}  // namespace
}  // namespace fuzzing
