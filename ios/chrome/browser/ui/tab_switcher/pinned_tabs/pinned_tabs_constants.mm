// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/pinned_tabs/pinned_tabs_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Pinned view dimensions.
const CGFloat kPinnedViewDragEnabledHeight = 94.0f;
const CGFloat kPinnedViewDefaultHeight = 68.0f;
const CGFloat kPinnedViewCornerRadius = 15.0f;

// Pinned view constraints.
const CGFloat kPinnedViewHorizontalPadding = 6.0f;
const CGFloat kPinnedViewBottomPadding = 8.0f;
const CGFloat kPinnedViewTopPadding = 24.0f;

// Pinned view animations.
const NSTimeInterval kPinnedViewFadeInTime = 0.2;
const NSTimeInterval kPinnedViewDragAnimationTime = 0.2;

// Pinned cell identifier.
NSString* const kPinnedCellIdentifier = @"PinnedCellIdentifier";

// Pinned cell dimensions.
const CGFloat kPinnedCelldHeight = 36.0f;
const CGFloat kPinnedCelldWidth = 168.0f;

// Pinned cell constraints.
const CGFloat kPinnedCellCornerRadius = 15.0f;
const CGFloat kPinnedCellHorizontalPadding = 8.0f;
const CGFloat kPinnedCellTitleLeadingPadding = 4.0f;

// Pinned cell collection view layout constraints.
const CGFloat kPinnedCellVerticalLayoutInsets = 16.0f;
const CGFloat kPinnedCellHorizontalLayoutInsets = 8.0f;
