// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const NSUInteger kControlStateSpotlighted = 0x00010000;

const CGFloat kToolbarBackgroundColor = 0xF2F2F2;
const CGFloat kIncognitoToolbarBackgroundColor = 0x505050;
const CGFloat kNTPBackgroundColorBrightnessIncognito = 34.0 / 255.0;

const CGFloat kTopButtonsBottomMargin = 3.0f;
const CGFloat kBottomButtonsBottomMargin = 2.0f;
const CGFloat kAdaptiveToolbarMargin = 4.0f;
const CGFloat kAdaptiveToolbarStackViewSpacing = 11.0f;

const CGFloat kProgressBarHeight = 2.0f;

const CGFloat kToolbarSeparatorHeight = 0.1f;

const CGFloat kAdaptiveToolbarButtonHeight = 44.0f;
const CGFloat kAdaptiveToolbarButtonWidth = 44.0f;
const CGFloat kSearchButtonWidth = 70.0f;
const CGFloat kCancelButtonHorizontalInset = 8;
const CGFloat kToolbarDimmedButtonAlpha = 0.5;

const CGFloat kBlurBackgroundGrayscaleComponent = 0.98;
const CGFloat kBlurBackgroundAlpha = 0.4;

const CGFloat kToolbarButtonTintColorAlpha = 0.5;
const CGFloat kToolbarButtonTintColorAlphaHighlighted = 0.10;
const CGFloat kIncognitoToolbarButtonTintColorAlphaHighlighted = 0.21;
const CGFloat kToolbarSpotlightAlpha = 0.07;
const CGFloat kDimmedToolbarSpotlightAlpha = 0.14;

const CGFloat kExpandedLocationBarHorizontalMargin = 8;
const CGFloat kContractedLocationBarHorizontalMargin = 19;

const CGFloat kAdaptiveLocationBarBackgroundAlpha = 0.09;
const CGFloat kAdaptiveLocationBarBackgroundAlphaIncognito = 0.12;
const CGFloat kAdaptiveLocationBarVerticalMargin = 6.0f;
const CGFloat kAdaptiveLocationBarVerticalMarginFullscreen = 3.0f;
const CGFloat kLocationBarVerticalMarginDynamicType = -1.0f;
const CGFloat kAdaptiveLocationBarExtraVerticalMargin = 1.0f;

const CGFloat kTopToolbarUnsplitMargin = 2;
const CGFloat kAdaptiveToolbarHeight = 48;
const CGFloat kNonDynamicToolbarHeight = 14;
const CGFloat kToolbarHeightFullscreen = 20;
const CGFloat kNonDynamicToolbarHeightFullscreen = 3;

NSString* const kToolbarToolsMenuButtonIdentifier =
    @"kToolbarToolsMenuButtonIdentifier";
NSString* const kToolbarStackButtonIdentifier =
    @"kToolbarStackButtonIdentifier";
NSString* const kToolbarShareButtonIdentifier =
    @"kToolbarShareButtonIdentifier";
NSString* const kToolbarSearchButtonIdentifier =
    @"kToolbarSearchButtonIdentifier";
NSString* const kToolbarCancelOmniboxEditButtonIdentifier =
    @"kToolbarCancelOmniboxEditButtonIdentifier";

const NSInteger kTabGridButtonFontSize = 13;

const CGFloat kLocationBarTintBlue = 0x1A73E8;
const CGFloat kLocationBarFontSize = 15.0f;
