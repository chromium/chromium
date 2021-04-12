// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/bottom_sheet/bottom_sheet_navigation_controller.h"

#import <algorithm>

#import "base/check.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/bottom_sheet/child_bottom_sheet_view_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Maximum height for BottomSheetNavigationController. This is a ratio related
// the window height.
CGFloat kMaxBottomSheetHeightRatioWithWindow = .75;

}  // namespace

@implementation BottomSheetNavigationController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
}

- (void)pushViewController:(UIViewController*)viewController
                  animated:(BOOL)animated {
  // |viewController.view| has to be a UIScrollView.
  DCHECK([viewController.view isKindOfClass:[UIScrollView class]]);
  DCHECK([viewController
      conformsToProtocol:@protocol(ChildBottomSheetViewController)]);
  [super pushViewController:viewController animated:animated];
}

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

@end
