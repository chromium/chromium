// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/bottom_sheet/bottom_sheet_navigation_controller.h"

#import <algorithm>

#import "base/check.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/bottom_sheet/child_bottom_sheet_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Maximum height for BottomSheetNavigationController. This is a ratio related
// the window height.
CGFloat kMaxBottomSheetHeightRatioWithWindow = .75;

}  // namespace

@interface BottomSheetNavigationController ()

// View to get transparent blurred background.
@property(nonatomic, strong, readwrite) UIVisualEffectView* visualEffectView;

@end

@implementation BottomSheetNavigationController

- (CGSize)layoutFittingSize {
  CGFloat width = self.view.frame.size.width;
  UINavigationController* navigationController =
      self.childViewControllers.lastObject;
  DCHECK([navigationController
      conformsToProtocol:@protocol(ChildBottomSheetViewController)]);
  UIViewController<ChildBottomSheetViewController>* childNavigationController =
      static_cast<UIViewController<ChildBottomSheetViewController>*>(
          navigationController);
  CGFloat height =
      [childNavigationController layoutFittingHeightForWidth:width];
  CGFloat maxViewHeight =
      self.view.window.frame.size.height * kMaxBottomSheetHeightRatioWithWindow;
  return CGSizeMake(width, std::min(height, maxViewHeight));
}

- (void)didUpdateControllerViewFrame {
  self.visualEffectView.frame = self.view.bounds;
  // The dimmer view should never be under the bottom sheet view, since the
  // background of the botther sheet is transparent.
  CGRect dimmerViewFrame = self.backgroundDimmerView.superview.bounds;
  dimmerViewFrame.size.height = self.view.frame.origin.y;
  self.backgroundDimmerView.frame = dimmerViewFrame;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  UIVisualEffect* blurEffect = nil;
  if (@available(iOS 13, *)) {
    blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThickMaterial];
  } else {
    blurEffect = [UIBlurEffect effectWithStyle:UIBlurEffectStyleLight];
  }
  self.visualEffectView =
      [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  [self.view insertSubview:self.visualEffectView atIndex:0];
  self.visualEffectView.frame = self.view.bounds;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.navigationBar setBackgroundImage:[[UIImage alloc] init]
                           forBarMetrics:UIBarMetricsDefault];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self didUpdateControllerViewFrame];
}

#pragma mark - UINavigationController

- (void)pushViewController:(UIViewController*)viewController
                  animated:(BOOL)animated {
  DCHECK([viewController
      conformsToProtocol:@protocol(ChildBottomSheetViewController)]);
  [super pushViewController:viewController animated:animated];
}

@end
