// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_manager.h"

#include "base/test/scoped_amount_of_physical_memory_override.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_config.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/display/test/test_screen.h"

namespace content {

namespace {

using display::test::TestScreen;

constexpr size_t kMB = 1024 * 1024;
constexpr size_t kBytesPerPixel = 4;
}  // namespace

struct TestCase {
  uint64_t ram_mb;
  size_t budget_before_resize;
  size_t budget_after_resize;
};

class NavigationEntryScreenshotManagerTest
    : public testing::Test,
      public testing::WithParamInterface<TestCase> {
 public:
  NavigationEntryScreenshotManagerTest()
      : task_environment_(std::make_unique<content::BrowserTaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::IO)),
        memory_(base::MiB(GetParam().ram_mb)),
        min_required_physical_rm_mb_auto_reset_(
            NavigationTransitionConfig::SetMinRequiredPhysicalRamMbForTesting(
                0)) {}

 private:
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_;
  base::AutoReset<int> min_required_physical_rm_mb_auto_reset_;
};

TEST_P(NavigationEntryScreenshotManagerTest, MaxCacheSize) {
  TestScreen screen(/*create_display=*/true,
                    /*register_screen=*/true);
  NavigationEntryScreenshotManager manager;

  EXPECT_EQ(manager.GetMaxCacheSize(), GetParam().budget_before_resize);

  // Resize display.
  auto display = screen.GetPrimaryDisplay();
  display.SetScaleAndBounds(1, {0, 0, 1600, 900});
  auto& display_list = screen.display_list();
  display_list.UpdateDisplay(display);

  EXPECT_EQ(manager.GetMaxCacheSize(), GetParam().budget_after_resize);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    NavigationEntryScreenshotManagerTest,
    testing::Values(
        TestCase{100,
                 static_cast<size_t>(
                     TestScreen::kDefaultScreenBounds.size().Area64()) *
                     kBytesPerPixel,
                 1600 * 900 * kBytesPerPixel},
        TestCase{1000, 5 * kMB, 1600 * 900 * kBytesPerPixel},
        TestCase{2000, 10 * kMB, 10 * kMB}),
    [](const testing::TestParamInfo<TestCase>& info) {
      return base::ToString(info.param.ram_mb);
    });

}  // namespace content
