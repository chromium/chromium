// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/enterprise_dialog/coordinator/enterprise_dialog_coordinator.h"

#import "base/functional/callback.h"
#import "ios/chrome/browser/enterprise/enterprise_dialog/model/warning_dialog.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/web/public/web_state.h"

@implementation EnterpriseDialogCoordinator {
  // The underlying alert controller used to show the dialog.
  UIAlertController* _alertController;
  // The type of warning dialog to be displayed.
  enterprise::DialogType _dialogType;
  // The domain of the organization that triggered the dialog.
  std::string _organizationDomain;
  // The callback to be invoked when the user taps on the warning dialog.
  base::OnceCallback<void(bool)> _callback;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                dialogType:(enterprise::DialogType)dialogType
                        organizationDomain:(std::string_view)organizationDomain
                                  callback:
                                      (base::OnceCallback<void(bool)>)callback {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _dialogType = dialogType;
    _organizationDomain = std::string(organizationDomain);
    _callback = std::move(callback);
  }
  return self;
}

- (void)dealloc {
  // The callback should have been consumed by the time the coordinator is
  // deallocated.
  CHECK(!_callback);
}

- (void)start {
  [self showWarningAlert];
}

- (void)stop {
  [_alertController.presentingViewController dismissViewControllerAnimated:YES
                                                                completion:nil];
  _alertController = nil;

  [self restoreFocus];

  // If stop is called before a choice was made, consider it a "cancel".
  if (_callback) {
    std::move(_callback).Run(false);
  }
}

// Constructs and shows the warning alert using UIAlertController.
- (void)showWarningAlert {
  enterprise::WarningDialog warningDialog =
      enterprise::GetWarningDialog(_dialogType, _organizationDomain);
  _alertController =
      [UIAlertController alertControllerWithTitle:warningDialog.title
                                          message:warningDialog.label
                                   preferredStyle:UIAlertControllerStyleAlert];

  __weak EnterpriseDialogCoordinator* weakSelf = self;

  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:warningDialog.cancel_button_id
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* action) {
                               [weakSelf onWarningDialogDismissed:false];
                             }];
  [_alertController addAction:cancelAction];
  _alertController.preferredAction = cancelAction;

  UIAlertAction* okAction =
      [UIAlertAction actionWithTitle:warningDialog.ok_button_id
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* action) {
                               [weakSelf onWarningDialogDismissed:true];
                             }];
  [_alertController addAction:okAction];

  // Hide the keyboard to prevent it from flickering when dismissing the alert.
  [self dismissKeyboard];

  _alertController.view.accessibilityViewIsModal = YES;
  [self.baseViewController presentViewController:_alertController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - Private

// Called when the user dismisses the dialog, and it invokes the callback.
- (void)onWarningDialogDismissed:(bool)result {
  if (_callback) {
    std::move(_callback).Run(result);
  }

  [self restoreFocus];
}

// Restores focus to the active web content.
- (void)restoreFocus {
  if (!self.browser) {
    return;
  }
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  UIView* viewToFocus = activeWebState ? activeWebState->GetView() : nil;
  if (viewToFocus) {
    // Delay focus restoration slightly to ensure the alert dismissal transition
    // has started and VoiceOver can focus on the underlying view.
    auto postAccessibilityChangeBlock = ^{
      UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                      viewToFocus);
    };
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)),
        dispatch_get_main_queue(), postAccessibilityChangeBlock);
  }
}

// Dismisses the keyboard in the current window.
- (void)dismissKeyboard {
  UIWindow* window = self.baseViewController.view.window;

  UIResponder* firstResponder =
      GetFirstResponderInWindowScene(window.windowScene);
  [firstResponder resignFirstResponder];
}

@end
