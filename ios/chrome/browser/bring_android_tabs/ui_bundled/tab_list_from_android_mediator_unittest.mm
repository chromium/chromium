// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bring_android_tabs/ui_bundled/tab_list_from_android_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "ios/chrome/browser/bring_android_tabs/model/bring_android_tabs_to_ios_service.h"
#import "ios/chrome/browser/bring_android_tabs/model/fake_bring_android_tabs_to_ios_service.h"
#import "ios/chrome/browser/bring_android_tabs/model/metrics.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

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
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetInstance(),
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDefaultFactory());
    profile_ = std::move(builder).Build();
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
            GetDispatcherForProfile(profile_.get());
    syncer::SyncService* sync_service =
        SyncServiceFactory::GetForProfile(profile_.get());
    sync_sessions::SessionSyncService* session_sync_service =
        SessionSyncServiceFactory::GetForProfile(profile_.get());
    PrefService* prefs = profile_->GetPrefs();
    fake_bring_android_tabs_service_ = new FakeBringAndroidTabsToIOSService(
        SetOfTabs(), dispatcher, sync_service, session_sync_service, prefs);
  }

  // Creates the mediator.
  void CreateMediator() {
    mediator_ = [[TabListFromAndroidMediator alloc]
        initWithBringAndroidTabsService:fake_bring_android_tabs_service_
                              URLLoader:nullptr
                          faviconLoader:IOSChromeFaviconLoaderFactory::
                                            GetForProfile(profile_.get())];
  }

 protected:
  // Environment mocks.
  web::WebTaskEnvironment task_environment_;
  raw_ptr<FakeBringAndroidTabsToIOSService> fake_bring_android_tabs_service_;
  // Mediator dependencies.
  std::unique_ptr<TestProfileIOS> profile_;
  TabListFromAndroidMediator* mediator_;
};

// Tests when the user taps "open", the mediator logs histogram and opens tabs
// on request.
TEST_F(TabListFromAndroidMediatorTest, DeselectAndOpenTabs) {
  base::HistogramTester histogram_tester;
  NSArray* array = @[ @0, @2 ];
  [mediator_
      tabListFromAndroidViewControllerDidTapOpenButtonWithTabIndices:array];

  histogram_tester.ExpectUniqueSample(
      bring_android_tabs::kTabListActionHistogramName,
      bring_android_tabs::TabsListActionType::kOpenTabs, 1);
  histogram_tester.ExpectUniqueSample(
      bring_android_tabs::kDeselectedTabCountHistogramName, 1, 1);

  // Check that the correct tabs are opened.
  std::vector<size_t> opened_tabs =
      fake_bring_android_tabs_service_->opened_tabs_at_indices();
  ASSERT_EQ(opened_tabs.size(), 2u);
  EXPECT_EQ(opened_tabs[0], 0u);
  EXPECT_EQ(opened_tabs[1], 2u);
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
  EXPECT_EQ(fake_bring_android_tabs_service_->opened_tabs_at_indices().size(),
            0u);
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
  EXPECT_EQ(fake_bring_android_tabs_service_->opened_tabs_at_indices().size(),
            0u);
}
