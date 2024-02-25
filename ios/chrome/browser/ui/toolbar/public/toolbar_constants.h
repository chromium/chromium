// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_TOOLBAR_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_TOOLBAR_CONSTANTS_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

extern const NSUInteger kControlStateSpotlighted;

// All kxxxColor constants are RGB values stored in a Hex integer. These will be
// converted into UIColors using the UIColorFromRGB() function, from
// uikit_ui_util.h

// Toolbar styling.
extern const CGFloat kToolbarBackgroundColor;
extern const CGFloat kIncognitoToolbarBackgroundColor;
// The brightness of the toolbar's background color (visible on NTPs when the
// background view is hidden).
extern const CGFloat kNTPBackgroundColorBrightnessIncognito;

// Stackview constraints.
extern const CGFloat kTopButtonsBottomMargin;
extern const CGFloat kBottomButtonsTopMargin;
extern const CGFloat kAdaptiveToolbarMargin;
extern const CGFloat kAdaptiveToolbarStackViewSpacing;

// Progress Bar Height.
extern const CGFloat kProgressBarHeight;

// Separator.
// Height of the separator. Should be aligned to upper pixel.
extern const CGFloat kToolbarSeparatorHeight;

// Toolbar Buttons.
extern const CGFloat kAdaptiveToolbarButtonHeight;
extern const CGFloat kAdaptiveToolbarButtonWidth;
extern const CGFloat kSearchButtonWidth;
extern const CGFloat kCancelButtonHorizontalInset;

// Background color of the blur view.
extern const CGFloat kBlurBackgroundGrayscaleComponent;
extern const CGFloat kBlurBackgroundAlpha;

// Alpha for the tint color of the buttons.
extern const CGFloat kToolbarButtonTintColorAlpha;
// Alpha for the tint color of the buttons when in the highlighted state.
extern const CGFloat kToolbarButtonTintColorAlphaHighlighted;
extern const CGFloat kIncognitoToolbarButtonTintColorAlphaHighlighted;
// Alpha for the spotlight view's background, when the toolbar is dimmed or not.
extern const CGFloat kToolbarSpotlightAlpha;
extern const CGFloat kDimmedToolbarSpotlightAlpha;

// Adaptive toolbar position constants.
extern const CGFloat kExpandedLocationBarHorizontalMargin;
extern const CGFloat kContractedLocationBarHorizontalMargin;

// Top adaptive Location bar constants.
extern const CGFloat kAdaptiveLocationBarVerticalMargin;
extern const CGFloat kAdaptiveLocationBarVerticalMarginFullscreen;

// Bottom adaptive location bar constants.
extern const CGFloat kBottomAdaptiveLocationBarTopMargin;
extern const CGFloat kBottomAdaptiveLocationBarBottomMargin;
extern const CGFloat kBottomAdaptiveLocationBarVerticalMarginFullscreen;

// Additional margin, which should grow only when the preferred content size is
// non-default.
extern const CGFloat kLocationBarVerticalMarginDynamicType;

// Top margin of the top toolbar when the adaptive toolbar is unsplit.
extern const CGFloat kTopToolbarUnsplitMargin;
// Height of the omnibox in the toolbar. Used for both toolbars.
extern const CGFloat kToolbarOmniboxHeight;
// Height of the primary toolbar with omnibox with default font size.
extern const CGFloat kPrimaryToolbarWithOmniboxHeight;
// Height of the secondary toolbar without the omnibox with default font size.
extern const CGFloat kSecondaryToolbarWithoutOmniboxHeight;
// Height of the part of the toolbar not scaling up when the user changes the
// preferred font size.
extern const CGFloat kNonDynamicToolbarHeight;
// Height of the toolbar when in fullscreen.
extern const CGFloat kToolbarHeightFullscreen;
// Height of the part of the toolbar not scaling up when the user changes the
// preferred font size.
extern const CGFloat kNonDynamicToolbarHeightFullscreen;

// Accessibility identifier of the tools menu button.
extern NSString* const kToolbarToolsMenuButtonIdentifier;
// Accessibility identifier of the stack button.
extern NSString* const kToolbarStackButtonIdentifier;
// Accessibility identifier of the share button.
extern NSString* const kToolbarShareButtonIdentifier;
// Accessibility identifier of the NewTab button.
extern NSString* const kToolbarNewTabButtonIdentifier;
// Accessibility identifier of the cancel omnibox edit button.
extern NSString* const kToolbarCancelOmniboxEditButtonIdentifier;

// Font size for the TabGrid button containing the tab count.
extern const NSInteger kTabGridButtonFontSize;

// Tint color for location bar and omnibox.
extern const CGFloat kLocationBarTintBlue;

// Font sizes used in omnibox and location bar.
extern const CGFloat kLocationBarFontSize;

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_TOOLBAR_CONSTANTS_H_
