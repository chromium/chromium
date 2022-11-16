// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/new_tab_page_app_interface.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "components/ntp_snippets/content_suggestion.h"
#import "components/ntp_snippets/content_suggestions_service.h"
#import "components/ntp_snippets/mock_content_suggestions_provider.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#import "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory_util.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_provider_test_singleton.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_test_utils.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using content_suggestions::SearchFieldWidth;
using ntp_snippets::AdditionalSuggestionsHelper;
using ntp_snippets::Category;
using ntp_snippets::CategoryStatus;
using ntp_snippets::ContentSuggestion;
using ntp_snippets::ContentSuggestionsService;
using ntp_snippets::CreateChromeContentSuggestionsService;
using ntp_snippets::KnownCategories;
using ntp_snippets::MockContentSuggestionsProvider;
using testing::_;
using testing::Invoke;
using testing::WithArg;

namespace {
// Returns a suggestion created from the `category`, `suggestion_id` and the
// `url`.
ContentSuggestion CreateSuggestion(Category category,
                                   std::string suggestion_id,
                                   GURL url) {
  ContentSuggestion suggestion(category, suggestion_id, url);
  suggestion.set_title(base::UTF8ToUTF16(url.spec()));

  return suggestion;
}

}  // namespace

@implementation NewTabPageAppInterface

+ (void)setUpService {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  // Sets the ContentSuggestionsService associated with this browserState to a
  // service with no provider registered, allowing to register fake providers
  // which do not require internet connection. The previous service is deleted.
  IOSChromeContentSuggestionsServiceFactory::GetInstance()->SetTestingFactory(
      browserState,
      base::BindRepeating(&CreateChromeContentSuggestionsService));

  ContentSuggestionsService* service =
      IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
          browserState);
  [[ContentSuggestionsTestSingleton sharedInstance]
      registerArticleProvider:service];
}

+ (void)resetService {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();

  // Resets the Service associated with this browserState to a new service with
  // no providers. The previous service is deleted.
  IOSChromeContentSuggestionsServiceFactory::GetInstance()->SetTestingFactory(
      browserState,
      base::BindRepeating(&CreateChromeContentSuggestionsService));
}

+ (void)makeSuggestionsAvailable {
  [self provider]->FireCategoryStatusChanged([self category],
                                             CategoryStatus::AVAILABLE);
}

+ (void)disableSuggestions {
  [self provider]->FireCategoryStatusChanged(
      [self category], CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED);
}

+ (void)addNumberOfSuggestions:(NSInteger)numberOfSuggestions
      additionalSuggestionsURL:(NSURL*)URL {
  GURL newURL = net::GURLWithNSURL(URL);
  std::vector<ContentSuggestion> suggestions;
  for (NSInteger i = 1; i <= numberOfSuggestions; i++) {
    std::string index = base::SysNSStringToUTF8(@(i).stringValue);
    suggestions.push_back(
        CreateSuggestion([self category], "chromium" + index,
                         GURL("http://chromium.org/" + index)));
  }
  [self provider]->FireSuggestionsChanged([self category],
                                          std::move(suggestions));

  if (URL) {
    // Set up the action when "More" is tapped.
    [[ContentSuggestionsTestSingleton sharedInstance]
        resetAdditionalSuggestionsHelperWithURL:newURL];
    EXPECT_CALL(*[self provider], FetchMock(_, _, _))
        .WillRepeatedly(WithArg<2>(
            Invoke([[ContentSuggestionsTestSingleton sharedInstance]
                       additionalSuggestionsHelper],
                   &AdditionalSuggestionsHelper::SendAdditionalSuggestions)));
  }
}

+ (void)addSuggestionNumber:(NSInteger)suggestionNumber {
  std::string index = base::NumberToString(suggestionNumber);
  std::vector<ContentSuggestion> suggestions;
  suggestions.push_back(CreateSuggestion([self category], "chromium" + index,
                                         GURL("http://chromium.org/" + index)));
  [self provider]->FireSuggestionsChanged([self category],
                                          std::move(suggestions));
}

+ (NSString*)defaultSearchEngine {
  // Get the default Search Engine.
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  TemplateURLService* service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  const TemplateURL* default_provider = service->GetDefaultSearchProvider();
  DCHECK(default_provider);
  return base::SysUTF16ToNSString(default_provider->short_name());
}

+ (void)resetSearchEngineTo:(NSString*)defaultSearchEngine {
  std::u16string defaultSearchEngineString =
      base::SysNSStringToUTF16(defaultSearchEngine);
  // Set the search engine back to the default in case the test fails before
  // cleaning it up.
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  TemplateURLService* service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  std::vector<TemplateURL*> urls = service->GetTemplateURLs();

  for (auto iter = urls.begin(); iter != urls.end(); ++iter) {
    if (defaultSearchEngineString == (*iter)->short_name()) {
      service->SetUserSelectedDefaultSearchProvider(*iter);
    }
  }
}

+ (CGFloat)searchFieldWidthForCollectionWidth:(CGFloat)collectionWidth
                              traitCollection:
                                  (UITraitCollection*)traitCollection {
  return content_suggestions::SearchFieldWidth(collectionWidth,
                                               traitCollection);
}

+ (UICollectionView*)collectionView {
  return ntp_home::CollectionView();
}

+ (UICollectionView*)contentSuggestionsCollectionView {
  return ntp_home::ContentSuggestionsCollectionView();
}

+ (UIView*)fakeOmnibox {
  return ntp_home::FakeOmnibox();
}

+ (UILabel*)discoverHeaderLabel {
  return ntp_home::DiscoverHeaderLabel();
}

#pragma mark - Helper

+ (MockContentSuggestionsProvider*)provider {
  return [[ContentSuggestionsTestSingleton sharedInstance] provider];
}

+ (Category)category {
  return Category::FromKnownCategory(KnownCategories::ARTICLES);
}

@end
