// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_consumer.h"
#import "ios/chrome/browser/orchestrator/ui_bundled/location_bar_animatee.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"

@class LayoutGuideCenter;
@protocol ActivityServiceCommands;
@protocol ApplicationCommands;
@protocol BadgeViewVisibilityDelegate;
@protocol ContextualPanelEntrypointVisibilityDelegate;
@protocol HelpCommands;
@protocol LensOverlayCommands;
@protocol LocationBarOffsetProvider;
@protocol LoadQueryCommands;
@protocol TextFieldViewContaining;
namespace feature_engagement {
class Tracker;
}

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

// Notifies the delegate about a tap on the Search Copied Text context menu
// action.
- (void)locationBarSearchCopiedTextTapped;

// Starts a reverse image search for the image currently in the pasteboard.
- (void)searchCopiedImage;

// Starts a Lens search for the image currently in the pasteboard.
- (void)lensCopiedImage;

// Displays/hides the contextual panel entrypoint view. If `display` is NO, the
// view will always be hidden. However, if it is YES, it will only be unhidden
// if it should currently be displayed.
- (void)displayContextualPanelEntrypointView:(BOOL)display;

@end

// The view controller displaying the location bar. Manages the two states of
// the omnibox - the editing and the non-editing states. In the editing state,
// the omnibox textfield is displayed; in the non-editing state, the current
// location is displayed.
@interface LocationBarViewController : UIViewController <FullscreenUIElement,
                                                         LocationBarAnimatee,
                                                         LocationBarConsumer>

@property(nonatomic, assign) BOOL incognito;

// The dispatcher for the share button, voice search, and long press actions.
@property(nonatomic, weak) id<ActivityServiceCommands,
                              ApplicationCommands,
                              LoadQueryCommands,
                              LensOverlayCommands,
                              OmniboxCommands>
    dispatcher;

// Delegate for this location bar view controller.
@property(nonatomic, weak) id<LocationBarViewControllerDelegate> delegate;

// The offset provider for the edit/steady transition animation.
@property(nonatomic, weak) id<LocationBarOffsetProvider> offsetProvider;

// The layout guide center to use to refer to the first suggestion label.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Feature engagement tracker.
@property(nonatomic, assign) feature_engagement::Tracker* tracker;

// Displays the voice search button instead of the share button in steady state,
// and adds the voice search button to the empty textfield.
@property(nonatomic, assign) BOOL voiceSearchEnabled;

// The help command handler.
@property(nonatomic, weak) id<HelpCommands> helpCommandsHandler;

// Sets the edit view to use in the editing state. This must be set before the
// view of this view controller is initialized. This must only be called once.
- (void)setEditView:(UIView<TextFieldViewContaining>*)editView;

// Sets the badge view to display badges. This must be set before the
// view of this view controller is initialized. This must only be called once.
- (void)setBadgeView:(UIView*)badgeView;

// Sets the Contextual Panel Entrypoint view to display different entrypoint
// UIs. This must be called only once and set before the view of this view
// controller is initialized.
- (void)setContextualPanelEntrypointView:(UIView*)contextualPanelEntrypointView;

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

// Whether the location bar is currently in a state where the large Contextual
// Panel entrypoint can be shown (i.e. it is not hidden).
- (BOOL)canShowLargeContextualPanelEntrypoint;

// Sets the location label of the location bar centered relative to the content
// around it when centered is passed as YES. Otherwise, resets it to the
// "absolute" center.
- (void)setLocationBarLabelCenteredBetweenContent:(BOOL)centered;

// Returns the contextual panel entrypoint visibility delegate.
- (id<ContextualPanelEntrypointVisibilityDelegate>)
    contextualEntrypointVisibilityDelegate;

// Returns the badge view visibility delegate.
- (id<BadgeViewVisibilityDelegate>)badgeViewVisibilityDelegate;

// Attempts to show the lens overlay IPH.
- (void)attemptShowingLensOverlayIPH;

// Records the lens overlay entrypoint availability in the location bar.
- (void)recordLensOverlayAvailability;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_VIEW_CONTROLLER_H_
