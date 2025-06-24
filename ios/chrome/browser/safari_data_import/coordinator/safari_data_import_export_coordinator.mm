// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_export_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_import_coordinator.h"

@interface SafariDataImportExportCoordinator () <UINavigationControllerDelegate>

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
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:[self viewController]];
  _navigationController.delegate = self;
  _navigationController.modalInPresentation = YES;
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  self.transitioningDelegate = nil;
  _navigationController.delegate = nil;
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:nil];
  _navigationController = nil;
  [_importCoordinator stop];
}

#pragma mark - UINavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
       didShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  CHECK_EQ(navigationController, _navigationController);
  if (viewController == _navigationController.viewControllers[0]) {
    /// Handle user going back from import stage.
    [_importCoordinator stop];
    _importCoordinator = nil;
  }
}

#pragma mark - Private

/// Retrieves the view controller to be displayed.
/// TODO(crbug.com/420703283): Replace with the
/// ConfirmationAlertViewController for the import data screen.
- (UIViewController*)viewController {
  UIViewController* viewController = [[UIViewController alloc] init];
  viewController.view.backgroundColor = UIColor.whiteColor;
  UIButton* primaryButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [primaryButton setTitle:@"Proceed to Import Data"
                 forState:UIControlStateNormal];
  [primaryButton addTarget:self
                    action:@selector(primaryButtonTapped)
          forControlEvents:UIControlEventTouchUpInside];
  primaryButton.translatesAutoresizingMaskIntoConstraints = NO;
  [viewController.view addSubview:primaryButton];
  [NSLayoutConstraint activateConstraints:@[
    [primaryButton.centerXAnchor
        constraintEqualToAnchor:viewController.view.centerXAnchor],
    [primaryButton.centerYAnchor
        constraintEqualToAnchor:viewController.view.centerYAnchor]
  ]];
  return viewController;
}

/// TODO(crbug.com/420703283): Remove.
- (void)primaryButtonTapped {
  CHECK(!_importCoordinator);
  _importCoordinator = [[SafariDataImportImportCoordinator alloc]
      initWithBaseNavigationController:_navigationController
                               browser:self.browser];
  _importCoordinator.transitioningDelegate = self.transitioningDelegate;
  [_importCoordinator start];
}

@end
