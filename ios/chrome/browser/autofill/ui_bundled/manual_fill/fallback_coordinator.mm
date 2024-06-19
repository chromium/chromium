// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/fallback_coordinator.h"

#import "base/memory/ref_counted.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/fallback_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/public/provider/chrome/browser/keyboard/keyboard_api.h"
#import "ui/base/device_form_factor.h"

@interface FallbackCoordinator ()<UIPopoverPresentationControllerDelegate>

@end

@implementation FallbackCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                          injectionHandler:
                              (ManualFillInjectionHandler*)injectionHandler {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _injectionHandler = injectionHandler;
  }
  return self;
}

- (BOOL)dismissIfNecessaryThenDoCompletion:(void (^)(void))completion {
  // On iPad, dismiss the popover before the settings are presented.
  if ((ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) &&
      self.viewController.presentingViewController) {
    [self.viewController dismissViewControllerAnimated:true
                                            completion:completion];
    return YES;
  } else {
    if (completion) {
      completion();
      if ((ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)) {
        [self.delegate fallbackCoordinatorDidDismissPopover:self];
      }
    }
    return NO;
  }
}

- (void)presentFromButton:(UIButton*)button {
  self.viewController.modalPresentationStyle = UIModalPresentationPopover;

  // `topFrontWindow` is used in order to present above the keyboard. This way
  // the popover will be dismissed on keyboard interaction and it won't be
  // covered when the keyboard is near the top of the screen.
  UIWindow* topFrontWindow = ios::provider::GetKeyboardWindow();
  [topFrontWindow.rootViewController presentViewController:self.viewController
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
