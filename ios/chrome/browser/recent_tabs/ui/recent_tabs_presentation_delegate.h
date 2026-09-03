// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_RECENT_TABS_UI_RECENT_TABS_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_RECENT_TABS_UI_RECENT_TABS_PRESENTATION_DELEGATE_H_

#import <UIKit/UIKit.h>

#include <string>

#import "components/sessions/core/session_id.h"

namespace synced_sessions {
struct DistantSession;
struct DistantTab;
}

// Presentation commands that depend on the context from which they are
// presented.
@protocol RecentTabsPresentationDelegate <NSObject>
// Tells the receiver to show the tab UI for regular tabs. NO-OP if the correct
// tab UI is already visible. Receiver may also dismiss recent tabs.
- (void)showActiveRegularTabFromRecentTabs;
// Tells the receiver to show the history UI. Receiver may also dismiss recent
// tabs.
- (void)showHistoryFromRecentTabs;
// Tells the receiver to show the History Sync Opt-In screen. If the user has
// signed-in just before this step for the sole purpose of enabling history sync
// (Eg. using the Recent Tabs sync promo), `dedicatedSignInDone` will be `YES`,
// and the user is signed-out if history opt-in is declined.
- (void)showHistorySyncOptInAfterDedicatedSignIn:(BOOL)dedicatedSignInDone;
// Tells the receiver to open all tabs from the given `session`.
- (void)openAllTabsFromSession:(const synced_sessions::DistantSession*)session;
// Tells the receiver to restore the tab with the given `sessionId`.
- (void)openTabWithTabRestoreEntryId:(SessionID)sessionId;
// Tells the receiver to open the given `distantTab`.
- (void)openDistantTab:(const synced_sessions::DistantTab*)distantTab;
// Asks the presenter to display the reauthenticate the primary account.
// The primary should be available.
- (void)showPrimaryAccountReauth;
// Tells the receiver to delete the foreign session with `sessionTag`.
- (void)deleteForeignSession:(const std::string&)sessionTag;
@end

#endif  // IOS_CHROME_BROWSER_RECENT_TABS_UI_RECENT_TABS_PRESENTATION_DELEGATE_H_
