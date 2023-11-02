// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_PRESENTATION_DELEGATE_H_

#import <UIKit/UIKit.h>

namespace synced_sessions {
class DistantSession;
}

// Presentation commands that depend on the context from which they are
// presented.
@protocol RecentTabsPresentationDelegate <NSObject>
// Tells the receiver to show the tab UI for regular tabs. NO-OP if the correct
// tab UI is already visible. Receiver may also dismiss recent tabs.
- (void)showActiveRegularTabFromRecentTabs;
// Tells the receiver to show the history UI. Receiver may also dismiss recent
// tabs. If `searchTerms` is not empty, it will be used to pre-populate the
// search bar and filter results.
- (void)showHistoryFromRecentTabsFilteredBySearchTerms:(NSString*)searchTerms;
// Tells the receiver to open all tabs from the given `session`.
- (void)openAllTabsFromSession:(const synced_sessions::DistantSession*)session;

@optional
// Tells the receiver to display the tab grid. It is assumed the tab grid will
// already be aware of the ongoing search mode and terms. If this method is not
// implemented, the "Search Open Tabs" Suggested Action will not be displayed.
- (void)showRegularTabGridFromRecentTabs;
@end

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_PRESENTATION_DELEGATE_H_
