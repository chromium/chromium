// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_container_view.h"

#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_container_view_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"

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

- (void)setFullscreenProgress:(CGFloat)progress {
  if (_fullscreenProgress == progress) {
    return;
  }
  _fullscreenProgress = progress;
  [self updatePositioning];
}

- (void)setAppBarPosition:(AppBarPosition)appBarPosition {
  if (_appBarPosition == appBarPosition) {
    return;
  }
  _appBarPosition = appBarPosition;
  [self updatePositioning];
}

- (void)layoutSubviews {
  [super layoutSubviews];
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
  CGSize windowSize = self.window.bounds.size;
  CGFloat tallSideLength = MAX(windowSize.width, windowSize.height);
  CGFloat containerLength = tallSideLength * 1.5;
  _heightConstraint.constant = containerLength;

  CGFloat shortSideLength = MIN(windowSize.width, windowSize.height);
  _appBarWidthConstraint.constant = shortSideLength;

  CGFloat offset = (containerLength - tallSideLength) / 2;

  CGFloat extraOffset = 0;
  switch (self.appBarPosition) {
    case AppBarPosition::kBottom:
      extraOffset = (1 - self.fullscreenProgress) *
                    (kAppBarHeight - kAppBarHeightFullscreen);
      break;

    case AppBarPosition::kLeft:
      [[fallthrough]];
    case AppBarPosition::kRight:
      extraOffset = kAppBarHeight - kAppBarHeightLandscape;
      break;
    case AppBarPosition::kNone:
      break;
  }

  _appBarVerticalPositioning.constant = -offset + extraOffset;
}

@end
