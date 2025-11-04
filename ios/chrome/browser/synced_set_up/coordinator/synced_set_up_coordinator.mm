// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_coordinator.h"

#import "base/functional/bind.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/prefs/cross_device_pref_tracker/cross_device_pref_tracker_factory.h"
#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_coordinator_delegate.h"
#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_mediator.h"
#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_mediator_delegate.h"
#import "ios/chrome/browser/synced_set_up/ui/synced_set_up_animator.h"
#import "ios/chrome/browser/synced_set_up/ui/synced_set_up_view_controller.h"

namespace {

// Time delay before automatically dismissing the Synced Set Up interstitial.
constexpr base::TimeDelta kDismissalDelay = base::Seconds(5);

}  // namespace

@interface SyncedSetUpCoordinator () <SyncedSetUpMediatorDelegate,
                                      UIViewControllerTransitioningDelegate>
@end

@implementation SyncedSetUpCoordinator {
  // Mediator for Synced Set Up. Used to determine and apply a set of synced
  // prefs to the local device.
  SyncedSetUpMediator* _mediator;
  // View controller responsible for displaying the Synced Set Up interstitial.
  SyncedSetUpViewController* _viewController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  ProfileIOS* profile = self.profile;

  sync_preferences::CrossDevicePrefTracker* tracker =
      CrossDevicePrefTrackerFactory::GetForProfile(profile);
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  syncer::DeviceInfoSyncService* deviceInfoSyncService =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);
  AppStartupParameters* startupParameters =
      self.browser->GetSceneState().controller.startupParameters;
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);

  _mediator =
      [[SyncedSetUpMediator alloc] initWithPrefTracker:tracker
                                 authenticationService:authService
                                 accountManagerService:accountManagerService
                                 deviceInfoSyncService:deviceInfoSyncService
                                    profilePrefService:profile->GetPrefs()
                                     startupParameters:startupParameters
                                       identityManager:identityManager];
  _mediator.delegate = self;

  _viewController = [[SyncedSetUpViewController alloc] init];
  _viewController.modalPresentationStyle = UIModalPresentationCustom;
  _viewController.transitioningDelegate = self;

  _mediator.consumer = _viewController;
}

- (void)stop {
  if (!_viewController.isBeingDismissed) {
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }

  _mediator.delegate = nil;
  _mediator.consumer = nil;
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
}

#pragma mark - SyncedSetUpMediatorDelegate

- (void)syncedSetUpMediatorDidComplete:(SyncedSetUpMediator*)mediator {
  CHECK_EQ(_mediator, mediator);
  [self dismissSyncedSetUp];
}

#pragma mark - UIViewControllerTransitioningDelegate

// Called when the view controller is being presented.
- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForPresentedController:(UIViewController*)presented
                         presentingController:(UIViewController*)presenting
                             sourceController:(UIViewController*)source {
  return [[SyncedSetUpAnimator alloc] initForPresenting:YES];
}

// Called when the view controller is being dismissed.
- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  return [[SyncedSetUpAnimator alloc] initForPresenting:NO];
}

#pragma mark - Private Methods

// Shows the full-screen welcome interstitial for the Synced Set Up flow.
- (void)showSyncedSetUpInterstitial {
  if (!_viewController || _viewController.presentingViewController) {
    return;
  }

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];

  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf dismissSyncedSetUp];
      }),
      kDismissalDelay);
}

// Dismisses the full-screen interstitial.
- (void)dismissSyncedSetUp {
  if (_viewController.isBeingDismissed) {
    return;
  }

  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
}

@end
