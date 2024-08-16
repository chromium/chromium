// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/browser_container/browser_container_consumer.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_action_handler_delegate.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_ui_updating.h"

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

class BrowserPolicyConnectorIOS;
class FollowBrowserAgent;
@protocol LensCommands;
class OverlayPresenter;
@protocol PopupMenuConsumer;
class PrefService;
class ReadingListBrowserAgent;
class ReadingListModel;
class TemplateURLService;
class UrlLoadingBrowserAgent;
class WebStateList;

// Mediator for the popup menu. This object is in charge of creating and
// updating the items of the popup menu.
@interface PopupMenuMediator
    : NSObject <BrowserContainerConsumer, PopupMenuActionHandlerDelegate>

// Initializes the mediator with whether it `isIncognito`, a `readingListModel`
// used to display the badge for the reading list entry, and a
// `browserPolicyConnector` used to check if the browser is managed by policy.
- (instancetype)initWithIsIncognito:(BOOL)isIncognito
                   readingListModel:(ReadingListModel*)readingListModel
             browserPolicyConnector:
                 (BrowserPolicyConnectorIOS*)browserPolicyConnector
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The WebStateList that this mediator listens for any changes on the current
// WebState.
@property(nonatomic, assign) WebStateList* webStateList;
// The overlay presenter for OverlayModality::kWebContentArea.  This mediator
// listens for overlay presentation events to determine whether the "Add to
// Reading List" button should be enabled.
@property(nonatomic, assign) OverlayPresenter* webContentAreaOverlayPresenter;
// The consumer to be configured with this mediator.
@property(nonatomic, strong) id<PopupMenuConsumer> popupMenu;
// Handler for Lens commands.
@property(nonatomic, weak) id<LensCommands> lensCommandsHandler;
// Records events for the use of in-product help. The mediator does not take
// ownership of tracker. Tracker must not be destroyed during lifetime of the
// object.
@property(nonatomic, assign) feature_engagement::Tracker* engagementTracker;
// The bookmarks model to know if the page is bookmarked.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;
// Pref service to retrieve preference values.
@property(nonatomic, assign) PrefService* prefService;
// The template url service to use for checking whether search by image is
// available.
@property(nonatomic, assign) TemplateURLService* templateURLService;
// The URL loading service, used to load the reverse image search.
@property(nonatomic, assign) UrlLoadingBrowserAgent* URLLoadingBrowserAgent;
// The FollowBrowserAgent used to manage web channels subscriptions.
@property(nonatomic, assign) FollowBrowserAgent* followBrowserAgent;
// The ReadingListBrowserAgent used to add urls to reading list.
@property(nonatomic, assign) ReadingListBrowserAgent* readingListBrowserAgent;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_MEDIATOR_H_
