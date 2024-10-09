// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_COLORS_SEMANTIC_COLOR_NAMES_H_
#define IOS_CHROME_COMMON_UI_COLORS_SEMANTIC_COLOR_NAMES_H_

#import <UIKit/UIKit.h>

// **************
// Element Colors
// **************

extern NSString* const kBackgroundColor;
extern NSString* const kCloseButtonColor;
extern NSString* const kDisabledTintColor;
// Background color used in the rounded squares behind favicons.
extern NSString* const kFaviconBackgroundColor;
// Primary grouped background color.
extern NSString* const kGroupedPrimaryBackgroundColor;
// Secondary grouped background color.
extern NSString* const kGroupedSecondaryBackgroundColor;
// Background shadow's color.
extern NSString* const kBackgroundShadowColor;
// Ink color for an MDC button.
extern NSString* const kMDCInkColor;
// Ink color for a secondary style MDC button (button with transparent
// background).
extern NSString* const kMDCSecondaryInkColor;
// Color used to tint placeholder images and icons.
extern NSString* const kPlaceholderImageTintColor;
// Primary background color.
extern NSString* const kPrimaryBackgroundColor;
extern NSString* const kScrimBackgroundColor;
extern NSString* const kDarkerScrimBackgroundColor;
// Secondary background color.
extern NSString* const kSecondaryBackgroundColor;
extern NSString* const kSeparatorColor;
extern NSString* const kSolidButtonTextColor;
extern NSString* const kTableViewRowHighlightColor;
extern NSString* const kTertiaryBackgroundColor;
extern NSString* const kUpdatedTertiaryBackgroundColor;
extern NSString* const kTextPrimaryColor;
// Primary text color when the background color is inverted.
extern NSString* const kInvertedTextPrimaryColor;
extern NSString* const kTextSecondaryColor;
// Secondary text color when the background color is inverted.
extern NSString* const kInvertedTextSecondaryColor;
extern NSString* const kTextTertiaryColor;
extern NSString* const kTextQuaternaryColor;
extern NSString* const kTextLightTertiaryDarkPrimaryColor;
extern NSString* const kTextfieldBackgroundColor;
extern NSString* const kTextfieldFocusedBackgroundColor;
extern NSString* const kTextfieldHighlightBackgroundColor;
extern NSString* const kTextfieldPlaceholderColor;
// Color used for buttons on a toolbar.
extern NSString* const kToolbarButtonColor;
// Color used for a shadow/separator next to a toolbar.
extern NSString* const kToolbarShadowColor;
// Background color for omnibox keyboard buttons.
extern NSString* const kOmniboxKeyboardButtonColor;
// Color for separator between omnibox suggestions.
extern NSString* const kOmniboxSuggestionRowSeparatorColor;
// Color for separator between omnibox popout suggestions.
extern NSString* const kOmniboxPopoutSuggestionRowSeparatorColor;
// Background colorfor the tab group favicon views.
extern NSString* const kTabGroupFaviconBackgroundColor;
// Color for the omnibox popout overlay.
extern NSString* const kOmniboxPopoutOverlayColor;
// Color of the Tab Strip.
extern NSString* const kTabStripBackgroundColor;
extern NSString* const kTabStripV3BackgroundColor;
// Color of the symbol of the Tab Strip new tab button.
extern NSString* const kTabStripNewTabButtonColor;
// Colors for the TabGroup.
extern NSString* const kTabGroupPinkColor;
extern NSString* const kTabGroupCyanColor;
extern NSString* const kTabGroupPurpleColor;
extern NSString* const kTabGroupGreenColor;
extern NSString* const kTabGroupGreyColor;
// A semi transparent background color that ensures symbols are visible in
// light and dark modes.
extern NSString* const kWhiteBlackAlpha50Color;
// Lens overlay consent dialog description text color.
extern NSString* const kLensOverlayConsentDialogDescriptionColor;
extern NSString* const kLensOverlayConsentDialogAnimationPlayerButtonColor;
// ***************
// Standard Colors
// ***************

// Black/White and White/Black colors for light/dark styles.
extern NSString* const kSolidBlackColor;
extern NSString* const kSolidWhiteColor;

// Standard blue color. This is most commonly used for the tint color on
// standard buttons and controls.
extern NSString* const kBlueColor;
// Lighter blue color sometimes used as background for buttons or views where
// the main content is `kBlueColor` (e.g the background of the collections
// shortcuts on the NTP).
extern NSString* const kBlueHaloColor;

// Blue palette.
extern NSString* const kBlue100Color;
extern NSString* const kBlue300Color;
extern NSString* const kBlue400Color;
extern NSString* const kBlue500Color;
extern NSString* const kBlue600Color;
extern NSString* const kBlue700Color;
// Static blue palette (same color for light and dark modes).
extern NSString* const kStaticBlueColor;
extern NSString* const kStaticBlue400Color;

// Standard green color.
extern NSString* const kGreenColor;

// Green palette.
extern NSString* const kGreen100Color;
extern NSString* const kGreen300Color;
extern NSString* const kGreen400Color;
extern NSString* const kGreen500Color;
extern NSString* const kGreen600Color;
extern NSString* const kGreen700Color;
extern NSString* const kGreen800Color;
// Static Green palette (same color for light and dark modes).
extern NSString* const kStaticGreen50Color;
extern NSString* const kStaticGreen700Color;

// Standard red color. This is most commonly used for the tint color on
// destructive controls.
extern NSString* const kRedColor;

// Red palette
extern NSString* const kRed50Color;
extern NSString* const kRed100Color;
extern NSString* const kRed300Color;
extern NSString* const kRed400Color;
extern NSString* const kRed500Color;
extern NSString* const kRed600Color;

// Pink palette.
extern NSString* const kPink400Color;
extern NSString* const kPink500Color;
extern NSString* const kPink600Color;
extern NSString* const kPink700Color;

// Purple palette.
extern NSString* const kPurple500Color;
extern NSString* const kPurple600Color;

// Yellow palette.
extern NSString* const kYellow500Color;
extern NSString* const kYellow600Color;

// Orange palette.
extern NSString* const kOrange500Color;
extern NSString* const kOrange600Color;

// Cyan palette.
extern NSString* const kCyan600Color;
extern NSString* const kCyan700Color;

// Grey Color Palette.
extern NSString* const kGrey50Color;
extern NSString* const kGrey100Color;
extern NSString* const kGrey200Color;
extern NSString* const kGrey300Color;
extern NSString* const kGrey400Color;
extern NSString* const kGrey500Color;
extern NSString* const kGrey600Color;
extern NSString* const kGrey700Color;
extern NSString* const kGrey800Color;
extern NSString* const kGrey900Color;
// Static Grey palette (same color for light and dark modes).
extern NSString* const kStaticGrey300Color;
extern NSString* const kStaticGrey600Color;
extern NSString* const kStaticGrey900Color;

// **********************
// Light Mode only colors (alpha = 0 in dark mode)
// **********************

// Grey palette
extern NSString* const kLightOnlyGrey200Color;

#endif  // IOS_CHROME_COMMON_UI_COLORS_SEMANTIC_COLOR_NAMES_H_
