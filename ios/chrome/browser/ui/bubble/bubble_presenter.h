// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_PRESENTER_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/shared/public/commands/help_commands.h"

@protocol BubblePresenterDelegate;
@class BubbleViewControllerPresenter;
@class CommandDispatcher;
class HostContentSettingsMap;
@class LayoutGuideCenter;
class PrefService;
@class SceneState;
@protocol TabStripCommands;
@protocol ToolbarCommands;
class UrlLoadingNotifierBrowserAgent;
class WebStateList;

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

namespace segmentation_platform {
class DeviceSwitcherResultDispatcher;
}  // namespace segmentation_platform

// TODO(crbug.com/1454553): refactor the class.
// Object handling the presentation of the different bubbles tips. The class is
// holding all the bubble presenters.
@interface BubblePresenter : NSObject <HelpCommands>

// Initializes a BubblePresenter whose bubbles are presented on the
// `rootViewController`.
- (instancetype)
    initWithDeviceSwitcherResultDispatcher:
        (segmentation_platform::DeviceSwitcherResultDispatcher*)
            deviceSwitcherResultDispatcher
                    hostContentSettingsMap:(HostContentSettingsMap*)settingsMap
                           loadingNotifier:(UrlLoadingNotifierBrowserAgent*)
                                               urlLoadingNotifier
                               prefService:(PrefService*)prefService
                                sceneState:(SceneState*)sceneState
                   tabStripCommandsHandler:
                       (id<TabStripCommands>)tabStripCommandsHandler
                                   tracker:(feature_engagement::Tracker*)
                                               engagementTracker
                              webStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Used to display the new incognito tab tip in-product help promotion bubble.
@property(nonatomic, strong, readonly)
    BubbleViewControllerPresenter* incognitoTabTipBubblePresenter;

@property(nonatomic, weak) id<BubblePresenterDelegate> delegate;
@property(nonatomic, weak) UIViewController* rootViewController;
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;
@property(nonatomic, weak) id<ToolbarCommands> toolbarCommandsHandler;

// Stops this presenter.
- (void)stop;

// Presents a bubble associated with the Discover feed header's menu button.
- (void)presentDiscoverFeedHeaderTipBubble;

// Shows a relevant Follow help bubble while browsing a site, if applicable.
- (void)presentFollowWhileBrowsingTipBubble;

// Shows a help bubble to let the user know that they can change the default
// mode (Desktop/Mobile) of the websites.
- (void)presentDefaultSiteViewTipBubble;

// Presents a help bubble for What's New, if applicable.
- (void)presentWhatsNewBottomToolbarBubble;

// Presents a help bubble to inform the user that they can track the price of
// the item on the current website.
- (void)presentPriceNotificationsWhileBrowsingTipBubble;

// Presents a help bubble to inform the user that they can tap the Lens
// button in the omnibox keyboard to search with their camera.
- (void)presentLensKeyboardTipBubble;

// Presents a help bubble to inform the user that their tracked packages will
// appear in the Magic Stack.
- (void)presentParcelTrackingTipBubble;

@end

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_PRESENTER_H_
