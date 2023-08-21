// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/follow/follow_action_state.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_consumer.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"

namespace bookmarks {
class BookmarkModel;
}
namespace feature_engagement {
class Tracker;
}
namespace web {
class WebState;
}
namespace supervised_user {
class SupervisedUserService;
}
namespace syncer {
class SyncService;
}

@protocol ActivityServiceCommands;
@protocol ApplicationCommands;
@protocol BookmarksCommands;
@protocol BrowserCoordinatorCommands;
class BrowserPolicyConnectorIOS;
@protocol FindInPageCommands;
class FollowBrowserAgent;
@protocol OverflowMenuCustomizationCommands;
@class OverflowMenuOrderer;
class OverlayPresenter;
@protocol PageInfoCommands;
@protocol PopupMenuCommands;
class PrefService;
@protocol PriceNotificationsCommands;
class PromosManager;
class ReadingListBrowserAgent;
class ReadingListModel;
@protocol TextZoomCommands;
class WebNavigationBrowserAgent;
class WebStateList;

// Mediator for the overflow menu. This object is in charge of creating and
// updating the items of the overflow menu.
@interface OverflowMenuMediator : NSObject <BrowserContainerConsumer>

// The data model for the overflow menu.
@property(nonatomic, weak) OverflowMenuModel* model;

// The WebStateList that this mediator listens for any changes on the current
// WebState.
@property(nonatomic, assign) WebStateList* webStateList;

// Dispatcher.
@property(nonatomic, weak) id<ActivityServiceCommands,
                              ApplicationCommands,
                              BrowserCoordinatorCommands,
                              FindInPageCommands,
                              OverflowMenuCustomizationCommands,
                              PriceNotificationsCommands,
                              TextZoomCommands>
    dispatcher;

@property(nonatomic, weak) id<BookmarksCommands> bookmarksCommandsHandler;
@property(nonatomic, weak) id<PopupMenuCommands> popupMenuCommandsHandler;
@property(nonatomic, weak) id<PageInfoCommands> pageInfoCommandsHandler;

// Navigation agent for reloading pages.
@property(nonatomic, assign) WebNavigationBrowserAgent* navigationAgent;

// If the current session is off the record or not.
@property(nonatomic, assign) bool isIncognito;

// The Orderer to control the order of the overflow menu.
@property(nonatomic, weak) OverflowMenuOrderer* menuOrderer;

// BaseViewController for presenting some UI.
@property(nonatomic, weak) UIViewController* baseViewController;

// Bookmarks models to know if the page is bookmarked.
@property(nonatomic, assign)
    bookmarks::BookmarkModel* localOrSyncableBookmarkModel;
@property(nonatomic, assign) bookmarks::BookmarkModel* accountBookmarkModel;

// Readinglist model to know if model has finished loading.
@property(nonatomic, assign) ReadingListModel* readingListModel;

// Pref service to retrieve browser state preference values.
@property(nonatomic, assign) PrefService* browserStatePrefs;

// Pref service to retrieve local state preference values.
@property(nonatomic, assign) PrefService* localStatePrefs;

// The overlay presenter for OverlayModality::kWebContentArea.  This mediator
// listens for overlay presentation events to determine whether the "Add to
// Reading List" button should be enabled.
@property(nonatomic, assign) OverlayPresenter* webContentAreaOverlayPresenter;

// Records events for the use of in-product help. The mediator does not take
// ownership of tracker. Tracker must not be destroyed during lifetime of the
// object.
@property(nonatomic, assign) feature_engagement::Tracker* engagementTracker;

// The current browser policy connector.
@property(nonatomic, assign) BrowserPolicyConnectorIOS* browserPolicyConnector;

// The FollowBrowserAgent used to manage web channels subscriptions.
@property(nonatomic, assign) FollowBrowserAgent* followBrowserAgent;

// The Sync Service that provides the status of Sync.
@property(nonatomic, assign) syncer::SyncService* syncService;

// Service that describes the supervision state of the account.
@property(nonatomic, assign)
    supervised_user::SupervisedUserService* supervisedUserService;

// The Promos Manager to alert if the user uses What's New.
@property(nonatomic, assign) PromosManager* promosManager;

// The ReadingListBrowserAgent used to add urls to reading list.
@property(nonatomic, assign) ReadingListBrowserAgent* readingListBrowserAgent;

// Updates the pin state of the tab corresponding to the given `webState` in
// `webStateList`.
+ (void)setTabPinned:(BOOL)pinned
            webState:(web::WebState*)webState
        webStateList:(WebStateList*)webStateList;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_MEDIATOR_H_
