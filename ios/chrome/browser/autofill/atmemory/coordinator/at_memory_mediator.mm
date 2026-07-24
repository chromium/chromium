// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_mediator.h"

#import "ios/chrome/browser/autofill/atmemory/coordinator/scoped_at_memory_search_provider_override.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_search_provider.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_consumer.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_content_injector.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

@interface AtMemoryMediator () {
  // Current search text entered by the user.
  NSString* _searchText;
  // Current list of search result items retrieved from the search provider.
  NSArray<AtMemorySearchResultItem*>* _searchResults;
  // Search provider used to fetch search results and granular fill items.
  id<AtMemorySearchProvider> _searchProvider;
}
@end

@implementation AtMemoryMediator

- (instancetype)init {
  self = [super init];
  if (self) {
    _searchProvider = ScopedAtMemorySearchProviderOverride::Get();
  }
  return self;
}

- (void)setConsumer:(id<AtMemoryConsumer>)consumer {
  _consumer = consumer;
}

#pragma mark - AtMemoryViewControllerDelegate

- (void)atMemoryViewController:(AtMemoryViewController*)viewController
           didChangeSearchText:(NSString*)searchText {
  _searchText = [searchText copy];
  [self.consumer setSearchQuery:searchText];
}

- (void)atMemoryViewControllerDidTapSearch:
    (AtMemoryViewController*)viewController {
  [self.consumer setSearchLoading:YES];

  __weak AtMemoryMediator* weakSelf = self;
  NSTimeInterval delay = _searchProvider ? 0.05 : 5.0;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        [weakSelf didFinishSearch];
      });
}

- (void)atMemoryViewController:(AtMemoryViewController*)viewController
    didTapSearchResultInfoForItem:(AtMemorySearchResultItem*)item {
  NSArray<AtMemoryGranularFillItem*>* granularItems =
      _searchProvider ? [_searchProvider granularFillItemsForItem:item] : @[];
  [self.consumer setGranularFillItems:granularItems];
}

- (void)atMemoryViewController:(AtMemoryViewController*)viewController
              didSelectContent:(NSString*)content {
  [self.contentInjector userDidPickContent:content
                             passwordField:NO
                             requiresHTTPS:NO];
  [viewController.atMemoryHandler dismissAtMemory];
}

#pragma mark - Private

- (void)didFinishSearch {
  [self.consumer setSearchLoading:NO];
  _searchResults = _searchProvider
                       ? [_searchProvider searchResultsForText:_searchText]
                       : @[];
}

@end
