// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_mediator.h"

#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_consumer.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_item.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_item.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_result_item.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_content_injector.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

static NSArray<AtMemorySearchItem*>* gRecentFills = nil;

namespace {

// TODO(crbug.com/532090671): Delete mock data methods once integration is
// completed.
NSArray<AtMemorySearchResultItem*>* GetMockSearchResults() {
  AtMemorySearchResultItem* item1 = [[AtMemorySearchResultItem alloc] init];
  item1.fillingText = @"Landabout Hotel, 110-2494-0000-24955, ...";
  item1.subtitle = @"Reservation · Tokyo · 18 May";
  item1.iconSymbolName = kTextSparkSymbol;
  return @[ item1 ];
}

NSArray<AtMemoryGranularFillItem*>* GetMockGranularFillItems() {
  AtMemoryGranularFillItem* nameItem =
      [[AtMemoryGranularFillItem alloc] initWithType:0];
  nameItem.attributeName = @"Name";
  nameItem.attributeValue = @[ @"Alex Beckett" ];
  return @[ nameItem ];
}

}  // namespace

@implementation AtMemoryMediator

+ (void)setRecentFills:(NSArray<AtMemorySearchItem*>*)recentFills {
  gRecentFills = [recentFills copy];
}

- (NSArray<AtMemorySearchItem*>*)recentFills {
  return gRecentFills;
}

- (void)setConsumer:(id<AtMemoryConsumer>)consumer {
  _consumer = consumer;
  [_consumer setGranularFillItems:GetMockGranularFillItems()];
  if (self.recentFills.count > 0) {
    [consumer setRecentFills:self.recentFills];
    [consumer setViewState:autofill::AtMemoryViewState::kRecentFills];
  } else {
    [consumer setViewState:autofill::AtMemoryViewState::kEmpty];
  }
}

#pragma mark - AtMemoryViewControllerDelegate

- (void)atMemoryViewController:(AtMemoryViewController*)viewController
           didChangeSearchText:(NSString*)searchText {
  if (searchText.length == 0) {
    if (self.recentFills.count > 0) {
      [self.consumer setViewState:autofill::AtMemoryViewState::kRecentFills];
    } else {
      [self.consumer setViewState:autofill::AtMemoryViewState::kEmpty];
    }
  } else {
    [self.consumer setSearchQuery:searchText];
    [self.consumer setViewState:autofill::AtMemoryViewState::kSearch];
  }
}

- (void)atMemoryViewControllerDidTapSearch:
    (AtMemoryViewController*)viewController {
  [self.consumer setSearchLoading:YES];

  __weak AtMemoryMediator* weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(5.0 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        [weakSelf didFinishSearch];
      });
}

- (void)atMemoryViewControllerDidTapSearchResultInfo:
    (AtMemoryViewController*)viewController {
  [self.consumer setViewState:autofill::AtMemoryViewState::kGranularFill];
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
  [self.consumer setSearchResults:GetMockSearchResults()];
  [self.consumer setViewState:autofill::AtMemoryViewState::kSearchResults];
}

@end
