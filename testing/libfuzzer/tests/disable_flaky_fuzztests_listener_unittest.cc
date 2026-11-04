// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/libfuzzer/disable_flaky_fuzztests_listener.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fuzztest_init_helper {
namespace {

// Updates FUZZTEST_FUZZ_FOR in the C runtime environment read by std::getenv()
// and FuzzTest. Avoids base::ScopedEnvironmentVariableOverride, which calls
// Win32 SetEnvironmentVariable() on Windows without updating the UCRT
// environment block read by std::getenv().
void SetCrtFuzzForEnvVar(std::optional<std::string_view> value) {
#ifdef _WIN32
  std::string str(value.value_or(""));
  _putenv_s("FUZZTEST_FUZZ_FOR", str.c_str());
#else
  if (value.has_value()) {
    std::string str(*value);
    setenv("FUZZTEST_FUZZ_FOR", str.c_str(), /*replace=*/1);
  } else {
    unsetenv("FUZZTEST_FUZZ_FOR");
  }
#endif
}

class ScopedFuzzForEnvVar {
 public:
  explicit ScopedFuzzForEnvVar(std::optional<std::string_view> new_value) {
    if (const char* existing = std::getenv("FUZZTEST_FUZZ_FOR")) {
      original_value_ = existing;
    }
    SetCrtFuzzForEnvVar(new_value);
  }

  ScopedFuzzForEnvVar(const ScopedFuzzForEnvVar&) = delete;
  ScopedFuzzForEnvVar& operator=(const ScopedFuzzForEnvVar&) = delete;

  ~ScopedFuzzForEnvVar() { SetCrtFuzzForEnvVar(original_value_); }

