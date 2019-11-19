// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/badges/badge_consumer.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_consumer.h"
#import "ios/chrome/browser/ui/orchestrator/location_bar_animatee.h"

@class InfobarMetricsRecorder;
@class OmniboxTextFieldIOS;
@protocol ActivityServiceCommands;
@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol InfobarCommands;
@protocol LocationBarOffsetProvider;
@protocol LoadQueryCommands;

@protocol LocationBarViewControllerDelegate<NSObject>

// Notifies the delegate about a tap on the steady-state location bar.
- (void)locationBarSteadyViewTapped;

// Notifies the delegate about a tap on the Copy entry in the editing menu.
- (void)locationBarCopyTapped;

@end

// The view controller displaying the location bar. Manages the two states of
// the omnibox - the editing and the non-editing states. In the editing state,
// the omnibox textfield is displayed; in the non-editing state, the current
// location is displayed.
@interface LocationBarViewController
    : UIViewController <FullscreenUIElement, LocationBarAnimatee>

// Sets the edit view to use in the editing state. This must be set before the
// view of this view controller is initialized. This must only be called once.
- (void)setEditView:(UIView*)editView;

// Sets the badge view to display badges. This must be set before the
// view of this view controller is initialized. This must only be called once.
- (void)setBadgeView:(UIView*)badgeView;

@property(nonatomic, assign) BOOL incognito;

// The dispatcher for the share button, voice search, and long press actions.
@property(nonatomic, weak) id<ActivityServiceCommands,
                              ApplicationCommands,
                              BrowserCommands,
                              InfobarCommands,
                              LoadQueryCommands>
    dispatcher;

// Delegate for this location bar view controller.
@property(nonatomic, weak) id<LocationBarViewControllerDelegate> delegate;

// The offset provider for the edit/steady transition animation.
@property(nonatomic, weak) id<LocationBarOffsetProvider> offsetProvider;

// Switches between the two states of the location bar:
// - editing state, with the textfield;
// - non-editing state, with location icon and text.
- (void)switchToEditing:(BOOL)editing;

// Updates the location icon to become |icon| and use the new |statusText| for
// a11y labeling.
- (void)updateLocationIcon:(UIImage*)icon
        securityStatusText:(NSString*)statusText;
// Updates the location text in the non-editing mode.
// |clipTail| indicates whether the tail or the head should be clipped when the
// location text is too long.
- (void)updateLocationText:(NSString*)text clipTail:(BOOL)clipTail;
// Updates the location view to show a fake placeholder in the steady location
// view and hides the trailing button if |isNTP|. Otherwise, shows the
// location text and the button as normal.
- (void)updateForNTP:(BOOL)isNTP;
// Sets |enabled| of the share button.
- (void)setShareButtonEnabled:(BOOL)enabled;

// Displays the voice search button instead of the share button in steady state,
// and adds the voice search button to the empty textfield.
@property(nonatomic, assign) BOOL voiceSearchEnabled;

// Whether the default search engine supports search-by-image. This controls the
// edit menu option to do an image search.
@property(nonatomic, assign) BOOL searchByImageEnabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_VIEW_CONTROLLER_H_
