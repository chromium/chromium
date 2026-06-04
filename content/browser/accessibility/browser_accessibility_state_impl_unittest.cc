// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "content/browser/accessibility/browser_accessibility_state_impl_win.h"
#endif

#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_mode_observer.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/test_ax_node_id_delegate.h"
#include "ui/accessibility/platform/test_ax_platform_tree_manager_delegate.h"
#include "ui/events/base_event_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/accessibility/browser_accessibility_state_impl_android.h"
#endif

namespace content {

class BrowserAccessibilityStateImplTest : public ::testing::Test {
 public:
  BrowserAccessibilityStateImplTest() = default;
  BrowserAccessibilityStateImplTest(const BrowserAccessibilityStateImplTest&) =
      delete;
  ~BrowserAccessibilityStateImplTest() override = default;

 protected:
  void SetUp() override {
    // Set the initial time to something non-zero.
    task_environment_.FastForwardBy(base::Seconds(100));
    state_ = BrowserAccessibilityStateImpl::GetInstance();
  }

  raw_ptr<BrowserAccessibilityStateImpl> state_;
  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ui::TestAXPlatformTreeManagerDelegate>
      test_browser_accessibility_delegate_;
  ui::TestAXNodeIdDelegate node_id_delegate_;
};



namespace {
using ::testing::_;

class MockAXModeObserver : public ui::AXModeObserver {
 public:
  MOCK_METHOD(void, OnAXModeAdded, (ui::AXMode mode), (override));
};

}  // namespace

TEST_F(BrowserAccessibilityStateImplTest,
       EnablingAccessibilityTwiceSendsASingleNotification) {
  // Initially accessibility should be disabled.
  EXPECT_EQ(ui::AXPlatform::GetInstance().GetMode(), ui::AXMode());

  auto& ax_platform = ui::AXPlatform::GetInstance();
  ::testing::StrictMock<MockAXModeObserver> mock_observer;
  base::ScopedObservation<ui::AXPlatform, ui::AXModeObserver>
      scoped_observation(&mock_observer);
  scoped_observation.Observe(&ax_platform);

  // Enable accessibility.
  EXPECT_CALL(mock_observer, OnAXModeAdded(ui::kAXModeComplete));
  ScopedAccessibilityModeOverride scoped_mode(ui::kAXModeComplete);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // A second call should be a no-op.
  ScopedAccessibilityModeOverride scoped_mode_2(ui::kAXModeComplete);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
}

// Tests platform activation filtering.
TEST_F(BrowserAccessibilityStateImplTest, PlatformActivationFiltering) {
  // Disabled by default in all unit tests.
  ASSERT_FALSE(state_->IsActivationFromPlatformEnabled());
  ASSERT_EQ(state_->GetAccessibilityMode(), ui::AXMode());

  {
    // Adding a modes from the platform is a no-op.
    auto complete = state_->CreateScopedModeForProcess(
        ui::kAXModeComplete | ui::AXMode::kFromPlatform);
    ASSERT_EQ(state_->GetAccessibilityMode(), ui::AXMode());

    // Until platform activation is enabled.
    state_->SetActivationFromPlatformEnabled(true);
    ASSERT_TRUE(state_->IsActivationFromPlatformEnabled());
    EXPECT_EQ(state_->GetAccessibilityMode(), ui::kAXModeComplete);

    // Enabling when already enabled does nothing.
    state_->SetActivationFromPlatformEnabled(true);
    ASSERT_TRUE(state_->IsActivationFromPlatformEnabled());
    EXPECT_EQ(state_->GetAccessibilityMode(), ui::kAXModeComplete);

    state_->SetActivationFromPlatformEnabled(false);
  }

  {
    // Adding modes without the bit works as expected.
    auto basic = state_->CreateScopedModeForProcess(ui::kAXModeBasic);
    EXPECT_EQ(state_->GetAccessibilityMode() & ui::kAXModeBasic,
              ui::kAXModeBasic);

    // And filtering has no impact.
    state_->SetActivationFromPlatformEnabled(true);
    EXPECT_EQ(state_->GetAccessibilityMode() & ui::kAXModeBasic,
              ui::kAXModeBasic);
    state_->SetActivationFromPlatformEnabled(false);
  }
}

#if BUILDFLAG(IS_WIN)

namespace {

constexpr char kClientProcessNativeApisHistogram[] =
    "Accessibility.WinUIA.ClientProcess.NativeAPIs";
constexpr char kClientProcessWebContentsHistogram[] =
    "Accessibility.WinUIA.ClientProcess.WebContents";
constexpr char kClientDisconnectedHistogram[] =
    "Accessibility.WinUIA.ClientDisconnected";

void ExpectNoUiaClientProcessHistograms(
    base::HistogramTester& histogram_tester) {
  histogram_tester.ExpectTotalCount(kClientProcessNativeApisHistogram, 0);
  histogram_tester.ExpectTotalCount(kClientProcessWebContentsHistogram, 0);
}

void ExpectNarratorAndNvdaSamples(base::HistogramTester& histogram_tester,
                                  const char* histogram_name) {
  histogram_tester.ExpectBucketCount(
      histogram_name, internal::HashUiaClientProcessName("narrator.exe"), 1);
  histogram_tester.ExpectBucketCount(
      histogram_name, internal::HashUiaClientProcessName("nvda.exe"), 1);
  histogram_tester.ExpectTotalCount(histogram_name, 2);
}

}  // namespace

TEST(BrowserAccessibilityStateImplWinTest,
     RecordsNativeApisClientProcessHistogramForNewNativeApisMode) {
  base::HistogramTester histogram_tester;

  internal::RecordUiaClientProcessHistogramsForModeChange(
      ui::AXMode(), ui::AXMode(ui::AXMode::kNativeAPIs),
      {"narrator.exe", "nvda.exe"});

  ExpectNarratorAndNvdaSamples(histogram_tester,
                               kClientProcessNativeApisHistogram);
  histogram_tester.ExpectTotalCount(kClientProcessWebContentsHistogram, 0);
}

TEST(BrowserAccessibilityStateImplWinTest,
     RecordsWebContentsClientProcessHistogramForNewWebContentsMode) {
  base::HistogramTester histogram_tester;

  internal::RecordUiaClientProcessHistogramsForModeChange(
      ui::AXMode(), ui::AXMode(ui::AXMode::kWebContents),
      {"narrator.exe", "nvda.exe"});

  ExpectNarratorAndNvdaSamples(histogram_tester,
                               kClientProcessWebContentsHistogram);
  histogram_tester.ExpectTotalCount(kClientProcessNativeApisHistogram, 0);
}

TEST(BrowserAccessibilityStateImplWinTest,
     RecordsBothClientProcessHistogramsForNewNativeApisAndWebContentsModes) {
  base::HistogramTester histogram_tester;

  internal::RecordUiaClientProcessHistogramsForModeChange(
      ui::AXMode(),
      ui::AXMode(ui::AXMode::kNativeAPIs | ui::AXMode::kWebContents),
      {"narrator.exe", "nvda.exe"});

  ExpectNarratorAndNvdaSamples(histogram_tester,
                               kClientProcessNativeApisHistogram);
  ExpectNarratorAndNvdaSamples(histogram_tester,
                               kClientProcessWebContentsHistogram);
}

TEST(BrowserAccessibilityStateImplWinTest,
     DoesNotRecordClientProcessHistogramsForNoMode) {
  base::HistogramTester histogram_tester;

  internal::RecordUiaClientProcessHistogramsForModeChange(
      ui::AXMode(), ui::AXMode(), {"narrator.exe"});

  ExpectNoUiaClientProcessHistograms(histogram_tester);
}

TEST(BrowserAccessibilityStateImplWinTest,
     DoesNotRecordClientProcessHistogramsForUntrackedMode) {
  base::HistogramTester histogram_tester;

  internal::RecordUiaClientProcessHistogramsForModeChange(
      ui::AXMode(), ui::AXMode(ui::AXMode::kExtendedProperties),
      {"narrator.exe"});

  ExpectNoUiaClientProcessHistograms(histogram_tester);
}

TEST(BrowserAccessibilityStateImplWinTest,
     DoesNotRecordClientProcessHistogramsForAlreadyEnabledTrackedMode) {
  base::HistogramTester histogram_tester;

  internal::RecordUiaClientProcessHistogramsForModeChange(
      ui::AXMode(ui::AXMode::kNativeAPIs),
      ui::AXMode(ui::AXMode::kNativeAPIs | ui::AXMode::kExtendedProperties),
      {"narrator.exe"});

  ExpectNoUiaClientProcessHistograms(histogram_tester);
}

TEST(BrowserAccessibilityStateImplWinTest,
     DoesNotRecordClientProcessHistogramsWithoutConnectedClients) {
  base::HistogramTester histogram_tester;

  internal::RecordUiaClientProcessHistogramsForModeChange(
      ui::AXMode(),
      ui::AXMode(ui::AXMode::kNativeAPIs | ui::AXMode::kWebContents), {});

  ExpectNoUiaClientProcessHistograms(histogram_tester);
}

TEST(BrowserAccessibilityStateImplWinTest,
     DeduplicatesClientProcessHistograms) {
  base::HistogramTester histogram_tester;

  internal::RecordUiaClientProcessHistogramsForModeChange(
      ui::AXMode(), ui::AXMode(ui::AXMode::kNativeAPIs),
      {"narrator.exe", "narrator.exe"});

  histogram_tester.ExpectBucketCount(
      kClientProcessNativeApisHistogram,
      internal::HashUiaClientProcessName("narrator.exe"), 1);
  histogram_tester.ExpectTotalCount(kClientProcessNativeApisHistogram, 1);
  histogram_tester.ExpectTotalCount(kClientProcessWebContentsHistogram, 0);
}

TEST(BrowserAccessibilityStateImplWinTest, RecordsDisconnectHistogram) {
  base::HistogramTester histogram_tester;

  internal::RecordUiaClientDisconnectedHistogram("narrator.exe");

  histogram_tester.ExpectUniqueSample(
      kClientDisconnectedHistogram,
      internal::HashUiaClientProcessName("narrator.exe"), 1);
}

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)

