// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_WEB_STATE_TAB_SWITCHER_ITEM_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_WEB_STATE_TAB_SWITCHER_ITEM_H_

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_switcher_item.h"

namespace web {
class WebState;
}  // namespace web

// A TabSwitcherItem from the `webState` info.
// It overrides the data source methods from TabSwitcherItem.
@interface WebStateTabSwitcherItem : TabSwitcherItem

- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithIdentifier:(web::WebStateID)identifier NS_UNAVAILABLE;

// The web state represented by this item.
@property(nonatomic, readonly) web::WebState* webState;

#pragma mark - Favicons

// Favicon to use for NTP. Default is an empty image.
// Subclasses can override this method to customize it.
- (UIImage*)NTPFavicon;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_WEB_STATE_TAB_SWITCHER_ITEM_H_
