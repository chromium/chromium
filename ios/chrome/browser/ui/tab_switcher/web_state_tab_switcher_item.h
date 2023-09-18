// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_WEB_STATE_TAB_SWITCHER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_WEB_STATE_TAB_SWITCHER_ITEM_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"

namespace web {
class WebState;
}  // namespace web

// A TabSwitcherItem from the `webState` info.
// It overrides the data source methods from TabSwitcherItem.
@interface WebStateTabSwitcherItem : TabSwitcherItem

- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithIdentifier:(web::WebStateID)identifier NS_UNAVAILABLE;

#pragma mark - Favicons

// Default favicon to use if the tab has no favicon available yet. Default is
// `kGlobeAmericasSymbol`.
- (UIImage*)defaultFavicon;

// Favicon to use for NTP. Default is nil.
// Subclasses can override this method to customize it.
- (UIImage*)NTPFavicon;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_WEB_STATE_TAB_SWITCHER_ITEM_H_
