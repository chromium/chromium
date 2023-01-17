// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MENU_TAB_CONTEXT_MENU_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_MENU_TAB_CONTEXT_MENU_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/sharing/activity_services/activity_scenario.h"

class GURL;

namespace synced_sessions {
class DistantSession;
}

// Methods used to create context menu actions for tabs.
@protocol TabContextMenuDelegate <NSObject>

// Tells the delegate to trigger the URL sharing flow for the given `URL` and
// `title`, with the origin `view` representing the UI component for that URL.
// TODO(crbug.com/1196956): Investigate removing `view` as a parameter.
- (void)shareURL:(const GURL&)URL
           title:(NSString*)title
        scenario:(ActivityScenario)scenario
        fromView:(UIView*)view;

// Tells the delegate to remove Sessions corresponding to the given the table
// view's `sectionIdentifier`.
- (void)removeSessionAtTableSectionWithIdentifier:(NSInteger)sectionIdentifier;

// Asks the delegate for the Session corresponding to the given the table view's
// `sectionIdentifier`.
- (synced_sessions::DistantSession const*)sessionForTableSectionWithIdentifier:
    (NSInteger)sectionIdentifier;

@optional
// Tells the delegate to add `URL` and `title` to the reading list.
- (void)addToReadingListURL:(const GURL&)URL title:(NSString*)title;

// Tells the delegate to create a bookmark for `URL` with `title`.
- (void)bookmarkURL:(const GURL&)URL title:(NSString*)title;

// Tells the delegate to edit the bookmark for `URL`.
- (void)editBookmarkWithURL:(const GURL&)URL;

// Tells the delegate to open the tab grid selection mode.
- (void)selectTabs;

// Tells the delegate to pin a tab with the item identifier `identifier`.
- (void)pinTabWithIdentifier:(NSString*)identifier;

// Tells the delegate to unpin a tab with the item identifier `identifier`.
- (void)unpinTabWithIdentifier:(NSString*)identifier;

// Tells the delegate to close the tab with the item identifier `identifier`.
// `incognito`tracks the incognito state of the tab.
// `pinned` tracks the pinned state of the tab.
- (void)closeTabWithIdentifier:(NSString*)identifier
                     incognito:(BOOL)incognito
                        pinned:(BOOL)pinned;

@end

#endif  // IOS_CHROME_BROWSER_UI_MENU_TAB_CONTEXT_MENU_DELEGATE_H_
