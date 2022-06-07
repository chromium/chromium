// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/follow/follow_action_state.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_consumer.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_carousel_metrics_delegate.h"

namespace bookmarks {
class BookmarkModel;
}
namespace feature_engagement {
class Tracker;
}
@protocol ApplicationCommands;
@protocol BrowserCommands;
class BrowserPolicyConnectorIOS;
class OverlayPresenter;
class PrefService;
@protocol FindInPageCommands;
@protocol TextZoomCommands;
class WebNavigationBrowserAgent;
class WebStateList;
@class FeedMetricsRecorder;

// Mediator for the overflow menu. This object is in charge of creating and
// updating the items of the overflow menu.
@interface OverflowMenuMediator
    : NSObject <BrowserContainerConsumer, PopupMenuCarouselMetricsDelegate>

// The data model for the overflow menu.
@property(nonatomic, readonly) OverflowMenuModel* overflowMenuModel;

// The WebStateList that this mediator listens for any changes on the current
// WebState.
@property(nonatomic, assign) WebStateList* webStateList;

// Dispatcher.
// TODO(crbug.com/906662): This class uses BrowserCoordinatorCommands via their
// includion in BrowserCommands. That dependency should be explicit, and instead
// of a single parameter for all command protocols, separate handler properties
// should be used for each necessary protocol (see ToolbarButtonActionsHandler
// for an example of this).
// TODO(crbug.com/1323758): This uses PageInfoCommands via inclusion in
// BrowserCommands, and should instead use a dedicated handler.
// TODO(crbug.com/1323764): This uses PopupMenuCommands via inclusion in
// BrowserCommands, and should instead use a dedicated handler.
@property(nonatomic, weak) id<ApplicationCommands,
                              BrowserCommands,
                              FindInPageCommands,
                              TextZoomCommands>
    dispatcher;

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

// The metrics recorder to record follow related metrics.
@property(nonatomic, assign) FeedMetricsRecorder* feedMetricsRecorder;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_MEDIATOR_H_
