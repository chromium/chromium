// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_view_configuration.h"

#include "base/command_line.h"
#include "base/test/scoped_amount_of_physical_memory_override.h"
#include "base/test/scoped_command_line.h"
#include "build/build_config.h"
#include "device/vr/public/cpp/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/geometry/size.h"

namespace device {

namespace {

constexpr uint32_t kRecommendedImageWidth = 1440;
constexpr uint32_t kRecommendedImageHeight = 1584;

constexpr uint32_t kMaxImageWidth = 8192;
constexpr uint32_t kMaxImageHeight = 8192;

#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_LINUX)
// Per-view max texture width limit is 16384 / 2 = 8192.
// Raw max is 8192x8192.
// max_scale_w = 8192 / 1440 = 5.6888...
// max_scale_h = 8192 / 1584 = 5.1717...
// max_scale_limit = min(5.6888, 5.1717) = 5.1717...
constexpr double kDefaultDoubleWideScaleFactor = 5.1717;
#endif

constexpr XrViewConfigurationView kDefaultXrViewProperties{
    XR_TYPE_VIEW_CONFIGURATION_VIEW,
    nullptr,
    kRecommendedImageWidth,
    kMaxImageWidth,
    kRecommendedImageHeight,
    kMaxImageHeight,
    /*recommendedSwapchainSampleCount=*/1,
    /*maxSwapchainSampleCount=*/1};

// Max texture size 16384x16384 (e.g. GL_MAX_TEXTURE_SIZE on Android)
constexpr gfx::Size kMaxTextureSize{16384, 16384};

// Note that this doesn't account for clamping as we reach the edge of the max
// texture size for double-wide images, but is good for simple validations.
void ValidateScale(const OpenXrViewProperties& properties, double scale) {
  uint32_t expected_width = std::round(scale * kRecommendedImageWidth);
  uint32_t expected_height = std::round(scale * kRecommendedImageHeight);
  EXPECT_EQ(properties.Width(), expected_width);
  EXPECT_EQ(properties.Height(), expected_height);
  EXPECT_NEAR(properties.RecommendedViewportScale(), 1 / scale, 1e-4f);
}

}  // namespace

// Viewport scaling isn't supported on Windows or Linux, validate default
// behavior.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

TEST(OpenXrViewPropertiesTest, ValidateNoFramebufferScale) {
  OpenXrViewProperties properties(kDefaultXrViewProperties, /*view_count=*/2,
                                  kMaxTextureSize);

  // Since framebuffer scaling isn't supported, a scale of 1.0 should be
  // reported by default.
  ValidateScale(properties, 1.0);
}

TEST(OpenXrViewPropertiesTest, CommandLineSwitchIgnored) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kWebXrMaxFramebufferScale, "2.0");

  OpenXrViewProperties properties(kDefaultXrViewProperties, /*view_count=*/2,
                                  kMaxTextureSize);
  // Command-line override should not be applied if viewport scaling isn't
  // supported.
  ValidateScale(properties, 1.0);
}

#else

TEST(OpenXrViewPropertiesTest, LowMemoryDeviceClamping) {
  // Override memory to 8GB (low-memory threshold is <=8GB).
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::GiBU(8));

  OpenXrViewProperties properties(kDefaultXrViewProperties, /*view_count=*/2,
                                  kMaxTextureSize);

  // Low memory should suggest a max of 1.5 scale.
  ValidateScale(properties, 1.5);
}

TEST(OpenXrViewPropertiesTest, HighMemoryDeviceAspectPreservingMax) {
  // Override memory to 16GB (above 8GB threshold).
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::GiBU(16));

  OpenXrViewProperties properties(kDefaultXrViewProperties, /*view_count=*/2,
                                  kMaxTextureSize);

  ValidateScale(properties, kDefaultDoubleWideScaleFactor);
}

TEST(OpenXrViewPropertiesTest, CommandLineSwitchOverride) {
  // Override memory to 8GB (low-memory threshold is <=8GB). The command line
  // should ignore the default low-memory scale.
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::GiBU(8));
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kWebXrMaxFramebufferScale, "2.0");

  OpenXrViewProperties properties(kDefaultXrViewProperties, /*view_count=*/2,
                                  kMaxTextureSize);
  ValidateScale(properties, 2.0);
}

TEST(OpenXrViewPropertiesTest, CommandLineSwitchOverride_InvalidZero) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kWebXrMaxFramebufferScale, "0.0");

  OpenXrViewProperties properties(kDefaultXrViewProperties, /*view_count=*/2,
                                  kMaxTextureSize);
  ValidateScale(properties, kDefaultDoubleWideScaleFactor);
}

TEST(OpenXrViewPropertiesTest, CommandLineSwitchOverride_InvalidNegative) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kWebXrMaxFramebufferScale, "-2.0");

  OpenXrViewProperties properties(kDefaultXrViewProperties, /*view_count=*/2,
                                  kMaxTextureSize);
  ValidateScale(properties, kDefaultDoubleWideScaleFactor);
}

TEST(OpenXrViewPropertiesTest, CommandLineSwitchOverride_InvalidNan) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kWebXrMaxFramebufferScale, "Hello World");

  OpenXrViewProperties properties(kDefaultXrViewProperties, /*view_count=*/2,
                                  kMaxTextureSize);
  ValidateScale(properties, kDefaultDoubleWideScaleFactor);
}

TEST(OpenXrViewPropertiesTest, CommandLineSwitchOverride_InvalidLarge) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kWebXrMaxFramebufferScale, "10");

  OpenXrViewProperties properties(kDefaultXrViewProperties, /*view_count=*/2,
                                  kMaxTextureSize);
  // If the command line passes a value higher than what is allowed based on the
  // max texture size, we need to clamp to the max value possible for the
  // hardware.
  ValidateScale(properties, kDefaultDoubleWideScaleFactor);
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

}  // namespace device
