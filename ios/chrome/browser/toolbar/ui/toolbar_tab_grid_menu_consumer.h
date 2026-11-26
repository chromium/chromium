// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_TAB_GRID_MENU_CONSUMER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_TAB_GRID_MENU_CONSUMER_H_

#import <UIKit/UIKit.h>

// Receives the context menu of the tab grid buttons, which is shared by all the
// surfaces showing them.
@protocol ToolbarTabGridMenuConsumer <NSObject>

// Sets the context menu of the tab grid buttons. `nil` disables it.
- (void)setTabGridMenu:(UIMenu*)menu;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_TAB_GRID_MENU_CONSUMER_H_
