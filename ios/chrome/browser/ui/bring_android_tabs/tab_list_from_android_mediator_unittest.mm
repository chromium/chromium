// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bring_android_tabs/tab_list_from_android_mediator.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "ios/chrome/browser/bring_android_tabs/bring_android_tabs_to_ios_service.h"
#import "ios/chrome/browser/bring_android_tabs/fake_bring_android_tabs_to_ios_service.h"
#import "ios/chrome/browser/bring_android_tabs/metrics.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/sync/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/distant_tab.h"
#import "ios/chrome/browser/url_loading/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const std::vector<std::string> kTestUrls{
    "http://chromium.org", "http://google.com", "http://example.com"};
}  // namespace

// Test fixture for TabListFromAndroidMediator.
class TabListFromAndroidMediatorTest : public PlatformTest {
 protected:
  TabListFromAndroidMediatorTest() : PlatformTest() {
    SetUpEnvironment();
    CreateBringAndroidTabsToIOSService();
    CreateMediator();
  }

  // Sets up the environment.
  void SetUpEnvironment() {
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
  }

  // Creates the set of tabs to pass to the FakeBringAndroidTabsToIOSService and
  // open.
  std::vector<std::unique_ptr<synced_sessions::DistantTab>> SetOfTabs() {
    std::vector<std::unique_ptr<synced_sessions::DistantTab>> tabs;
    for (std::string url : kTestUrls) {
      std::unique_ptr<synced_sessions::DistantTab> tab =
          std::make_unique<synced_sessions::DistantTab>();
      tab->virtual_url = GURL(url);
      tabs.push_back(std::move(tab));
    }
    return tabs;
  }

  // Creates the fake BringAndroidTabsToIOSService that takes a set of tabs as
  // input.
  void CreateBringAndroidTabsToIOSService() {
    segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDispatcherForBrowserState(browser_state_.get());
    syncer::SyncService* sync_service =
        SyncServiceFactory::GetForBrowserState(browser_state_.get());
    sync_sessions::SessionSyncService* session_sync_service =
        SessionSyncServiceFactory::GetForBrowserState(browser_state_.get());
    PrefService* prefs = browser_state_->GetPrefs();
    fake_bring_android_tabs_service_ = new FakeBringAndroidTabsToIOSService(
        SetOfTabs(), dispatcher, sync_service, session_sync_service, prefs);
  }

  // Creates the mediator.
  void CreateMediator() {
    mediator_ = [[TabListFromAndroidMediator alloc]
        initWithBringAndroidTabsService:fake_bring_android_tabs_service_
                              URLLoader:url_loader_
                          faviconLoader:IOSChromeFaviconLoaderFactory::
                                            GetForBrowserState(
                                                browser_state_.get())];
  }

 protected:
  // Environment mocks.
  web::WebTaskEnvironment task_environment_;
  FakeUrlLoadingBrowserAgent* url_loader_;
  FakeBringAndroidTabsToIOSService* fake_bring_android_tabs_service_;
  // Mediator dependencies.
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  TabListFromAndroidMediator* mediator_;
};

// Tests when the user taps "open", the mediator logs histogram and opens tabs
// on request.
TEST_F(TabListFromAndroidMediatorTest, OpenTabs) {
  base::HistogramTester histogram_tester;
  NSArray* array = @[ @0, @1, @2 ];
  [mediator_
      tabListFromAndroidViewControllerDidTapOpenButtonWithTabIndices:array];

  histogram_tester.ExpectUniqueSample(
      bring_android_tabs::kTabListActionHistogramName,
      bring_android_tabs::TabsListActionType::kOpenTabs, 1);
  // Expect to log 0 becuase [array count] - GetNumberOfAndroidTabs() = 0.
  histogram_tester.ExpectUniqueSample(
      bring_android_tabs::kDeselectedTabCountHistogramName, 0, 1);

  web::NavigationManager::WebLoadParams url_params =
      url_loader_->last_params.web_params;
  EXPECT_EQ(GURL(kTestUrls[2]), url_params.url);
  EXPECT_EQ(static_cast<int>(kTestUrls.size()),
            url_loader_->load_new_tab_call_count);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                           url_params.transition_type));
}

// Tests that the mediator logs histogram when the user taps the cancel button.
TEST_F(TabListFromAndroidMediatorTest, TapCancelButton) {
  base::HistogramTester histogram_tester;
  [mediator_ tabListFromAndroidViewControllerDidDismissWithSwipe:NO
                                          numberOfDeselectedTabs:10];
  histogram_tester.ExpectUniqueSample(
      bring_android_tabs::kTabListActionHistogramName,
      bring_android_tabs::TabsListActionType::kCancel, 1);
  histogram_tester.ExpectUniqueSample(
      bring_android_tabs::kDeselectedTabCountHistogramName, 10, 1);
}

// Tests that the mediator logs histogram when the user swipes down to dismiss
// the table.
TEST_F(TabListFromAndroidMediatorTest, SwipeToDismiss) {
  base::HistogramTester histogram_tester;
  [mediator_ tabListFromAndroidViewControllerDidDismissWithSwipe:YES
                                          numberOfDeselectedTabs:5];
  histogram_tester.ExpectUniqueSample(
      bring_android_tabs::kTabListActionHistogramName,
      bring_android_tabs::TabsListActionType::kSwipeDown, 1);
  histogram_tester.ExpectUniqueSample(
      bring_android_tabs::kDeselectedTabCountHistogramName, 5, 1);
}
