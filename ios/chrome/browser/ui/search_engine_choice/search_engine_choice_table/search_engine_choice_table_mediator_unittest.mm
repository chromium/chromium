// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/search_engine_choice_table_mediator.h"

#import <memory>
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/country_codes/country_codes.h"
#import "components/search_engines/search_engines_pref_names.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/cells/snippet_search_engine_item.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/search_engine_choice_table_consumer.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

// Create an empty implementation of the consumer just to fetch the list of
// search engines for testing.
@interface SearchEngineChoiceTableTestConsumer
    : NSObject <SearchEngineChoiceTableConsumer>
@end

@implementation SearchEngineChoiceTableTestConsumer

@synthesize searchEngines = _searchEngines;

- (void)reloadData {
}

- (void)faviconAttributesUpdatedForItem:(SnippetSearchEngineItem*)item {
}

@end

class SearchEngineChoiceTableMediatorTest : public PlatformTest {
 protected:
  SearchEngineChoiceTableMediatorTest() {
    feature_list_.InitAndEnableFeature(switches::kSearchEngineChoice);
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeFaviconLoaderFactory::GetInstance(),
        IOSChromeFaviconLoaderFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::FaviconServiceFactory::GetInstance(),
        ios::FaviconServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());
    browser_state_ = test_cbs_builder.Build();
    DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(true);
    template_url_service_ = ios::TemplateURLServiceFactory::GetForBrowserState(
        browser_state_.get());
    TemplateURLService::RegisterProfilePrefs(pref_service_.registry());

    // The search engine choice feature is only enabled for countries in the
    // EEA region. Override the country checks to simulate being in Belgium.
    // TODO(b/307713013): Set the country using the PrefService rather than
    // command-line flags.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, "BE");
    FaviconLoader* faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForBrowserState(browser_state_.get());
    mediator_ = [[SearchEngineChoiceTableMediator alloc]
        initWithTemplateURLService:template_url_service_
                       prefService:&pref_service_
                     faviconLoader:faviconLoader];
    // This is when the list of search engines is set
    mediator_.consumer = consumer_;
  }

  ~SearchEngineChoiceTableMediatorTest() override {
    DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(false);
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kSearchEngineChoiceCountry);
    [mediator_ disconnect];
  }

  TemplateURL* ConvertToTemplateUrl(SnippetSearchEngineItem* item) {
    return template_url_service_->GetTemplateURLForKeyword(
        TemplateURL::GenerateKeyword(item.URL));
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  TemplateURLService* template_url_service_;
  SearchEngineChoiceTableMediator* mediator_ = nil;
  SearchEngineChoiceTableTestConsumer* consumer_ =
      [[SearchEngineChoiceTableTestConsumer alloc] init];
};

// Tests that the mediator correctly sets the user's default search engine and
// that the timestamp pref is set.
TEST_F(SearchEngineChoiceTableMediatorTest, SavesDefaultSearchEngine) {
  // This will be a different search engine each time, since the list is
  // randomly shuffled by the TemplateUrlService.
  mediator_.selectedRow = 1;
  [mediator_ saveDefaultSearchEngine];

  NSString* expected_default_engine_name =
      consumer_.searchEngines[mediator_.selectedRow].name;
  NSString* default_search_engine_name = base::SysUTF16ToNSString(
      template_url_service_->GetDefaultSearchProvider()->short_name());
  ASSERT_TRUE([expected_default_engine_name
      isEqualToString:default_search_engine_name]);
  // We don't care about the value, we just need to check that something was
  // written.
  EXPECT_GT(pref_service_.GetInt64(
                prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
            0);
  EXPECT_FALSE(
      pref_service_
          .GetString(prefs::kDefaultSearchProviderChoiceScreenCompletionVersion)
          .empty());
}

// Tests that the list of search engines is correctly initialized.
TEST_F(SearchEngineChoiceTableMediatorTest, SearchEngineListCorrectlyCreated) {
  std::vector<std::unique_ptr<TemplateURL>> search_engines =
      template_url_service_->GetTemplateURLsForChoiceScreen();

  // The list is randomly shuffled every time it is generated so we just check
  // that all the elements are there.
  ASSERT_EQ([consumer_.searchEngines count], search_engines.size());
  for (SnippetSearchEngineItem* item in consumer_.searchEngines) {
    ASSERT_TRUE(
        base::ranges::find(search_engines, base::SysNSStringToUTF16(item.name),
                           &TemplateURL::short_name) != search_engines.end());
  }
}
