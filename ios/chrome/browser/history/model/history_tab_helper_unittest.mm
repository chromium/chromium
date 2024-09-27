// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/model/history_tab_helper.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/task_environment.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class HistoryTabHelperTest : public PlatformTest {
 public:
  void SetUp() override {
    TestProfileIOS::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());
    profile_ = std::move(test_cbs_builder).Build();

    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));

    web_state_.SetBrowserState(profile_.get());
    HistoryTabHelper::CreateForWebState(&web_state_);
  }

  // Queries the history service for information about the given `url` and
  // returns the response.  Spins the runloop until a response is received.
  void QueryURL(const GURL& url) {
    history::HistoryService* service =
        ios::HistoryServiceFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);

    base::RunLoop loop;
    service->QueryURL(
        url, /*want_visits=*/true,
        base::BindLambdaForTesting([&](history::QueryURLResult result) {
          latest_row_result_ = std::move(result.row);
          latest_visits_result_ = std::move(result.visits);
          loop.Quit();
        }),
        &tracker_);
    loop.Run();
  }

  // Adds an entry for the given `url` to the history database.
  void AddVisitForURL(const GURL& url) {
    history::HistoryService* service =
        ios::HistoryServiceFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);
    service->AddPage(
        url, base::Time::Now(), 0, 0, GURL(), history::RedirectList(),
        ui::PAGE_TRANSITION_MANUAL_SUBFRAME, history::SOURCE_BROWSED, false);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeNavigationManager> navigation_manager_;
  base::CancelableTaskTracker tracker_;

  // Cached data from the last call to `QueryURL()`.
  history::URLRow latest_row_result_;
  history::VisitVector latest_visits_result_;
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
  item->SetTitle(std::u16string());
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

TEST_F(HistoryTabHelperTest, ShouldUpdateVisitDurationInHistory) {
  const GURL url1("https://url1.com");
  const GURL url2("https://url2.com");

  HistoryTabHelper* helper = HistoryTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(helper);

  web::FakeNavigationContext navigation_context;
  navigation_context.SetHasCommitted(true);
  navigation_context.SetResponseHeaders(
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 234 OK\r\n\r\n"));

  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  navigation_manager_->SetLastCommittedItem(item.get());

  // Navigate to `url1`.
  item->SetURL(url1);
  item->SetTimestamp(base::Time::Now());
  navigation_context.SetUrl(url1);
  static_cast<web::WebStateObserver*>(helper)->DidFinishNavigation(
      &web_state_, &navigation_context);

  // Make sure the visit showed up.
  QueryURL(url1);
  ASSERT_EQ(latest_row_result_.url(), url1);
  ASSERT_FALSE(latest_visits_result_.empty());
  // The duration shouldn't be set yet, since the visit is still open.
  EXPECT_TRUE(latest_visits_result_.back().visit_duration.is_zero());

  // Once the user navigates on, the duration of the first visit should be
  // populated.
  item->SetURL(url2);
  item->SetTimestamp(base::Time::Now());
  navigation_context.SetUrl(url2);
  static_cast<web::WebStateObserver*>(helper)->DidFinishNavigation(
      &web_state_, &navigation_context);

  // The duration of the first visit should be populated now.
  QueryURL(url1);
  ASSERT_EQ(latest_row_result_.url(), url1);
  ASSERT_FALSE(latest_visits_result_.empty());
  EXPECT_FALSE(latest_visits_result_.back().visit_duration.is_zero());
  // ...but not the duration of the second visit yet.
  QueryURL(url2);
  ASSERT_EQ(latest_row_result_.url(), url2);
  ASSERT_FALSE(latest_visits_result_.empty());
  EXPECT_TRUE(latest_visits_result_.back().visit_duration.is_zero());

  // Closing the tab should finish the second visit and populate its duration.
  static_cast<web::WebStateObserver*>(helper)->WebStateDestroyed(&web_state_);
  QueryURL(url2);
  ASSERT_EQ(latest_row_result_.url(), url2);
  ASSERT_FALSE(latest_visits_result_.empty());
  EXPECT_FALSE(latest_visits_result_.back().visit_duration.is_zero());
}

TEST_F(HistoryTabHelperTest,
       CreateAddPageArgsPopulatesOnVisitContextAnnotations) {
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  GURL test_url("https://www.google.com/");
  item->SetVirtualURL(test_url);

  web::FakeNavigationContext context;
  context.SetUrl(test_url);
  context.SetHasCommitted(true);

  std::string raw_response_headers = "HTTP/1.1 234 OK\r\n\r\n";
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      net::HttpResponseHeaders::TryToCreate(raw_response_headers);
  DCHECK(response_headers);
  context.SetResponseHeaders(response_headers);

  HistoryTabHelper* helper = HistoryTabHelper::FromWebState(&web_state_);
  history::HistoryAddPageArgs args =
      helper->CreateHistoryAddPageArgs(item.get(), &context);

  // Make sure the `context_annotations` are populated.
  ASSERT_TRUE(args.context_annotations.has_value());
  // Most of the actual fields can't be verified here, because the corresponding
  // data sources don't exist in this unit test (e.g. there's no Browser, no
  // other TabHelpers, etc). At least check the response code that was set up
  // above.
  EXPECT_EQ(args.context_annotations->response_code, 234);
}