// Test Accessibility Histogram Recording.
TEST(BrowserAccessibilityStateImplAndroidTest,
     RecordAccessibilityTechHistograms) {
  base::HistogramTester histogram_tester;

  static constexpr std::array<uint32_t, 7> service_hashes = {
      0x1630cddb,  // Switch Access
      0x349d4b1a,  // TalkBack
      0xa5a469fc,  // Sound Amplifier
      0xb13e6179,  // Action Blocks
      0xb38ef877,  // Voice Access
      0xbc2897b4,  // BrailleBack
      0xf2c0d757,  // Accessibility Menu
  };

  static constexpr std::string_view histogram =
      "Accessibility.Android.RunningAccessibilityTools";

  // Try an unknown hash
  ASSERT_FALSE(RecordAssistiveTechHistogram(0, false));

  // Ensure we start at zero.
  histogram_tester.ExpectTotalCount(histogram, 0);

  // Start recording.
  for (int i = 0; i < service_hashes.size(); ++i) {
    ASSERT_TRUE(RecordAssistiveTechHistogram(service_hashes[i], false));
    histogram_tester.ExpectTotalCount(histogram, i + 1);
  }

  // Duplicate one histogram.
  ASSERT_TRUE(RecordAssistiveTechHistogram(service_hashes[0], false));
  histogram_tester.ExpectTotalCount(histogram, service_hashes.size() + 1);

  // Try an unknown accessibility tool.
  ASSERT_TRUE(RecordAssistiveTechHistogram(0, true));
  histogram_tester.ExpectTotalCount(histogram, service_hashes.size() + 2);
}

