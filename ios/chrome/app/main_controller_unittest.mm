// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/main_controller.h"

#import "base/test/ios/wait_util.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/open_from_clipboard/fake_clipboard_recent_content.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/bookmarks/model/account_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_test_utils.h"
#import "ios/chrome/browser/sessions/fake_tab_restore_service.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Tests MainController.
class MainControllerTest : public PlatformTest {
 protected:
  MainControllerTest() {
    ClipboardRecentContent::SetInstance(
        std::make_unique<FakeClipboardRecentContent>());

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(IOSChromeTabRestoreServiceFactory::GetInstance(),
                              FakeTabRestoreService::GetTestingFactory());
    builder.AddTestingFactory(
        ios::LocalOrSyncableBookmarkModelFactory::GetInstance(),
        ios::LocalOrSyncableBookmarkModelFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        ReadingListModelFactory::GetInstance(),
        base::BindRepeating(&BuildReadingListModelWithFakeStorage,
                            std::vector<scoped_refptr<ReadingListEntry>>()));

    browser_state_ = builder.Build();

    bookmarks::BookmarkModel* bookmarks_model =
        ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
            browser_state_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmarks_model);
    bookmarks::BookmarkModel* account_bookmark_model =
        ios::AccountBookmarkModelFactory::GetForBrowserState(
            browser_state_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(account_bookmark_model);

    app_state_ = [[AppState alloc] initWithStartupInformation:nil];
  }

  MainController* CreateMainController() {
    MainController* main_controller = [[MainController alloc] init];
    main_controller.appState = app_state_;
    return main_controller;
  }

  web::WebTaskEnvironment task_environment_;
  AppState* app_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

// Tests removing the browsing data with no interface provider. There is no
// clear repro steps for this issue but it is happening in the wild. Prevent
// regression on crbug.com/1522188.
TEST_F(MainControllerTest, RemoveBrowsingDataNoInterfaceProvider) {
  MainController* main_controller = CreateMainController();

  base::RunLoop run_loop;
  [main_controller
      removeBrowsingDataForBrowserState:browser_state_.get()
                             timePeriod:browsing_data::TimePeriod::ALL_TIME
                             removeMask:BrowsingDataRemoveMask::REMOVE_ALL
                        completionBlock:base::CallbackToBlock(
                                            run_loop.QuitClosure())];
  run_loop.Run();
}
