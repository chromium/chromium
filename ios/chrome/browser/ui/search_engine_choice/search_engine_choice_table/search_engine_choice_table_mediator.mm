// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/search_engine_choice_table_mediator.h"

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
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/cells/snippet_search_engine_item.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/search_engine_choice_table_consumer.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_ui_util.h"

namespace {

// Creates a SnippetSearchEngineItem for `template_url`. The template url can
// only be for a prepopulated search engine. This function doesn't support
// custom search engine.
SnippetSearchEngineItem* CreateSnippetSearchEngineItemFromTemplateURL(
    const TemplateURL& template_url) {
  SnippetSearchEngineItem* item = nil;
  // Only works for prepopulated search engines.
  CHECK_GT(template_url.prepopulate_id(), 0, base::NotFatalUntil::M124)
      << base::UTF16ToUTF8(template_url.short_name());
  item = [[SnippetSearchEngineItem alloc] initWithType:kItemTypeEnumZero];
  // Add the name and snippet to the item.
  item.name = base::SysUTF16ToNSString(template_url.short_name());
  std::u16string string =
      search_engines::GetMarketingSnippetString(template_url.data());
  item.snippetDescription = base::SysUTF16ToNSString(string);
  // Add the favicon to the item.
  item.faviconImage = SearchEngineFaviconFromTemplateURL(template_url);
  return item;
}

}  // namespace

@interface SearchEngineChoiceTableMediator () <SearchEngineObserving>
@end

@implementation SearchEngineChoiceTableMediator {
  raw_ptr<TemplateURLService> _templateURLService;  // weak
  raw_ptr<PrefService> _prefService;
  std::unique_ptr<SearchEngineObserverBridge> _observer;
  // The list of URLs of prepopulated search engines and search engines that are
  // created by policy.
  std::vector<std::unique_ptr<TemplateURL>> _urlList;
  // The corresponding list of search engines as items that can be inserted into
  // a tableView.
  NSArray<SnippetSearchEngineItem*>* _searchEngineList;
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
    _searchEngineList = [[NSMutableArray<SnippetSearchEngineItem*> alloc] init];
  }
  return self;
}

- (void)saveDefaultSearchEngine {
  _templateURLService->SetUserSelectedDefaultSearchProvider(
      _urlList[self.selectedRow].get(),
      search_engines::ChoiceMadeLocation::kChoiceScreen);
}

- (void)disconnect {
  _observer.reset();
  _templateURLService = nullptr;
}

#pragma mark - Properties

- (void)setConsumer:(id<SearchEngineChoiceTableConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self loadSearchEngines];
  }
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  [self.consumer reloadData];
}

#pragma mark - Private

// Loads all TemplateURLs from TemplateURLService and classifies them into
// `_firstList` and `_secondList`. If a TemplateURL is
// prepopulated, created by policy or the default search engine, it will get
// into the first list, otherwise the second list.
- (void)loadSearchEngines {
  _urlList = _templateURLService->GetTemplateURLsForChoiceScreen();
  NSMutableArray<SnippetSearchEngineItem*>* searchEngineList =
      [[NSMutableArray<SnippetSearchEngineItem*> alloc]
          initWithCapacity:_urlList.size()];

  // Convert TemplateURLs to SnippetSearchEngineItems.
  for (auto& templateURL : _urlList) {
    SnippetSearchEngineItem* item =
        CreateSnippetSearchEngineItemFromTemplateURL(*templateURL);
    [searchEngineList addObject:item];
  }

  _searchEngineList = [searchEngineList copy];
  [self.consumer setSearchEngines:_searchEngineList];
}

@end
