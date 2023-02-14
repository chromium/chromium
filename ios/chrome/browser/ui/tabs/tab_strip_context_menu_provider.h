// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTEXT_MENU_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTEXT_MENU_PROVIDER_H_

#import <UIKit/UIKit.h>

// Protocol that provides tab strip context menus.
@protocol TabStripContextMenuProvider

// Returns an UIMenu instance for the given `identifier` and `pinnedState`.
- (UIMenu*)menuForWebStateIdentifier:(NSString*)identifier
                         pinnedState:(BOOL)pinnedState;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTEXT_MENU_PROVIDER_H_
