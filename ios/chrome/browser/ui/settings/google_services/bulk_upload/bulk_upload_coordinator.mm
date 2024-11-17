// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_view_controller_presentation_delegate.h"

@interface BulkUploadCoordinator () <
    BulkUploadMediatorDelegate,
    BulkUploadViewControllerPresentationDelegate>
@end

@implementation BulkUploadCoordinator {
  BulkUploadMediator* _mediator;
  BulkUploadViewController* _viewController;
  // Whether the view controller was dismissed outside of this coordinator.
  BOOL _viewControllerIsDismissed;
  id<SnackbarCommands> _snackbarCommandsHandler;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  return self;
}

- (void)dealloc {
  DCHECK(!_mediator);
}

- (void)start {
  [super start];
  base::RecordAction(base::UserMetricsAction("Signin_BulkUpload_Open"));
  _viewController = [[BulkUploadViewController alloc] init];
  _viewController.delegate = self;

  ProfileIOS* profile = self.browser->GetProfile();
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  _mediator = [[BulkUploadMediator alloc] initWithSyncService:syncService
                                              identityManager:identityManager];
  _mediator.delegate = self;
  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  DCHECK(_viewController);
  DCHECK(_mediator);
  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator.delegate = nil;
  _mediator = nil;
  _viewController.mutator = nil;
  _viewController.delegate = nil;
  if (!_viewControllerIsDismissed) {
    if (_viewController.presentingViewController) {
      [_viewController.presentingViewController
          dismissViewControllerAnimated:YES
                             completion:nil];
    }
  }
  _viewController = nil;
}

#pragma mark - Private

- (id<SnackbarCommands>)snackbarCommandsHandler {
  // Using lazy loading here to avoid potential crashes with SnackbarCommands
  // not being yet dispatched.
  if (!_snackbarCommandsHandler) {
    _snackbarCommandsHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), SnackbarCommands);
  }
  return _snackbarCommandsHandler;
}

#pragma mark - BulkUploadViewControllerPresentationDelegate

- (void)viewControllerWantsToBeDismissed:(BulkUploadViewController*)controller {
  [self.delegate bulkUploadCoordinatorShouldStop:self];
}

- (void)viewControllerIsBeingDismissed:(BulkUploadViewController*)controller {
  _viewControllerIsDismissed = YES;
  [self.delegate bulkUploadCoordinatorShouldStop:self];
}

#pragma mark - BulkUploadMediatorDelegate

- (void)mediatorWantsToBeDismissed:(BulkUploadMediator*)mediator {
  [self.delegate bulkUploadCoordinatorShouldStop:self];
}

- (void)displayInSnackbar:(NSString*)message {
  [self.snackbarCommandsHandler
      showSnackbarMessage:CreateSnackbarMessage(message)];
}

@end
