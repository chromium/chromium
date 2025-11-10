// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_coordinator.h"

#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
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
#import "ios/web/public/web_state.h"

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
  id<SnackbarCommands> snackbarCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);

  _mediator = [[SyncedSetUpMediator alloc]
          initWithPrefTracker:tracker
        authenticationService:authService
        accountManagerService:accountManagerService
        deviceInfoSyncService:deviceInfoSyncService
           profilePrefService:profile->GetPrefs()
              identityManager:identityManager
                 webStateList:self.browser->GetWebStateList()
            startupParameters:startupParameters
      snackbarCommandsHandler:snackbarCommandsHandler];

  _viewController = [[SyncedSetUpViewController alloc] init];
  _viewController.modalPresentationStyle = UIModalPresentationCustom;
  _viewController.transitioningDelegate = self;
  _mediator.consumer = _viewController;

  _mediator.delegate = self;
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

- (void)recordSyncedSetUpShown:(SyncedSetUpMediator*)mediator {
  CHECK_EQ(_mediator, mediator);
  PrefService* profilePrefService = self.profile->GetPrefs();
  CHECK(profilePrefService);

  int impressionCount =
      profilePrefService->GetInteger(prefs::kSyncedSetUpImpressionCount);
  profilePrefService->SetInteger(prefs::kSyncedSetUpImpressionCount,
                                 impressionCount + 1);
}

- (void)mediatorWillStartPostFirstRunFlow:(SyncedSetUpMediator*)mediator {
  CHECK_EQ(_mediator, mediator);
  [self showSyncedSetUpInterstitial];
}

- (void)mediatorWillStartFromUrlPage:(SyncedSetUpMediator*)mediator {
  CHECK_EQ(_mediator, mediator);
  [_mediator applyPrefs];
}

- (void)syncedSetUpMediatorDidComplete:(SyncedSetUpMediator*)mediator {
  CHECK_EQ(_mediator, mediator);
  [self.delegate syncedSetUpCoordinatorWantsToBeDismissed:self];
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

  base::UmaHistogramBoolean("IOS.SyncedSetUp.Interstitial.Shown", true);

  __weak __typeof(self) weakSelf = self;
  __weak __typeof(_mediator) weakMediator = _mediator;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:^{
                                        [weakMediator applyPrefs];
                                      }];

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf dismissSyncedSetUpInterstitial];
      }),
      kDismissalDelay);
}

// Dismisses the full-screen interstitial.
- (void)dismissSyncedSetUpInterstitial {
  if (_viewController.isBeingDismissed) {
    return;
  }

  __weak __typeof(_mediator) weakMediator = _mediator;

  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakMediator maybeShowSnackbar];
                         }];
}

@end
