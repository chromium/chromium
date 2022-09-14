// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_view_controller.h"

#import "base/notreached.h"
#import "ios/chrome/browser/ui/util/animation_util.h"
#import "ios/chrome/common/material_timing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kAnimationDuration = kMaterialDuration3;
// The vertical offset distance used in the sink-down animation.
const CGFloat kVerticalOffset = 8.0f;

BubbleView* BubbleViewWithType(BubbleViewType bubbleViewType,
                               NSString* text,
                               NSString* title,
                               UIImage* image,
                               BubbleArrowDirection arrowDirection,
                               BubbleAlignment alignment,
                               id<BubbleViewDelegate> delegate) {
  BOOL showTitle = NO;
  BOOL showImage = NO;
  BOOL showCloseButton = NO;
  BOOL showSnoozeButton = NO;
  NSTextAlignment textAlignment = NSTextAlignmentNatural;

  switch (bubbleViewType) {
    case BubbleViewTypeDefault:
      textAlignment = NSTextAlignmentCenter;
      break;
    case BubbleViewTypeWithClose:
      showCloseButton = YES;
      break;
    case BubbleViewTypeRich:
      showCloseButton = YES;
      showTitle = YES;
      showImage = YES;
      break;
    case BubbleViewTypeRichWithSnooze:
      showCloseButton = YES;
      showTitle = YES;
      showImage = YES;
      showSnoozeButton = YES;
      break;
  }
  BubbleView* bubbleView =
      [[BubbleView alloc] initWithText:text
                        arrowDirection:arrowDirection
                             alignment:alignment
                      showsCloseButton:showCloseButton
                                 title:showTitle ? title : nil
                                 image:showImage ? image : nil
                     showsSnoozeButton:showSnoozeButton
                         textAlignment:textAlignment
                              delegate:delegate];
  return bubbleView;
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
  [UIView animateWithDuration:kAnimationDuration
                        delay:0.0
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:^{
                     [self.view setFrame:frame];
                     [self.view setAlpha:1.0f];
                   }
                   completion:nil];
}

- (void)dismissAnimated:(BOOL)animated {
  NSTimeInterval duration = (animated ? kAnimationDuration : 0.0);
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
