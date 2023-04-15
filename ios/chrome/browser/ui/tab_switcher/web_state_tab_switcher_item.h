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
@interface WebStateTabSwitcherItem : TabSwitcherItem

- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithIdentifier:(NSString*)identifier NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_WEB_STATE_TAB_SWITCHER_ITEM_H_
