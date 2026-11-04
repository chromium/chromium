// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_LIBFUZZER_DISABLE_FLAKY_FUZZTESTS_LISTENER_H_
#define TESTING_LIBFUZZER_DISABLE_FLAKY_FUZZTESTS_LISTENER_H_

#include <string_view>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace fuzztest_init_helper {

struct KnownFlakyFuzzTest {
  std::string_view suite;
  std::string_view name;

  constexpr bool operator==(const KnownFlakyFuzzTest&) const = default;
};

// GoogleTest event listener that sets FUZZTEST_FUZZ_FOR=0s during unit-test
// execution for an allowlist of known flaky FUZZ_TESTs (when
// FUZZTEST_FUZZ_FOR is not already set in the environment).
class DisableFlakyFuzzTestsListener : public testing::EmptyTestEventListener {
 public:
  // Uses the production allowlist of known flaky FUZZ_TESTs.
  DisableFlakyFuzzTestsListener();

  // Uses a custom allowlist of flaky FUZZ_TESTs (for testing).
  explicit DisableFlakyFuzzTestsListener(
      std::vector<KnownFlakyFuzzTest> flaky_tests);

  DisableFlakyFuzzTestsListener(const DisableFlakyFuzzTestsListener&) = delete;
  DisableFlakyFuzzTestsListener& operator=(
      const DisableFlakyFuzzTestsListener&) = delete;

  ~DisableFlakyFuzzTestsListener() override;

  void OnTestStart(const testing::TestInfo& test_info) override;
  void OnTestEnd(const testing::TestInfo& test_info) override;

 private:
  std::vector<KnownFlakyFuzzTest> flaky_tests_;
  bool did_override_env_ = false;
};

}  // namespace fuzztest_init_helper

#endif  // TESTING_LIBFUZZER_DISABLE_FLAKY_FUZZTESTS_LISTENER_H_
