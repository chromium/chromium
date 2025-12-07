// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_constants.h"

const CGFloat kBadgeHeightMultiplier = 0.72;

const CGFloat kLabelTrailingSpaceMultiplier = 0.375;
const CGFloat kLabelLeadingSpaceMultiplier = 0.095;

const CGFloat kSeparatorHeightMultiplier = 0.35;
const CGFloat kSeparatorWidthConstant = 1;

const NSTimeInterval kBadgeDisplayingAnimationTime = 0.3;

const NSTimeInterval kBadgeContainerExpandAnimationTime = 0.2;
const NSTimeInterval kBadgeContainerCollapseAnimationTime = 0.3;

const float kBadgeContainerShadowOpacity = 0.09f;
const float kBadgeContainerShadowRadius = 5.0f;
const CGSize kBadgeContainerShadowOffset = {0, 3};

const CGFloat kBadgeSymbolPointSize = 15;

const CGFloat kUnifiedBadgeSymbolPointSize = 18;

NSString* const kLocationBarBadgeImageViewIdentifier =
    @"LocationBarBadgeImageViewAXID";

NSString* const kLocationBarBadgeLabelIdentifier = @"LocationBarBadgeLabelAXID";
