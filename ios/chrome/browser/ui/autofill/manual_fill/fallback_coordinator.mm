// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/fallback_coordinator.h"

#include "base/memory/ref_counted.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/fallback_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FallbackCoordinator ()<UIPopoverPresentationControllerDelegate>

@end

@implementation FallbackCoordinator

- (instancetype)
initWithBaseViewController:(UIViewController*)viewController
              browserState:(ios::ChromeBrowserState*)browserState
          injectionHandler:(ManualFillInjectionHandler*)injectionHandler {
  self = [super initWithBaseViewController:viewController
                              browserState:browserState];
  if (self) {
    _injectionHandler = injectionHandler;
  }
  return self;
}

- (BOOL)dismissIfNecessaryThenDoCompletion:(void (^)(void))completion {
  // On iPad, dismiss the popover before the settings are presented.
  if (IsIPadIdiom() && self.viewController.presentingViewController) {
    [self.viewController dismissViewControllerAnimated:true
                                            completion:completion];
    return YES;
  } else {
    if (completion) {
      completion();
    }
    return NO;
  }
}

- (void)presentFromButton:(UIButton*)button {
  self.viewController.modalPresentationStyle = UIModalPresentationPopover;

  // The |button.window.rootViewController| is used in order to present above
  // the keyboard. This way the popover will be dismissed on keyboard
  // interaction and it won't be covered when the keyboard is near the top of
  // the screen.
  [button.window.rootViewController presentViewController:self.viewController
                                                 animated:YES
                                               completion:nil];

  UIPopoverPresentationController* popoverPresentationController =
      self.viewController.popoverPresentationController;
  popoverPresentationController.sourceView = button;
  popoverPresentationController.sourceRect = button.bounds;
  popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionUp | UIPopoverArrowDirectionDown;
  popoverPresentationController.delegate = self;
  popoverPresentationController.backgroundColor =
      [UIColor colorNamed:kBackgroundColor];
}

#pragma mark - ChromeCoordinator

- (void)stop {
  [super stop];
  if (![self dismissIfNecessaryThenDoCompletion:nil]) {
    // dismissIfNecessaryThenDoCompletion dismisses, via the UIKit API, only
    // for popovers (iPads). For iPhones we need to remove the view.
    [self.viewController.view removeFromSuperview];
  }
}

#pragma mark - UIPopoverPresentationControllerDelegate

- (void)popoverPresentationControllerDidDismissPopover:
    (UIPopoverPresentationController*)popoverPresentationController {
  base::RecordAction(base::UserMetricsAction("ManualFallback_ClosePopover"));
  [self.delegate fallbackCoordinatorDidDismissPopover:self];
}

@end
