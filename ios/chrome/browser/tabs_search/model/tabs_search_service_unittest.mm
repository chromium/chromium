// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs_search/model/tabs_search_service.h"

#import <memory>
#import <vector>

#import "base/i18n/case_conversion.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/utf_string_conversions.h"
#import "components/sessions/core/tab_restore_service_impl.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/tabs/model/closing_web_state_observer_browser_agent.h"
#import "ios/chrome/browser/tabs_search/model/tabs_search_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {
// A search term which matches all WebStates.
const char16_t kSearchQueryMatchesAll[] = u"State";
// A search term which matches no WebStates.
const char16_t kSearchQueryMatchesNone[] = u"term";

// URL details for a sample WebState.
const char kWebState1Url[] =
    "http://www.url1.com/some/path/to/page?param=value";
const char16_t kWebState1Domain[] = u"url1";
const char16_t kWebState1PartialPath[] = u"path";
const char16_t kWebState1Param[] = u"param";
const char16_t kWebState1ParamValue[] = u"value";
// Title for a sample WebState.
const char16_t kWebState1Title[] = u"Web State 1";

// URL for a second sample WebState.
const char kWebState2Url[] = "http://www.url2.com/";
// Title for a second sample WebState.
const char16_t kWebState2Title[] = u"Web State 2";

}  // namespace

// Test fixture to test the search service.
class TabsSearchServiceTest : public PlatformTest {
 public:
  TabsSearchServiceTest() {
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        IOSChromeTabRestoreServiceFactory::GetInstance(),
        IOSChromeTabRestoreServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());
    profile_ = std::move(test_profile_builder).Build();

    browser_list_ = BrowserListFactory::GetForProfile(profile_.get());

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    ClosingWebStateObserverBrowserAgent::CreateForBrowser(browser_.get());

    browser_list_->AddBrowser(browser_.get());

    other_browser_ = std::make_unique<TestBrowser>(profile_.get());
    browser_list_->AddBrowser(other_browser_.get());

    incognito_browser_ =
        std::make_unique<TestBrowser>(profile_->GetOffTheRecordProfile());
    browser_list_->AddBrowser(incognito_browser_.get());

    other_incognito_browser_ =
        std::make_unique<TestBrowser>(profile_->GetOffTheRecordProfile());
    browser_list_->AddBrowser(other_incognito_browser_.get());
  }

 protected:
  // Appends a new web state to the web state list of `browser`.
  web::WebState* AppendNewWebState(Browser* browser,
                                   const std::u16string& title,
                                   const GURL& url) {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetVisibleURL(url);
    fake_web_state->SetTitle(title);

    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();

    navigation_manager->AddItem(url, ui::PAGE_TRANSITION_LINK);
    int item_index = navigation_manager->GetLastCommittedItemIndex();
    navigation_manager->GetItemAtIndex(item_index)->SetTitle(title);

    fake_web_state->SetNavigationManager(std::move(navigation_manager));

    web::FakeWebState* inserted_web_state = fake_web_state.get();
    browser->GetWebStateList()->InsertWebState(
        std::move(fake_web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    return inserted_web_state;
  }

  // Returns the associated search service for normal profile.
  TabsSearchService* search_service() {
    return TabsSearchServiceFactory::GetForProfile(profile_.get());
  }

  // Returns the associated search service for off the record profile.
  TabsSearchService* incognito_search_service() {
    return TabsSearchServiceFactory::GetForProfile(
        profile_->GetOffTheRecordProfile());
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<Browser> other_browser_;
  std::unique_ptr<Browser> incognito_browser_;
  std::unique_ptr<Browser> other_incognito_browser_;
  raw_ptr<BrowserList> browser_list_;
};

// Tests that no results are returned when there are no WebStates.
TEST_F(TabsSearchServiceTest, NoWebStates) {
  __block bool results_received = false;
  search_service()->Search(
      kSearchQueryMatchesAll,
      base::BindOnce(
          ^(std::vector<TabsSearchService::TabsSearchBrowserResults> results) {
            EXPECT_TRUE(results.empty());
            results_received = true;
          }));

  ASSERT_TRUE(results_received);
}

// Tests that no results are returned when the search term doesn't match any
// of the current tabs.
TEST_F(TabsSearchServiceTest, NoMatchingResults) {
  AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));

  __block bool results_received = false;
  search_service()->Search(
      kSearchQueryMatchesNone,
      base::BindOnce(
          ^(std::vector<TabsSearchService::TabsSearchBrowserResults> results) {
            EXPECT_TRUE(results.empty());
            results_received = true;
          }));

  ASSERT_TRUE(results_received);
}

