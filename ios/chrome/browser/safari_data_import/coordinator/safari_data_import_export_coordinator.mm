// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_export_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_child_coordinator_delegate.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_import_coordinator.h"
#import "ios/chrome/browser/safari_data_import/public/metrics.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_export_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/public/provider/chrome/browser/safari_data_import/safari_data_import_api.h"

@interface SafariDataImportExportCoordinator () <ConfirmationAlertActionHandler,
                                                 UINavigationControllerDelegate>

@end

@implementation SafariDataImportExportCoordinator {
  /// The navigation controller that presents the view controller controlled by
  /// this coordinator as the root view controller.
  UINavigationController* _navigationController;
  /// The coordinator that displays the steps to import Safari data to Chrome.
  /// Will push view to the `_navigationController` when started.
  SafariDataImportImportCoordinator* _importCoordinator;
}

- (void)start {
  SafariDataImportExportViewController* viewController =
      [[SafariDataImportExportViewController alloc] init];
  viewController.actionHandler = self;
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  viewController.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(didTapCancelButton)];
  _navigationController.delegate = self;
  _navigationController.modalInPresentation = YES;
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  self.delegate = nil;
  _navigationController.delegate = nil;
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:nil];
  _navigationController = nil;
  [_importCoordinator stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  RecordActionOnSafariExportEducationScreen(
      SafariDataImportExportEducationAction::kGoToSetting);
  ios::provider::OpenSettingsToExportDataFromSafari();
}

- (void)confirmationAlertSecondaryAction {
  if (_importCoordinator) {
    return;
  }
  RecordActionOnSafariExportEducationScreen(
      SafariDataImportExportEducationAction::kContinue);
  _importCoordinator = [[SafariDataImportImportCoordinator alloc]
      initWithBaseNavigationController:_navigationController
                               browser:self.browser];
  _importCoordinator.delegate = self.delegate;
  [_importCoordinator start];
}

#pragma mark - UINavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
       didShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  CHECK_EQ(navigationController, _navigationController);
  if (_importCoordinator &&
      viewController == _navigationController.viewControllers[0]) {
    /// Handle user going back from import stage.
    RecordSafariDataImportTapsBackAtImportStage(_importCoordinator.importStage);
    [_importCoordinator stop];
    _importCoordinator = nil;
  }
}

#pragma mark - Private

// Dismisses the sheet.
- (void)didTapCancelButton {
  RecordActionOnSafariExportEducationScreen(
      SafariDataImportExportEducationAction::kCancel);
  [self.delegate safariDataImportCoordinatorWillDismissWorkflow:self];
}

@end
