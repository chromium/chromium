// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller.h"

#import "base/notreached.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/animation_util.h"
#import "ios/chrome/common/material_timing.h"

namespace {
// The vertical offset distance used in the sink-down animation.
const CGFloat kVerticalOffset = 8.0f;

BubbleView* BubbleViewWithType(BubbleViewType bubble_view_type,
                               NSString* text,
                               NSString* title,
                               UIImage* image,
                               BubbleArrowDirection arrow_direction,
                               BubbleAlignment alignment,
                               id<BubbleViewDelegate> delegate) {
  BOOL show_title = NO;
  BOOL show_image = NO;
  BOOL show_close_button = NO;
  BOOL show_snooze_button = NO;
  NSTextAlignment text_alignment = NSTextAlignmentNatural;

  switch (bubble_view_type) {
    case BubbleViewTypeDefault:
      text_alignment = NSTextAlignmentCenter;
      break;
    case BubbleViewTypeWithClose:
      show_close_button = YES;
      break;
    case BubbleViewTypeRich:
      show_title = YES;
      show_image = !IsRichBubbleWithoutImageEnabled();
      break;
    case BubbleViewTypeRichWithSnooze:
      show_title = YES;
      show_image = !IsRichBubbleWithoutImageEnabled();
      show_snooze_button = YES;
      break;
  }
  BubbleView* bubble_view =
      [[BubbleView alloc] initWithText:text
                        arrowDirection:arrow_direction
                             alignment:alignment
                      showsCloseButton:show_close_button
                                 title:show_title ? title : nil
                                 image:show_image ? image : nil
                     showsSnoozeButton:show_snooze_button
                         textAlignment:text_alignment
                              delegate:delegate];
  return bubble_view;
}

}  // namespace

@interface BubbleViewController ()
@property(nonatomic, copy, readonly) NSString* text;
@property(nonatomic, strong, readonly) UIImage* image;
@property(nonatomic, assign, readonly) BubbleArrowDirection arrowDirection;
@property(nonatomic, assign, readonly) BubbleAlignment alignment;
@property(nonatomic, weak) id<BubbleViewDelegate> delegate;
@property(nonatomic, assign, readonly) BubbleViewType bubbleViewType;
@property(nonatomic, strong) BubbleView* view;
@end

@implementation BubbleViewController
@synthesize text = _text;
@synthesize arrowDirection = _arrowDirection;
@synthesize alignment = _alignment;
@dynamic view;

- (instancetype)initWithText:(NSString*)text
                       title:(NSString*)titleString
                       image:(UIImage*)image
              arrowDirection:(BubbleArrowDirection)direction
                   alignment:(BubbleAlignment)alignment
              bubbleViewType:(BubbleViewType)type
                    delegate:(id<BubbleViewDelegate>)delegate {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _text = text;
    _image = image;
    self.title = [titleString copy];
    _arrowDirection = direction;
    _alignment = alignment;
    _bubbleViewType = type;
    _delegate = delegate;
  }
  return self;
}

- (void)loadView {
  self.view =
      BubbleViewWithType(self.bubbleViewType, self.text, self.title, self.image,
                         self.arrowDirection, self.alignment, self.delegate);
  // Begin hidden.
  [self.view setAlpha:0.0f];
  [self.view setHidden:YES];
}

// Animate the bubble view in with a fade-in and sink-down animation.
- (void)animateContentIn {
  // Set the frame's origin to be slightly higher on the screen, so that the
  // view will be properly positioned once it sinks down.
  CGRect frame = self.view.frame;
  frame.origin.y = frame.origin.y - kVerticalOffset;
  [self.view setFrame:frame];
  [self.view setHidden:NO];

  // Set the y-coordinate of `frame.origin` to its final value.
  frame.origin.y = frame.origin.y + kVerticalOffset;
  [UIView animateWithDuration:kMaterialDuration3
                        delay:0.0
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:^{
                     [self.view setFrame:frame];
                     [self.view setAlpha:1.0f];
                   }
                   completion:nil];
}

- (void)setArrowHidden:(BOOL)hidden animated:(BOOL)animated {
  [self.view setArrowHidden:hidden animated:animated];
}

- (void)dismissAnimated:(BOOL)animated {
  NSTimeInterval duration = (animated ? kMaterialDuration3 : 0.0);
  [UIView animateWithDuration:duration
      animations:^{
        [self.view setAlpha:0.0f];
      }
      completion:^(BOOL finished) {
        [self.view setHidden:YES];
        [self willMoveToParentViewController:nil];
        [self.view removeFromSuperview];
        [self removeFromParentViewController];
      }];
}

- (void)setBubbleAlignmentOffset:(CGFloat)alignmentOffset {
  self.view.alignmentOffset = alignmentOffset;
}

@end
