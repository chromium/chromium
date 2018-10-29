// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_view_controller.h"

#include "base/logging.h"
#include "ios/chrome/browser/ui/util/animation_util.h"
#import "ios/chrome/common/material_timing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kAnimationDuration = ios::material::kDuration3;
// The vertical offset distance used in the sink-down animation.
const CGFloat kVerticalOffset = 8.0f;
}  // namespace

@interface BubbleViewController ()
@property(nonatomic, copy, readonly) NSString* text;
@property(nonatomic, assign, readonly) BubbleArrowDirection arrowDirection;
@property(nonatomic, assign, readonly) BubbleAlignment alignment;
@end

@implementation BubbleViewController
@synthesize text = _text;
@synthesize arrowDirection = _arrowDirection;
@synthesize alignment = _alignment;

- (instancetype)initWithText:(NSString*)text
              arrowDirection:(BubbleArrowDirection)direction
                   alignment:(BubbleAlignment)alignment {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _text = text;
    _arrowDirection = direction;
    _alignment = alignment;
  }
  return self;
}

- (void)loadView {
  self.view = [[BubbleView alloc] initWithText:self.text
                                arrowDirection:self.arrowDirection
                                     alignment:self.alignment];
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

  // Set the y-coordinate of |frame.origin| to its final value.
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

@end
