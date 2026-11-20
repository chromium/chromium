// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/renderer_process_security_state.h"

#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/renderer_process_security_state.h"

namespace blink {

namespace {

constexpr auto kMojoJS = RendererProcessCapability::kMojoJSAllowedForProcess;
constexpr auto kMojoJSTest =
    RendererProcessCapability::kMojoJSTestAllowedForProcess;
constexpr auto kInvalidCapability =
    static_cast<RendererProcessCapability>(1U << 31);
constexpr uint32_t kAllTestCapabilities =
    RendererProcessCapabilityMask(kMojoJS) |
    RendererProcessCapabilityMask(kMojoJSTest);

TEST(RendererProcessSecurityStateTest, CapabilitiesForTesting) {
  EXPECT_TRUE(IsValidRendererProcessCapability(kMojoJS));
  EXPECT_TRUE(IsValidRendererProcessCapability(kMojoJSTest));
  EXPECT_FALSE(IsValidRendererProcessCapability(kInvalidCapability));

  RendererProcessSecurityState::InitializeForTesting();
  EXPECT_FALSE(RendererProcessSecurityState::InitializeForTesting());

  const struct {
    uint32_t capabilities;
    bool expect_mojo_js;
    bool expect_mojo_js_test;
  } kCases[] = {
      {0, false, false},
      {RendererProcessCapabilityMask(kMojoJS), true, false},
      {RendererProcessCapabilityMask(kMojoJSTest), false, true},
      {kAllTestCapabilities, true, true},
      {0xffffffff, true, true},
  };
  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.capabilities);
    RendererProcessSecurityState::SetCapabilitiesForTesting(
        test_case.capabilities);
    EXPECT_EQ(test_case.expect_mojo_js,
              RendererProcessSecurityState::HasCapability(kMojoJS));
    EXPECT_EQ(test_case.expect_mojo_js_test,
              RendererProcessSecurityState::HasCapability(kMojoJSTest));
  }

#if defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)
  EXPECT_CHECK_DEATH(
      RendererProcessSecurityState::HasCapability(kInvalidCapability));
#endif

  // Blink's shared unit-test process enables all test features globally. Leave
  // the capability state in the corresponding configuration for later tests.
  RendererProcessSecurityState::SetCapabilitiesForTesting(kAllTestCapabilities);
}

}  // namespace

}  // namespace blink
