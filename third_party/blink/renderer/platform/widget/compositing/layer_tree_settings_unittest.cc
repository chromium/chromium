// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_settings.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#include "base/byte_size.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_amount_of_physical_memory_override.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#include "cc/tiles/image_decode_cache_utils.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#endif

namespace blink {

// Verify desktop memory limit calculations.
#if !BUILDFLAG(IS_ANDROID)
TEST(LayerTreeSettings, IgnoreGivenMemoryPolicy) {
  auto policy =
      GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256), gfx::Size(), 1.f);
  EXPECT_EQ(512u * 1024u * 1024u, policy.bytes_limit_when_visible);
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
            policy.priority_cutoff_when_visible);
}

TEST(LayerTreeSettings, LargeScreensUseMoreMemory) {
  auto policy = GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                                   gfx::Size(4096, 2160), 1.f);
  EXPECT_EQ(977272832u, policy.bytes_limit_when_visible);
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
            policy.priority_cutoff_when_visible);

  policy = GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                              gfx::Size(2056, 1329), 2.f);
  EXPECT_EQ(1152u * 1024u * 1024u, policy.bytes_limit_when_visible);
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
            policy.priority_cutoff_when_visible);
}
#else
namespace {

[[nodiscard]] base::ScopedClosureRunner SetIsDesktopForTesting(
    bool is_desktop = true) {
  base::android::device_info::set_is_desktop_for_testing(is_desktop);
  return base::ScopedClosureRunner(base::BindOnce(
      &base::android::device_info::reset_is_desktop_for_testing));
}

class WebViewPlatformSupport : public TestingPlatformSupport {
 public:
  bool IsSynchronousCompositingEnabledForAndroidWebView() override {
    return true;
  }
};

}  // namespace

TEST(LayerTreeSettings, DesktopAndroidMemoryPolicy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ::features::kDesktopAndroidUnifiedCompositorLimits);
  auto reset_desktop = SetIsDesktopForTesting(true);
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::GiB(8));

  auto policy =
      GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256), gfx::Size(), 1.f);
  EXPECT_EQ(512u * 1024u * 1024u, policy.bytes_limit_when_visible);
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
            policy.priority_cutoff_when_visible);

  policy = GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                              gfx::Size(2056, 1329), 2.f);
  EXPECT_EQ(1152u * 1024u * 1024u, policy.bytes_limit_when_visible);
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
            policy.priority_cutoff_when_visible);
}

TEST(LayerTreeSettings, DesktopAndroidFeatureDisabledMemoryPolicy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      ::features::kDesktopAndroidUnifiedCompositorLimits);
  auto reset_desktop = SetIsDesktopForTesting(true);
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::GiB(8));

  auto policy = GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                                   gfx::Size(2056, 1329), 2.f);
  EXPECT_EQ(256u * 1024u * 1024u, policy.bytes_limit_when_visible);
}

TEST(LayerTreeSettings, NonDesktopAndroidMemoryPolicy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ::features::kDesktopAndroidUnifiedCompositorLimits);
  auto reset_desktop = SetIsDesktopForTesting(false);
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::GiB(8));

  auto policy = GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                                   gfx::Size(2056, 1329), 2.f);
  EXPECT_EQ(256u * 1024u * 1024u, policy.bytes_limit_when_visible);
}

TEST(LayerTreeSettings, DesktopAndroidImageDecodeWorkingSet) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ::features::kDesktopAndroidUnifiedCompositorLimits);
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::GiB(8));

  {
    auto reset_desktop = SetIsDesktopForTesting(true);
    size_t desktop_working_set =
        cc::ImageDecodeCacheUtils::GetWorkingSetBytesForImageDecode(
            /*for_renderer=*/true);
    EXPECT_EQ(256u * 1024u * 1024u, desktop_working_set);
  }

  {
    auto reset_desktop = SetIsDesktopForTesting(false);
    size_t non_desktop_working_set =
        cc::ImageDecodeCacheUtils::GetWorkingSetBytesForImageDecode(
            /*for_renderer=*/true);
    EXPECT_EQ(128u * 1024u * 1024u, non_desktop_working_set);
  }
}

TEST(LayerTreeSettings, DesktopAndroidWebViewMemoryPolicy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ::features::kDesktopAndroidUnifiedCompositorLimits);
  auto reset_desktop = SetIsDesktopForTesting(true);
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::GiB(8));

  ScopedTestingPlatformSupport<WebViewPlatformSupport> platform;

  auto policy = GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                                   gfx::Size(2056, 1329), 2.f);
  EXPECT_EQ(256u * 1024u * 1024u, policy.bytes_limit_when_visible);
}
#endif

}  // namespace blink
