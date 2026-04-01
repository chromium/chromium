// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/public/ntp_home_constants.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace ntp_home {
NSString* FakeOmniboxAccessibilityID() {
  return @"NTPHomeFakeOmniboxAccessibilityID";
}

NSString* DiscoverHeaderTitleAccessibilityID() {
  return @"DiscoverHeaderTitleAccessibilityID";
}

NSString* NTPLogoAccessibilityID() {
  return @"NTPLogoAccessibilityID";
}

const CGFloat kMostVisitedBottomMarginIPad = 80;
const CGFloat kMostVisitedBottomMarginIPhone = 60;
const CGFloat kSuggestionPeekingHeight = 60;

const CGFloat kIdentityAvatarDimension = 32;
const CGFloat kHeaderIconMargin = 8;
const CGFloat kIdentityAvatarPadding = 8;
const CGFloat kSignedOutIdentityIconSize = 24;
const CGFloat kNTPMenuButtonIconSize = 17;
const CGFloat kNTPMenuButtonDimension = 37;
const CGFloat kNTPMenuButtonCornerRadius = 11;
const CGFloat kNTPMenuButtonLightUnthemedAlpha = 0.75;

UIColor* NTPBackgroundColor() {
  return [UIColor colorNamed:kBackgroundColor];
}

}  // namespace ntp_home
