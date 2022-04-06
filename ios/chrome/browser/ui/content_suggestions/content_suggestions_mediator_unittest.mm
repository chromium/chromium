// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"

#include "base/time/default_clock.h"
#include "components/favicon/core/large_icon_service_impl.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/ntp_tiles/icon_cacher.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_cache_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/url_loading/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

std::unique_ptr<KeyedService> BuildReadingListModel(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  std::unique_ptr<ReadingListModelImpl> reading_list_model(
      new ReadingListModelImpl(nullptr, browser_state->GetPrefs(),
                               base::DefaultClock::GetInstance()));
  return reading_list_model;
}

}  // namespace

@protocol
    ContentSuggestionsMediatorDispatcher <BrowserCommands, SnackbarCommands>
@end

// Testing Suite for ContentSuggestionsMediator
class ContentSuggestionsMediatorTest : public PlatformTest {
 public:
  ContentSuggestionsMediatorTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();
    large_icon_service_.reset(new favicon::LargeIconServiceImpl(
        &mock_favicon_service_, nullptr, 32, favicon_base::IconType::kTouchIcon,
        "test_chrome"));
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
    fake_web_state_ = std::make_unique<web::FakeWebState>();
    InitializeReadingListModel();
    dispatcher_ =
        OCMProtocolMock(@protocol(ContentSuggestionsMediatorDispatcher));

    favicon::LargeIconService* largeIconService =
        IOSChromeLargeIconServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    LargeIconCache* cache = IOSChromeLargeIconCacheFactory::GetForBrowserState(
        chrome_browser_state_.get());
    std::unique_ptr<ntp_tiles::MostVisitedSites> mostVisitedSites =
        std::make_unique<ntp_tiles::MostVisitedSites>(
            &pref_service_, /*top_sites*/ nullptr, /*popular_sites*/ nullptr,
            /*custom_links*/ nullptr, /*icon_cacher*/ nullptr,
            /*supervisor=*/nullptr, true);
    ntp_tiles::MostVisitedSites::RegisterProfilePrefs(pref_service_.registry());
    ReadingListModel* readingListModel =
        ReadingListModelFactory::GetForBrowserState(
            chrome_browser_state_.get());
    mediator_ = [[ContentSuggestionsMediator alloc]
             initWithLargeIconService:largeIconService
                       largeIconCache:cache
                      mostVisitedSite:std::move(mostVisitedSites)
                     readingListModel:readingListModel
                          prefService:chrome_browser_state_.get()->GetPrefs()
        isGoogleDefaultSearchProvider:NO
                              browser:browser_.get()];
    mediator_.dispatcher = dispatcher_;
    mediator_.webStateList = browser_.get()->GetWebStateList();
    mediator_.webState = fake_web_state_.get();

    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));
  }

 protected:
  // Initialize reading list model and its required tab helpers.
  void InitializeReadingListModel() {
    fake_web_state_->SetBrowserState(chrome_browser_state_.get());
    ReadingListModelFactory::GetInstance()->SetTestingFactoryAndUse(
        chrome_browser_state_.get(),
        base::BindRepeating(&BuildReadingListModel));
  }

  web::WebTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  IOSChromeScopedTestingLocalState local_state_;
  testing::StrictMock<favicon::MockFaviconService> mock_favicon_service_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  id dispatcher_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  std::unique_ptr<favicon::LargeIconServiceImpl> large_icon_service_;
  ContentSuggestionsMediator* mediator_;
  FakeUrlLoadingBrowserAgent* url_loader_;
};

// Tests that the command is sent to the dispatcher when opening the Reading
// List.
TEST_F(ContentSuggestionsMediatorTest, TestOpenReadingList) {
  OCMExpect([dispatcher_ showReadingList]);

  // Action.
  [mediator_ openReadingList];

  // Test.
  EXPECT_OCMOCK_VERIFY(dispatcher_);
}

// Tests that the command is sent to the loader when opening a most visited.
TEST_F(ContentSuggestionsMediatorTest, TestOpenMostVisited) {
  GURL url = GURL("http://chromium.org");
  ContentSuggestionsMostVisitedItem* item =
      [[ContentSuggestionsMostVisitedItem alloc] initWithType:0];
  item.URL = url;

  // Action.
  [mediator_ openMostVisitedItem:item atIndex:0];

  // Test.
  EXPECT_EQ(url, url_loader_->last_params.web_params.url);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      url_loader_->last_params.web_params.transition_type));
}
