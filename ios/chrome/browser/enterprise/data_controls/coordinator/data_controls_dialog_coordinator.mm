// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/coordinator/data_controls_dialog_coordinator.h"

#import "base/functional/callback.h"
#import "ios/chrome/browser/enterprise/data_controls/utils/data_controls_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

@implementation DataControlsDialogCoordinator {
  // The underlying alert controller used to show the dialog.
  UIAlertController* _alertController;
  // The type of warning dialog to be displayed.
  data_controls::DataControlsDialog::Type _dialogType;
  // The domain of the organization that triggered the dialog.
  std::string _organizationDomain;
  // The callback to be invoked when the user taps on the warning dialog.
  base::OnceCallback<void(bool)> _callback;
}

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                    dialogType:
                        (data_controls::DataControlsDialog::Type)dialogType
            organizationDomain:(std::string_view)organizationDomain
                      callback:(base::OnceCallback<void(bool)>)callback {
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

  // If stop is called before a choice was made, consider it a "cancel".
  if (_callback) {
    std::move(_callback).Run(false);
  }
}

// Constructs and shows the warning alert using UIAlertController.
- (void)showWarningAlert {
  data_controls::WarningDialog warningDialog =
      data_controls::GetWarningDialog(_dialogType, _organizationDomain);
  _alertController =
      [UIAlertController alertControllerWithTitle:warningDialog.title
                                          message:warningDialog.label
                                   preferredStyle:UIAlertControllerStyleAlert];

  __weak DataControlsDialogCoordinator* weakSelf = self;

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
}

// Dismisses the keyboard in the current window.
- (void)dismissKeyboard {
  UIWindow* window = self.baseViewController.view.window;

  UIResponder* firstResponder =
      GetFirstResponderInWindowScene(window.windowScene);
  [firstResponder resignFirstResponder];
}

@end
