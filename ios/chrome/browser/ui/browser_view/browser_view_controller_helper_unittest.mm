// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/browser_view_controller_helper.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/gtest_support.h"
#include "third_party/ocmock/ocmock_extensions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

static const char kWebUrl[] = "http://www.chromium.org";
static const char16_t kWebUrl16[] = u"http://www.chromium.org";
static const char kNativeUrl[] = "chrome://version";

class BrowserViewControllerHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::BookmarkModelFactory::GetInstance(),
        ios::BookmarkModelFactory::GetDefaultFactory());

    chrome_browser_state_ = test_cbs_builder.Build();
    bookmarks::test::WaitForBookmarkModelToLoad(
        ios::BookmarkModelFactory::GetForBrowserState(
            chrome_browser_state_.get()));

    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(chrome_browser_state_.get());

    helper_ = [[BrowserViewControllerHelper alloc] init];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<web::FakeWebState> web_state_;
  BrowserViewControllerHelper* helper_;
};

TEST_F(BrowserViewControllerHelperTest, TestWhenCurrentWebStateIsNull) {
  EXPECT_FALSE([helper_ isToolbarLoading:nullptr]);
  EXPECT_FALSE([helper_ isWebStateBookmarked:nullptr]);
}

TEST_F(BrowserViewControllerHelperTest, TestIsLoading) {
  // An active webstate that is loading.
  web_state_->SetLoading(true);
  EXPECT_TRUE([helper_ isToolbarLoading:web_state_.get()]);

  // An active webstate that is not loading.
  web_state_->SetLoading(false);
  EXPECT_FALSE([helper_ isToolbarLoading:web_state_.get()]);

  // An active webstate that is pointing at a native URL.
  web_state_->SetLoading(true);
  web_state_->SetCurrentURL(GURL(kNativeUrl));
  EXPECT_FALSE([helper_ isToolbarLoading:web_state_.get()]);
}

TEST_F(BrowserViewControllerHelperTest, TestisWebStateBookmarked) {
  // Set the curent tab to `kWebUrl` and create a bookmark for `kWebUrl`, then
  // verify that the location bar model indicates that the URL is bookmarked.
  web_state_->SetCurrentURL(GURL(kWebUrl));
  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_browser_state_.get());
  const bookmarks::BookmarkNode* bookmarks =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node = bookmark_model->AddURL(
      bookmarks, bookmarks->children().size(), kWebUrl16, GURL(kWebUrl));
  EXPECT_TRUE([helper_ isWebStateBookmarked:web_state_.get()]);

  // Remove the bookmark and verify the location bar model indicates that the
  // URL is not bookmarked.
  bookmark_model->Remove(node);
  EXPECT_FALSE([helper_ isWebStateBookmarked:web_state_.get()]);
}

}  // namespace
