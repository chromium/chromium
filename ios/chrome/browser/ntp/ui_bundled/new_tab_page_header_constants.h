// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_CONSTANTS_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_CONSTANTS_H_

#import <CoreGraphics/CoreGraphics.h>

namespace ntp_header {

// The scroll distance within which to animate the search field from its
// initial frame to its final full bleed frame.
extern const CGFloat kAnimationDistance;

extern const CGFloat kFakeLocationBarTopConstraint;

extern const CGFloat kScrolledToTopOmniboxBottomMargin;

extern const CGFloat kCenteredHintLabelSidePadding;

extern const CGFloat kHintLabelHeightMargin;

// The margin to add to the fake omnibox to have it correctly positioned when
// the NTP is scrolled to the top.
extern const CGFloat kFakeOmniboxScrolledToTopMargin;

}  // namespace ntp_header

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_CONSTANTS_H_
