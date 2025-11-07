// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/coordinator/search_engine_choice_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "components/search_engines/choice_made_location.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/search_engine_choice/ui/search_engine_choice_consumer.h"
#import "ios/chrome/browser/search_engine_choice/ui/search_engine_choice_ui_util.h"
#import "ios/chrome/browser/search_engine_choice/ui/snippet_search_engine_element.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"

namespace {

// Creates a SnippetSearchEngineElement for `template_url`. The template url can
// only be for a prepopulated search engine. This function doesn't support
// custom search engine.
SnippetSearchEngineElement* CreateSnippetSearchEngineElementFromTemplateURL(
    const TemplateURL& template_url) {
  SnippetSearchEngineElement* element = nil;
  // Only works for prepopulated search engines.
  CHECK_GT(template_url.prepopulate_id(), 0)
      << base::UTF16ToUTF8(template_url.short_name());
  element = [[SnippetSearchEngineElement alloc] init];
  // Add the name and snippet to the element.
  element.name = base::SysUTF16ToNSString(template_url.short_name());
  element.snippetDescription =
      base::SysUTF16ToNSString(template_url.GetMarketingSnippet());
  // Add the favicon to the element.
  element.faviconImage = SearchEngineFaviconFromTemplateURL(template_url);
  // With the choice screen, all the search engines should have embedded icons,
  // since the search engine list cannot modified by the user.
  CHECK(element.faviconImage) << base::SysNSStringToUTF8(element.name);
  element.keyword = base::SysUTF16ToNSString(template_url.keyword());
  return element;
}

}  // namespace

@implementation SearchEngineChoiceMediator {
  raw_ptr<search_engines::SearchEngineChoiceService> _searchEngineChoiceService;
  raw_ptr<TemplateURLService> _templateURLService;
  regional_capabilities::RegionalCapabilitiesService::ChoiceScreenDesign
      _choiceScreenDesign;
  // The template URLs to be shown on the choice screen and some associated
  // data.
  std::unique_ptr<search_engines::ChoiceScreenData> _choiceScreenData;
  NSString* _selectedSearchEngineKeyword;
}

- (instancetype)
     initWithTemplateURLService:(TemplateURLService*)templateURLService
      searchEngineChoiceService:
          (search_engines::SearchEngineChoiceService*)searchEngineChoiceService
    regionalCapabilitiesService:
        (regional_capabilities::RegionalCapabilitiesService*)
            regionalCapabilitiesService {
  self = [super init];
  if (self) {
    _templateURLService = templateURLService;
    _searchEngineChoiceService = searchEngineChoiceService;
    _templateURLService->Load();
    // The caller should ensure that GetChoiceScreenDesign() returns an optional
    // that is set.
    std::optional<
        regional_capabilities::RegionalCapabilitiesService::ChoiceScreenDesign>
        choiceScreenDesign =
            regionalCapabilitiesService->GetChoiceScreenDesign();
    CHECK(choiceScreenDesign.has_value());
    _choiceScreenDesign = std::move(choiceScreenDesign.value());
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
    _consumer.titleStringID = _choiceScreenDesign.title_string_id;
    _consumer.subtitle1StringID = _choiceScreenDesign.subtitle_1_string_id;
    _consumer.subtitle1LearnMoreSuffixStringID =
        _choiceScreenDesign.subtitle_1_learn_more_suffix_string_id;
    _consumer.subtitle1LearnMoreA11yStringID =
        _choiceScreenDesign.subtitle_1_learn_more_a11y_string_id;
    if (_choiceScreenDesign.subtitle_2_string_id.has_value()) {
      _consumer.subtitle2StringID =
          _choiceScreenDesign.subtitle_2_string_id.value();
    }
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
  // Current sesarch engine to highlight if any.
  const TemplateURL* currentDefaultSearchEngineToHighlight =
      _choiceScreenData->current_default_to_highlight();
  // Convert TemplateURLs to SnippetSearchEngineElements.
  BOOL currentDefaultSearchEngineToHighlightFound = NO;
  for (auto& templateURL : _choiceScreenData->search_engines()) {
    SnippetSearchEngineElement* element =
        CreateSnippetSearchEngineElementFromTemplateURL(*templateURL);
    if (currentDefaultSearchEngineToHighlight == templateURL.get()) {
      element.currentDefaultState = CurrentDefaultState::kIsCurrentDefault;
      currentDefaultSearchEngineToHighlightFound = YES;
    } else if (currentDefaultSearchEngineToHighlight) {
      element.currentDefaultState = CurrentDefaultState::kHasCurrentDefault;
    }
    [searchEngineList addObject:element];
  }
  // The current default search engine must be part of the search engine list.
  std::u16string keyword;
  if (currentDefaultSearchEngineToHighlight) {
    keyword = currentDefaultSearchEngineToHighlight->keyword();
  }
  CHECK(!currentDefaultSearchEngineToHighlight ||
            currentDefaultSearchEngineToHighlightFound,
        base::NotFatalUntil::M150)
      << base::UTF16ToUTF8(keyword);
  self.consumer.searchEngines = [searchEngineList copy];
}

@end
