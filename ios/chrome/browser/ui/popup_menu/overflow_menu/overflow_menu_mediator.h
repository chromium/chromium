// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/follow/model/follow_action_state.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_consumer.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_action_provider.h"
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
namespace syncer {
class SyncService;
}

@protocol ActivityServiceCommands;
@protocol ApplicationCommands;
class AuthenticationService;
@protocol BookmarksCommands;
@protocol BrowserCoordinatorCommands;
class BrowserPolicyConnectorIOS;
@protocol FindInPageCommands;
class FollowBrowserAgent;
@protocol HelpCommands;
@protocol LensOverlayCommands;
@protocol OverflowMenuCustomizationCommands;
@class OverflowMenuOrderer;
class OverlayPresenter;
@protocol PageInfoCommands;
@protocol PopupMenuCommands;
class PrefService;
@protocol PriceNotificationsCommands;
class PromosManager;
@protocol QuickDeleteCommands;
class ReadingListBrowserAgent;
class ReadingListModel;
@protocol SettingsCommands;
class TabBasedIPHBrowserAgent;
class TemplateURLService;
@protocol TextZoomCommands;
class WebNavigationBrowserAgent;
class WebStateList;
@protocol WhatsNewCommands;

// Mediator for the overflow menu. This object is in charge of creating and
// updating the items of the overflow menu.
@interface OverflowMenuMediator
    : NSObject <BrowserContainerConsumer, OverflowMenuActionProvider>

// The data model for the overflow menu.
@property(nonatomic, weak) OverflowMenuModel* model;

// The WebStateList that this mediator listens for any changes on the current
// WebState.
@property(nonatomic, assign) WebStateList* webStateList;

// Command Handlers.
@property(nonatomic, weak) id<ActivityServiceCommands> activityServiceHandler;
@property(nonatomic, weak) id<ApplicationCommands> applicationHandler;
@property(nonatomic, weak) id<SettingsCommands> settingsHandler;
@property(nonatomic, weak) id<BookmarksCommands> bookmarksHandler;
@property(nonatomic, weak) id<LensOverlayCommands> lensOverlayHandler;
@property(nonatomic, weak) id<BrowserCoordinatorCommands>
    browserCoordinatorHandler;
@property(nonatomic, weak) id<FindInPageCommands> findInPageHandler;
@property(nonatomic, weak) id<HelpCommands> helpHandler;
@property(nonatomic, weak) id<OverflowMenuCustomizationCommands>
    overflowMenuCustomizationHandler;
@property(nonatomic, weak) id<PageInfoCommands> pageInfoHandler;
@property(nonatomic, weak) id<PopupMenuCommands> popupMenuHandler;
@property(nonatomic, weak) id<PriceNotificationsCommands>
    priceNotificationHandler;
@property(nonatomic, weak) id<TextZoomCommands> textZoomHandler;
@property(nonatomic, weak) id<QuickDeleteCommands> quickDeleteHandler;
@property(nonatomic, weak) id<WhatsNewCommands> whatsNewHandler;

// Navigation agent for reloading pages.
@property(nonatomic, assign) WebNavigationBrowserAgent* navigationAgent;

// If the current session is off the record or not.
@property(nonatomic, assign) bool isIncognito;

// The Orderer to control the order of the overflow menu.
@property(nonatomic, weak) OverflowMenuOrderer* menuOrderer;

// BaseViewController for presenting some UI.
@property(nonatomic, weak) UIViewController* baseViewController;

// Bookmark model to know if the page is bookmarked.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;

// Readinglist model to know if model has finished loading.
@property(nonatomic, assign) ReadingListModel* readingListModel;

// Pref service to retrieve profile preference values.
@property(nonatomic, assign) PrefService* profilePrefs;

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

// The Promos Manager to alert if the user uses What's New.
@property(nonatomic, assign) PromosManager* promosManager;

// The ReadingListBrowserAgent used to add urls to reading list.
@property(nonatomic, assign) ReadingListBrowserAgent* readingListBrowserAgent;

// The AuthenticationService to get sign-in info.
@property(nonatomic, assign) AuthenticationService* authenticationService;

// The TabBasedIPHBrowserAgent to handle tab based in-product help bubbles.
@property(nonatomic, assign) TabBasedIPHBrowserAgent* tabBasedIPHBrowserAgent;

// TemplateURLService to observe default search engine change.
@property(nonatomic, assign) TemplateURLService* templateURLService;

// If settings destination has a blue dot.
@property(nonatomic, assign) bool hasSettingsBlueDot;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_MEDIATOR_H_
