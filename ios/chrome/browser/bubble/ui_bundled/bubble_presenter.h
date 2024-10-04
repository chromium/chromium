// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_PRESENTER_H_
#define IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_PRESENTER_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"

@protocol BubblePresenterDelegate;
@class BubbleViewControllerPresenter;
@class FeedMetricsRecorder;
class HostContentSettingsMap;
@class LayoutGuideCenter;
class OverlayPresenter;
@protocol PopupMenuCommands;
@protocol TabStripCommands;
@protocol ToolbarCommands;
class WebStateList;

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

namespace segmentation_platform {
class DeviceSwitcherResultDispatcher;
}  // namespace segmentation_platform

// Object handling the presentation of the different bubbles tips. The class is
// holding all the bubble presenters.
@interface BubblePresenter : NSObject

// Initializes a BubblePresenter whose bubbles are presented on the
// root view controller of the owning browser.
- (instancetype)
        initWithLayoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter
                engagementTracker:
                    (raw_ptr<feature_engagement::Tracker>)engagementTracker
                     webStateList:(raw_ptr<WebStateList>)webStateList
    overlayPresenterForWebContent:
        (raw_ptr<OverlayPresenter>)webContentOverlayPresenter
                    infobarBanner:(raw_ptr<OverlayPresenter>)bannerPresenter
                     infobarModal:(raw_ptr<OverlayPresenter>)modalPresenter
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Delegate object to handle interactions.
@property(nonatomic, weak) id<BubblePresenterDelegate> delegate;

// The view controller that presents the bubbles.
@property(nonatomic, weak) UIViewController* rootViewController;

// Optionally presents a bubble associated with the Discover feed's menu button.
// The eligibility can depend on the UI hierarchy at the moment, the
// configuration and the display history of the bubble, etc.
- (void)presentDiscoverFeedMenuTipBubble;

// Optionally presents a help bubble associated with the NTP customization
// menu's entrypoint. The eligibility can depend on the UI hierarchy at the
// moment, the configuration and the display history of the bubble, etc.
- (void)presentHomeCustomizationTipBubble;

// Optionally presents a relevant Follow help bubble while browsing a site.
// The eligibility can depend on the UI hierarchy at the moment, the
// configuration and the display history of the bubble, etc.
- (void)presentFollowWhileBrowsingTipBubbleAndLogWithRecorder:
            (FeedMetricsRecorder*)recorder
                                             popupMenuHandler:
                                                 (id<PopupMenuCommands>)
                                                     popupMenuHandler;

// Optionally presents a help bubble to let the user know that they can change
// the default mode (Desktop/Mobile) of the websites. The eligibility can depend
// on the UI hierarchy at the moment, the configuration and the display history
// of the bubble, etc.
- (void)presentDefaultSiteViewTipBubbleWithSettingsMap:
            (raw_ptr<HostContentSettingsMap>)settingsMap
                                      popupMenuHandler:(id<PopupMenuCommands>)
                                                           popupMenuHandler;

// Optionally presents a help bubble for What's New.
// The eligibility can depend on the UI hierarchy at the moment, the
// configuration and the display history of the bubble, etc.
- (void)presentWhatsNewBottomToolbarBubbleWithPopupMenuHandler:
    (id<PopupMenuCommands>)popupMenuHandler;

// Optionally presents a help bubble to inform the user that they can track the
// price of the item on the current website. The eligibility can depend on the
// UI hierarchy at the moment, the configuration and the display history of the
// bubble, etc.
- (void)presentPriceNotificationsWhileBrowsingTipBubbleWithPopupMenuHandler:
    (id<PopupMenuCommands>)popupMenuHandler;

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

// Optionally present a bubble associated with the lens overlay.
// The eligibility can depend on the UI hierarchy at the moment, the
// configuration and the display history of the bubble.
- (void)presentLensOverlayTipBubble;

// Optionally presents a gesture IPH associated with the pull-to-refresh
// feature. The eligibility can depend on the UI hierarchy at the moment, the
// configuration and the display history of the bubble, etc.
- (void)
    presentPullToRefreshGestureInProductHelpWithDeviceSwitcherResultDispatcher:
        (raw_ptr<segmentation_platform::DeviceSwitcherResultDispatcher>)
            deviceSwitcherResultDispatcher;

// Optionally presents a full screen IPH associated with the swipe to
// navigate back/forward feature. The eligibility can depend on the UI
// hierarchy at the moment, the configuration and the display history of
// the bubble, etc.
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

// Stops observing all objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_PRESENTER_H_
