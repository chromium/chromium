// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_container_view.h"

#import "ios/chrome/browser/app_bar/ui/app_bar_container_view_delegate.h"

namespace {
constexpr CGFloat kDefaultAppBarWidth = 300;
}

@implementation AppBarContainerView {
  NSLayoutConstraint* _heightConstraint;
  NSLayoutConstraint* _appBarVerticalPositioning;
  NSLayoutConstraint* _appBarWidthConstraint;
  UIView* _appBar;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _heightConstraint = [self.heightAnchor constraintEqualToConstant:0];
    _heightConstraint.active = YES;
    [self.widthAnchor constraintEqualToAnchor:self.heightAnchor].active = YES;
  }
  return self;
}

- (void)setAppBar:(UIView*)appBar {
  if (_appBar == appBar) {
    return;
  }
  if ([_appBar isDescendantOfView:self]) {
    [_appBar removeFromSuperview];
    _appBarWidthConstraint.active = NO;
    _appBarVerticalPositioning.active = NO;
  }

  _appBar = appBar;

  if (!_appBar) {
    return;
  }

  [self addSubview:_appBar];
  _appBar.translatesAutoresizingMaskIntoConstraints = NO;
  [_appBar.centerXAnchor constraintEqualToAnchor:self.centerXAnchor].active =
      YES;

  _appBarVerticalPositioning =
      [appBar.bottomAnchor constraintEqualToAnchor:self.bottomAnchor];
  _appBarVerticalPositioning.active = YES;

  _appBarWidthConstraint =
      [appBar.widthAnchor constraintEqualToConstant:kDefaultAppBarWidth];
  _appBarWidthConstraint.active = YES;
  [self updatePositioning];
}

#pragma mark - UIView

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  // Only receive touches for subviews and let the others go through.
  for (UIView* subview in self.subviews) {
    if ([subview pointInside:[self convertPoint:point toView:subview]
                   withEvent:event]) {
      return YES;
    }
  }
  return NO;
}

- (void)didMoveToWindow {
  [super didMoveToWindow];
  [self updatePositioning];
  [self.delegate appBarContainerDidMoveToWindow:self];
}

#pragma mark - Private

// Updates the position and size of the container view and the app bar.
- (void)updatePositioning {
  if (!self.window || !_appBar) {
    return;
  }
  UIWindowScene* windowScene = self.window.windowScene;
  CGSize sceneSize = windowScene.screen.bounds.size;
  CGFloat tallSideLength = MAX(sceneSize.width, sceneSize.height);
  CGFloat containerLength = tallSideLength * 1.5;
  _heightConstraint.constant = containerLength;

  CGFloat shortSideLength = MIN(sceneSize.width, sceneSize.height);
  _appBarWidthConstraint.constant = shortSideLength;

  CGFloat offset = (containerLength - tallSideLength) / 2;
  _appBarVerticalPositioning.constant = -offset;
}

@end