// Tests that an exact page title matches the correct WebState.
TEST_F(TabsSearchServiceTest, MatchExactTitle) {
  web::WebState* expected_web_state =
      AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));
  AppendNewWebState(browser_.get(), kWebState2Title, GURL(kWebState2Url));

  __block bool results_received = false;
  search_service()->Search(
      kWebState1Title,
      base::BindOnce(
          ^(std::vector<TabsSearchService::TabsSearchBrowserResults> results) {
            ASSERT_EQ(1ul, results.size());
            TabsSearchService::TabsSearchBrowserResults& browser_results =
                results.front();
            EXPECT_EQ(expected_web_state, browser_results.web_states.front());
            EXPECT_EQ(browser_.get(), browser_results.browser);
            results_received = true;
          }));

  ASSERT_TRUE(results_received);
}

// Tests that a partial page title matches the correct WebStates.
TEST_F(TabsSearchServiceTest, MatchPartialTitle) {
  web::WebState* web_state_1 =
      AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));
  web::WebState* web_state_2 =
      AppendNewWebState(browser_.get(), kWebState2Title, GURL(kWebState2Url));

  __block bool results_received = false;
  search_service()->Search(
      kSearchQueryMatchesAll,
      base::BindOnce(
          ^(std::vector<TabsSearchService::TabsSearchBrowserResults> results) {
            ASSERT_EQ(1ul, results.size());
            TabsSearchService::TabsSearchBrowserResults& browser_results =
                results.front();
            ASSERT_EQ(2ul, browser_results.web_states.size());
            EXPECT_EQ(browser_results.browser, browser_.get());
            EXPECT_EQ(browser_results.web_states.front(), web_state_1);
            EXPECT_EQ(browser_results.web_states.back(), web_state_2);
            results_received = true;
          }));

  ASSERT_TRUE(results_received);
}

// Tests that WebStates can be matched across multiple Browsers.
TEST_F(TabsSearchServiceTest, MatchAcrossBrowsers) {
  web::WebState* web_state_1 =
      AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));
  web::WebState* web_state_2 = AppendNewWebState(
      other_browser_.get(), kWebState2Title, GURL(kWebState2Url));

  __block bool results_received = false;
  search_service()->Search(
      kSearchQueryMatchesAll,
      base::BindOnce(^(
          std::vector<TabsSearchService::TabsSearchBrowserResults> results) {
        ASSERT_EQ(2ul, results.size());
        TabsSearchService::TabsSearchBrowserResults* browser_result = nullptr;
        TabsSearchService::TabsSearchBrowserResults* other_browser_result =
            nullptr;
        if (results.front().browser == browser_.get()) {
          browser_result = &results.front();
          other_browser_result = &results.back();
        } else {
          other_browser_result = &results.front();
          browser_result = &results.back();
        }
        ASSERT_TRUE(browser_result);
        ASSERT_EQ(browser_result->browser, browser_.get());
        ASSERT_EQ(1ul, browser_result->web_states.size());

        ASSERT_TRUE(other_browser_result);
        ASSERT_EQ(other_browser_result->browser, other_browser_.get());
        ASSERT_EQ(1ul, other_browser_result->web_states.size());

        EXPECT_EQ(web_state_1, browser_result->web_states.front());
        EXPECT_EQ(web_state_2, other_browser_result->web_states.front());
        results_received = true;
      }));

  ASSERT_TRUE(results_received);
}

