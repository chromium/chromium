// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/choice_made_location.h"
#import "components/search_engines/prepopulated_engines.h"
#import "components/search_engines/search_engine_choice_utils.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/template_url_service_observer.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_consumer.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_ui_util.h"
#import "ios/chrome/browser/ui/search_engine_choice/snippet_search_engine_element.h"

namespace {

// Creates a SnippetSearchEngineElement for `template_url`. The template url can
// only be for a prepopulated search engine. This function doesn't support
// custom search engine.
SnippetSearchEngineElement* CreateSnippetSearchEngineElementFromTemplateURL(
    const TemplateURL& template_url) {
  SnippetSearchEngineElement* element = nil;
  // Only works for prepopulated search engines.
  CHECK_GT(template_url.prepopulate_id(), 0, base::NotFatalUntil::M127)
      << base::UTF16ToUTF8(template_url.short_name());
  element = [[SnippetSearchEngineElement alloc] init];
  // Add the name and snippet to the element.
  element.name = base::SysUTF16ToNSString(template_url.short_name());
  std::u16string string =
      search_engines::GetMarketingSnippetString(template_url.data());
  element.snippetDescription = base::SysUTF16ToNSString(string);
  // Add the favicon to the element.
  element.faviconImage = SearchEngineFaviconFromTemplateURL(template_url);
  element.keyword = base::SysUTF16ToNSString(template_url.keyword());
  return element;
}

}  // namespace

@interface SearchEngineChoiceMediator () <SearchEngineObserving>
@end

@implementation SearchEngineChoiceMediator {
  raw_ptr<TemplateURLService> _templateURLService;  // weak
  raw_ptr<PrefService> _prefService;
  std::unique_ptr<SearchEngineObserverBridge> _observer;
  // The list of URLs of prepopulated search engines and search engines that are
  // created by policy.
  std::vector<std::unique_ptr<TemplateURL>> _templateUrlList;
  NSString* _selectedSearchEngineKeyword;
}

- (instancetype)initWithTemplateURLService:
                    (TemplateURLService*)templateURLService
                               prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _templateURLService = templateURLService;
    _prefService = prefService;
    _observer =
        std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
    _templateURLService->Load();
  }
  return self;
}

- (void)saveDefaultSearchEngine {
  CHECK(_selectedSearchEngineKeyword);
  // When the default search engine is saved, there is no reason to observe
  // `_templateURLService` anymore since the dialog should disappear right
  // after.
  _observer.reset();
  std::u16string keyword =
      base::SysNSStringToUTF16(_selectedSearchEngineKeyword);
  TemplateURL* selectedTemplateURL = nil;
  for (auto& templateURL : _templateUrlList) {
    if (templateURL->keyword() == keyword) {
      selectedTemplateURL = templateURL.get();
    }
  }
  CHECK(selectedTemplateURL);
  _templateURLService->SetUserSelectedDefaultSearchProvider(
      selectedTemplateURL, search_engines::ChoiceMadeLocation::kChoiceScreen);
}

- (void)disconnect {
  _observer.reset();
  _templateURLService = nullptr;
  _prefService = nullptr;
}

#pragma mark - Properties

- (void)setConsumer:(id<SearchEngineChoiceConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self loadSearchEngines];
  }
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  [self loadSearchEngines];
}

#pragma mark - SearchEngineChoiceMutator

- (void)selectSearchEnginewWithKeyword:(NSString*)keyword {
  _selectedSearchEngineKeyword = keyword;
}

#pragma mark - Private

// Loads all TemplateURLs from TemplateURLService and classifies them into
// `_firstList` and `_secondList`. If a TemplateURL is
// prepopulated, created by policy or the default search engine, it will get
// into the first list, otherwise the second list.
- (void)loadSearchEngines {
  _templateUrlList = _templateURLService->GetTemplateURLsForChoiceScreen();
  NSMutableArray<SnippetSearchEngineElement*>* searchEngineList =
      [[NSMutableArray<SnippetSearchEngineElement*> alloc]
          initWithCapacity:_templateUrlList.size()];
  // Convert TemplateURLs to SnippetSearchEngineElements.
  for (auto& templateURL : _templateUrlList) {
    SnippetSearchEngineElement* element =
        CreateSnippetSearchEngineElementFromTemplateURL(*templateURL);
    [searchEngineList addObject:element];
  }
  self.consumer.searchEngines = [searchEngineList copy];
}

@end
