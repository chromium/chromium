// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_COLORS_SEMANTIC_COLOR_NAMES_H_
#define IOS_CHROME_COMMON_COLORS_SEMANTIC_COLOR_NAMES_H_

#import <UIKit/UIKit.h>

// Element Colors

extern NSString* const kBackgroundColor;
extern NSString* const kCloseButtonColor;
extern NSString* const kDisabledTintColor;
// Background color used in the rounded squares behind favicons.
extern NSString* const kFaviconBackgroundColor;
// Ink color for an MDC button.
extern NSString* const kMDCInkColor;
// Ink color for a secondary style MDC button (button with transparent
// background).
extern NSString* const kMDCSecondaryInkColor;
// Color used to tint placeholder images and icons.
extern NSString* const kPlaceholderImageTintColor;
extern NSString* const kScrimBackgroundColor;
extern NSString* const kSeparatorColor;
extern NSString* const kSolidButtonTextColor;
extern NSString* const kTableViewRowHighlightColor;
extern NSString* const kTextPrimaryColor;
extern NSString* const kTextSecondaryColor;
extern NSString* const kTextfieldBackgroundColor;
extern NSString* const kTextfieldPlaceholderColor;
// Color used for buttons on a toolbar.
extern NSString* const kToolbarButtonColor;
// Color used for a shadow/separator next to a toolbar.
extern NSString* const kToolbarShadowColor;

// Standard Colors

// Standard blue color. This is most commonly used for the tint color on
// standard buttons and controls.
extern NSString* const kBlueColor;
// Lighter blue color sometimes used as background for buttons or views where
// the main content is |kBlueColor| (e.g the background of the collections
// shortcuts on the NTP).
extern NSString* const kBlueHaloColor;
// Standard green color.
extern NSString* const kGreenColor;
// Standard red color. This is most commonly used for the tint color on
// destructive controls.
extern NSString* const kRedColor;

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

// Temporary colors for iOS 12. Because overridePreferredInterfaceStyle isn't
// available in iOS 12, any views that should always be dark (e.g. incognito)
// need to use colorsets that always use the dark variant.
// TODO(crbug.com/981889): Clean up after iOS 12 support is dropped.

extern NSString* const kBackgroundDarkColor;
extern NSString* const kCloseButtonDarkColor;
extern NSString* const kTableViewRowHighlightDarkColor;
extern NSString* const kTextPrimaryDarkColor;
extern NSString* const kTextSecondaryDarkColor;
extern NSString* const kTextfieldBackgroundDarkColor;
extern NSString* const kTextfieldPlaceholderDarkColor;
extern NSString* const kToolbarButtonDarkColor;

extern NSString* const kBlueDarkColor;
extern NSString* const kGreenDarkColor;
extern NSString* const kRedDarkColor;

#endif  // IOS_CHROME_COMMON_COLORS_SEMANTIC_COLOR_NAMES_H_
