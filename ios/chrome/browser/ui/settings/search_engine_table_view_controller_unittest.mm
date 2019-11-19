// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/search_engine_table_view_controller.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/settings/cells/search_engine_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using TemplateURLPrepopulateData::GetAllPrepopulatedEngines;
using TemplateURLPrepopulateData::PrepopulatedEngine;

namespace {

const char kUmaSelectDefaultSearchEngine[] =
    "Search.iOS.SelectDefaultSearchEngine";

// Prepopulated search engines.
const std::string kEngineP1Name = "prepopulated-1";
const GURL kEngineP1Url = GURL("https://p1.com?q={searchTerms}");
const std::string kEngineP2Name = "prepopulated-2";
const GURL kEngineP2Url = GURL("https://p2.com?q={searchTerms}");
const std::string kEngineP3Name = "prepopulated-3";
const GURL kEngineP3Url = GURL("https://p3.com?q={searchTerms}");

// Custom search engines.
const std::string kEngineC1Name = "custom-1";
const GURL kEngineC1Url = GURL("https://c1.com?q={searchTerms}");
const std::string kEngineC2Name = "custom-2";
const GURL kEngineC2Url = GURL("https://c2.com?q={searchTerms}");
const std::string kEngineC3Name = "custom-3";
const GURL kEngineC3Url = GURL("https://c3.com?q={searchTerms}");
const std::string kEngineC4Name = "custom-4";
const GURL kEngineC4Url = GURL("https://c4.com?q={searchTerms}");

class SearchEngineTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();
    DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(true);
    template_url_service_ = ios::TemplateURLServiceFactory::GetForBrowserState(
        chrome_browser_state_.get());
    template_url_service_->Load();
  }

  void TearDown() override {
    DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(false);
    ChromeTableViewControllerTest::TearDown();
  }

  ChromeTableViewController* InstantiateController() override {
    return [[SearchEngineTableViewController alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  }

  // Adds a prepopulated search engine to TemplateURLService.
  // |prepopulate_id| should be big enough (>1000) to avoid collision with real
  // prepopulated search engines. The collision happens when
  // TemplateURLService::SetUserSelectedDefaultSearchProvider is called, in the
  // callback of PrefService the DefaultSearchManager will update the searchable
  // URL of default search engine from prepopulated search engines list.
  TemplateURL* AddPriorSearchEngine(const std::string& short_name,
                                    const GURL& searchable_url,
                                    int prepopulate_id,
                                    bool set_default) {
    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16(short_name));
    data.SetKeyword(base::ASCIIToUTF16(short_name));
    data.SetURL(searchable_url.possibly_invalid_spec());
    data.favicon_url = TemplateURL::GenerateFaviconURL(searchable_url);
    data.prepopulate_id = prepopulate_id;
    TemplateURL* url =
        template_url_service_->Add(std::make_unique<TemplateURL>(data));
    if (set_default)
      template_url_service_->SetUserSelectedDefaultSearchProvider(url);
    return url;
  }

  // Adds a custom search engine to TemplateURLService.
  TemplateURL* AddCustomSearchEngine(const std::string& short_name,
                                     const GURL& searchable_url,
                                     base::Time last_visited_time,
                                     bool set_default) {
    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16(short_name));
    data.SetKeyword(base::ASCIIToUTF16(short_name));
    data.SetURL(searchable_url.possibly_invalid_spec());
    data.favicon_url = TemplateURL::GenerateFaviconURL(searchable_url);
    data.last_visited = last_visited_time;
    TemplateURL* url =
        template_url_service_->Add(std::make_unique<TemplateURL>(data));
    if (set_default)
      template_url_service_->SetUserSelectedDefaultSearchProvider(url);
    return url;
  }

  void CheckItem(NSString* expected_text,
                 NSString* expected_detail_text,
                 const GURL& expected_url,
                 bool expected_checked,
                 int section,
                 int row) {
    SearchEngineItem* item = base::mac::ObjCCastStrict<SearchEngineItem>(
        GetTableViewItem(section, row));
    EXPECT_NSEQ(expected_text, item.text);
    EXPECT_NSEQ(expected_detail_text, item.detailText);
    EXPECT_EQ(expected_url, item.URL);
    EXPECT_EQ(expected_checked ? UITableViewCellAccessoryCheckmark
                               : UITableViewCellAccessoryNone,
              item.accessoryType);
  }

  // Checks a SearchEngineItem with data from a fabricated TemplateURL. The
  // SearchEngineItem in the |row| of |section| should contain a title and a
  // subtitle that are equal to |expected_text| and an URL which can be
  // generated by filling empty query word into |expected_searchable_url|. If
  // |expected_checked| is true, the SearchEngineItem should have a
  // UITableViewCellAccessoryCheckmark.
  void CheckPrepopulatedItem(const std::string& expected_text,
                             const GURL& expected_searchable_url,
                             bool expected_checked,
                             int section,
                             int row) {
    TemplateURLData data;
    data.SetURL(expected_searchable_url.possibly_invalid_spec());
    const std::string expected_url =
        TemplateURL(data).url_ref().ReplaceSearchTerms(
            TemplateURLRef::SearchTermsArgs(base::string16()),
            template_url_service_->search_terms_data());
    CheckItem(base::SysUTF8ToNSString(expected_text),
              base::SysUTF8ToNSString(expected_text), GURL(expected_url),
              expected_checked, section, row);
  }

  // Checks a SearchEngineItem with data from a fabricated TemplateURL. The
  // SearchEngineItem in the |row| of |section| should contain a title and a
  // subtitle that are equal to |expected_text| and an URL
  // which can be generated from |expected_searchable_url| by
  // TemplateURL::GenerateFaviconURL. If |expected_checked| is true, the
  // SearchEngineItem should have a UITableViewCellAccessoryCheckmark.
  void CheckCustomItem(const std::string& expected_text,
                       const GURL& expected_searchable_url,
                       bool expected_checked,
                       int section,
                       int row) {
    CheckItem(base::SysUTF8ToNSString(expected_text),
              base::SysUTF8ToNSString(expected_text),
              TemplateURL::GenerateFaviconURL(expected_searchable_url),
              expected_checked, section, row);
  }

  // Checks a SearchEngineItem with data from a real prepopulated
  // TemplateURL. The SearchEngineItem in the |row| of |section| should
  // contain a title equal to |expected_text|, a subtitle equal to
  // |expected_detail_text|, and an URL equal to |expected_favicon_url|. If
  // |expected_checked| is true, the SearchEngineItem should have a
  // UITableViewCellAccessoryCheckmark.
  void CheckRealItem(const TemplateURL* turl,
                     bool expected_checked,
                     int section,
                     int row) {
    CheckItem(base::SysUTF16ToNSString(turl->short_name()),
              base::SysUTF16ToNSString(turl->keyword()),
              GURL(turl->url_ref().ReplaceSearchTerms(
                  TemplateURLRef::SearchTermsArgs(base::string16()),
                  template_url_service_->search_terms_data())),
              expected_checked, section, row);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  base::HistogramTester histogram_tester_;
  TemplateURLService* template_url_service_;  // weak
};

