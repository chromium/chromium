// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_MODEL_H_
#define IOS_CHROME_BROWSER_TABS_TAB_MODEL_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <memory>

#import "ios/chrome/browser/sessions/session_window_restoring.h"
#import "ios/web/public/navigation/navigation_manager.h"

#include "ui/base/page_transition_types.h"

class GURL;
@class SessionServiceIOS;
@class SessionWindowIOS;
class TabModelSyncedWindowDelegate;
class TabUsageRecorder;
class WebStateList;

namespace ios {
class ChromeBrowserState;
}

namespace web {
struct Referrer;
class WebState;
}

namespace TabModelConstants {

// Position the tab automatically. This value is used as index parameter in
// methods that require an index when the caller doesn't have a preference
// on the position where the tab will be open.
NSUInteger const kTabPositionAutomatically = NSNotFound;

}  // namespace TabModelConstants

// A model of a tab "strip". Although the UI representation may not be a
// traditional strip at all, tabs are still accessed via an integral index.
// The model knows about the currently selected tab in order to maintain
// consistency between multiple views that need the current tab to be
// synchronized.
@interface TabModel : NSObject <SessionWindowRestoring>

// The delegate for sync.
@property(nonatomic, readonly)
    TabModelSyncedWindowDelegate* syncedWindowDelegate;

// BrowserState associated with this TabModel.
@property(nonatomic, readonly) ios::ChromeBrowserState* browserState;

// Records UMA metrics about Tab usage.
@property(nonatomic, readonly) TabUsageRecorder* tabUsageRecorder;

// YES if this tab set is off the record.
@property(nonatomic, readonly, getter=isOffTheRecord) BOOL offTheRecord;

// NO if the model has at least one tab.
@property(nonatomic, readonly, getter=isEmpty) BOOL empty;

// Determines the number of tabs in the model.
@property(nonatomic, readonly) NSUInteger count;

// The WebStateList owned by the TabModel.
@property(nonatomic, readonly) WebStateList* webStateList;

// YES if there is a session restoration in progress.
@property(nonatomic, readonly, getter=isRestoringSession) BOOL restoringSession;

// Initializes tabs from a restored session. |-setCurrentTab| needs to be called
// in order to display the views associated with the tabs. Waits until the views
// are ready. |browserState| cannot be nil. |service| cannot be nil; this class
// creates intermediate SessionWindowIOS objects which must be consumed by a
// session service before they are deallocated.
- (instancetype)initWithSessionService:(SessionServiceIOS*)service
                          browserState:(ios::ChromeBrowserState*)browserState
                          webStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Add/modify tabs.

// Opens a tab at the specified URL. For certain transition types, will consult
// the order controller and thus may only use |index| as a hint.
// |parentWebState| may be nil if there is no parent associated with this new
// tab. |openedByDOM| is YES if the page was opened by DOM. The |index|
// parameter can be set to TabModelConstants::kTabPositionAutomatically if the
// caller doesn't have a preference for the position of the tab.
- (web::WebState*)insertWebStateWithURL:(const GURL&)URL
                               referrer:(const web::Referrer&)referrer
                             transition:(ui::PageTransition)transition
                                 opener:(web::WebState*)parentWebState
                            openedByDOM:(BOOL)openedByDOM
                                atIndex:(NSUInteger)index
                           inBackground:(BOOL)inBackground;

// As above, but using WebLoadParams to specify various optional parameters.
- (web::WebState*)insertWebStateWithLoadParams:
                      (const web::NavigationManager::WebLoadParams&)params
                                        opener:(web::WebState*)parentWebState
                                   openedByDOM:(BOOL)openedByDOM
                                       atIndex:(NSUInteger)index
                                  inBackground:(BOOL)inBackground;

// Opens a new blank tab in response to DOM window opening action. Creates a web
// state with empty navigation manager.
- (web::WebState*)insertOpenByDOMWebStateWithOpener:
    (web::WebState*)parentWebState;

// Closes the tab at the given |index|. |index| must be valid.
- (void)closeTabAtIndex:(NSUInteger)index;

// Closes ALL the tabs.
- (void)closeAllTabs;

// Records tab session metrics.
- (void)recordSessionMetrics;

// Sets whether the user is primarily interacting with this tab model.
- (void)setPrimary:(BOOL)primary;

// Tells the receiver to disconnect from the model object it depends on. This
// should be called before destroying the browser state that the receiver was
// initialized with.
// It is safe to call this method multiple times.
// At this point the tab model will no longer ever be active, and will likely be
// deallocated soon. Calling any other methods or accessing any properties on
// the tab model after this is called is unsafe.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_MODEL_H_
