// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_main_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/feature_list.h"
#import "ios/chrome/browser/passwords/model/features.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_coordinator_transitioning_delegate.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_export_coordinator.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_main_mediator.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_ui_handler.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_entry_point_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface SafariDataImportMainCoordinator () <
    ConfirmationAlertActionHandler,
    SafariDataImportCoordinatorTransitioningDelegate>

@end

@implementation SafariDataImportMainCoordinator {
  /// Mediator for the main workflow.
  SafariDataImportMainMediator* _mediator;
  /// View controller for the entry point of the Ssafari data import workflow.
  SafariDataImportEntryPointViewController* _viewController;
  /// Coordinator that displays the next step in the Safari data importing
  /// process. Its view controller will be presented on top of
  /// `_viewController`.
  SafariDataImportExportCoordinator* _exportCoordinator;
}

- (void)start {
  CHECK(base::FeatureList::IsEnabled(kImportPasswordsFromSafari));
  _viewController = [[SafariDataImportEntryPointViewController alloc] init];
  _viewController.modalInPresentation = YES;
  _viewController.actionHandler = self;
  PromosManager* promosManager =
      PromosManagerFactory::GetForProfile(self.profile);
  _mediator = [[SafariDataImportMainMediator alloc]
      initWithUIBlockerTarget:self.browser->GetSceneState()
                promosManager:promosManager];
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  __weak __typeof(self) weakSelf = self;
  SafariDataImportMainMediator* mediator = _mediator;
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [mediator disconnect];
                           [weakSelf.UIHandler safariDataImportDidDismiss];
                         }];
  _viewController = nil;
  [_exportCoordinator stop];
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
  [_mediator registerReminder];
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
