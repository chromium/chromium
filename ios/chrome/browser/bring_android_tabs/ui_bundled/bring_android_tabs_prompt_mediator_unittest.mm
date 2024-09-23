// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bring_android_tabs/ui_bundled/bring_android_tabs_prompt_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "ios/chrome/browser/bring_android_tabs/model/bring_android_tabs_to_ios_service.h"
#import "ios/chrome/browser/bring_android_tabs/model/fake_bring_android_tabs_to_ios_service.h"
#import "ios/chrome/browser/bring_android_tabs/model/metrics.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for BringAndroidTabsPromptMediator.
class BringAndroidTabsPromptMediatorTest : public PlatformTest {
 public:
  BringAndroidTabsPromptMediatorTest() : PlatformTest() {
    // Environment setup.
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetInstance(),
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDefaultFactory());
    profile_ = std::move(builder).Build();

    // Create a tab in the mock BringAndroidTabsToIOS service.
    std::vector<std::unique_ptr<synced_sessions::DistantTab>> tabs;
    std::unique_ptr<synced_sessions::DistantTab> tab =
        std::make_unique<synced_sessions::DistantTab>();
    tabs.push_back(std::move(tab));

    // Create the BringAndroidTabsToIOSService.
    segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDispatcherForProfile(profile_.get());
    syncer::SyncService* sync_service =
        SyncServiceFactory::GetForProfile(profile_.get());
    sync_sessions::SessionSyncService* session_sync_service =
        SessionSyncServiceFactory::GetForProfile(profile_.get());
    PrefService* prefs = profile_->GetPrefs();
    fake_bring_android_tabs_service_ = new FakeBringAndroidTabsToIOSService(
        std::move(tabs), dispatcher, sync_service, session_sync_service, prefs);

    // Create the mediator.
    mediator_ = [[BringAndroidTabsPromptMediator alloc]
        initWithBringAndroidTabsService:fake_bring_android_tabs_service_
                              URLLoader:nullptr];
  }
  // Property accessors.
  id<BringAndroidTabsPromptViewControllerDelegate> delegate() {
    return mediator_;
  }
  FakeBringAndroidTabsToIOSService* bring_android_tabs_service() {
    return fake_bring_android_tabs_service_;
  }

 private:
  // Environment mocks.
  web::WebTaskEnvironment task_environment_;
  raw_ptr<FakeBringAndroidTabsToIOSService> fake_bring_android_tabs_service_;
  // Mediator dependencies.
  std::unique_ptr<TestProfileIOS> profile_;
  BringAndroidTabsPromptMediator* mediator_;
};

// Tests that when the prompt is displayed, the mediator logs histogram and
// updates the profile pref accordingly.
TEST_F(BringAndroidTabsPromptMediatorTest, ShowPrompt) {
  base::HistogramTester histogram_tester;
  [delegate() bringAndroidTabsPromptViewControllerDidShow];
  EXPECT_TRUE(bring_android_tabs_service()->displayed());
  EXPECT_FALSE(bring_android_tabs_service()->interacted());
  // Verify that no duplicate metric is logged in histogram.
  [delegate() bringAndroidTabsPromptViewControllerDidShow];
  histogram_tester.ExpectUniqueSample(
      bring_android_tabs::kTabCountHistogramName, 1, 1);
}

// Tests when the prompt is displayed and the user taps "open tabs", the
// mediator logs histogram, opens tabs on request, and that the prompt display
// is recorded.
TEST_F(BringAndroidTabsPromptMediatorTest, OpenTabs) {
  base::HistogramTester histogram_tester;
  [delegate() bringAndroidTabsPromptViewControllerDidShow];
  [delegate() bringAndroidTabsPromptViewControllerDidTapOpenAllButton];
  histogram_tester.ExpectUniqueSample(
      bring_android_tabs::kPromptActionHistogramName,
      bring_android_tabs::PromptActionType::kOpenTabs, 1);
  EXPECT_TRUE(bring_android_tabs_service()->interacted());
  std::vector<size_t> opened_tabs =
      bring_android_tabs_service()->opened_tabs_at_indices();
  ASSERT_EQ(opened_tabs.size(), 1u);
  EXPECT_EQ(opened_tabs[0], 0u);
}

// Tests that mediator logs histogram when the user taps "reviews tabs", and the
// interaction is recorded so that it would not be displayed again.
TEST_F(BringAndroidTabsPromptMediatorTest, ReviewTabs) {
  base::HistogramTester histogram_tester;
  [delegate() bringAndroidTabsPromptViewControllerDidShow];
  [delegate() bringAndroidTabsPromptViewControllerDidTapReviewButton];
  histogram_tester.ExpectUniqueSample(
      bring_android_tabs::kPromptActionHistogramName,
      bring_android_tabs::PromptActionType::kReviewTabs, 1);
  EXPECT_TRUE(bring_android_tabs_service()->interacted());
  EXPECT_EQ(bring_android_tabs_service()->opened_tabs_at_indices().size(), 0u);
}

// Tests that mediator logs histogram when the user closes the prompt, and the
// interaction is recorded so that it would not be displayed again.
TEST_F(BringAndroidTabsPromptMediatorTest, TapCloseButton) {
  base::HistogramTester histogram_tester;
  [delegate() bringAndroidTabsPromptViewControllerDidShow];
  [delegate() bringAndroidTabsPromptViewControllerDidDismissWithSwipe:NO];
  histogram_tester.ExpectUniqueSample(
      bring_android_tabs::kPromptActionHistogramName,
      bring_android_tabs::PromptActionType::kCancel, 1);
  EXPECT_TRUE(bring_android_tabs_service()->interacted());
  EXPECT_EQ(bring_android_tabs_service()->opened_tabs_at_indices().size(), 0u);
}

// Tests that mediator logs histogram when the user swipes the prompt down, and
// the interaction is recorded so that it would not be displayed again.
TEST_F(BringAndroidTabsPromptMediatorTest, SwipeToDismiss) {
  base::HistogramTester histogram_tester;
  [delegate() bringAndroidTabsPromptViewControllerDidShow];
  [delegate() bringAndroidTabsPromptViewControllerDidDismissWithSwipe:YES];
  histogram_tester.ExpectUniqueSample(
      bring_android_tabs::kPromptActionHistogramName,
      bring_android_tabs::PromptActionType::kSwipeToDismiss, 1);
  EXPECT_TRUE(bring_android_tabs_service()->interacted());
  EXPECT_EQ(bring_android_tabs_service()->opened_tabs_at_indices().size(), 0u);
}
