// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_PRESENTATION_DELEGATE_H_

#import <UIKit/UIKit.h>

// Presentation commands that depend on the context from which they are
// presented.
@protocol RecentTabsPresentationDelegate
// Tells the receiver to dismiss recent tabs. This may be used by a keyboard
// escape shortcut. Receiver may choose to ignore this message.
- (void)dismissRecentTabs;
// Tells the receiver to show the tab UI for regular tabs. NO-OP if the correct
// tab UI is already visible. Receiver may also dismiss recent tabs.
- (void)showActiveRegularTabFromRecentTabs;
// Tells the receiver to show the history UI. Receiver may also dismiss recent
// tabs.
- (void)showHistoryFromRecentTabs;
@end

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_PRESENTATION_DELEGATE_H_