#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(BrowserAccessibilityStateImplTest,
       NativeAdaptedWebContentsInvariantFiltering) {
  TestBrowserContext browser_context;

  {
    // Mode with ONLY kNativeAdaptedWebContents should not trigger kWebContents.
    // In fact, since kWebContents is not enabled, any higher bits (above
    // NativeAPIs) are filtered out.
    auto scoped_mode = state_->CreateScopedModeForProcess(
        ui::AXMode::kNativeAdaptedWebContents);
    EXPECT_EQ(ui::AXMode(),
              state_->GetAccessibilityModeForBrowserContext(&browser_context));
  }

  {
    // Mode with kNativeAdaptedWebContents and kNativeAPIs should upgrade to
    // include kWebContents.
    auto scoped_mode = state_->CreateScopedModeForProcess(
        ui::AXMode::kNativeAdaptedWebContents | ui::AXMode::kNativeAPIs);
    ui::AXMode result_mode =
        state_->GetAccessibilityModeForBrowserContext(&browser_context);
    EXPECT_TRUE(result_mode.has_mode(ui::AXMode::kNativeAdaptedWebContents));
    EXPECT_TRUE(result_mode.has_mode(ui::AXMode::kNativeAPIs));
    EXPECT_TRUE(result_mode.has_mode(ui::AXMode::kWebContents));
  }
}

TEST_F(BrowserAccessibilityStateImplTest,
       NativeAdaptedWebContentsMetricsAreStripped) {
  base::HistogramTester histogram_tester;

  {
    // Applying kNativeAdaptedWebContents and kNativeAPIs should NOT record the
    // kNativeAdaptedWebContents flag in process-wide histograms to avoid UMA
    // pollution.
    auto scoped_mode = state_->CreateScopedModeForProcess(
        ui::AXMode::kNativeAdaptedWebContents | ui::AXMode::kNativeAPIs);

    // We expect UMA_AX_MODE_NATIVE_APIS to be recorded, but NOT
    // UMA_AX_MODE_NATIVE_ADAPTED_WEB_CONTENTS.
    histogram_tester.ExpectBucketCount(
        "Accessibility.ModeFlag",
        ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_NATIVE_APIS, 1);
    histogram_tester.ExpectBucketCount(
        "Accessibility.ModeFlag",
        ui::AXMode::ModeFlagHistogramValue::
            UMA_AX_MODE_NATIVE_ADAPTED_WEB_CONTENTS,
        0);
  }
}

}  // namespace content
