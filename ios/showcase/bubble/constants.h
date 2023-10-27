// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_BUBBLE_CONSTANTS_H_
#define IOS_SHOWCASE_BUBBLE_CONSTANTS_H_

// Text of label that will appear when the side swipe bubble's "Dismiss" button
// is tapped.
inline constexpr NSString* const kSideSwipeBubbleViewDismissedByTapText =
    @"IPH dismissed because close button is pressed";

// Text of label that will appear when the side swipe bubble times out.
inline constexpr NSString* const kSideSwipeBubbleViewTimeoutText =
    @"IPH dismissed due to time out";

#endif  // IOS_SHOWCASE_BUBBLE_CONSTANTS_H_
