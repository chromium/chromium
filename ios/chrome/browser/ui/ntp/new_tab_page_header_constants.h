// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_HEADER_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_HEADER_CONSTANTS_H_

#import <CoreGraphics/CoreGraphics.h>

namespace ntp_header {

// The scroll distance within which to animate the search field from its
// initial frame to its final full bleed frame.
extern const CGFloat kAnimationDistance;

extern const CGFloat kFakeLocationBarTopConstraint;

extern const CGFloat kScrolledToTopOmniboxBottomMargin;

extern const CGFloat kHintLabelSidePadding;

extern const CGFloat kHintLabelHeightMargin;

// The margin added to the fake omnibox to have at the right position.
extern const CGFloat kMaxTopMarginDiff;

// The margin to add to the fake omnibox to have it correctly positioned when
// the NTP is scrolled to the top.
extern const CGFloat kFakeOmniboxScrolledToTopMargin;

}  // namespace ntp_header

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_HEADER_CONSTANTS_H_
