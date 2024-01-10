// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/search_engine_choice_table_mediator.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/search_engine_choice_utils.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/template_url_service_observer.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/cells/snippet_search_engine_item.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/search_engine_choice_table_consumer.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"

namespace {

// Creates a SnippetSearchEngineItem for `templateURL`.
SnippetSearchEngineItem* CreateSnippetSearchEngineItemFromTemplateURL(
    TemplateURL* template_url,
    TemplateURLService* template_url_service) {
  SnippetSearchEngineItem* item = nil;
  if (template_url->prepopulate_id() > 0) {
    item = [[SnippetSearchEngineItem alloc] initWithType:kItemTypeEnumZero];
    // Fake up a page URL for favicons of prepopulated search engines, since
    // favicons may be fetched from Google server which doesn't suppoprt
    // icon URL.
    std::string empty_page_url = template_url->url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(std::u16string()),
        template_url_service->search_terms_data());
    item.URL = GURL(empty_page_url);
  } else {
    item = [[SnippetSearchEngineItem alloc] initWithType:kItemTypeEnumZero];
    // Use icon URL for favicons of custom search engines.
    item.URL = template_url->favicon_url();
  }
  item.name = base::SysUTF16ToNSString(template_url->short_name());
  std::u16string string =
      search_engines::GetMarketingSnippetString(template_url->data());
  item.snippetDescription = base::SysUTF16ToNSString(string);
  return item;
}

}  // namespace

@interface SearchEngineChoiceTableMediator () <SearchEngineObserving>
@end

@implementation SearchEngineChoiceTableMediator {
  TemplateURLService* _templateURLService;  // weak
  PrefService* _prefService;
  std::unique_ptr<SearchEngineObserverBridge> _observer;
  // The list of URLs of prepopulated search engines and search engines that are
  // created by policy.
  std::vector<std::unique_ptr<TemplateURL>> _urlList;
  // The corresponding list of search engines as items that can be inserted into
  // a tableView.
  NSArray<SnippetSearchEngineItem*>* _searchEngineList;
  // FaviconLoader is a keyed service that uses LargeIconService to retrieve
  // favicon images.
  FaviconLoader* _faviconLoader;
}

- (instancetype)initWithTemplateURLService:
                    (TemplateURLService*)templateURLService
                               prefService:(PrefService*)prefService
                             faviconLoader:(FaviconLoader*)faviconLoader {
  self = [super init];
  if (self) {
    _templateURLService = templateURLService;
    _faviconLoader = faviconLoader;
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
      _urlList[self.selectedRow].get());
  search_engines::RecordChoiceMade(
      _prefService, search_engines::ChoiceMadeLocation::kChoiceScreen,
      _templateURLService);
}

- (void)disconnect {
  _observer.reset();
  _templateURLService = nullptr;
  _faviconLoader = nullptr;
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
        CreateSnippetSearchEngineItemFromTemplateURL(templateURL.get(),
                                                     _templateURLService);
    [searchEngineList addObject:item];
    __weak __typeof(self) weakSelf = self;
    _faviconLoader->FaviconForPageUrl(
        item.URL, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
        /*fallback_to_google_server=*/YES, ^(FaviconAttributes* attributes) {
          if (attributes.faviconImage) {
            item.faviconImage = attributes.faviconImage;
          } else {
            item.faviconImage = [UIImage imageNamed:@"default_world_favicon"];
          }
          [weakSelf.consumer faviconAttributesUpdatedForItem:item];
          if (item.checked) {
            [weakSelf.faviconUpdateConsumer updateFaviconImageForItem:item];
          }
        });
  }

  _searchEngineList = [searchEngineList copy];
  [self.consumer setSearchEngines:_searchEngineList];
}

@end
