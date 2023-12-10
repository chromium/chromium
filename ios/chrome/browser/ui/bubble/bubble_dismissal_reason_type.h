// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_DISMISSAL_REASON_TYPE_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_DISMISSAL_REASON_TYPE_H_

#import "components/feature_engagement/public/tracker.h"

// Possible types of dismissal reasons.
// These enums are persisted as histogram entries, so this enum should be
// treated as append-only and kept in sync with InProductHelpDismissalReason in
// enums.xml.
enum class IPHDismissalReasonType {
  kUnknown = 0,
  kTimedOut = 1,
  kOnKeyboardHide = 2,
  kTappedIPH = 3,
  // kTappedOutside = 4 // Removed, split into kTappedOutsideIPHAndAnchorView
  // and kTappedAnchorView.
  kTappedClose = 5,
  kTappedSnooze = 6,
  kTappedOutsideIPHAndAnchorView = 7,
  kTappedAnchorView = 8,
  kMaxValue = kTappedAnchorView,
};

// Used for the bubble's dismissal callback.
using CallbackWithIPHDismissalReasonType =
    void (^)(IPHDismissalReasonType reason,
             feature_engagement::Tracker::SnoozeAction action);

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_DISMISSAL_REASON_TYPE_H_
