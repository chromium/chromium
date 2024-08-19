// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MENU_TAB_CONTEXT_MENU_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_MENU_TAB_CONTEXT_MENU_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/ui/sharing/sharing_scenario.h"

class GURL;
class TabGroup;

namespace synced_sessions {
struct DistantSession;
}

namespace web {
class WebStateID;
}  // namespace web

// Methods used to create context menu actions for tabs.
@protocol TabContextMenuDelegate <NSObject>

// Tells the delegate to trigger the URL sharing flow for the given `URL` and
// `title`, with the origin `view` representing the UI component for that URL.
// TODO(crbug.com/40759896): Investigate removing `view` as a parameter.
- (void)shareURL:(const GURL&)URL
           title:(NSString*)title
        scenario:(SharingScenario)scenario
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
- (void)pinTabWithIdentifier:(web::WebStateID)identifier;

// Tells the delegate to unpin a tab with the item identifier `identifier`.
- (void)unpinTabWithIdentifier:(web::WebStateID)identifier;

// Tells the delegate to create a new tab group with the given identifier
// `identifier`. `incognito` YES if the given tab is incognito.
- (void)createNewTabGroupWithIdentifier:(web::WebStateID)identifier
                              incognito:(BOOL)incognito;

// Tells the delegate to display the group edition view of the group of the
// given identifier.
- (void)editTabGroup:(base::WeakPtr<const TabGroup>)group
           incognito:(BOOL)incognito;

// Tells the delegate to close the tab with the item identifier `identifier`.
// `incognito` tracks the incognito state of the tab.
- (void)closeTabWithIdentifier:(web::WebStateID)identifier
                     incognito:(BOOL)incognito;

// Tells the delegate to delete the group. `incognito` tracks the incognito
// state of the group. `sourceView` is the view that the delete action
// originated from.
- (void)deleteTabGroup:(base::WeakPtr<const TabGroup>)group
             incognito:(BOOL)incognito
            sourceView:(UIView*)sourceView;

// Tells the delegate to close the group. `incognito` tracks the incognito state
// of the group.
- (void)closeTabGroup:(base::WeakPtr<const TabGroup>)group
            incognito:(BOOL)incognito;

// Tells the delegate to ungroup the `group`. `incognito` tracks the incognito
// state of the group. `sourceView` is the view that the delete action
// originated from.
- (void)ungroupTabGroup:(base::WeakPtr<const TabGroup>)group
              incognito:(BOOL)incognito
             sourceView:(UIView*)sourceView;

@end

#endif  // IOS_CHROME_BROWSER_UI_MENU_TAB_CONTEXT_MENU_DELEGATE_H_
