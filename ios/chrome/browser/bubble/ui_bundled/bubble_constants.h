// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility identifier for the close button.
extern NSString* const kBubbleViewCloseButtonIdentifier;
// Accessibility identifier for the title label.
extern NSString* const kBubbleViewTitleLabelIdentifier;
// Accessibility identifier for the label.
extern NSString* const kBubbleViewLabelIdentifier;
// Accessibility identifier for the image view.
extern NSString* const kBubbleViewImageViewIdentifier;
// Accessibility identifier for the snooze button.
extern NSString* const kBubbleViewSnoozeButtonIdentifier;
// Accessibility identifier for the arrow view.
extern NSString* const kBubbleViewArrowViewIdentifier;
// How long, in seconds, the bubble is visible on the screen.
extern NSTimeInterval const kBubbleVisibilityDuration;
// How long, in seconds, the default "long duration" bubbles are visible.
extern NSTimeInterval const kDefaultLongDurationBubbleVisibility;

// Metric name for bubble dismissal tracking.
extern const char kUMAIPHDismissalReason[];

// Metric name for gestural bubble dismissal tracking.
extern const char kUMAGesturalIPHDismissalReason[];

// Direction for the bubble to point.
typedef NS_ENUM(NSInteger, BubbleArrowDirection) {
  // Bubble is below the target UI element and the arrow is pointing up.
  BubbleArrowDirectionUp,
  // Bubble is above the target UI element and the arrow is pointing down.
  BubbleArrowDirectionDown,
  // Bubble is on the trailing side of the target UI element and the arrow is
  // pointing to the leadng edge.
  BubbleArrowDirectionLeading,
  // Bubble is on the leading side of the target UI element and the arrow is
  // pointing to the trailing edge.
  BubbleArrowDirectionTrailing,
};

// Alignment of the bubble's arrow relative to the rest of the bubble.
typedef NS_ENUM(NSInteger, BubbleAlignment) {
  // When bubble arrow direction is up or down, arrow is aligned to the leading
  // edge of the bubble; if arrow direction is leading or trailing, arrow is
  // aligned to the top edge of the bubble.
  BubbleAlignmentTopOrLeading,
  // Arrow is center aligned on the bubble.
  BubbleAlignmentCenter,
  // When bubble arrow direction is up or down, arrow is aligned to the trailing
  // edge of the bubble; if arrow direction is leading or trailing, arrow is
  // aligned to the bottom edge of the bubble.
  BubbleAlignmentBottomOrTrailing,
};

// Type of bubble views. BubbleViewTypeDefault uses sizeThatFits for its size,
// the other types use the full screen width with a maximum limit size.
typedef NS_ENUM(NSInteger, BubbleViewType) {
  // Bubble view with text.
  BubbleViewTypeDefault,
  // Bubble view with text and close button.
  BubbleViewTypeWithClose,
  // Bubble view with title, text and image.
  BubbleViewTypeRich,
  // Bubble view with title, text, image and snooze button.
  BubbleViewTypeRichWithSnooze,
};

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
  kVoiceOverAnnouncementEnded = 9,
  kSwipedAsInstructedByGestureIPH = 10,
  kMaxValue = kSwipedAsInstructedByGestureIPH,
};


#endif  // IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_CONSTANTS_H_
