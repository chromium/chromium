// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/coordinator/fallback_coordinator.h"

#import "base/memory/ref_counted.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/autofill/manual_fill/model/manual_fill_injection_handler.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/fallback_view_controller.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/public/provider/chrome/browser/keyboard/keyboard_api.h"

@interface FallbackCoordinator () <UIPopoverPresentationControllerDelegate>

@end

@implementation FallbackCoordinator {
  // Button used to anchor the popover presentation.
  __weak UIButton* _sourceButton;
}

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
  // Dismiss the popover before the settings are presented.
  if (self.viewController.presentingViewController) {
    // When using a popover view, fallback coordinators are coordinators of
    // subviews of the expanded manual fill view, so they can't dismiss the
    // entire popup, otherwise changing the type of autofill data between
    // passwords, addresses or credit cards would dismiss the entire popup. In
    // this case, only the completion block is executed. The popup will be
    // dismissed when the expanded manual fill coordinator stops.
    if (completion) {
      completion();
    }
    return YES;
  } else {
    if (completion) {
      completion();
      if ([ManualFillUtil shouldUsePopover:self.baseViewController]) {
        [self.delegate fallbackCoordinatorDidDismissPopover:self];
      }
    }
    return NO;
  }
}

- (void)presentFromButton:(UIButton*)button {
  self.viewController.modalPresentationStyle = UIModalPresentationPopover;

  // `topFrontWindow` is used in order to present above the keyboard. This way,
  // the popover will be dismissed on keyboard interaction and it won't be
  // covered when the keyboard is near the top of the screen.
  UIWindow* topFrontWindow = ios::provider::GetKeyboardWindow();
  [topFrontWindow.rootViewController presentViewController:self.viewController
                                                  animated:YES
                                                completion:nil];

  _sourceButton = button;
  UIPopoverPresentationController* popoverPresentationController =
      self.viewController.popoverPresentationController;
  if (topFrontWindow && button.window &&
      CGRectIsNull([button convertRect:button.bounds
                     toCoordinateSpace:topFrontWindow])) {
    // When `topFrontWindow` belongs to a separate screen coordinate space from
    // `button.window` (e.g., `_UIKeyboardInputScene` vs the application's
    // `UIWindowScene`), UIKit cannot convert `button.bounds` across windows.
    // Anchor in `topFrontWindow` using `button`'s window coordinates instead.
    popoverPresentationController.sourceView = topFrontWindow;
    popoverPresentationController.sourceRect = [button convertRect:button.bounds
                                                            toView:nil];
  } else {
    popoverPresentationController.sourceView = button;
    popoverPresentationController.sourceRect = button.bounds;
  }
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
    // for popovers. For input views we need to remove the view.
    [self.viewController.view removeFromSuperview];
  }
}

#pragma mark - UIPopoverPresentationControllerDelegate

- (void)popoverPresentationController:
            (UIPopoverPresentationController*)popoverPresentationController
          willRepositionPopoverToRect:(inout CGRect*)rect
                               inView:(inout UIView**)view {
  if (_sourceButton.window && *view != _sourceButton) {
    *rect = [_sourceButton convertRect:_sourceButton.bounds toView:nil];
  }
}

- (void)popoverPresentationControllerDidDismissPopover:
    (UIPopoverPresentationController*)popoverPresentationController {
  base::RecordAction(base::UserMetricsAction("ManualFallback_ClosePopover"));
  [self.delegate fallbackCoordinatorDidDismissPopover:self];
}

@end
