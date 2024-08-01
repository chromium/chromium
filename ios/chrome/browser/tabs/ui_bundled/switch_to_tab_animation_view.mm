// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/ui_bundled/switch_to_tab_animation_view.h"

#import "base/i18n/rtl.h"

namespace {
const CGFloat kMiddleMargin = 80;
const CGFloat kAnimationTime = 0.35;
}  // namespace

@implementation SwitchToTabAnimationView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor colorWithWhite:0.13 alpha:1];
  }
  return self;
}

- (void)animateFromCurrentView:(UIView*)currentView
                     toNewView:(UIView*)newView
                    inPosition:(SwitchToTabAnimationPosition)position {
  CGRect leftViewInitialFrame = self.bounds;
  CGRect rightViewInitialFrame = self.bounds;
  CGRect leftViewFinalFrame = self.bounds;
  CGRect rightViewFinalFrame = self.bounds;
  UIView* rightView;
  UIView* leftView;

  BOOL directionLeft =
      ((position == SwitchToTabAnimationPositionBefore) &&
       !base::i18n::IsRTL()) ||
      ((position == SwitchToTabAnimationPositionAfter) && base::i18n::IsRTL());

  if (directionLeft) {
    rightView = currentView;
    leftView = newView;
    leftViewInitialFrame.origin.x -= self.bounds.size.width + kMiddleMargin;
    rightViewFinalFrame.origin.x += self.bounds.size.width + kMiddleMargin;
  } else {
    rightView = newView;
    leftView = currentView;
    rightViewInitialFrame.origin.x += self.bounds.size.width + kMiddleMargin;
    leftViewFinalFrame.origin.x -= self.bounds.size.width + kMiddleMargin;
  }

  [self addSubview:rightView];
  [self addSubview:leftView];

  leftView.frame = leftViewInitialFrame;
  rightView.frame = rightViewInitialFrame;

  [UIView animateWithDuration:kAnimationTime
      animations:^{
        leftView.frame = leftViewFinalFrame;
        rightView.frame = rightViewFinalFrame;
      }
      completion:^(BOOL finished) {
        [self removeFromSuperview];
      }];
}

@end
