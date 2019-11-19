// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/legacy_infobar_container_view_controller.h"

#include "base/ios/block_types.h"
#include "base/logging.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/infobars/infobar_positioner.h"
#import "ios/chrome/browser/ui/infobars/infobar_ui_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Duration for the alpha change animation.
const CGFloat kAlphaChangeAnimationDuration = 0.35;
}  // namespace

@interface LegacyInfobarContainerViewController () <FullscreenUIElement> {
  // Observer that notifies this object of fullscreen events.
  std::unique_ptr<FullscreenUIUpdater> _fullscreenUIUpdater;
}

// Whether the controller's view is currently available.
// YES from viewDidAppear to viewDidDisappear.
@property(nonatomic, assign, getter=isVisible) BOOL visible;

// Observes scrolling events in the main content area and notifies the observers
// of the current fullscreen progress value.
@property(nonatomic, assign) FullscreenController* fullscreenController;

@end

@implementation LegacyInfobarContainerViewController

- (instancetype)initWithFullscreenController:
    (FullscreenController*)fullscreenController {
  DCHECK(fullscreenController);
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _fullscreenController = fullscreenController;
  }
  return self;
}

#pragma mark - UIViewController

// Whenever the container or contained views are re-drawn update the layout to
// match their new size or position.
- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updateLayoutAnimated:YES];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  self.visible = YES;

  if (!_fullscreenUIUpdater && !self.disableFullscreenSupport) {
    _fullscreenUIUpdater =
        std::make_unique<FullscreenUIUpdater>(self.fullscreenController, self);
  }
}

- (void)viewDidDisappear:(BOOL)animated {
  if (_fullscreenUIUpdater && !self.disableFullscreenSupport) {
    _fullscreenUIUpdater = nullptr;
  }

  self.visible = NO;
  [super viewDidDisappear:animated];
}

#pragma mark - InfobarConsumer

- (void)addInfoBarWithDelegate:(id<InfobarUIDelegate>)infoBarDelegate {
  UIView* infoBarView = infoBarDelegate.view;
  [self.view addSubview:infoBarView];
  infoBarView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [infoBarView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [infoBarView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [infoBarView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [infoBarView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor]
  ]];
}

- (void)infobarManagerWillChange {
  // NO-OP. This legacy container doesn't need to clean up any state after the
  // InfobarManager has changed.
}

- (void)setUserInteractionEnabled:(BOOL)enabled {
  [self.view setUserInteractionEnabled:enabled];
}

- (void)updateLayoutAnimated:(BOOL)animated {
  // Update the infobarContainer height.
  CGRect containerFrame = self.view.frame;
  CGFloat height = [self topmostVisibleInfoBarHeight];
  containerFrame.origin.y =
      CGRectGetMaxY([self.positioner parentView].frame) - height;
  containerFrame.size.height = height;

  __weak __typeof(self) weakSelf = self;
  auto completion = ^(BOOL finished) {
    __typeof(self) strongSelf = weakSelf;
    // Return if weakSelf has been niled or is not visible since there's no view
    // to send an A11y post notification to.
    if (!strongSelf.visible)
      return;
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                    strongSelf.view);
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

#pragma mark - FullscreenUIElement methods

- (void)updateForFullscreenProgress:(CGFloat)progress {
  for (UIView* view in self.view.subviews) {
    if ([view conformsToProtocol:@protocol(FullscreenUIElement)]) {
      [(id<FullscreenUIElement>)view updateForFullscreenProgress:progress];
    }
  }
  [self.view setNeedsLayout];
  [self.view layoutIfNeeded];
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
