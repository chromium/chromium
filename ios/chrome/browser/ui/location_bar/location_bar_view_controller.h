// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/badges/badge_consumer.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"
#import "ios/chrome/browser/ui/orchestrator/location_bar_animatee.h"

@class InfobarMetricsRecorder;
@class LayoutGuideCenter;
@class OmniboxTextFieldIOS;
@protocol ActivityServiceCommands;
@protocol ApplicationCommands;
@protocol LocationBarOffsetProvider;
@protocol LoadQueryCommands;

@protocol LocationBarViewControllerDelegate<NSObject>

// Notifies the delegate about a tap on the steady-state location bar.
- (void)locationBarSteadyViewTapped;

// Notifies the delegate about a tap on the Copy entry in the editing menu.
- (void)locationBarCopyTapped;

// Returns the target that location bar scribble events should be forwarded to.
- (UIResponder<UITextInput>*)omniboxScribbleForwardingTarget;

// Request the scribble target to be focused.
- (void)locationBarRequestScribbleTargetFocus;

// Notifies the delegate about a tap on the share button to record metrics.
- (void)recordShareButtonPressed;

// Notifies the delegate about a tap on the Visit Copied Link context menu
// action.
- (void)locationBarVisitCopyLinkTapped;

// Starts a reverse image search for the image currently in the pasteboard.
- (void)searchCopiedImage;

// Starts a Lens search for the image currently in the pasteboard.
- (void)lensCopiedImage;

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
                              LoadQueryCommands,
                              OmniboxCommands>
    dispatcher;

// Delegate for this location bar view controller.
@property(nonatomic, weak) id<LocationBarViewControllerDelegate> delegate;

// The offset provider for the edit/steady transition animation.
@property(nonatomic, weak) id<LocationBarOffsetProvider> offsetProvider;

// The layout guide center to use to refer to the first suggestion label.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Switches between the two states of the location bar:
// - editing state, with the textfield;
// - non-editing state, with location icon and text.
- (void)switchToEditing:(BOOL)editing;

// Updates the location icon to become `icon` and use the new `statusText` for
// a11y labeling.
- (void)updateLocationIcon:(UIImage*)icon
        securityStatusText:(NSString*)statusText;
// Updates the location text in the non-editing mode.
// `clipTail` indicates whether the tail or the head should be clipped when the
// location text is too long.
- (void)updateLocationText:(NSString*)text clipTail:(BOOL)clipTail;
// Updates the location view to show a fake placeholder in the steady location
// view and hides the trailing button if `isNTP`. Otherwise, shows the
// location text and the button as normal.
- (void)updateForNTP:(BOOL)isNTP;
// Sets `enabled` of the share button.
- (void)setShareButtonEnabled:(BOOL)enabled;

// Displays the voice search button instead of the share button in steady state,
// and adds the voice search button to the empty textfield.
@property(nonatomic, assign) BOOL voiceSearchEnabled;

// Whether the default search engine supports search-by-image. This controls the
// edit menu option to do an image search.
@property(nonatomic, assign) BOOL searchByImageEnabled;

// Whether the default search engine supports Lensing images. This controls the
// edit menu option to do an image search.
@property(nonatomic, assign) BOOL lensImageEnabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_VIEW_CONTROLLER_H_