// Tests that no items are shown if TemplateURLService is empty.
TEST_F(SearchEngineTableViewControllerTest, TestNoUrl) {
  CreateController();
  CheckController();
  EXPECT_EQ(0, NumberOfSections());
}

// Tests that items are displayed correctly when TemplateURLService is filled
// and a prepopulated search engine is selected as default.
TEST_F(SearchEngineTableViewControllerTest,
       TestUrlsLoadedWithPrepopulatedSearchEngineAsDefault) {
  AddPriorSearchEngine(kEngineP3Name, kEngineP3Url, 1003, false);
  AddPriorSearchEngine(kEngineP1Name, kEngineP1Url, 1001, false);
  AddPriorSearchEngine(kEngineP2Name, kEngineP2Url, 1002, true);

  AddCustomSearchEngine(kEngineC4Name, kEngineC4Url,
                        base::Time::Now() - base::TimeDelta::FromDays(10),
                        false);
  AddCustomSearchEngine(kEngineC1Name, kEngineC1Url,
                        base::Time::Now() - base::TimeDelta::FromSeconds(10),
                        false);
  AddCustomSearchEngine(kEngineC3Name, kEngineC3Url,
                        base::Time::Now() - base::TimeDelta::FromHours(10),
                        false);
  AddCustomSearchEngine(kEngineC2Name, kEngineC2Url,
                        base::Time::Now() - base::TimeDelta::FromMinutes(10),
                        false);

  CreateController();
  CheckController();

  ASSERT_EQ(2, NumberOfSections());
  ASSERT_EQ(3, NumberOfItemsInSection(0));
  CheckPrepopulatedItem(kEngineP1Name, kEngineP1Url, false, 0, 0);
  CheckPrepopulatedItem(kEngineP2Name, kEngineP2Url, true, 0, 1);
  CheckPrepopulatedItem(kEngineP3Name, kEngineP3Url, false, 0, 2);

  ASSERT_EQ(3, NumberOfItemsInSection(1));
  CheckCustomItem(kEngineC1Name, kEngineC1Url, false, 1, 0);
  CheckCustomItem(kEngineC2Name, kEngineC2Url, false, 1, 1);
  CheckCustomItem(kEngineC3Name, kEngineC3Url, false, 1, 2);
}

