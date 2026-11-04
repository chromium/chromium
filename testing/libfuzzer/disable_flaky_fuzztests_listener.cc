// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/libfuzzer/disable_flaky_fuzztests_listener.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string_view>
#include <utility>
#include <vector>

namespace fuzztest_init_helper {

namespace {

// List of FUZZ_TESTs identified as flaky in November 2026 by querying LUCI
// Analysis for failure clusters across all registered FUZZ_TESTs in Chromium.
// These tests frequently crash on random mutations in under 1s.
//
// Do not add new tests to this list.
constexpr auto kKnownFlakyFuzzTests = std::to_array<KnownFlakyFuzzTest>({
    {"AccessibilityDomScenarioRunner", "AnimationAndAria"},
    {"AccessibilityDomScenarioRunner", "CanvasFallbackContent"},
    {"AccessibilityDomScenarioRunner", "CarouselAndAria"},
    {"AccessibilityDomScenarioRunner", "CustomizableSelectAndAria"},
    {"AccessibilityDomScenarioRunner", "DialogAndAria"},
    {"AccessibilityDomScenarioRunner", "HtmlAndAria"},
    {"AccessibilityDomScenarioRunner", "MathMLAndAria"},
    {"AccessibilityDomScenarioRunner", "ProblematicMarkup"},
    {"AccessibilityDomScenarioRunner", "ReadingFlowAndOrder"},
    {"AccessibilityDomScenarioRunner", "SelectAndAria"},
    {"AccessibilityDomScenarioRunner", "SvgAndAria"},
    {"AccessibilityDomScenarioRunner", "ValidTableAndAria"},
    {"CBORReaderFuzzTest", "ReadAndWriteIsIdempotentAndDoesNotCrash"},
    {"CBORReaderFuzzTest", "ReadValidAndTruncatedCBORDoesNotCrash"},
    {"CBORReaderFuzzTest", "ReadValidCBORWithConfigDoesNotCrash"},
    {"CBORReaderFuzzTest", "ReadWithConfigDoesNotCrash"},
    {"HTMLDomScenarioRunner", "HTML"},
    {"HTMLDomScenarioRunner", "MathMLInHTML"},
    {"HTMLDomScenarioRunner", "SVGInHTML"},
});

// Modifies the process-global environment block, which is not thread-safe.
// This is expected to be safe in practice because GoogleTest runs tests and
// listener hooks sequentially on the main thread outside the test body and
// fixture setup/teardown.
void SetFuzzForEnvVar(const char* value) {
#ifdef _WIN32
  _putenv_s("FUZZTEST_FUZZ_FOR", value ? value : "");
#else
  if (value) {
    setenv("FUZZTEST_FUZZ_FOR", value, /*replace=*/1);
  } else {
    unsetenv("FUZZTEST_FUZZ_FOR");
  }
#endif
}

}  // namespace

DisableFlakyFuzzTestsListener::DisableFlakyFuzzTestsListener()
    : DisableFlakyFuzzTestsListener(
          std::vector<KnownFlakyFuzzTest>(kKnownFlakyFuzzTests.begin(),
                                          kKnownFlakyFuzzTests.end())) {}

DisableFlakyFuzzTestsListener::DisableFlakyFuzzTestsListener(
    std::vector<KnownFlakyFuzzTest> flaky_tests)
    : flaky_tests_(std::move(flaky_tests)) {}

DisableFlakyFuzzTestsListener::~DisableFlakyFuzzTestsListener() = default;

void DisableFlakyFuzzTestsListener::OnTestStart(
    const testing::TestInfo& test_info) {
  if (!std::ranges::contains(flaky_tests_, KnownFlakyFuzzTest{
                                               test_info.test_suite_name(),
                                               test_info.name(),
                                           })) {
    return;
  }
  const char* existing = std::getenv("FUZZTEST_FUZZ_FOR");
  if (existing != nullptr && !std::string_view(existing).empty()) {
    return;
  }
  SetFuzzForEnvVar("0s");
  did_override_env_ = true;
}

void DisableFlakyFuzzTestsListener::OnTestEnd(
    const testing::TestInfo& /*test_info*/) {
  if (did_override_env_) {
    SetFuzzForEnvVar(nullptr);
    did_override_env_ = false;
  }
}

}  // namespace fuzztest_init_helper
