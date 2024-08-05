// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_PRESENTER_H_
#define IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_PRESENTER_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"

@protocol BubblePresenterDelegate;
@class BubbleViewControllerPresenter;
class HostContentSettingsMap;
@class LayoutGuideCenter;
class OverlayPresenter;
@protocol TabStripCommands;
@protocol ToolbarCommands;
class WebStateList;

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

namespace segmentation_platform {
class DeviceSwitcherResultDispatcher;
}  // namespace segmentation_platform

// TODO(crbug.com/40272358): refactor the class.
// Object handling the presentation of the different bubbles tips. The class is
// holding all the bubble presenters.
@interface BubblePresenter : NSObject

// Initializes a BubblePresenter whose bubbles are presented on the
// `rootViewController`.
- (instancetype)
    initWithDeviceSwitcherResultDispatcher:
        (segmentation_platform::DeviceSwitcherResultDispatcher*)
            deviceSwitcherResultDispatcher
                    hostContentSettingsMap:(HostContentSettingsMap*)settingsMap
                   tabStripCommandsHandler:
                       (id<TabStripCommands>)tabStripCommandsHandler
                                   tracker:(feature_engagement::Tracker*)
                                               engagementTracker
                              webStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<BubblePresenterDelegate> delegate;
@property(nonatomic, weak) UIViewController* rootViewController;
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;
@property(nonatomic, weak) id<ToolbarCommands> toolbarCommandsHandler;

// Overlay observers.
@property(nonatomic, assign) raw_ptr<OverlayPresenter>
    webContentOverlayPresenter;
@property(nonatomic, assign) raw_ptr<OverlayPresenter> infobarBannerPresenter;
@property(nonatomic, assign) raw_ptr<OverlayPresenter> infobarModalPresenter;

// Optionally presents a bubble associated with the Discover feed's menu button.
// The eligibility can depend on the UI hierarchy at the moment, the
// configuration and the display history of the bubble, etc.
- (void)presentDiscoverFeedMenuTipBubble;

// Optionally presents a relevant Follow help bubble while browsing a site.
// The eligibility can depend on the UI hierarchy at the moment, the
// configuration and the display history of the bubble, etc.
- (void)presentFollowWhileBrowsingTipBubble;

// Optionally presents a help bubble to let the user know that they can change
// the default mode (Desktop/Mobile) of the websites. The eligibility can depend
// on the UI hierarchy at the moment, the configuration and the display history
// of the bubble, etc.
- (void)presentDefaultSiteViewTipBubble;

// Optionally presents a help bubble for What's New.
// The eligibility can depend on the UI hierarchy at the moment, the
// configuration and the display history of the bubble, etc.
- (void)presentWhatsNewBottomToolbarBubble;

// Optionally presents a help bubble to inform the user that they can track the
// price of the item on the current website. The eligibility can depend on the
// UI hierarchy at the moment, the configuration and the display history of the
// bubble, etc.
- (void)presentPriceNotificationsWhileBrowsingTipBubble;

// Optionally presents a help bubble to inform the user that they can tap the
// Lens button in the omnibox keyboard to search with their camera. The
// eligibility can depend on the UI hierarchy at the moment, the configuration
// and the display history of the bubble, etc.
- (void)presentLensKeyboardTipBubble;

// Optionally presents a help bubble to inform the user that their tracked
// packages will appear in the Magic Stack. The eligibility can depend on the UI
// hierarchy at the moment, the configuration and the display history of the
// bubble, etc.
- (void)presentParcelTrackingTipBubble;

// Optionally presents a help bubble for the share button.
// The eligibility can depend on the UI hierarchy at the moment, the
// configuration and the display history of the bubble, etc.
- (void)presentShareButtonHelpBubbleIfEligible;

// Optionally presents a bubble associated with the tab grid iph.
// The eligibility can depend on the UI hierarchy at the moment, the
// configuration and the display history of the bubble, etc.
- (void)presentTabGridToolbarItemBubble;

// Optionally presents a bubble associated with the new tab iph.
// The eligibility can depend on the UI hierarchy at the moment, the
// configuration and the display history of the bubble, etc.
- (void)presentNewTabToolbarItemBubble;

// Optionally presents a gesture IPH associated with
// the pull-to-refresh feature.
// The eligibility can depend on the UI hierarchy at the moment, the
// configuration and the display history of the bubble, etc.
- (void)presentPullToRefreshGestureInProductHelp;

// Optionally presents a full screen IPH associated with the swipe to navigate
// back/forward feature.
// The eligibility can depend on the UI hierarchy at the moment, the
// configuration and the display history of the bubble, etc.
- (void)presentBackForwardSwipeGestureInProductHelp;

// Optionally presents a full screen IPH associated with the swipe to navigate
// back/forward feature.
// The eligibility can depend on the UI hierarchy at the moment, the
// configuration and the display history of the bubble, etc.
- (void)presentToolbarSwipeGestureInProductHelp;

// Delegate method to be invoked when the user has performed a swipe on the
// toolbar to switch tabs. Remove `toolbarSwipeGestureIPH` if visible.
- (void)handleToolbarSwipeGesture;

// Delegate method to be invoked when a gestural in-product help view is visible
// but the user has tapped outside of it. Do nothing if invoked when there is no
// IPH view.
- (void)handleTapOutsideOfVisibleGestureInProductHelp;

// Dismisses all bubbles.
- (void)hideAllHelpBubbles;

// Stops this presenter.
- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_PRESENTER_H_