// Tests that items are displayed correctly when TemplateURLService is filled
// and a custom search engine is selected as default.
TEST_F(SearchEngineTableViewControllerTest,
       TestUrlsLoadedWithCustomSearchEngineAsDefault) {
  AddPriorSearchEngine(kEngineP3Name, kEngineP3Url, 1003, false);
  AddPriorSearchEngine(kEngineP1Name, kEngineP1Url, 1001, false);
  AddPriorSearchEngine(kEngineP2Name, kEngineP2Url, 1002, false);

  AddCustomSearchEngine(kEngineC4Name, kEngineC4Url,
                        base::Time::Now() - base::TimeDelta::FromDays(10),
                        false);
  AddCustomSearchEngine(kEngineC1Name, kEngineC1Url,
                        base::Time::Now() - base::TimeDelta::FromSeconds(10),
                        false);
  AddCustomSearchEngine(kEngineC3Name, kEngineC3Url,
                        base::Time::Now() - base::TimeDelta::FromHours(10),
                        false);
  AddCustomSearchEngine(kEngineC2Name, kEngineC2Url,
                        base::Time::Now() - base::TimeDelta::FromMinutes(10),
                        true);

  CreateController();
  CheckController();

  ASSERT_EQ(2, NumberOfSections());
  ASSERT_EQ(4, NumberOfItemsInSection(0));
  CheckPrepopulatedItem(kEngineP1Name, kEngineP1Url, false, 0, 0);
  CheckPrepopulatedItem(kEngineP2Name, kEngineP2Url, false, 0, 1);
  CheckPrepopulatedItem(kEngineP3Name, kEngineP3Url, false, 0, 2);
  CheckCustomItem(kEngineC2Name, kEngineC2Url, true, 0, 3);

  ASSERT_EQ(2, NumberOfItemsInSection(1));
  CheckCustomItem(kEngineC1Name, kEngineC1Url, false, 1, 0);
  CheckCustomItem(kEngineC3Name, kEngineC3Url, false, 1, 1);
}

// Tests that when TemplateURLService add or remove TemplateURLs, or update
// default search engine, the controller will update the displayed items.
TEST_F(SearchEngineTableViewControllerTest, TestUrlModifiedByService) {
  TemplateURL* url_p1 =
      AddPriorSearchEngine(kEngineP1Name, kEngineP1Url, 1001, true);

  CreateController();
  CheckController();

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(1, NumberOfItemsInSection(0));
  CheckPrepopulatedItem(kEngineP1Name, kEngineP1Url, true, 0, 0);

  TemplateURL* url_p2 =
      AddPriorSearchEngine(kEngineP2Name, kEngineP2Url, 1002, false);

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(2, NumberOfItemsInSection(0));
  CheckPrepopulatedItem(kEngineP1Name, kEngineP1Url, true, 0, 0);
  CheckPrepopulatedItem(kEngineP2Name, kEngineP2Url, false, 0, 1);

  template_url_service_->SetUserSelectedDefaultSearchProvider(url_p2);

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(2, NumberOfItemsInSection(0));
  CheckPrepopulatedItem(kEngineP1Name, kEngineP1Url, false, 0, 0);
  CheckPrepopulatedItem(kEngineP2Name, kEngineP2Url, true, 0, 1);

  template_url_service_->SetUserSelectedDefaultSearchProvider(url_p1);
  template_url_service_->Remove(url_p2);

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(1, NumberOfItemsInSection(0));
  CheckPrepopulatedItem(kEngineP1Name, kEngineP1Url, true, 0, 0);
}