 private:
  std::optional<std::string> original_value_;
};

const testing::TestInfo* FindDifferentTestInfo(
    const testing::TestInfo* current) {
  const testing::UnitTest* unit_test = testing::UnitTest::GetInstance();
  for (int i = 0; i < unit_test->total_test_suite_count(); ++i) {
    const testing::TestSuite* suite = unit_test->GetTestSuite(i);
    for (int j = 0; j < suite->total_test_count(); ++j) {
      const testing::TestInfo* candidate = suite->GetTestInfo(j);
      if (std::string_view(candidate->test_suite_name()) !=
              current->test_suite_name() ||
          std::string_view(candidate->name()) != current->name()) {
        return candidate;
      }
    }
  }
  return nullptr;
}

TEST(DisableFlakyFuzzTestsListenerTest,
     OverridesUnsetEnvVarForMatchingTestAndCleansUpOnTestEnd) {
  ScopedFuzzForEnvVar env_guard(std::nullopt);
  const testing::TestInfo* current =
      testing::UnitTest::GetInstance()->current_test_info();
  ASSERT_NE(current, nullptr);

  const std::vector<KnownFlakyFuzzTest> allowlist = {
      {"AAA_Suite", "Before"},
      {current->test_suite_name(), current->name()},
      {"ZZZ_Suite", "After"},
  };

  DisableFlakyFuzzTestsListener listener(allowlist);
  ASSERT_EQ(std::getenv("FUZZTEST_FUZZ_FOR"), nullptr);

  listener.OnTestStart(*current);
  EXPECT_STREQ(std::getenv("FUZZTEST_FUZZ_FOR"), "0s");

  listener.OnTestEnd(*current);
  EXPECT_EQ(std::getenv("FUZZTEST_FUZZ_FOR"), nullptr);
}

#ifndef _WIN32
// On Windows CRT, _putenv_s("VAR", "") removes the variable rather than storing
// an empty string. On POSIX/Fuchsia, setenv("VAR", "", 1) stores an empty
// string, which should also be treated as unset and overridden to "0s".
TEST(DisableFlakyFuzzTestsListenerTest,
     OverridesEmptyEnvVarForMatchingTestAndCleansUpOnTestEnd) {
  ScopedFuzzForEnvVar env_guard("");
  const testing::TestInfo* current =
      testing::UnitTest::GetInstance()->current_test_info();
  ASSERT_NE(current, nullptr);

  const std::vector<KnownFlakyFuzzTest> allowlist = {
      {current->test_suite_name(), current->name()},
  };
  DisableFlakyFuzzTestsListener listener(allowlist);
  ASSERT_STREQ(std::getenv("FUZZTEST_FUZZ_FOR"), "");

  listener.OnTestStart(*current);
  EXPECT_STREQ(std::getenv("FUZZTEST_FUZZ_FOR"), "0s");

  listener.OnTestEnd(*current);
  EXPECT_EQ(std::getenv("FUZZTEST_FUZZ_FOR"), nullptr);
}
#endif  // !_WIN32

TEST(DisableFlakyFuzzTestsListenerTest, IgnoresNonMatchingTest) {
  ScopedFuzzForEnvVar env_guard(std::nullopt);
  const testing::TestInfo* current =
      testing::UnitTest::GetInstance()->current_test_info();
  ASSERT_NE(current, nullptr);

  const std::vector<KnownFlakyFuzzTest> allowlist = {
      {current->test_suite_name(), "AAA_DifferentName"},
      {current->test_suite_name(), "ZZZ_DifferentName"},
      {"AAA_DifferentSuite", current->name()},
      {"ZZZ_DifferentSuite", current->name()},
  };

  DisableFlakyFuzzTestsListener listener(allowlist);
  listener.OnTestStart(*current);
  EXPECT_EQ(std::getenv("FUZZTEST_FUZZ_FOR"), nullptr);

  listener.OnTestEnd(*current);
  EXPECT_EQ(std::getenv("FUZZTEST_FUZZ_FOR"), nullptr);
}

TEST(DisableFlakyFuzzTestsListenerTest, HandlesEmptyAllowlist) {
  ScopedFuzzForEnvVar env_guard(std::nullopt);
  const testing::TestInfo* current =
      testing::UnitTest::GetInstance()->current_test_info();
  ASSERT_NE(current, nullptr);

  DisableFlakyFuzzTestsListener listener(std::vector<KnownFlakyFuzzTest>{});
  listener.OnTestStart(*current);
  EXPECT_EQ(std::getenv("FUZZTEST_FUZZ_FOR"), nullptr);

  listener.OnTestEnd(*current);
  EXPECT_EQ(std::getenv("FUZZTEST_FUZZ_FOR"), nullptr);
}

TEST(DisableFlakyFuzzTestsListenerTest, PreservesExistingNonEmptyEnvVar) {
  ScopedFuzzForEnvVar env_guard("5s");
  const testing::TestInfo* current =
      testing::UnitTest::GetInstance()->current_test_info();
  ASSERT_NE(current, nullptr);

  const std::vector<KnownFlakyFuzzTest> allowlist = {
      {current->test_suite_name(), current->name()},
  };
  DisableFlakyFuzzTestsListener listener(allowlist);

  listener.OnTestStart(*current);
  EXPECT_STREQ(std::getenv("FUZZTEST_FUZZ_FOR"), "5s");

  listener.OnTestEnd(*current);
  EXPECT_STREQ(std::getenv("FUZZTEST_FUZZ_FOR"), "5s");
}

TEST(DisableFlakyFuzzTestsListenerTest,
     ResetsOverrideStateAcrossSequentialTests) {
  ScopedFuzzForEnvVar env_guard(std::nullopt);
  const testing::TestInfo* matching =
      testing::UnitTest::GetInstance()->current_test_info();
  ASSERT_NE(matching, nullptr);
  const testing::TestInfo* non_matching = FindDifferentTestInfo(matching);
  ASSERT_NE(non_matching, nullptr);

  const std::vector<KnownFlakyFuzzTest> allowlist = {
      {matching->test_suite_name(), matching->name()},
  };
  DisableFlakyFuzzTestsListener listener(allowlist);

  // First test matches and overrides FUZZTEST_FUZZ_FOR, then cleans up.
  listener.OnTestStart(*matching);
  EXPECT_STREQ(std::getenv("FUZZTEST_FUZZ_FOR"), "0s");
  listener.OnTestEnd(*matching);
  EXPECT_EQ(std::getenv("FUZZTEST_FUZZ_FOR"), nullptr);

  // Second test does not match; a user-supplied FUZZTEST_FUZZ_FOR must not be
  // cleared by OnTestEnd.
  SetCrtFuzzForEnvVar("5s");
  listener.OnTestStart(*non_matching);
  EXPECT_STREQ(std::getenv("FUZZTEST_FUZZ_FOR"), "5s");
  listener.OnTestEnd(*non_matching);
  EXPECT_STREQ(std::getenv("FUZZTEST_FUZZ_FOR"), "5s");
}

// End-to-end check that registering DisableFlakyFuzzTestsListener with
// GoogleTest's TestEventListeners applies FUZZTEST_FUZZ_FOR=0s during a
// matching test body and does not leak to non-matching tests.
class RegisteredListenerEnvironment : public testing::Environment {
 public:
  void SetUp() override {
    if (const char* existing = std::getenv("FUZZTEST_FUZZ_FOR")) {
      initial_env_ = existing;
    }
    listener_ = new DisableFlakyFuzzTestsListener(
        {{"DisableFlakyFuzzTestsRegisteredListenerTest",
          "AppliesOverrideDuringMatchingTest"}});
    testing::UnitTest::GetInstance()->listeners().Append(listener_);
  }

  void TearDown() override {
    if (listener_ != nullptr) {
      delete testing::UnitTest::GetInstance()->listeners().Release(
          listener_.ExtractAsDangling());
    }
  }

  const std::optional<std::string>& initial_env() const { return initial_env_; }

 private:
  raw_ptr<DisableFlakyFuzzTestsListener> listener_ = nullptr;
  std::optional<std::string> initial_env_;
};

RegisteredListenerEnvironment* const g_registered_listener_env =
    static_cast<RegisteredListenerEnvironment*>(
        testing::AddGlobalTestEnvironment(new RegisteredListenerEnvironment()));

TEST(DisableFlakyFuzzTestsRegisteredListenerTest,
     AppliesOverrideDuringMatchingTest) {
  const auto& initial = g_registered_listener_env->initial_env();
  if (initial.has_value() && !initial->empty()) {
    EXPECT_STREQ(std::getenv("FUZZTEST_FUZZ_FOR"), initial->c_str());
  } else {
    EXPECT_STREQ(std::getenv("FUZZTEST_FUZZ_FOR"), "0s");
  }
}

TEST(DisableFlakyFuzzTestsRegisteredListenerTest,
     DoesNotAffectNonMatchingTest) {
  const auto& initial = g_registered_listener_env->initial_env();
  if (initial.has_value()) {
    EXPECT_STREQ(std::getenv("FUZZTEST_FUZZ_FOR"), initial->c_str());
  } else {
    EXPECT_EQ(std::getenv("FUZZTEST_FUZZ_FOR"), nullptr);
  }
}

}  // namespace
}  // namespace fuzztest_init_helper
