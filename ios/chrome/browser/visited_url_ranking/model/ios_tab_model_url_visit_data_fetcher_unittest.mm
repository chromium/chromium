// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/visited_url_ranking/model/ios_tab_model_url_visit_data_fetcher.h"

#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "components/visited_url_ranking/public/fetcher_config.h"
#import "components/visited_url_ranking/public/url_visit.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

using tab_groups::TabGroupId;

class IOSTabModelURLVisitDataFetcherTest : public PlatformTest {
 protected:
  IOSTabModelURLVisitDataFetcherTest() {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();

    main_browser_ = std::make_unique<TestBrowser>(profile_.get());
    otr_browser_ =
        std::make_unique<TestBrowser>(profile_->GetOffTheRecordProfile());

    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(main_browser_.get());
    browser_list->AddBrowser(otr_browser_.get());
  }

  std::unique_ptr<web::FakeWebState> CreateFakeWebStateWithURL(
      const GURL& url) {
    auto web_state = std::make_unique<web::FakeWebState>();
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->AddItem(url, ui::PAGE_TRANSITION_LINK);
    navigation_manager->GetItemAtIndex(0)->SetTimestamp(base::Time::Now());
    navigation_manager->SetLastCommittedItem(
        navigation_manager->GetItemAtIndex(0));
    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state->SetBrowserState(profile_.get());
    web_state->SetNavigationItemCount(1);
    web_state->SetCurrentURL(url);
    IOSChromeSessionTabHelper::CreateForWebState(web_state.get());
    return web_state;
  }

  void CreateNormalWebStatesWithURLs(std::vector<std::string> normal_urls) {
    for (unsigned int i = 0; i < normal_urls.size(); i++) {
      auto web_state = CreateFakeWebStateWithURL(GURL(normal_urls[i]));
      main_browser_->GetWebStateList()->InsertWebState(
          std::move(web_state), WebStateList::InsertionParams::AtIndex(i));
    }
  }

