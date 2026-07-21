// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/test/fake_at_memory_search_provider.h"

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_item.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_result_item.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

@implementation FakeAtMemorySearchProvider {
  NSArray<NSDictionary*>* _searchResultsConfig;
  NSArray<NSDictionary*>* _granularFillItemsConfig;
}

- (void)setSearchResults:(NSArray<NSDictionary*>*)searchResults
       granularFillItems:(NSArray<NSDictionary*>*)granularFillItems {
  _searchResultsConfig = [searchResults copy];
  _granularFillItemsConfig = [granularFillItems copy];
}

- (NSArray<AtMemorySearchResultItem*>*)searchResultsForText:(NSString*)text {
  NSMutableArray<AtMemorySearchResultItem*>* results = [NSMutableArray array];
  for (NSDictionary* dict in _searchResultsConfig) {
    AtMemorySearchResultItem* item = [[AtMemorySearchResultItem alloc] init];
    item.fillingText = [NSString stringWithFormat:@"%@", dict[@"fillingText"]];
    item.subtitle = [NSString stringWithFormat:@"%@", dict[@"subtitle"]];
    item.iconSymbolName =
        dict[@"iconSymbolName"]
            ? [NSString stringWithFormat:@"%@", dict[@"iconSymbolName"]]
            : kTextSparkSymbol;
    [results addObject:item];
  }
  return results;
}

- (NSArray<AtMemoryGranularFillItem*>*)granularFillItemsForItem:
    (AtMemorySearchResultItem*)item {
  NSMutableArray<AtMemoryGranularFillItem*>* items = [NSMutableArray array];
  for (NSDictionary* dict in _granularFillItemsConfig) {
    AtMemoryGranularFillItem* fillItem =
        [[AtMemoryGranularFillItem alloc] initWithType:0];
    fillItem.attributeName = [NSString stringWithFormat:@"%@", dict[@"name"]];
    id values = dict[@"values"];
    NSMutableArray<NSString*>* arrayValues = [NSMutableArray array];
    if ([values respondsToSelector:@selector(count)]) {
      NSUInteger count = [values count];
      for (NSUInteger i = 0; i < count; ++i) {
        [arrayValues addObject:[NSString stringWithFormat:@"%@", values[i]]];
      }
    }
    fillItem.attributeValue = arrayValues;
    [items addObject:fillItem];
  }
  return items;
}

@end
