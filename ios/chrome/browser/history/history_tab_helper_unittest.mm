// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/history_tab_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class HistoryTabHelperTest : public PlatformTest {
 public:
  void SetUp() override {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    ASSERT_TRUE(chrome_browser_state_->CreateHistoryService(true));

    web_state_.SetBrowserState(chrome_browser_state_.get());
    HistoryTabHelper::CreateForWebState(&web_state_);
  }

  // Queries the history service for information about the given |url| and
  // returns the response.  Spins the runloop until a response is received.
  void QueryURL(const GURL& url) {
    history::HistoryService* service =
        ios::HistoryServiceFactory::GetForBrowserState(
            chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);

    base::RunLoop loop;
    service->QueryURL(
        url, false,
        base::BindLambdaForTesting([&](history::QueryURLResult result) {
          latest_row_result_ = std::move(result.row);
          loop.Quit();
        }),
        &tracker_);
    loop.Run();
  }

  // Adds an entry for the given |url| to the history database.
  void AddVisitForURL(const GURL& url) {
    history::HistoryService* service =
        ios::HistoryServiceFactory::GetForBrowserState(
            chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
    service->AddPage(
        url, base::Time::Now(), NULL, 0, GURL(), history::RedirectList(),
        ui::PAGE_TRANSITION_MANUAL_SUBFRAME, history::SOURCE_BROWSED, false);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  web::TestWebState web_state_;
  base::CancelableTaskTracker tracker_;

  // Cached data from the last call to |QueryURL()|.
  history::URLRow latest_row_result_;
};

}  // namespace

// Tests that different urls can have different titles.
TEST_F(HistoryTabHelperTest, MultipleURLsWithTitles) {
  GURL first_url("https://first.google.com/");
  GURL second_url("https://second.google.com/");
  std::string first_title = "First Title";
  std::string second_title = "Second Title";
  HistoryTabHelper* helper = HistoryTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(helper);

  std::unique_ptr<web::NavigationItem> first_item =
      web::NavigationItem::Create();
  first_item->SetVirtualURL(first_url);
  first_item->SetTitle(base::UTF8ToUTF16(first_title));

  std::unique_ptr<web::NavigationItem> second_item =
      web::NavigationItem::Create();
  second_item->SetVirtualURL(second_url);
  second_item->SetTitle(base::UTF8ToUTF16(second_title));

  AddVisitForURL(first_url);
  AddVisitForURL(second_url);
  helper->UpdateHistoryPageTitle(*first_item);
  helper->UpdateHistoryPageTitle(*second_item);

  // Verify that the first title was set properly.
  QueryURL(first_url);
  EXPECT_EQ(first_url, latest_row_result_.url());
  EXPECT_EQ(base::UTF8ToUTF16(first_title), latest_row_result_.title());

  // Verify that the first title was set properly.
  QueryURL(second_url);
  EXPECT_EQ(second_url, latest_row_result_.url());
  EXPECT_EQ(base::UTF8ToUTF16(second_title), latest_row_result_.title());
}

// Tests that page titles are set properly and can be modified.
TEST_F(HistoryTabHelperTest, TitleUpdateForOneURL) {
  GURL test_url("https://www.google.com/");
  std::string first_title = "First Title";
  std::string second_title = "Second Title";
  HistoryTabHelper* helper = HistoryTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(helper);

  // Set the title and read it back again.
  AddVisitForURL(test_url);
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  item->SetVirtualURL(test_url);
  item->SetTitle(base::UTF8ToUTF16(first_title));
  helper->UpdateHistoryPageTitle(*item);
  QueryURL(test_url);
  EXPECT_EQ(test_url, latest_row_result_.url());
  EXPECT_EQ(base::UTF8ToUTF16(first_title), latest_row_result_.title());

  // Update the title and read it back again.
  std::unique_ptr<web::NavigationItem> update = web::NavigationItem::Create();
  update->SetVirtualURL(test_url);
  update->SetTitle(base::UTF8ToUTF16(second_title));
  helper->UpdateHistoryPageTitle(*update);
  QueryURL(test_url);
  EXPECT_EQ(base::UTF8ToUTF16(second_title), latest_row_result_.title());
}

// Tests that an empty title is not written to the history database.  (In the
// current implementation, the page's URL is used as its title.)
TEST_F(HistoryTabHelperTest, EmptyTitleIsNotWrittenToHistory) {
  GURL test_url("https://www.google.com/");
  std::string test_title = "";
  HistoryTabHelper* helper = HistoryTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(helper);

  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  item->SetVirtualURL(test_url);
  item->SetTitle(base::UTF8ToUTF16(test_title));

  AddVisitForURL(test_url);
  helper->UpdateHistoryPageTitle(*item);
  QueryURL(test_url);

  EXPECT_EQ(test_url, latest_row_result_.url());
  EXPECT_FALSE(latest_row_result_.title().empty());
}

// Tests that setting the empty title overwrites a previous, non-empty title.
TEST_F(HistoryTabHelperTest, EmptyTitleOverwritesPreviousTitle) {
  GURL test_url("https://www.google.com/");
  std::string test_title = "Test Title";
  HistoryTabHelper* helper = HistoryTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(helper);

  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  item->SetVirtualURL(test_url);
  item->SetTitle(base::UTF8ToUTF16(test_title));

  AddVisitForURL(test_url);
  helper->UpdateHistoryPageTitle(*item);
  QueryURL(test_url);
  EXPECT_EQ(test_url, latest_row_result_.url());
  EXPECT_EQ(base::UTF8ToUTF16(test_title), latest_row_result_.title());

  // Set the empty title and make sure the title is updated.
  item->SetTitle(base::string16());
  helper->UpdateHistoryPageTitle(*item);
  QueryURL(test_url);
  EXPECT_NE(base::UTF8ToUTF16(test_title), latest_row_result_.title());
}

// Tests that the ntp is not saved to history.
TEST_F(HistoryTabHelperTest, TestNTPNotAdded) {
  HistoryTabHelper* helper = HistoryTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(helper);

  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  GURL test_url("https://www.google.com/");
  item->SetVirtualURL(test_url);
  AddVisitForURL(test_url);
  QueryURL(test_url);
  EXPECT_EQ(test_url, latest_row_result_.url());

  item = web::NavigationItem::Create();
  GURL ntp_url(kChromeUIAboutNewTabURL);
  item->SetVirtualURL(ntp_url);
  AddVisitForURL(ntp_url);
  QueryURL(ntp_url);
  EXPECT_NE(ntp_url, latest_row_result_.url());
}

// Tests that a file:// URL isn't added to history.
TEST_F(HistoryTabHelperTest, TestFileNotAdded) {
  HistoryTabHelper* helper = HistoryTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(helper);

  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  GURL test_url("https://www.google.com/");
  item->SetVirtualURL(test_url);
  AddVisitForURL(test_url);
  QueryURL(test_url);
  EXPECT_EQ(test_url, latest_row_result_.url());

  item = web::NavigationItem::Create();
  GURL file_url("file://path/to/file");
  item->SetVirtualURL(file_url);
  AddVisitForURL(file_url);
  QueryURL(file_url);
  EXPECT_NE(file_url, latest_row_result_.url());
}