// Tests that matches from incognito tabs are not returned for normal browser
// state.
TEST_F(TabsSearchServiceTest, NoIncognitoResults) {
  web::WebState* expected_web_state =
      AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));
  AppendNewWebState(incognito_browser_.get(), kWebState2Title,
                    GURL(kWebState2Url));

  __block bool results_received = false;
  search_service()->Search(
      kSearchQueryMatchesAll,
      base::BindOnce(
          ^(std::vector<TabsSearchService::TabsSearchBrowserResults> results) {
            ASSERT_EQ(1ul, results.size());
            ASSERT_EQ(1ul, results.front().web_states.size());
            EXPECT_EQ(browser_.get(), results.front().browser);
            EXPECT_EQ(expected_web_state, results.front().web_states.front());
            results_received = true;
          }));

  ASSERT_TRUE(results_received);
}

// Tests that only incognito tabs are returned when searching off the record
// profile.
TEST_F(TabsSearchServiceTest, IncognitoResults) {
  AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));
  web::WebState* expected_web_state = AppendNewWebState(
      incognito_browser_.get(), kWebState2Title, GURL(kWebState2Url));

  __block bool results_received = false;
  incognito_search_service()->Search(
      kSearchQueryMatchesAll,
      base::BindOnce(
          ^(std::vector<TabsSearchService::TabsSearchBrowserResults> results) {
            ASSERT_EQ(1ul, results.size());
            ASSERT_EQ(1ul, results.front().web_states.size());
            EXPECT_EQ(incognito_browser_.get(), results.front().browser);
            EXPECT_EQ(expected_web_state, results.front().web_states.front());
            results_received = true;
          }));

  ASSERT_TRUE(results_received);
}

// Tests that a WebState is matched when the entire current URL is used as the
// search term.
TEST_F(TabsSearchServiceTest, MatchExactURL) {
  web::WebState* expected_web_state =
      AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));

  __block bool results_received = false;
  std::string full_url_string(kWebState1Url);
  search_service()->Search(
      base::UTF8ToUTF16(full_url_string),
      base::BindOnce(
          ^(std::vector<TabsSearchService::TabsSearchBrowserResults> results) {
            ASSERT_EQ(1ul, results.size());
            ASSERT_EQ(1ul, results.front().web_states.size());
            EXPECT_EQ(browser_.get(), results.front().browser);
            EXPECT_EQ(expected_web_state, results.front().web_states.front());
            results_received = true;
          }));

  ASSERT_TRUE(results_received);
}

// Tests that a WebState is matched when only the domain of the current URL is
// used as the search term.
TEST_F(TabsSearchServiceTest, MatchURLDomain) {
  web::WebState* expected_web_state =
      AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));

  __block bool results_received = false;
  search_service()->Search(
      kWebState1Domain,
      base::BindOnce(
          ^(std::vector<TabsSearchService::TabsSearchBrowserResults> results) {
            ASSERT_EQ(1ul, results.size());
            ASSERT_EQ(1ul, results.front().web_states.size());
            EXPECT_EQ(browser_.get(), results.front().browser);
            EXPECT_EQ(expected_web_state, results.front().web_states.front());
            results_received = true;
          }));

  ASSERT_TRUE(results_received);
}

// Tests that a WebState is matched when only a portion of the current URL path
// is used as the search term.
TEST_F(TabsSearchServiceTest, MatchURLPath) {
  web::WebState* expected_web_state =
      AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));

  __block bool results_received = false;
  search_service()->Search(
      kWebState1PartialPath,
      base::BindOnce(
          ^(std::vector<TabsSearchService::TabsSearchBrowserResults> results) {
            ASSERT_EQ(1ul, results.size());
            ASSERT_EQ(1ul, results.front().web_states.size());
            EXPECT_EQ(browser_.get(), results.front().browser);
            EXPECT_EQ(expected_web_state, results.front().web_states.front());
            results_received = true;
          }));

  ASSERT_TRUE(results_received);
}

