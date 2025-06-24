// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_import_coordinator.h"

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_coordinator_transitioning_delegate.h"

@implementation SafariDataImportImportCoordinator {
  /// The view controller pushed onto the base navigation controller; user
  /// interacts with it to present other views.
  UIViewController* _mainViewController;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  [self configureMainViewController];
  [self.baseNavigationController pushViewController:_mainViewController
                                           animated:YES];
}

- (void)stop {
  self.transitioningDelegate = nil;
  _mainViewController = nil;
}

#pragma mark - Private

/// Configures the main view controller that will be pushed onto the navigation
/// stack.
- (void)configureMainViewController {
  /// TODO(crbug.com/420703283): Replace with the
  /// ConfirmationAlertViewController for the import data screen.
  UIViewController* viewController = [[UIViewController alloc] init];
  viewController.view.backgroundColor = UIColor.yellowColor;
  UIButton* primaryButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [primaryButton setTitle:@"Close" forState:UIControlStateNormal];
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
  viewController.navigationItem.hidesBackButton = NO;
  _mainViewController = viewController;
}

/// TODO(crbug.com/420703283): Remove.
- (void)primaryButtonTapped {
  [self.transitioningDelegate
      safariDataImportCoordinatorWillDismissWorkflow:self];
}

@end
