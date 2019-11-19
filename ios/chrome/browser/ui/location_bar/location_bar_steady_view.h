// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_STEADY_VIEW_H_
#define IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_STEADY_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/infobars/badge/infobar_badge_button.h"

// A color scheme used for the steady view elements.
@interface LocationBarSteadyViewColorScheme : NSObject

// The color of the location label and the location icon.
@property(nonatomic, strong) UIColor* fontColor;
// The color of the placeholder string.
@property(nonatomic, strong) UIColor* placeholderColor;
// The tint color of the trailing button.
@property(nonatomic, strong) UIColor* trailingButtonColor;

+ (LocationBarSteadyViewColorScheme*)standardScheme;
+ (LocationBarSteadyViewColorScheme*)incognitoScheme;

@end

// A simple view displaying the current URL and security status icon.
@interface LocationBarSteadyView : UIView

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Sets the location image. If |locationImage| is nil, hides the image view.
- (void)setLocationImage:(UIImage*)locationImage;

// Sets the location label's text.
- (void)setLocationLabelText:(NSString*)string;

// Sets the location label's text and styles it as if it were placeholder text.
- (void)setLocationLabelPlaceholderText:(NSString*)string;

// Displays the location badge view if |display| is YES, hides it if
// |display| is NO. Will animate change if |animated| is YES.
- (void)displayBadgeView:(BOOL)display animated:(BOOL)animated;

// Reorients the badgeView's position depending on FullScreen mode.
- (void)setFullScreenCollapsedMode:(BOOL)isFullScreenCollapsed;

// Toggles |enabled| state of the trailing button and updates accessibility
// appropriately.
- (void)enableTrailingButton:(BOOL)enabled;

// The tappable button representing the location bar.
@property(nonatomic, strong) UIButton* locationButton;
// The label displaying the current location URL.
@property(nonatomic, strong) UILabel* locationLabel;
// The view displaying badges in the leading corner of the view.
// TODO(crbug.com/991241): Pass into init as parameter.
@property(nonatomic, strong) UIView* badgeView;
// The button displayed in the trailing corner of the view, i.e. share button.
@property(nonatomic, strong) UIButton* trailingButton;
// The string that describes the current security level. Used for a11y.
@property(nonatomic, copy) NSString* securityLevelAccessibilityString;
// Current in-use color scheme.
@property(nonatomic, strong) LocationBarSteadyViewColorScheme* colorScheme;

@end

#endif  // IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_STEADY_VIEW_H_
