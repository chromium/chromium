// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/search_engine_table_view_controller_unittest.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/ui/settings/cells/settings_search_engine_item.h"
#import "ios/chrome/browser/ui/settings/search_engine_table_view_controller.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

SearchEngineTableViewControllerTest::SearchEngineTableViewControllerTest()
    : prepopulated_search_engine_({
          {"google.com", GURL("https://p1.com?q={searchTerms}")},
          {"bing.com", GURL("https://p2.com?q={searchTerms}")},
          {"duckduckgo.com", GURL("https://p3.com?q={searchTerms}")},
      }),
      custom_search_engine_({
          {"custom-1", GURL("https://c1.com?q={searchTerms}")},
          {"custom-2", GURL("https://c2.com?q={searchTerms}")},
          {"custom-3", GURL("https://c3.com?q={searchTerms}")},
          {"custom-4", GURL("https://c4.com?q={searchTerms}")},
      }) {}

SearchEngineTableViewControllerTest::~SearchEngineTableViewControllerTest() {}

void SearchEngineTableViewControllerTest::SetUp() {
  LegacyChromeTableViewControllerTest::SetUp();

  TestProfileIOS::Builder builder;

  builder.AddTestingFactory(
      ios::TemplateURLServiceFactory::GetInstance(),
      ios::TemplateURLServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(ios::FaviconServiceFactory::GetInstance(),
                            ios::FaviconServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      IOSChromeLargeIconServiceFactory::GetInstance(),
      IOSChromeLargeIconServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(IOSChromeFaviconLoaderFactory::GetInstance(),
                            IOSChromeFaviconLoaderFactory::GetDefaultFactory());
  builder.AddTestingFactory(ios::HistoryServiceFactory::GetInstance(),
                            ios::HistoryServiceFactory::GetDefaultFactory());
  profile_ = std::move(builder).Build();
  // Override the country checks to simulate being in Belgium.
  pref_service_ = profile_->GetTestingPrefService();
  DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(true);
  template_url_service_ =
      ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
  template_url_service_->Load();
}

void SearchEngineTableViewControllerTest::TearDown() {
  DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(false);
  [base::apple::ObjCCastStrict<SearchEngineTableViewController>(controller())
      settingsWillBeDismissed];
}

LegacyChromeTableViewController*
SearchEngineTableViewControllerTest::InstantiateController() {
  return
      [[SearchEngineTableViewController alloc] initWithProfile:profile_.get()];
}

// Adds a prepopulated search engine to TemplateURLService.
// `prepopulate_id` should be big enough (>1000) to avoid collision with real
// prepopulated search engines. The collision happens when
// TemplateURLService::SetUserSelectedDefaultSearchProvider is called, in the
// callback of PrefService the DefaultSearchManager will update the searchable
// URL of default search engine from prepopulated search engines list.
TemplateURL* SearchEngineTableViewControllerTest::AddPriorSearchEngine(
    const SearchEngineTest& search_engine,
    int prepopulate_id,
    bool set_default) {
  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16(search_engine.short_name));
  data.SetKeyword(base::ASCIIToUTF16(search_engine.short_name));
  data.SetURL(search_engine.searchable_url.possibly_invalid_spec());
  data.favicon_url =
      TemplateURL::GenerateFaviconURL(search_engine.searchable_url);
  data.prepopulate_id = prepopulate_id;
  TemplateURL* url =
      template_url_service_->Add(std::make_unique<TemplateURL>(data));
  if (set_default) {
    template_url_service_->SetUserSelectedDefaultSearchProvider(url);
  }
  return url;
}

// Adds a custom search engine to TemplateURLService.
TemplateURL* SearchEngineTableViewControllerTest::AddCustomSearchEngine(
    const SearchEngineTest& search_engine,
    base::Time last_visited_time,
    bool set_default) {
  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16(search_engine.short_name));
  data.SetKeyword(base::ASCIIToUTF16(search_engine.short_name));
  data.SetURL(search_engine.searchable_url.possibly_invalid_spec());
  data.favicon_url =
      TemplateURL::GenerateFaviconURL(search_engine.searchable_url);
  data.last_visited = last_visited_time;
  TemplateURL* url =
      template_url_service_->Add(std::make_unique<TemplateURL>(data));
  if (set_default) {
    template_url_service_->SetUserSelectedDefaultSearchProvider(url);
  }
  return url;
}

