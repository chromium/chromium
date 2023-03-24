// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bring_android_tabs/bring_android_tabs_prompt_mediator.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "ios/chrome/browser/bring_android_tabs/bring_android_tabs_to_ios_service.h"
#import "ios/chrome/browser/bring_android_tabs/metrics.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/sync/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/distant_tab.h"
#import "ios/chrome/browser/url_loading/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A fake BringAndroidTabsToIOSService for testing purpose, with permanently one
// tab.
class FakeServiceWithOneTab : public BringAndroidTabsToIOSService {
 public:
  FakeServiceWithOneTab(
      std::unique_ptr<synced_sessions::DistantTab> tab,
      segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher,
      syncer::SyncService* sync_service,
      sync_sessions::SessionSyncService* session_sync_service,
      PrefService* browser_state_prefs)
      : BringAndroidTabsToIOSService(dispatcher,
                                     sync_service,
                                     session_sync_service,
                                     browser_state_prefs),
        tab_(std::move(tab)) {}
  size_t GetNumberOfAndroidTabs() const override { return 1; }
  synced_sessions::DistantTab* GetTabAtIndex(size_t index) const override {
    return tab_.get();
  }
  void OnBringAndroidTabsPromptDisplayed() override { displayed_ = true; }
  void OnUserInteractWithBringAndroidTabsPrompt() override {
    interacted_ = true;
  }

  bool displayed() { return displayed_; }
  bool interacted() { return interacted_; }

 private:
  std::unique_ptr<synced_sessions::DistantTab> tab_;
  bool displayed_ = false;
  bool interacted_ = false;
};

// Test fixture for BringAndroidTabsPromptMediator.
class BringAndroidTabsPromptMediatorTest : public PlatformTest {
 public:
  BringAndroidTabsPromptMediatorTest() : PlatformTest() {
    // Environment setup.
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetInstance(),
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDefaultFactory());
    browser_state_ = builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));

    // Create a tab in the mock BringAndroidTabsToIOS service.
    std::unique_ptr<synced_sessions::DistantTab> tab =
        std::make_unique<synced_sessions::DistantTab>();
    tab->virtual_url = kTestUrl;
    // Create the BringAndroidTabsToIOSService.
    segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDispatcherForBrowserState(browser_state_.get());
    syncer::SyncService* sync_service =
        SyncServiceFactory::GetForBrowserState(browser_state_.get());
    sync_sessions::SessionSyncService* session_sync_service =
        SessionSyncServiceFactory::GetForBrowserState(browser_state_.get());
    PrefService* prefs = browser_state_->GetPrefs();
    fake_bring_android_tabs_service_ = new FakeServiceWithOneTab(
        std::move(tab), dispatcher, sync_service, session_sync_service, prefs);

    // Create the mediator.
    mediator_ = [[BringAndroidTabsPromptMediator alloc]
        initWithBringAndroidTabsService:fake_bring_android_tabs_service_
                              URLLoader:url_loader_];
  }
  // Property accessors.
  id<BringAndroidTabsPromptViewControllerDelegate> delegate() {
    return mediator_;
  }
  FakeServiceWithOneTab* bring_android_tabs_service() {
    return fake_bring_android_tabs_service_;
  }
  web::NavigationManager::WebLoadParams last_loaded_url_params() {
    return url_loader_->last_params.web_params;
  }

  const GURL kTestUrl = GURL("http://chromium.org");

 private:
  // Environment mocks.
  web::WebTaskEnvironment task_environment_;
  FakeUrlLoadingBrowserAgent* url_loader_;
  FakeServiceWithOneTab* fake_bring_android_tabs_service_;
  // Mediator dependencies.
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  BringAndroidTabsPromptMediator* mediator_;
};

// Tests that when the prompt is displayed, the mediator logs histogram and
// updates the profile pref accordingly.
TEST_F(BringAndroidTabsPromptMediatorTest, ShowPrompt) {
  base::HistogramTester histogram_tester;
  [delegate() bringAndroidTabsPromptViewControllerDidShow];
  histogram_tester.ExpectUniqueSample(
      bring_android_tabs::kTabCountHistogramName, 1, 1);
  EXPECT_TRUE(bring_android_tabs_service()->displayed());
  EXPECT_FALSE(bring_android_tabs_service()->interacted());
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
  EXPECT_EQ(kTestUrl, last_loaded_url_params().url);
  EXPECT_TRUE(
      ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                   last_loaded_url_params().transition_type));
  EXPECT_TRUE(bring_android_tabs_service()->interacted());
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
}

// Tests that mediator logs histogram when the user closes the prompt, and the
// interaction is recorded so that it would not be displayed again.
TEST_F(BringAndroidTabsPromptMediatorTest, TapCloseButton) {
  base::HistogramTester histogram_tester;
  [delegate() bringAndroidTabsPromptViewControllerDidShow];
  [delegate() bringAndroidTabsPromptViewControllerDidDismiss:NO];
  histogram_tester.ExpectUniqueSample(
      bring_android_tabs::kPromptActionHistogramName,
      bring_android_tabs::PromptActionType::kCancel, 1);
  EXPECT_TRUE(bring_android_tabs_service()->interacted());
}

// Tests that mediator logs histogram when the user swipes the prompt down, and
// the interaction is recorded so that it would not be displayed again.
TEST_F(BringAndroidTabsPromptMediatorTest, SwipeToDismiss) {
  base::HistogramTester histogram_tester;
  [delegate() bringAndroidTabsPromptViewControllerDidShow];
  [delegate() bringAndroidTabsPromptViewControllerDidDismiss:YES];
  histogram_tester.ExpectUniqueSample(
      bring_android_tabs::kPromptActionHistogramName,
      bring_android_tabs::PromptActionType::kSwipeToDismiss, 1);
  EXPECT_TRUE(bring_android_tabs_service()->interacted());
}
