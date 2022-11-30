// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/feed_sign_in_promo/sc_feed_sign_in_promo_coordinator.h"

#import "ios/chrome/browser/ui/ntp/feed_promos/feed_sign_in_promo_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Sets a custom radius for the half sheet presentation.
constexpr CGFloat kHalfSheetCornerRadius = 20;
}  // namespace

@interface SCFeedSignInPromoCoordinator ()

@property(nonatomic, strong)
    FeedSignInPromoViewController* signInPromoViewController;

@end

@implementation SCFeedSignInPromoCoordinator
@synthesize baseViewController = _baseViewController;

#pragma mark - Public Methods.

- (void)start {
  self.signInPromoViewController = [[FeedSignInPromoViewController alloc] init];
  if (@available(iOS 15, *)) {
    self.signInPromoViewController.modalPresentationStyle =
        UIModalPresentationPageSheet;
    UISheetPresentationController* presentationController =
        self.signInPromoViewController.sheetPresentationController;
    presentationController.prefersEdgeAttachedInCompactHeight = YES;
    presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached =
        YES;
    presentationController.detents = @[
      UISheetPresentationControllerDetent.mediumDetent,
      UISheetPresentationControllerDetent.largeDetent
    ];
    presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
  } else {
    self.signInPromoViewController.modalPresentationStyle =
        UIModalPresentationPageSheet;
  }
  [self.baseViewController setHidesBarsOnSwipe:NO];
  [self.baseViewController presentViewController:self.signInPromoViewController
                                        animated:YES
                                      completion:^(){
                                      }];
}

@end
