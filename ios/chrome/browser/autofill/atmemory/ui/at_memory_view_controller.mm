// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_view_controller.h"

#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation AtMemoryViewController {
  // The current search text.
  NSString* _searchQuery;
  // Whether search results are loading.
  BOOL _searchLoading;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
}

#pragma mark - AtMemoryConsumer

- (void)setGranularFillItems:(NSArray<AtMemoryGranularFillItem*>*)items {
}

- (void)setSearchQuery:(NSString*)query {
  _searchQuery = [query copy];
}

- (void)setSearchLoading:(BOOL)loading {
  _searchLoading = loading;
}

@end
