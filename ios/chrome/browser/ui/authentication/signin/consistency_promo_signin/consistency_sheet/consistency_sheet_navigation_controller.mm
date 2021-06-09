// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_sheet/consistency_sheet_navigation_controller.h"

#import <algorithm>

#import "base/check.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_sheet/child_consistency_sheet_view_controller.h"
#import "ios/chrome/common/ui/util/background_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Maximum height for ConsistencySheetNavigationController. This is a ratio
// related the window height.
CGFloat kMaxBottomSheetHeightRatioWithWindow = .75;

}  // namespace

@interface ConsistencySheetNavigationController ()

// View to get transparent blurred background.
@property(nonatomic, strong, readwrite) UIView* backgroundView;

@end

@implementation ConsistencySheetNavigationController

- (CGSize)layoutFittingSize {
  CGFloat width = self.view.frame.size.width;
  UINavigationController* navigationController =
      self.childViewControllers.lastObject;
  DCHECK([navigationController
      conformsToProtocol:@protocol(ChildConsistencySheetViewController)]);
  UIViewController<ChildConsistencySheetViewController>*
      childNavigationController =
          static_cast<UIViewController<ChildConsistencySheetViewController>*>(
              navigationController);
  CGFloat height =
      [childNavigationController layoutFittingHeightForWidth:width];
  CGFloat maxViewHeight =
      self.view.window.frame.size.height * kMaxBottomSheetHeightRatioWithWindow;
  return CGSizeMake(width, std::min(height, maxViewHeight));
}

- (void)didUpdateControllerViewFrame {
  self.backgroundView.frame = self.view.bounds;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.backgroundView = PrimaryBackgroundBlurView();
  [self.view insertSubview:self.backgroundView atIndex:0];
  self.backgroundView.frame = self.view.bounds;
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
      conformsToProtocol:@protocol(ChildConsistencySheetViewController)]);
  [super pushViewController:viewController animated:animated];
}

@end