  void CreateOtrWebStatesWithURLs(std::vector<std::string> otr_urls) {
    for (unsigned int i = 0; i < otr_urls.size(); i++) {
      auto web_state = CreateFakeWebStateWithURL(GURL(otr_urls[i]));
      otr_browser_->GetWebStateList()->InsertWebState(
          std::move(web_state), WebStateList::InsertionParams::AtIndex(i));
    }
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> main_browser_;
  std::unique_ptr<TestBrowser> otr_browser_;
};

// Tests normal flow. Fetch normal tabs and not OTR.
TEST_F(IOSTabModelURLVisitDataFetcherTest, FetchNormalTabsAndNotOtr) {
  std::vector<std::string> normal_urls = {"https://foo/bar", "https://car/tar",
                                          "https://hello/world"};
  CreateNormalWebStatesWithURLs(normal_urls);
  std::vector<std::string> otr_urls = {"https://foo.incognito/bar"};
  CreateOtrWebStatesWithURLs(otr_urls);

  auto fetcher =
      std::make_unique<visited_url_ranking::IOSTabModelURLVisitDataFetcher>(
          profile_.get());
  visited_url_ranking::FetchOptions options = visited_url_ranking::
      FetchOptions::CreateDefaultFetchOptionsForTabResumption();
  fetcher->FetchURLVisitData(
      options, visited_url_ranking::FetcherConfig(),
      base::BindOnce(^(visited_url_ranking::FetchResult result) {
        EXPECT_EQ(result.status,
                  visited_url_ranking::FetchResult::Status::kSuccess);
        ASSERT_EQ(result.data.size(), 3u);
        for (auto i = 0; i < 3; i++) {
          std::string key = normal_urls[i];
          EXPECT_TRUE(result.data.count(key));
          auto element_iterator = result.data.find(key);
          ASSERT_NE(element_iterator, result.data.end());
          visited_url_ranking::URLVisitAggregate::TabData& tab_data =
              std::get<visited_url_ranking::URLVisitAggregate::TabData>(
                  element_iterator->second);
          EXPECT_EQ(normal_urls[i], tab_data.last_active_tab.visit.url);
        }
      }));
}

// Tests that the info (group, pin state, time) is fetched correctly.
TEST_F(IOSTabModelURLVisitDataFetcherTest, FetchNormalTabsWithData) {
  base::Time now = base::Time::Now();
  std::vector<std::string> normal_urls = {"https://foo/bar", "https://car/tar",
                                          "https://hello/world"};
  CreateNormalWebStatesWithURLs(normal_urls);

  WebStateList* web_state_list = main_browser_->GetWebStateList();
  ASSERT_EQ(3, web_state_list->count());

  // First WebState is pinned and has time now - 1 hours.
  // Do this first as pinning tabs reorder them.
  EXPECT_EQ(GURL(normal_urls[0]),
            web_state_list->GetWebStateAt(0)->GetLastCommittedURL());
  web_state_list->SetWebStatePinnedAt(0, true);
  web_state_list->GetWebStateAt(0)
      ->GetNavigationManager()
      ->GetLastCommittedItem()
      ->SetTimestamp(now - base::Hours(1));

  // Second WebState has time now -7days, 1 hour. It should be ignored.
  EXPECT_EQ(GURL(normal_urls[1]),
            web_state_list->GetWebStateAt(1)->GetLastCommittedURL());
  web_state_list->GetWebStateAt(1)
      ->GetNavigationManager()
      ->GetLastCommittedItem()
      ->SetTimestamp(now - base::Hours(169));

  // Third WebState is in a group and has time now - 23 hours.
  tab_groups::TabGroupVisualData visual_data(
      u"test", tab_groups::TabGroupColorId::kGrey);
  EXPECT_EQ(GURL(normal_urls[2]),
            web_state_list->GetWebStateAt(2)->GetLastCommittedURL());
  web_state_list->CreateGroup({2}, visual_data, TabGroupId::GenerateNew());
  web_state_list->GetWebStateAt(2)
      ->GetNavigationManager()
      ->GetLastCommittedItem()
      ->SetTimestamp(now - base::Hours(23));

  auto fetcher =
      std::make_unique<visited_url_ranking::IOSTabModelURLVisitDataFetcher>(
          profile_.get());
  visited_url_ranking::FetchOptions options = visited_url_ranking::
      FetchOptions::CreateDefaultFetchOptionsForTabResumption();
  fetcher->FetchURLVisitData(
      options, visited_url_ranking::FetcherConfig(),
      base::BindOnce(^(visited_url_ranking::FetchResult result) {
        EXPECT_EQ(result.status,
                  visited_url_ranking::FetchResult::Status::kSuccess);
        ASSERT_EQ(result.data.size(), 2u);

        std::string key0 = normal_urls[0];
        EXPECT_TRUE(result.data.count(key0));
        auto element_iterator0 = result.data.find(key0);
        ASSERT_NE(element_iterator0, result.data.end());
        visited_url_ranking::URLVisitAggregate::TabData& tab_data0 =
            std::get<visited_url_ranking::URLVisitAggregate::TabData>(
                element_iterator0->second);
        auto tab0 = tab_data0.last_active_tab;
        EXPECT_EQ(normal_urls[0], tab0.visit.url);
        EXPECT_FALSE(tab_data0.in_group);
        EXPECT_TRUE(tab_data0.pinned);
        EXPECT_EQ(now - base::Hours(1), tab0.visit.last_modified);

        std::string key1 = normal_urls[2];
        EXPECT_TRUE(result.data.count(key1));
        auto element_iterator1 = result.data.find(key1);
        ASSERT_NE(element_iterator1, result.data.end());
        visited_url_ranking::URLVisitAggregate::TabData& tab_data1 =
            std::get<visited_url_ranking::URLVisitAggregate::TabData>(
                element_iterator1->second);
        auto tab1 = tab_data1.last_active_tab;
        EXPECT_EQ(normal_urls[2], tab1.visit.url);
        EXPECT_TRUE(tab_data1.in_group);
        EXPECT_FALSE(tab_data1.pinned);
        EXPECT_EQ(now - base::Hours(23), tab1.visit.last_modified);
      }));
}

// Tests that unrealized tabs are fetch without being realized.
TEST_F(IOSTabModelURLVisitDataFetcherTest, FetchUnrealizedTab) {
  std::string url = "https://foo/bar";
  base::Time now = base::Time::Now();
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetIsRealized(false);
  web_state->SetBrowserState(profile_.get());
  web_state->SetCurrentURL(GURL(url));
  web_state->SetLastActiveTime(now - base::Hours(1));
  IOSChromeSessionTabHelper::CreateForWebState(web_state.get());
  main_browser_->GetWebStateList()->InsertWebState(
      std::move(web_state), WebStateList::InsertionParams::AtIndex(0));
  auto fetcher =
      std::make_unique<visited_url_ranking::IOSTabModelURLVisitDataFetcher>(
          profile_.get());
  visited_url_ranking::FetchOptions options = visited_url_ranking::
      FetchOptions::CreateDefaultFetchOptionsForTabResumption();
  fetcher->FetchURLVisitData(
      options, visited_url_ranking::FetcherConfig(),
      base::BindOnce(^(visited_url_ranking::FetchResult result) {
        ASSERT_EQ(result.data.size(), 1u);
        EXPECT_TRUE(result.data.count(url));
        auto element_iterator = result.data.find(url);
        ASSERT_NE(element_iterator, result.data.end());
        visited_url_ranking::URLVisitAggregate::TabData& tab_data =
            std::get<visited_url_ranking::URLVisitAggregate::TabData>(
                element_iterator->second);
        EXPECT_EQ(url, tab_data.last_active_tab.visit.url);
        EXPECT_EQ(now - base::Hours(1),
                  tab_data.last_active_tab.visit.last_modified);
      }));
  EXPECT_FALSE(
      main_browser_->GetWebStateList()->GetWebStateAt(0)->IsRealized());
}

// Tests that tabs with same URL are aggregated.
TEST_F(IOSTabModelURLVisitDataFetcherTest, AggregateEntries) {
  base::Time now = base::Time::Now();
  std::string url = "https://foo/bar";
  std::string url2 = "https://car/tar";
  std::vector<std::string> normal_urls = {url, url, url2, url};
  CreateNormalWebStatesWithURLs(normal_urls);

  WebStateList* web_state_list = main_browser_->GetWebStateList();
  ASSERT_EQ(4, web_state_list->count());

  // First WebState is pinned and has time now - 2 hours.
  // Do this first as pinning tabs reorder them.
  web::FakeWebState* web_state0 =
      static_cast<web::FakeWebState*>(web_state_list->GetWebStateAt(0));
  EXPECT_EQ(GURL(url), web_state0->GetLastCommittedURL());
  web_state_list->SetWebStatePinnedAt(0, true);
  web_state0->GetNavigationManager()->GetLastCommittedItem()->SetTimestamp(
      now - base::Hours(2));
  web_state0->SetLastActiveTime(now - base::Hours(4));

  // Second WebState has a group and a time now - 1 hour. This is the last
  // tab for this URL.
  web::FakeWebState* web_state1 =
      static_cast<web::FakeWebState*>(web_state_list->GetWebStateAt(1));
  EXPECT_EQ(GURL(url), web_state1->GetLastCommittedURL());
  tab_groups::TabGroupVisualData visual_data(
      u"test", tab_groups::TabGroupColorId::kGrey);
  web_state_list->CreateGroup({1}, visual_data, TabGroupId::GenerateNew());
  web_state1->GetNavigationManager()->GetLastCommittedItem()->SetTimestamp(
      now - base::Hours(1));
  web_state1->SetLastActiveTime(now - base::Hours(5));

  // Third WebState has another URL.
  web::FakeWebState* web_state2 =
      static_cast<web::FakeWebState*>(web_state_list->GetWebStateAt(2));
  EXPECT_EQ(GURL(url2), web_state2->GetLastCommittedURL());
  web_state2->GetNavigationManager()->GetLastCommittedItem()->SetTimestamp(
      now - base::Hours(23));
  web_state2->SetLastActiveTime(now - base::Hours(23));

  // Fourth WebState has a group and a time now - 3 hour. No group, no pin.
  web::FakeWebState* web_state3 =
      static_cast<web::FakeWebState*>(web_state_list->GetWebStateAt(3));
  EXPECT_EQ(GURL(url), web_state3->GetLastCommittedURL());
  web_state3->GetNavigationManager()->GetLastCommittedItem()->SetTimestamp(
      now - base::Hours(3));
  web_state3->SetLastActiveTime(now - base::Hours(6));

  auto fetcher =
      std::make_unique<visited_url_ranking::IOSTabModelURLVisitDataFetcher>(
          profile_.get());
  visited_url_ranking::FetchOptions options = visited_url_ranking::
      FetchOptions::CreateDefaultFetchOptionsForTabResumption();
  fetcher->FetchURLVisitData(
      options, visited_url_ranking::FetcherConfig(),
      base::BindOnce(^(visited_url_ranking::FetchResult result) {
        ASSERT_EQ(result.data.size(), 2u);

        std::string key0 = url;
        EXPECT_TRUE(result.data.count(key0));
        auto element_iterator0 = result.data.find(key0);
        ASSERT_NE(element_iterator0, result.data.end());
        visited_url_ranking::URLVisitAggregate::TabData& tab_data0 =
            std::get<visited_url_ranking::URLVisitAggregate::TabData>(
                element_iterator0->second);
        auto tab0 = tab_data0.last_active_tab;
        EXPECT_EQ(key0, tab0.visit.url);
        EXPECT_TRUE(tab_data0.in_group);
        EXPECT_TRUE(tab_data0.pinned);
        EXPECT_EQ(IOSChromeSessionTabHelper::FromWebState(web_state1)
                      ->session_id()
                      .id(),
                  tab0.id);
        EXPECT_EQ(now - base::Hours(4), tab_data0.last_active);
        EXPECT_EQ(now - base::Hours(1), tab0.visit.last_modified);

        std::string key1 = url2;
        EXPECT_TRUE(result.data.count(key1));
        auto element_iterator1 = result.data.find(key1);
        ASSERT_NE(element_iterator1, result.data.end());
        visited_url_ranking::URLVisitAggregate::TabData& tab_data1 =
            std::get<visited_url_ranking::URLVisitAggregate::TabData>(
                element_iterator1->second);
        auto tab1 = tab_data1.last_active_tab;
        EXPECT_EQ(key1, tab1.visit.url);
        EXPECT_FALSE(tab_data1.in_group);
        EXPECT_FALSE(tab_data1.pinned);
        EXPECT_EQ(now - base::Hours(23), tab1.visit.last_modified);
      }));
}
