// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_item.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_constants.h"

@implementation PinnedItem

#pragma mark - Favicons

- (UIImage*)NTPFavicon {
  return CustomSymbolWithPointSize(kChromeProductSymbol,
                                   kPinnedCellFaviconSymbolPointSize);
}

@end
