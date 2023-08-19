// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_VIEW_H_

#import <UIKit/UIKit.h>

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
  // Bubble view with title, text, image and close button.
  BubbleViewTypeRich,
  // Bubble view with title, text, image, close button and snooze button.
  BubbleViewTypeRichWithSnooze,
};

// Delegate for actions happening in BubbleView.
@protocol BubbleViewDelegate <NSObject>

@optional

// User tapped on the close button.
- (void)didTapCloseButton;
// User tapped on the snooze button.
- (void)didTapSnoozeButton;

@end

// Speech bubble shaped view that displays a message.
@interface BubbleView : UIView

// Initialize with the given text, direction that the bubble should point,
// alignment of the bubble and optionals close button, title, image, snooze
// button, text alignment (for title, text and snooze button) and delegate.
- (instancetype)initWithText:(NSString*)text
              arrowDirection:(BubbleArrowDirection)direction
                   alignment:(BubbleAlignment)alignment
            showsCloseButton:(BOOL)shouldShowCloseButton
                       title:(NSString*)titleString
                       image:(UIImage*)image
           showsSnoozeButton:(BOOL)shouldShowSnoozeButton
               textAlignment:(NSTextAlignment)textAlignment
                    delegate:(id<BubbleViewDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

// Initialize with the given text, direction that the bubble should point, and
// alignment of the bubble. Optional arguments are set to nil. Text alignment is
// NSTextAlignmentCenter.
- (instancetype)initWithText:(NSString*)text
              arrowDirection:(BubbleArrowDirection)direction
                   alignment:(BubbleAlignment)alignment;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;

// Distance between the arrow's centerX and the (leading or trailing) edge of
// the bubble, depending on the BubbleAlignment. If BubbleAlignment is center,
// then `alignmentOffset` is ignored. `alignmentOffset` changes the minimum size
// of the bubble, thus might change the value of `sizeThatFits`.
@property(nonatomic) CGFloat alignmentOffset;

@end

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_VIEW_H_