void SearchEngineTableViewControllerTest::CheckItem(
    NSString* expected_text,
    NSString* expected_detail_text,
    bool expected_checked,
    int section,
    int row,
    bool enabled) {
  SettingsSearchEngineItem* item =
      base::apple::ObjCCastStrict<SettingsSearchEngineItem>(
          GetTableViewItem(section, row));
  EXPECT_NSEQ(expected_text, item.text);
  EXPECT_NSEQ(expected_detail_text, item.detailText);
  EXPECT_EQ(expected_checked ? UITableViewCellAccessoryCheckmark
                             : UITableViewCellAccessoryNone,
            item.accessoryType);
  EXPECT_EQ(enabled, item.enabled);
}

// Checks a LegacySettingsSearchEngineItem with data from a fabricated
// TemplateURL. The LegacySettingsSearchEngineItem in the `row` of `section`
// should contain a title and a subtitle that are equal to `expected_text` and
// an URL which can be generated by filling empty query word into
// `expected_searchable_url`. If `expected_checked` is true, the
// LegacySettingsSearchEngineItem should have a
// UITableViewCellAccessoryCheckmark.
void SearchEngineTableViewControllerTest::CheckPrepopulatedItem(
    const SearchEngineTest& search_engine,
    bool expected_checked,
    int section,
    int row,
    bool enabled) {
  TemplateURLData data;
  data.SetURL(search_engine.searchable_url.possibly_invalid_spec());
  const std::string expected_url =
      TemplateURL(data).url_ref().ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(std::u16string()),
          template_url_service_->search_terms_data());
  CheckItem(base::SysUTF8ToNSString(search_engine.short_name),
            base::SysUTF8ToNSString(search_engine.short_name), expected_checked,
            section, row, enabled);
}

// Checks a LegacySettingsSearchEngineItem with data from a fabricated
// TemplateURL. The LegacySettingsSearchEngineItem in the `row` of `section`
// should contain a title and a subtitle that are equal to `expected_text` and
// an URL which can be generated from `expected_searchable_url` by
// TemplateURL::GenerateFaviconURL. If `expected_checked` is true, the
// LegacySettingsSearchEngineItem should have a
// UITableViewCellAccessoryCheckmark.
void SearchEngineTableViewControllerTest::CheckCustomItem(
    const SearchEngineTest& search_engine,
    bool expected_checked,
    int section,
    int row,
    bool enabled) {
  CheckItem(base::SysUTF8ToNSString(search_engine.short_name),
            base::SysUTF8ToNSString(search_engine.short_name), expected_checked,
            section, row, enabled);
}

// Checks a LegacySettingsSearchEngineItem with data from a real prepopulated
// TemplateURL. The LegacySettingsSearchEngineItem in the `row` of `section`
// should contain a title equal to `expected_text`, a subtitle equal to
// `expected_detail_text`, and an URL equal to `expected_favicon_url`. If
// `expected_checked` is true, the LegacySettingsSearchEngineItem should have
// a UITableViewCellAccessoryCheckmark.
void SearchEngineTableViewControllerTest::CheckRealItem(const TemplateURL* turl,
                                                        bool expected_checked,
                                                        int section,
                                                        int row,
                                                        bool enabled) {
  CheckItem(base::SysUTF16ToNSString(turl->short_name()),
            base::SysUTF16ToNSString(turl->keyword()), expected_checked,
            section, row, enabled);
}

// Deletes items at `indexes` and wait util condition returns true or timeout.
[[nodiscard]] bool SearchEngineTableViewControllerTest::DeleteItemsAndWait(
    NSArray<NSIndexPath*>* indexes,
    ConditionBlock condition) {
  SearchEngineTableViewController* searchEngineController =
      static_cast<SearchEngineTableViewController*>(controller());
  [searchEngineController deleteItems:indexes];
  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, condition);
}
