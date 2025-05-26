// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_strip/ui/tab_strip_tab_item.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/pinned_tabs/pinned_tabs_constants.h"

namespace {

// Size of the NTP symbol in points.
const CGFloat kSymbolSize = 14.0;

}  // namespace

@implementation TabStripTabItem

#pragma mark - Favicons

- (UIImage*)NTPFavicon {
  return DefaultSymbolWithPointSize(kGlobeAmericasSymbol, kSymbolSize);
}

@end
