// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_mediator.h"

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_consumer.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/cells/snippet_search_engine_item.h"
#import "url/gurl.h"

@implementation SearchEngineChoiceMediator

#pragma mark - Properties

- (void)setSelectedItem:(SnippetSearchEngineItem*)item {
  if (_selectedItem == item) {
    return;
  }
  _selectedItem = item;

  [self.consumer updateFakeOmniboxWithFaviconImage:item.faviconImage
                                  searchEngineName:item.name];
}

- (void)disconnect {
  self.consumer = nil;
  _selectedItem = nullptr;
}

@end
