// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_main_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/feature_list.h"
#import "ios/chrome/browser/passwords/model/features.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_coordinator_transitioning_delegate.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_export_coordinator.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_ui_handler.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_entry_point_view_controller.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/scoped_ui_blocker.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface SafariDataImportMainCoordinator () <
    ConfirmationAlertActionHandler,
    SafariDataImportCoordinatorTransitioningDelegate>

@end

@implementation SafariDataImportMainCoordinator {
  /// View controller for the entry point of the Ssafari data import workflow.
  SafariDataImportEntryPointViewController* _viewController;
  /// Coordinator that displays the next step in the Safari data importing
  /// process. Its view controller will be presented on top of
  /// `_viewController`.
  SafariDataImportExportCoordinator* _exportCoordinator;
  /// UI blocker used while the workflow is presenting. This makes sure that the
  /// promos manager would not attempt to show another promo in the meantime.
  std::unique_ptr<ScopedUIBlocker> _UIBlocker;
}

- (void)start {
  /// TODO(crbug.com/420694579): Move UIBlocker to mediator.
  CHECK(base::FeatureList::IsEnabled(kImportPasswordsFromSafari));
  _UIBlocker = std::make_unique<ScopedUIBlocker>(self.browser->GetSceneState(),
                                                 UIBlockerExtent::kProfile);
  _viewController = [[SafariDataImportEntryPointViewController alloc] init];
  _viewController.actionHandler = self;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  __weak __typeof(self) weakSelf = self;
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf.UIHandler safariDataImportDidDismiss];
                         }];
  _viewController = nil;
  [_exportCoordinator stop];
  _UIBlocker.reset();
  self.delegate = nil;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  CHECK(!_exportCoordinator);
  _exportCoordinator = [[SafariDataImportExportCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser];
  _exportCoordinator.transitioningDelegate = self;
  [_exportCoordinator start];
}

- (void)confirmationAlertSecondaryAction {
  /// TODO(crbug.com/420703283): register reminder.
  [self.delegate safariImportWorkflowDidEndForCoordinator:self];
}

- (void)confirmationAlertDismissAction {
  [self.delegate safariImportWorkflowDidEndForCoordinator:self];
}

#pragma mark - SafariDataImportCoordinatorTransitioningDelegate

- (void)safariDataImportCoordinatorWillDismissWorkflow:
    (ChromeCoordinator*)coordinator {
  [self.delegate safariImportWorkflowDidEndForCoordinator:self];
}

@end
