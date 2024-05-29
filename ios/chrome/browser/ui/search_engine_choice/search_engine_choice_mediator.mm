// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/search_engines/choice_made_location.h"
#import "components/search_engines/prepopulated_engines.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
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
  // With the choice screen, all the search engines should have embedded icons,
  // since the search engine list cannot modified by the user.
  CHECK(element.faviconImage, base::NotFatalUntil::M127)
      << base::SysNSStringToUTF8(element.name);
  element.keyword = base::SysUTF16ToNSString(template_url.keyword());
  return element;
}

}  // namespace

@implementation SearchEngineChoiceMediator {
  raw_ptr<search_engines::SearchEngineChoiceService>
      _searchEngineChoiceService;                   // weak
  raw_ptr<TemplateURLService> _templateURLService;  // weak
  // The template URLs to be shown on the choice screen and some associated
  // data.
  std::unique_ptr<search_engines::ChoiceScreenData> _choiceScreenData;
  NSString* _selectedSearchEngineKeyword;
}

- (instancetype)
    initWithTemplateURLService:(TemplateURLService*)templateURLService
     searchEngineChoiceService:
         (search_engines::SearchEngineChoiceService*)searchEngineChoiceService {
  self = [super init];
  if (self) {
    _templateURLService = templateURLService;
    _searchEngineChoiceService = searchEngineChoiceService;
    _templateURLService->Load();
  }
  return self;
}

- (void)saveDefaultSearchEngine {
  CHECK(_selectedSearchEngineKeyword);
  std::u16string keyword =
      base::SysNSStringToUTF16(_selectedSearchEngineKeyword);
  TemplateURL* selectedTemplateURL = nil;
  search_engines::ChoiceScreenDisplayState display_state =
      _choiceScreenData->display_state();
  for (size_t i = 0; i < _choiceScreenData->search_engines().size(); ++i) {
    auto& templateURL = _choiceScreenData->search_engines()[i];
    if (templateURL->keyword() == keyword) {
      selectedTemplateURL = templateURL.get();
      display_state.selected_engine_index = i;
      break;
    }
  }
  CHECK(selectedTemplateURL);
  _templateURLService->SetUserSelectedDefaultSearchProvider(
      selectedTemplateURL, search_engines::ChoiceMadeLocation::kChoiceScreen);
  _searchEngineChoiceService->MaybeRecordChoiceScreenDisplayState(
      display_state);
}

- (void)disconnect {
  _searchEngineChoiceService = nullptr;
  _templateURLService = nullptr;
}

#pragma mark - Properties

- (void)setConsumer:(id<SearchEngineChoiceConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self loadSearchEngines];
  }
}

#pragma mark - SearchEngineChoiceMutator

- (void)selectSearchEnginewWithKeyword:(NSString*)keyword {
  _selectedSearchEngineKeyword = keyword;
}

#pragma mark - Private

// Loads all the data for the choice screen in `_choiceScreenData` and updates
// the consumer.
- (void)loadSearchEngines {
  _choiceScreenData = _templateURLService->GetChoiceScreenData();
  NSMutableArray<SnippetSearchEngineElement*>* searchEngineList =
      [[NSMutableArray<SnippetSearchEngineElement*> alloc]
          initWithCapacity:_choiceScreenData->search_engines().size()];
  // Convert TemplateURLs to SnippetSearchEngineElements.
  for (auto& templateURL : _choiceScreenData->search_engines()) {
    SnippetSearchEngineElement* element =
        CreateSnippetSearchEngineElementFromTemplateURL(*templateURL);
    [searchEngineList addObject:element];
  }
  self.consumer.searchEngines = [searchEngineList copy];
}

@end
