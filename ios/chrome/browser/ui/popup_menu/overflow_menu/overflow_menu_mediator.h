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
@protocol ActivityServiceCommands;
@protocol ApplicationCommands;
@protocol BookmarksCommands;
@protocol BrowserCommands;
@protocol BrowserCoordinatorCommands;
class BrowserPolicyConnectorIOS;
class OverlayPresenter;
@protocol PageInfoCommands;
@protocol PopupMenuCommands;
@protocol PriceNotificationsCommands;
class PrefService;
@protocol FindInPageCommands;
@protocol TextZoomCommands;
class WebNavigationBrowserAgent;
class WebStateList;
class FollowBrowserAgent;

// Mediator for the overflow menu. This object is in charge of creating and
// updating the items of the overflow menu.
@interface OverflowMenuMediator : NSObject <BrowserContainerConsumer>

// The data model for the overflow menu.
@property(nonatomic, readonly) OverflowMenuModel* overflowMenuModel;

// The WebStateList that this mediator listens for any changes on the current
// WebState.
@property(nonatomic, assign) WebStateList* webStateList;

// Dispatcher.
@property(nonatomic, weak) id<ActivityServiceCommands,
                              ApplicationCommands,
                              BrowserCommands,
                              BrowserCoordinatorCommands,
                              FindInPageCommands,
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

// BaseViewController for presenting some UI.
@property(nonatomic, weak) UIViewController* baseViewController;

// The bookmarks model to know if the page is bookmarked.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;

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

// The number of destinations immediately visible to the user when opening the
// new overflow menu (i.e. the number of "above-the-fold" destinations).
@property(nonatomic, assign) int numAboveFoldDestinations;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_MEDIATOR_H_
