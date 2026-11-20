// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/vertical_sidebar_cell.h"

#import <algorithm>

#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Inset of the hosted view's bounds that forwarded touches are clamped to. It
// keeps the clamped point strictly inside the bounds, so the hosted view's
// `-hitTest:withEvent:` accepts it: `-pointInside:withEvent:` excludes the
// bounds' max edges.
constexpr CGFloat kHitTestClampInset = 0.5;

}  // namespace

@implementation VerticalSidebarCell {
  UIView* _hostedView;
}

- (instancetype)initWithHostedView:(UIView*)hostedView {
  if ((self = [super initWithFrame:CGRectZero])) {
    _hostedView = hostedView;
    self.translatesAutoresizingMaskIntoConstraints = NO;
    hostedView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:hostedView];
    AddSameCenterConstraints(hostedView, self);
  }
  return self;
}

#pragma mark - UIView

- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
  UIView* hit = [super hitTest:point withEvent:event];
  if (hit != self) {
    return hit;
  }
  // Forward touches outside the hosted view to its closest point.
  CGRect bounds =
      CGRectInset(_hostedView.bounds, kHitTestClampInset, kHitTestClampInset);
  if (CGRectIsEmpty(bounds)) {
    return nil;
  }
  CGPoint pointInHostedView = [self convertPoint:point toView:_hostedView];
  pointInHostedView.x = std::clamp(pointInHostedView.x, CGRectGetMinX(bounds),
                                   CGRectGetMaxX(bounds));
  pointInHostedView.y = std::clamp(pointInHostedView.y, CGRectGetMinY(bounds),
                                   CGRectGetMaxY(bounds));
  return [_hostedView hitTest:pointInHostedView withEvent:event];
}

@end