// Tests that a WebState is matched when only a parameter name of the current
// URL is used as the search term.
TEST_F(TabsSearchServiceTest, MatchURLParam) {
  web::WebState* expected_web_state =
      AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));

  __block bool results_received = false;
  search_service()->Search(
      kWebState1Param,
      base::BindOnce(
          ^(std::vector<TabsSearchService::TabsSearchBrowserResults> results) {
            ASSERT_EQ(1ul, results.size());
            ASSERT_EQ(1ul, results.front().web_states.size());
            EXPECT_EQ(browser_.get(), results.front().browser);
            EXPECT_EQ(expected_web_state, results.front().web_states.front());
            results_received = true;
          }));

  ASSERT_TRUE(results_received);
}

// Tests that a WebState is matched when only a parameter value of the current
// URL is used as the search term.
TEST_F(TabsSearchServiceTest, MatchURLParamValue) {
  web::WebState* expected_web_state =
      AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));

  __block bool results_received = false;
  search_service()->Search(
      kWebState1ParamValue,
      base::BindOnce(
          ^(std::vector<TabsSearchService::TabsSearchBrowserResults> results) {
            ASSERT_EQ(1ul, results.size());
            ASSERT_EQ(1ul, results.front().web_states.size());
            EXPECT_EQ(browser_.get(), results.front().browser);
            EXPECT_EQ(expected_web_state, results.front().web_states.front());
            results_received = true;
          }));

  ASSERT_TRUE(results_received);
}

// Tests that the title search is case insensitive.
TEST_F(TabsSearchServiceTest, CaseInsensitiveTitleSearch) {
  web::WebState* expected_web_state =
      AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));

  auto uppercase_title = base::i18n::ToUpper(std::u16string(kWebState1Title));

  __block bool results_received = false;
  search_service()->Search(
      uppercase_title,
      base::BindOnce(
          ^(std::vector<TabsSearchService::TabsSearchBrowserResults> results) {
            ASSERT_EQ(1ul, results.size());
            ASSERT_EQ(1ul, results.front().web_states.size());
            EXPECT_EQ(browser_.get(), results.front().browser);
            EXPECT_EQ(expected_web_state, results.front().web_states.front());
            results_received = true;
          }));

  ASSERT_TRUE(results_received);
}

// Tests that the URL search is case insensitive.
TEST_F(TabsSearchServiceTest, CaseInsensitiveURLSearch) {
  web::WebState* expected_web_state =
      AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));

  auto uppercase_param_value = base::i18n::ToUpper(kWebState1ParamValue);

  __block bool results_received = false;
  search_service()->Search(
      uppercase_param_value,
      base::BindOnce(
          ^(std::vector<TabsSearchService::TabsSearchBrowserResults> results) {
            ASSERT_EQ(1ul, results.size());
            ASSERT_EQ(1ul, results.front().web_states.size());
            EXPECT_EQ(browser_.get(), results.front().browser);
            EXPECT_EQ(expected_web_state, results.front().web_states.front());
            results_received = true;
          }));

  ASSERT_TRUE(results_received);
}

// Tests that no recently closed tabs are returned before any tabs are closed.
TEST_F(TabsSearchServiceTest, RecentlyClosedNoResults) {
  AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));

  __block bool results_received = false;
  search_service()->SearchRecentlyClosed(
      kWebState1Title,
      base::BindOnce(
          ^(std::vector<TabsSearchService::RecentlyClosedItemPair> results) {
            ASSERT_EQ(0ul, results.size());
            results_received = true;
          }));

  ASSERT_TRUE(results_received);
}

