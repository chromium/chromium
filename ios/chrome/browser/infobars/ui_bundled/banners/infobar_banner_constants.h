// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_BANNERS_INFOBAR_BANNER_CONSTANTS_H_
#define IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_BANNERS_INFOBAR_BANNER_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Accessibility identifier of the Banner View.
extern NSString* const kInfobarBannerViewIdentifier;
// Accessibility identifier of the Banner Labels Stack View.
extern NSString* const kInfobarBannerLabelsStackViewIdentifier;
// Accessibility identifier of the Banner Accept Button.
extern NSString* const kInfobarBannerAcceptButtonIdentifier;
// Accessibility identifier of the Banner Open Modal Button.
extern NSString* const kInfobarBannerOpenModalButtonIdentifier;

// Banner icon size.
extern const CGFloat kInfobarBannerIconSize;

// The presented view maximum height.
extern const CGFloat kInfobarBannerMaxHeight;

// Revamped banner icon size.
extern const CGFloat kInfobarBannerRevampIconSize;

// Revamped banner corner radius.
extern const CGFloat kInfobarBannerRevampCornerRadius;

// Revamped banner container shadow radius.
extern const CGFloat kInfobarBannerRevampContainerShadowRadius;
// Revamped banner container shadow opacity.
extern const CGFloat kInfobarBannerRevampContainerShadowOpacity;
// Revamped banner container shadow Y offset.
extern const CGFloat kInfobarBannerRevampContainerShadowYOffset;

// Revamped banner button shadow radius.
extern const CGFloat kInfobarBannerRevampButtonShadowRadius;
// Revamped banner button shadow opacity.
extern const CGFloat kInfobarBannerRevampButtonShadowOpacity;
// Revamped banner button shadow Y offset.
extern const CGFloat kInfobarBannerRevampButtonShadowYOffset;

// Revamped banner horizontal edge padding.
extern const CGFloat kInfobarBannerRevampHorizontalEdgePadding;

// Revamped banner vertical padding.
extern const CGFloat kInfobarBannerRevampVerticalPadding;

// Revamped banner spacing after icon.
extern const CGFloat kInfobarBannerRevampSpacingAfterIcon;

// Revamped banner button horizontal padding.
extern const CGFloat kInfobarBannerRevampButtonHorizontalPadding;

// Revamped banner gear button minimum safe gap.
extern const CGFloat kInfobarBannerRevampGearButtonMinSafeGap;
// Revamped banner gear button maximum safe gap.
extern const CGFloat kInfobarBannerRevampGearButtonMaxSafeGap;

// Revamped banner minimum tap target size.
extern const CGFloat kInfobarBannerRevampMinimumTapTargetSize;

// Revamped banner button maximum height.
extern const CGFloat kInfobarBannerRevampButtonMaxHeight;

// Revamped banner minimum button vertical breathing room.
extern const CGFloat kInfobarBannerRevampMinimumButtonVerticalBreathingRoom;

#endif  // IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_BANNERS_INFOBAR_BANNER_CONSTANTS_H_
