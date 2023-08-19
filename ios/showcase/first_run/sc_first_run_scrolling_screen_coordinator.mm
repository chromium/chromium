// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/first_run/sc_first_run_scrolling_screen_coordinator.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"
#import "ios/showcase/first_run/sc_first_run_scrolling_screen_view_controller.h"

@interface SCFirstRunScrollingScreenCoordinator () <
    PromoStyleViewControllerDelegate>

@property(nonatomic, strong)
    SCFirstRunScrollingScreenViewController* screenViewController;

@end

@implementation SCFirstRunScrollingScreenCoordinator
@synthesize baseViewController = _baseViewController;

#pragma mark - Public Methods.

- (void)start {
  self.screenViewController =
      [[SCFirstRunScrollingScreenViewController alloc] init];
  self.screenViewController.delegate = self;
  self.screenViewController.modalPresentationStyle =
      UIModalPresentationFormSheet;
  [self.baseViewController setHidesBarsOnSwipe:NO];
  [self.baseViewController pushViewController:self.screenViewController
                                     animated:YES];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:@"Primary Button Tapped"
                                          message:@"This is a message from the "
                                                  @"coordinator."
                                   preferredStyle:UIAlertControllerStyleAlert];

  UIAlertAction* defaultAction =
      [UIAlertAction actionWithTitle:@"OK"
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* action){
                             }];

  [alert addAction:defaultAction];
  [self.screenViewController presentViewController:alert
                                          animated:YES
                                        completion:nil];
}

@end
