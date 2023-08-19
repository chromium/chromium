// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_presentation_controller.h"

#import <algorithm>
#import <cmath>

#import "base/check.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_positioner.h"

namespace {
// The presented view outer horizontal margins.
const CGFloat kContainerHorizontalPadding = 8;
// The presented view maximum width.
const CGFloat kContainerMaxWidth = 398;
// The presented view maximum height.
const CGFloat kContainerMaxHeight = 230;
// Minimum height or width frame change that should warrant a resizing of the
// container view in response to a relayout.
const CGFloat kMinimumSizeChange = 0.5;
}

@interface InfobarBannerPresentationController ()
// Delegate used to position the InfobarBanner.
@property(nonatomic, weak) id<InfobarBannerPositioner> bannerPositioner;
// Returns the frame of the infobar banner UI in window coordinates.
@property(nonatomic, readonly) CGRect bannerFrame;
@end

@implementation InfobarBannerPresentationController

- (instancetype)
    initWithPresentedViewController:(UIViewController*)presentedViewController
           presentingViewController:(UIViewController*)presentingViewController
                   bannerPositioner:
                       (id<InfobarBannerPositioner>)bannerPositioner {
  self = [super initWithPresentedViewController:presentedViewController
                       presentingViewController:presentingViewController];
  if (self) {
    _bannerPositioner = bannerPositioner;
  }
  return self;
}

#pragma mark - Accessors

- (CGRect)bannerFrame {
  DCHECK(self.bannerPositioner);
  UIWindow* window = self.containerView.window;
  CGRect bannerFrame = CGRectZero;

  // Calculate the Banner container width.
  CGFloat windowWidth = CGRectGetWidth(window.bounds);
  CGFloat bannerWidth = std::min(kContainerMaxWidth,
                                 windowWidth - 2 * kContainerHorizontalPadding);
  bannerFrame.size.width = bannerWidth;

  // Based on the container width, calculate the value in order to center the
  // Banner in the X axis.
  bannerFrame.origin.x = (windowWidth - CGRectGetWidth(bannerFrame)) / 2.0;
  bannerFrame.origin.y = [self.bannerPositioner bannerYPosition];

  // Calculate the Banner height needed to fit its content with frameWidth.
  UIView* bannerView = [self.bannerPositioner bannerView];
  [bannerView setNeedsLayout];
  [bannerView layoutIfNeeded];
  CGFloat bannerHeight =
      [bannerView systemLayoutSizeFittingSize:CGSizeMake(bannerWidth, 0)
                withHorizontalFittingPriority:UILayoutPriorityRequired
                      verticalFittingPriority:1]
          .height;
  bannerFrame.size.height = std::min(kContainerMaxHeight, bannerHeight);

  return bannerFrame;
}

#pragma mark - OverlayPresentationController

- (BOOL)resizesPresentationContainer {
  return YES;
}

#pragma mark - UIPresentationController

- (BOOL)shouldPresentInFullscreen {
  // OverlayPresentationController returns NO here so that banners presented via
  // OverlayPresenter are inserted into the correct place in the view hierarchy.
  // Returning NO adds the container view as a sibling view in front of the
  // presenting view controller's view.
  return [super shouldPresentInFullscreen];
}

- (void)presentationTransitionWillBegin {
  UIView* containerView = self.containerView;
  containerView.frame =
      [containerView.superview convertRect:self.bannerFrame
                                  fromView:containerView.window];
}

- (void)containerViewWillLayoutSubviews {
  CGRect bannerFrame = self.bannerFrame;
  UIView* containerView = self.containerView;
  UIWindow* window = containerView.window;

  UIView* bannerView = self.presentedView;
  CGRect newFrame = [bannerView.superview convertRect:bannerFrame
                                             fromView:window];
  if (std::fabs(newFrame.size.height - bannerView.frame.size.height) >
          kMinimumSizeChange ||
      std::fabs(newFrame.size.width - bannerView.frame.size.width) >
          kMinimumSizeChange) {
    bannerView.frame = newFrame;
    containerView.frame = newFrame;
    self.needsLayout = YES;
  }

  [super containerViewWillLayoutSubviews];
}

@end
