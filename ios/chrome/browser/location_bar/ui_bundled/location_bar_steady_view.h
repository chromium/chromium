// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_STEADY_VIEW_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_STEADY_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/elements/custom_highlight_button.h"

@protocol BadgeViewVisibilityDelegate;
@protocol ContextualPanelEntrypointVisibilityDelegate;
@class LocationBarBadgesContainerView;

// A color scheme used for the steady view elements.
@interface LocationBarSteadyViewColorScheme : NSObject

// The color of the location label and the location icon.
@property(nonatomic, strong) UIColor* fontColor;
// The color of the placeholder string.
@property(nonatomic, strong) UIColor* placeholderColor;
// The tint color of the trailing button.
@property(nonatomic, strong) UIColor* trailingButtonColor;

+ (LocationBarSteadyViewColorScheme*)standardScheme;

@end

// A simple view displaying the current URL and security status icon.
@interface LocationBarSteadyView : UIView

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Sets the location image. If `locationImage` is nil, hides the image view.
- (void)setLocationImage:(UIImage*)locationImage;

// Sets the location label's text.
- (void)setLocationLabelText:(NSString*)string;

// Sets the location label's text and styles it as if it were placeholder text.
- (void)setLocationLabelPlaceholderText:(NSString*)string;

// Reorients the badgeView's position depending on FullScreen mode.
- (void)setFullScreenCollapsedMode:(BOOL)isFullScreenCollapsed;

// Toggles `enabled` state of the trailing button and updates accessibility
// appropriately.
- (void)enableTrailingButton:(BOOL)enabled;

// Sets whether the contents are centered or aligned to the leading side.
- (void)setCentered:(BOOL)centered;

// Sets the location label of the location bar centered relative to the content
// around it when centered is passed as YES. Otherwise, resets it to the
// "absolute" center. This is called as part of an animation (therefore
// `layoutIfNeeded` is called) for the Contextual Panel entrypoint when an
// infoblock returns some high-confidence data, which makes the entrypoint
// display a label, momentarily using significant portion of the location bar.
- (void)setLocationBarLabelCenteredBetweenContent:(BOOL)centered;

// Sets the view displaying badges in the leading corner of the view.
- (void)setBadgeView:(UIView*)badgeView;

// Sets the view displaying the Contextual Panel's entrypoint.
- (void)setContextualPanelEntrypointView:(UIView*)contextualPanelEntrypointView;

// Returns the contextual panel entrypoint visibility delegate;
- (id<ContextualPanelEntrypointVisibilityDelegate>)
    contextualEntrypointVisibilityDelegate;

// Returns the badge view visibility delegate.
- (id<BadgeViewVisibilityDelegate>)badgeViewVisibilityDelegate;

// Set the placeholder view when there is no badge to display.
- (void)setPlaceholderView:(UIView*)placeholderView;

// The tappable button representing the location bar.
@property(nonatomic, strong) UIButton* locationButton;
// The label displaying the current location URL.
@property(nonatomic, strong) UILabel* locationLabel;
// The button displayed in the trailing corner of the view, i.e. share button.
@property(nonatomic, strong) CustomHighlightableButton* trailingButton;
// The string that describes the current security level. Used for a11y.
@property(nonatomic, copy) NSString* securityLevelAccessibilityString;
// Current in-use color scheme.
@property(nonatomic, strong) LocationBarSteadyViewColorScheme* colorScheme;
// The view containing the infobar badge and contextual panel entrypoint.
@property(nonatomic, strong)
    LocationBarBadgesContainerView* badgesContainerView;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_STEADY_VIEW_H_
