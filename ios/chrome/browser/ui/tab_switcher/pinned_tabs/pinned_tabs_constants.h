// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_PINNED_TABS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_PINNED_TABS_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Pinned view dimensions.
extern const CGFloat kPinnedViewDragEnabledHeight;
extern const CGFloat kPinnedViewDefaultHeight;
extern const CGFloat kPinnedViewCornerRadius;

// Pinned view constraints.
extern const CGFloat kPinnedViewHorizontalPadding;
extern const CGFloat kPinnedViewBottomPadding;
extern const CGFloat kPinnedViewTopPadding;

// Pinned view animations.
extern const NSTimeInterval kPinnedViewFadeInTime;
extern const NSTimeInterval kPinnedViewDragAnimationTime;
extern const NSTimeInterval kPinnedViewMoveAnimationTime;

// Pinned cell identifier.
extern NSString* const kPinnedCellIdentifier;

// Pinned cell dimensions.
extern const CGFloat kPinnedCelldHeight;
extern const CGFloat kPinnedCelldWidth;

// Pinned cell constraints.
extern const CGFloat kPinnedCellCornerRadius;
extern const CGFloat kPinnedCellHorizontalPadding;
extern const CGFloat kPinnedCellTitleLeadingPadding;
extern const CGFloat kPinnedCellFaviconWidth;
extern const CGFloat kPinnedCellFaviconContainerWidth;
extern const CGFloat kPinnedCellFaviconBorderWidth;
extern const CGFloat kPinnedCellFaviconContainerCornerRadius;
extern const CGFloat kPinnedCellFaviconCornerRadius;

// Pinned cell collection view layout constraints.
extern const CGFloat kPinnedCellVerticalLayoutInsets;
extern const CGFloat kPinnedCellHorizontalLayoutInsets;

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_PINNED_TABS_CONSTANTS_H_
