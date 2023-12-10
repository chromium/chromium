// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_mediator.h"

#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_consumer.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/cells/snippet_search_engine_item.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "url/gurl.h"

@implementation SearchEngineChoiceMediator {
  FaviconLoader* _faviconLoader;
}

- (instancetype)initWithFaviconLoader:(FaviconLoader*)faviconLoader {
  self = [super init];
  if (self) {
    _faviconLoader = faviconLoader;
  }
  return self;
}

#pragma mark - Properties

- (void)setSelectedItem:(SnippetSearchEngineItem*)item {
  if (_selectedItem == item) {
    return;
  }
  _selectedItem = item;

  __weak __typeof(self) weakSelf = self;
  _faviconLoader->FaviconForPageUrl(
      item.URL, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/YES, ^(FaviconAttributes* attributes) {
        [weakSelf.consumer
            updateFakeOmniboxWithFavicon:[[UIImageView alloc]
                                             initWithImage:attributes
                                                               .faviconImage]
                        SearchEngineName:item.name];
      });
}

- (void)disconnect {
  self.consumer = nil;
  _selectedItem = nullptr;
  _faviconLoader = nullptr;
}

@end