// Tests that when user change default search engine, all items can be displayed
// correctly and the change can be synced to the prefs.
TEST_F(SearchEngineTableViewControllerTest, TestChangeProvider) {
  // This test also needs to test the UMA, so load some real prepopulated search
  // engines to ensure the SearchEngineType is logged correctly. Don't use any
  // literal symbol(e.g. "google" or "AOL") from
  // "components/search_engines/prepopulated_engines.h" since it's a generated
  // file.
  std::vector<const PrepopulatedEngine*> prepopulated_engines =
      GetAllPrepopulatedEngines();
  ASSERT_LE(2UL, prepopulated_engines.size());

  TemplateURL* url_p1 =
      template_url_service_->Add(std::make_unique<TemplateURL>(
          *TemplateURLDataFromPrepopulatedEngine(*prepopulated_engines[0])));
  ASSERT_TRUE(url_p1);
  TemplateURL* url_p2 =
      template_url_service_->Add(std::make_unique<TemplateURL>(
          *TemplateURLDataFromPrepopulatedEngine(*prepopulated_engines[1])));
  ASSERT_TRUE(url_p2);

  // Expected indexes of prepopulated engines in the list.
  int url_p1_index = 0;
  int url_p2_index = 1;
  if (url_p1->prepopulate_id() > url_p2->prepopulate_id())
    std::swap(url_p1_index, url_p2_index);

  // Also add some custom search engines.
  TemplateURL* url_c1 = AddCustomSearchEngine(kEngineC1Name, kEngineC1Url,
                                              base::Time::Now(), false);
  AddCustomSearchEngine(kEngineC2Name, kEngineC2Url,
                        base::Time::Now() - base::TimeDelta::FromSeconds(10),
                        false);

  CreateController();
  CheckController();

  // Choose url_p1 as default.
  [controller() tableView:[controller() tableView]
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:url_p1_index
                                                 inSection:0]];

  ASSERT_EQ(2, NumberOfSections());
  // Check first list.
  ASSERT_EQ(2, NumberOfItemsInSection(0));
  CheckRealItem(url_p1, true, 0, url_p1_index);
  CheckRealItem(url_p2, false, 0, url_p2_index);
  // Check second list.
  ASSERT_EQ(2, NumberOfItemsInSection(1));
  CheckCustomItem(kEngineC1Name, kEngineC1Url, false, 1, 0);
  CheckCustomItem(kEngineC2Name, kEngineC2Url, false, 1, 1);
  // Check default search engine.
  EXPECT_EQ(url_p1, template_url_service_->GetDefaultSearchProvider());
  // Check UMA.
  histogram_tester_.ExpectUniqueSample(
      kUmaSelectDefaultSearchEngine,
      url_p1->GetEngineType(template_url_service_->search_terms_data()), 1);

  // Choose url_p2 as default.
  [controller() tableView:[controller() tableView]
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:url_p2_index
                                                 inSection:0]];

  ASSERT_EQ(2, NumberOfSections());
  // Check first list.
  ASSERT_EQ(2, NumberOfItemsInSection(0));
  CheckRealItem(url_p1, false, 0, url_p1_index);
  CheckRealItem(url_p2, true, 0, url_p2_index);
  // Check second list.
  ASSERT_EQ(2, NumberOfItemsInSection(1));
  CheckCustomItem(kEngineC1Name, kEngineC1Url, false, 1, 0);
  CheckCustomItem(kEngineC2Name, kEngineC2Url, false, 1, 1);
  // Check default search engine.
  EXPECT_EQ(url_p2, template_url_service_->GetDefaultSearchProvider());
  // Check UMA.
  histogram_tester_.ExpectBucketCount(
      kUmaSelectDefaultSearchEngine,
      url_p1->GetEngineType(template_url_service_->search_terms_data()), 1);
  histogram_tester_.ExpectBucketCount(
      kUmaSelectDefaultSearchEngine,
      url_p2->GetEngineType(template_url_service_->search_terms_data()), 1);
  histogram_tester_.ExpectTotalCount(kUmaSelectDefaultSearchEngine, 2);

  // Choose url_c1 as default.
  [controller() tableView:[controller() tableView]
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:1]];

  ASSERT_EQ(2, NumberOfSections());
  // Check first list.
  ASSERT_EQ(2, NumberOfItemsInSection(0));
  CheckRealItem(url_p1, false, 0, url_p1_index);
  CheckRealItem(url_p2, false, 0, url_p2_index);
  // Check second list.
  ASSERT_EQ(2, NumberOfItemsInSection(1));
  CheckCustomItem(kEngineC1Name, kEngineC1Url, true, 1, 0);
  CheckCustomItem(kEngineC2Name, kEngineC2Url, false, 1, 1);
  // Check default search engine.
  EXPECT_EQ(url_c1, template_url_service_->GetDefaultSearchProvider());
  // Check UMA.
  histogram_tester_.ExpectBucketCount(
      kUmaSelectDefaultSearchEngine,
      url_p1->GetEngineType(template_url_service_->search_terms_data()), 1);
  histogram_tester_.ExpectBucketCount(
      kUmaSelectDefaultSearchEngine,
      url_p2->GetEngineType(template_url_service_->search_terms_data()), 1);
  histogram_tester_.ExpectBucketCount(kUmaSelectDefaultSearchEngine,
                                      SEARCH_ENGINE_OTHER, 1);
  histogram_tester_.ExpectTotalCount(kUmaSelectDefaultSearchEngine, 3);

  // Check that the selection was written back to the prefs.
  const base::DictionaryValue* searchProviderDict =
      chrome_browser_state_->GetTestingPrefService()->GetDictionary(
          DefaultSearchManager::kDefaultSearchProviderDataPrefName);
  ASSERT_TRUE(searchProviderDict);
  base::string16 short_name;
  EXPECT_TRUE(searchProviderDict->GetString(DefaultSearchManager::kShortName,
                                            &short_name));
  EXPECT_EQ(url_c1->short_name(), short_name);
}

}  // namespace
