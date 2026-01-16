// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/synced_set_up_profile_agent.h"

#import <memory>

#import "base/callback_list.h"
#import "base/check.h"
#import "base/functional/bind.h"
#import "components/prefs/pref_service.h"
#import "components/sync_device_info/device_info.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "components/sync_device_info/local_device_info_provider.h"
#import "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"
#import "components/sync_preferences/cross_device_pref_tracker/timestamped_pref_value.h"
#import "components/sync_preferences/synced_set_up/utils.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/connection_information.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/synced_set_up_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/prefs/cross_device_pref_tracker/cross_device_pref_tracker_factory.h"
#import "ios/chrome/browser/sync/model/prefs/cross_device_pref_tracker/cross_device_pref_tracker_observer_bridge.h"
#import "ios/chrome/browser/synced_set_up/public/synced_set_up_metrics.h"
#import "ios/chrome/browser/synced_set_up/utils/utils.h"

namespace {

using ServiceStatus = ::sync_preferences::CrossDevicePrefTracker::ServiceStatus;

}  // namespace

@interface SyncedSetUpProfileAgent () <CrossDevicePrefTrackerObserver>
@end

@implementation SyncedSetUpProfileAgent {
  // Ensures the Synced Set Up confirmation is triggered only once per
  // foreground activation cycle. This prevents duplicate UIs in multi-window
  // scenarios (e.g., iPad split-screen).
  BOOL _activationAlreadyHandled;

  // Bridge to observe `CrossDevicePrefTracker` events.
  std::unique_ptr<CrossDevicePrefTrackerObserverBridge> _observer;

  // Callback that's invoked when the Profile is destroyed.
  base::CallbackListSubscription _subscription;
}

#pragma mark - SceneObservingProfileAgent

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  switch (level) {
    case SceneActivationLevelDisconnected:
    case SceneActivationLevelUnattached:
    case SceneActivationLevelBackground:
    case SceneActivationLevelForegroundInactive:
      // Reset when scenes become inactive or backgrounded, allowing the logic
      // to run again upon the next foreground activation.
      _activationAlreadyHandled = NO;
      break;
    case SceneActivationLevelForegroundActive:
      [self triggerSyncedSetUpIfEligibleWithSource:SyncedSetUpTriggerSource::
                                                       kSceneActivation];
      break;
  }
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage == ProfileInitStage::kFinal) {
    [self setUpObserverBridge];
    [self triggerSyncedSetUpIfEligibleWithSource:SyncedSetUpTriggerSource::
                                                     kSceneActivation];
  }
}

#pragma mark - CrossDevicePrefTrackerObserver

- (void)crossDevicePrefTracker:
            (sync_preferences::CrossDevicePrefTracker*)tracker
        serviceStatusDidChange:(ServiceStatus)status {
  if (status != ServiceStatus::kAvailable) {
    return;
  }

  [self triggerSyncedSetUpIfEligibleWithSource:SyncedSetUpTriggerSource::
                                                   kServiceStatusChange
                                       tracker:tracker];
}

- (void)
    crossDevicePrefTracker:(sync_preferences::CrossDevicePrefTracker*)tracker
       didChangeRemotePref:(std::string_view)prefName
                   toValue:
                       (const sync_preferences::TimestampedPrefValue&)prefValue
                fromDevice:(const syncer::DeviceInfo&)remoteDeviceInfo {
  [self triggerSyncedSetUpIfEligibleWithSource:SyncedSetUpTriggerSource::
                                                   kRemotePrefChange
                                       tracker:tracker];
}

#pragma mark - Private

// Convenience method for `-triggerSyncedSetUpIfEligibleWithSource:tracker`.
- (void)triggerSyncedSetUpIfEligibleWithSource:
    (SyncedSetUpTriggerSource)source {
  [self triggerSyncedSetUpIfEligibleWithSource:source tracker:nil];
}

// Evaluates all preconditions and triggers the Synced Set Up flow if
// applicable.
- (void)triggerSyncedSetUpIfEligibleWithSource:(SyncedSetUpTriggerSource)source
                                       tracker:(sync_preferences::
                                                    CrossDevicePrefTracker*)
                                                   tracker {
  CHECK(IsSyncedSetUpEnabled());

  if (_activationAlreadyHandled) {
    return;
  }

  PrefService* profilePrefService = self.profileState.profile->GetPrefs();
  if (!CanShowSyncedSetUp(profilePrefService)) {
    return;
  }

  // This agent must not initiate the Synced Set Up flow during First Run.
  if (self.profileState.appState.startupInformation.isFirstRun) {
    return;
  }

  SceneState* activeScene = GetEligibleSceneForSyncedSetUp(self.profileState);
  if (!activeScene) {
    return;
  }

  CommandDispatcher* dispatcher =
      activeScene.browserProviderInterface.mainBrowserProvider.browser
          ->GetCommandDispatcher();

  id<SyncedSetUpCommands> handler =
      HandlerForProtocol(dispatcher, SyncedSetUpCommands);
  if (!handler) {
    return;
  }

  if (![self hasRemotePrefsWithTracker:tracker]) {
    return;
  }

  LogSyncedSetUpTriggerSource(source);
  [handler showSyncedSetUpWithDismissalCompletion:nil];
  _activationAlreadyHandled = YES;
}

// Initializes the `CrossDevicePrefTracker` observer bridge.
- (void)setUpObserverBridge {
  _observer.reset();

  ProfileIOS* profile = self.profileState.profile;
  CHECK(profile);

  sync_preferences::CrossDevicePrefTracker* tracker =
      CrossDevicePrefTrackerFactory::GetForProfile(profile);

  if (tracker) {
    _observer =
        std::make_unique<CrossDevicePrefTrackerObserverBridge>(self, tracker);

    __weak __typeof(self) weakSelf = self;
    _subscription = profile->RegisterProfileDestroyedCallback(base::BindOnce(^{
      [weakSelf profileDestroyed];
    }));
  }
}

// Resets the observer bridge when the Profile is destroyed.
- (void)profileDestroyed {
  _observer.reset();
}

// Returns `YES` if there are valid remote prefs available.
// Uses `tracker` if provided; otherwise retrieves it from the factory.
- (BOOL)hasRemotePrefsWithTracker:
    (sync_preferences::CrossDevicePrefTracker*)tracker {
  ProfileIOS* profile = self.profileState.profile;
  CHECK(profile);

  if (!tracker) {
    tracker = CrossDevicePrefTrackerFactory::GetForProfile(profile);
  }

  if (!tracker || tracker->GetServiceStatus() != ServiceStatus::kAvailable) {
    return NO;
  }

  syncer::DeviceInfoSyncService* deviceInfoSyncService =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);

  if (!deviceInfoSyncService) {
    return NO;
  }

  syncer::LocalDeviceInfoProvider* localProvider =
      deviceInfoSyncService->GetLocalDeviceInfoProvider();

  if (!localProvider) {
    return NO;
  }

  auto remotePrefs =
      sync_preferences::synced_set_up::GetCrossDevicePrefsFromRemoteDevice(
          tracker, deviceInfoSyncService->GetDeviceInfoTracker(),
          localProvider->GetLocalDeviceInfo());

  return !remotePrefs.empty();
}

@end
