// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_VIEW_H_
#define IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_VIEW_H_

#import <UIKit/UIKit.h>

typedef NS_ENUM(NSInteger, BubbleAlignment);
typedef NS_ENUM(NSInteger, BubbleArrowDirection);

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

// If `YES`, the arrow hides behind the bubble; otherwise, it is visible and
// pointing to the anchor point. If `animated`, the arrow will be slid out of /
// back in the bubble.
//
// NOTE: This should only be called when the view is in the view hierarchy.
- (void)setArrowHidden:(BOOL)hidden animated:(BOOL)animated;

// Read-only property to check arrow direction.
@property(nonatomic, assign, readonly) BubbleArrowDirection direction;

// Distance between the arrow's centerX and the (leading or trailing) edge of
// the bubble, depending on the BubbleAlignment. If BubbleAlignment is center,
// then `alignmentOffset` is ignored. `alignmentOffset` changes the minimum size
// of the bubble, thus might change the value of `sizeThatFits`.
@property(nonatomic) CGFloat alignmentOffset;

@end

#endif  // IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_VIEW_H_