// Tests that no recently closed tabs are returned when the search term doesn't
// match the recently closed tabs.
TEST_F(TabsSearchServiceTest, RecentlyClosedNoMatch) {
  AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));

  browser_->GetWebStateList()->CloseWebStateAt(/*index=*/0,
                                               WebStateList::CLOSE_NO_FLAGS);

  __block bool results_received = false;
  search_service()->SearchRecentlyClosed(
      kSearchQueryMatchesNone,
      base::BindOnce(
          ^(std::vector<TabsSearchService::RecentlyClosedItemPair> results) {
            ASSERT_EQ(0ul, results.size());
            results_received = true;
          }));

  ASSERT_TRUE(results_received);
}

// Tests that a recently closed tab is matched based on a title string match.
TEST_F(TabsSearchServiceTest, RecentlyClosedMatchTitle) {
  AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));
  AppendNewWebState(browser_.get(), kWebState2Title, GURL(kWebState2Url));

  browser_->GetWebStateList()->CloseWebStateAt(/*index=*/0,
                                               WebStateList::CLOSE_NO_FLAGS);

  __block bool results_received = false;
  search_service()->SearchRecentlyClosed(
      kWebState1Title,
      base::BindOnce(^(
          std::vector<TabsSearchService::RecentlyClosedItemPair> results) {
        ASSERT_EQ(1ul, results.size());
        const sessions::SerializedNavigationEntry& first_navigation_entry =
            results.front().second;
        EXPECT_EQ(kWebState1Url, first_navigation_entry.virtual_url().spec());
        EXPECT_EQ(kWebState1Title, first_navigation_entry.title());
        results_received = true;
      }));

  ASSERT_TRUE(results_received);
}

// Tests that a recently closed tab is matched based on a url match.
TEST_F(TabsSearchServiceTest, RecentlyClosedMatchURL) {
  AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));
  AppendNewWebState(browser_.get(), kWebState2Title, GURL(kWebState2Url));

  browser_->GetWebStateList()->CloseWebStateAt(/*index=*/0,
                                               WebStateList::CLOSE_NO_FLAGS);

  __block bool results_received = false;
  search_service()->SearchRecentlyClosed(
      kWebState1ParamValue,
      base::BindOnce(^(
          std::vector<TabsSearchService::RecentlyClosedItemPair> results) {
        ASSERT_EQ(1ul, results.size());
        const sessions::SerializedNavigationEntry& first_navigation_entry =
            results.front().second;

        EXPECT_EQ(kWebState1Url, first_navigation_entry.virtual_url().spec());
        EXPECT_EQ(kWebState1Title, first_navigation_entry.title());

        results_received = true;
      }));

  ASSERT_TRUE(results_received);
}

// Tests that a recently closed tab is matched based on a title string match.
TEST_F(TabsSearchServiceTest, RecentlyClosedMatchTitleAllClosed) {
  AppendNewWebState(browser_.get(), kWebState1Title, GURL(kWebState1Url));
  AppendNewWebState(browser_.get(), kWebState2Title, GURL(kWebState2Url));
  // Add a webstate which will not match `kSearchQueryMatchesAll`.
  AppendNewWebState(browser_.get(), u"X", GURL("http://abc.xyz"));

  CloseAllWebStates(*browser_->GetWebStateList(), WebStateList::CLOSE_NO_FLAGS);

  __block bool results_received = false;
  search_service()->SearchRecentlyClosed(
      kSearchQueryMatchesAll,
      base::BindOnce(^(
          std::vector<TabsSearchService::RecentlyClosedItemPair> results) {
        ASSERT_EQ(2ul, results.size());

        const sessions::SerializedNavigationEntry& first_navigation_entry =
            results.front().second;
        EXPECT_EQ(kWebState1Url, first_navigation_entry.virtual_url().spec());
        EXPECT_EQ(kWebState1Title, first_navigation_entry.title());

        const sessions::SerializedNavigationEntry& last_navigation_entry =
            results.back().second;
        EXPECT_EQ(kWebState2Url, last_navigation_entry.virtual_url().spec());
        EXPECT_EQ(kWebState2Title, last_navigation_entry.title());
        results_received = true;
      }));

  ASSERT_TRUE(results_received);
}
