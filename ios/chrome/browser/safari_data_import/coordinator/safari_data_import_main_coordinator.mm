// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_main_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/feature_list.h"
#import "base/ios/block_types.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_child_coordinator_delegate.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_entry_point_mediator.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_export_coordinator.h"
#import "ios/chrome/browser/safari_data_import/model/features.h"
#import "ios/chrome/browser/safari_data_import/public/metrics.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_entry_point.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_ui_handler.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_entry_point_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface SafariDataImportMainCoordinator () <
    ConfirmationAlertActionHandler,
    SafariDataImportChildCoordinatorDelegate>

@end

@implementation SafariDataImportMainCoordinator {
  /// How the Safari data workflow is started.
  SafariDataImportEntryPoint _entryPoint;
  /// Mediator for the main workflow.
  SafariDataImportEntryPointMediator* _mediator;
  /// View controller for the entry point of the Ssafari data import workflow.
  SafariDataImportEntryPointViewController* _viewController;
  /// The navigation controller for the view controller.
  UINavigationController* _navigationController;
  /// Coordinator that displays the next step in the Safari data importing
  /// process. Its view controller will be presented on top of
  /// `_viewController`.
  SafariDataImportExportCoordinator* _exportCoordinator;
}

- (instancetype)initFromEntryPoint:(SafariDataImportEntryPoint)entryPoint
            withBaseViewController:(UIViewController*)viewController
                           browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _entryPoint = entryPoint;
  }
  return self;
}

- (void)start {
  if (!self.profile) {
    return;
  }
  CHECK(ShouldShowSafariDataImportEntryPoint(self.profile->GetPrefs()));
  _viewController = [[SafariDataImportEntryPointViewController alloc] init];
  _viewController.showReminderButton =
      _entryPoint != SafariDataImportEntryPoint::kSetting;
  _viewController.modalInPresentation = YES;
  _viewController.actionHandler = self;
  PromosManager* promosManager =
      PromosManagerFactory::GetForProfile(self.profile);
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(self.profile);
  _mediator = [[SafariDataImportEntryPointMediator alloc]
       initWithUIBlockerTarget:self.browser->GetSceneState()
                 promosManager:promosManager
      featureEngagementTracker:tracker];
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  id<SafariDataImportUIHandler> UIHandler = self.UIHandler;
  SafariDataImportEntryPointMediator* mediator = _mediator;
  ProceduralBlock dismissCompletionHandler = ^{
    [mediator disconnect];
    [UIHandler safariDataImportDidDismiss];
  };
  if (_navigationController.presentingViewController) {
    [_navigationController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:dismissCompletionHandler];
  } else {
    dismissCompletionHandler();
  }
  _viewController = nil;
  _navigationController = nil;
  [_exportCoordinator stop];
  self.delegate = nil;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  if (_exportCoordinator) {
    return;
  }
  RecordSafariImportActionOnEntryPoint(
      SafariDataImportEntryPointAction::kImport, _entryPoint);
  [_mediator notifyUsedOrDismissed];
  _exportCoordinator = [[SafariDataImportExportCoordinator alloc]
      initWithBaseViewController:_navigationController
                         browser:self.browser];
  _exportCoordinator.delegate = self;
  [_exportCoordinator start];
}

- (void)confirmationAlertSecondaryAction {
  RecordSafariImportActionOnEntryPoint(
      SafariDataImportEntryPointAction::kDismiss, _entryPoint);
  [_mediator notifyUsedOrDismissed];
  [self.delegate safariImportWorkflowDidEndForCoordinator:self];
}

- (void)confirmationAlertTertiaryAction {
  CHECK(_viewController.showReminderButton);
  RecordSafariImportActionOnEntryPoint(
      SafariDataImportEntryPointAction::kRemindMeLater, _entryPoint);
  [_mediator registerReminder];
  [self.delegate safariImportWorkflowDidEndForCoordinator:self];
}

#pragma mark - SafariDataImportChildCoordinatorDelegate

- (void)safariDataImportCoordinatorWillDismissWorkflow:
    (ChromeCoordinator*)coordinator {
  [self.delegate safariImportWorkflowDidEndForCoordinator:self];
}

@end
