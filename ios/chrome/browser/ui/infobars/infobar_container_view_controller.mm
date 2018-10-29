// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_container_view_controller.h"

#include "base/ios/block_types.h"
#include "ios/chrome/browser/infobars/infobar.h"
#import "ios/chrome/browser/ui/infobars/infobar_positioner.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Duration for the alpha change animation.
const CGFloat kAlphaChangeAnimationDuration = 0.35;
}  // namespace

@implementation InfobarContainerViewController

#pragma mark - InfobarConsumer

- (void)addInfoBar:(InfoBarIOS*)infoBarIOS position:(NSInteger)position {
  DCHECK_LE(static_cast<NSUInteger>(position), [[self.view subviews] count]);
  CGRect containerBounds = [self.view bounds];
  infoBarIOS->Layout(containerBounds);
  UIView<InfoBarViewSizing>* view = infoBarIOS->view();
  [self.view insertSubview:view atIndex:position];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [view.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [view.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    [view.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [view.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor]
  ]];
}

- (void)setUserInteractionEnabled:(BOOL)enabled {
  [self.view setUserInteractionEnabled:enabled];
}

#pragma mark - InfobarContainerStateDelegate

- (void)infoBarContainerStateDidChangeAnimated:(BOOL)animated {
  // Update the infobarContainer height.
  CGRect containerFrame = self.view.frame;
  CGFloat height = [self topmostVisibleInfoBarHeight];
  containerFrame.origin.y =
      CGRectGetMaxY([self.positioner parentView].frame) - height;
  containerFrame.size.height = height;

  BOOL isViewVisible = [self.positioner isParentViewVisible];
  auto completion = ^(BOOL finished) {
    if (!isViewVisible)
      return;
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                    self.view);
  };

  ProceduralBlock frameUpdates = ^{
    [self.view setFrame:containerFrame];
  };
  if (animated) {
    [UIView animateWithDuration:0.1
                     animations:frameUpdates
                     completion:completion];
  } else {
    frameUpdates();
    completion(YES);
  }
}

#pragma mark - Private Methods

// Animates |self.view| alpha to |alpha|.
- (void)animateInfoBarContainerToAlpha:(CGFloat)alpha {
  CGFloat oldAlpha = self.view.alpha;
  if (oldAlpha > 0 && alpha == 0) {
    [self.view setUserInteractionEnabled:NO];
  }

  [UIView transitionWithView:self.view
      duration:kAlphaChangeAnimationDuration
      options:UIViewAnimationOptionCurveEaseInOut
      animations:^{
        [self.view setAlpha:alpha];
      }
      completion:^(BOOL) {
        if (oldAlpha == 0 && alpha > 0) {
          [self.view setUserInteractionEnabled:YES];
        };
      }];
}

// Height of the frontmost infobar contained in |self.view| that is not hidden.
- (CGFloat)topmostVisibleInfoBarHeight {
  for (UIView* view in [self.view.subviews reverseObjectEnumerator]) {
    return [view sizeThatFits:self.view.frame.size].height;
  }
  return 0;
}

@end
