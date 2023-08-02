// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/number_badge_view.h"

#import <Foundation/Foundation.h>

#import "base/format_macros.h"
#import "ios/chrome/browser/ui/reading_list/text_badge_view.h"
#import "ios/chrome/common/material_timing.h"

namespace {

// The margin on all sides of the label.
const CGFloat kLabelMargin = 2.5f;

}  // namespace

@interface NumberBadgeView ()
@property(nonatomic, assign) NSInteger displayNumber;
// The pill-shaped badge that displays `displayNumber`.
@property(nonatomic, readonly, strong) TextBadgeView* textBadge;
// Indicate whether `textBadge` has been added as a subview of the
// `NumberBadgeView`.
@property(nonatomic, assign) BOOL didAddSubviews;
@end

@implementation NumberBadgeView

@synthesize displayNumber = _displayNumber;
@synthesize textBadge = _textBadge;
@synthesize didAddSubviews = _didAddSubviews;

#pragma mark - Lifecycle
- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _didAddSubviews = NO;
  }
  return self;
}

#pragma mark - Public methods

- (void)setNumber:(NSInteger)number animated:(BOOL)animated {
  // If the previous number and current number match, do nothing.
  if (self.displayNumber != number) {
    self.displayNumber = number;
    if (animated) {
      // If the view is being animated in, then `hidden` needs to be set to
      // `NO`. Otherwise the view is being animated out, in which case `hidden`
      // is already `NO`.
      self.hidden = NO;
      [UIView animateWithDuration:kMaterialDuration3
          animations:^{
            if (number > 0) {
              self.alpha = 1.0;
              // Only setting when > 0 as this makes the animation
              // look better than switching to 0 then fading out.
              [self.textBadge
                  setText:[NSString stringWithFormat:@"%" PRIdNS, number]];
            } else {
              self.alpha = 0.0;
            }
            [self setNeedsLayout];
            [self layoutIfNeeded];
          }
          completion:^(BOOL finished) {
            if (finished && self.alpha == 0.0) {
              self.hidden = YES;
            }
          }];
    } else {
      [self.textBadge setText:[NSString stringWithFormat:@"%" PRIdNS, number]];
      self.hidden = (number > 0 ? NO : YES);
      self.alpha = (number > 0 ? 1 : 0);
    }
  }
}

- (void)setBackgroundColor:(UIColor*)backgroundColor animated:(BOOL)animated {
  if (animated) {
    [UIView animateWithDuration:kMaterialDuration3
                     animations:^{
                       [self.textBadge setBackgroundColor:backgroundColor];
                     }];
  } else {
    [self.textBadge setBackgroundColor:backgroundColor];
  }
}

#pragma mark - UIView overrides

// Override `willMoveToSuperview` to add `textBadge` to the view hierarchy and
// perform additional setup operations.
- (void)willMoveToSuperview:(UIView*)newSuperview {
  if (!self.didAddSubviews) {
    [self addSubview:self.textBadge];
    self.didAddSubviews = YES;
    [self activateConstraints];
    self.backgroundColor = UIColor.clearColor;
    // Start hidden.
    self.alpha = 0.0;
    self.hidden = YES;
  }
  [super willMoveToSuperview:newSuperview];
}

#pragma mark - Private properties

// Lazily load `textBadge`.
- (TextBadgeView*)textBadge {
  if (!_textBadge) {
    _textBadge = [[TextBadgeView alloc] initWithText:@"0"
                               labelHorizontalMargin:kLabelMargin];
    [_textBadge setTranslatesAutoresizingMaskIntoConstraints:NO];
  }
  return _textBadge;
}

#pragma mark - Private methods

// Activate constraints to properly position `textBadge` within NumberBadgeView.
- (void)activateConstraints {
  [NSLayoutConstraint activateConstraints:@[
    [self.textBadge.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [self.textBadge.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [self.widthAnchor constraintEqualToAnchor:self.textBadge.widthAnchor],
    [self.heightAnchor constraintEqualToAnchor:self.textBadge.heightAnchor]
  ]];
}

@end
