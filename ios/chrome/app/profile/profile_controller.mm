// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/profile_controller.h"

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/notreached.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/deferred_initialization_task_names.h"
#import "ios/chrome/app/profile/application_storage_metrics.h"
#import "ios/chrome/app/profile/certificate_policy_profile_agent.h"
#import "ios/chrome/app/profile/docking_promo_profile_agent.h"
#import "ios/chrome/app/profile/features.h"
#import "ios/chrome/app/profile/first_run_profile_agent.h"
#import "ios/chrome/app/profile/identity_confirmation_profile_agent.h"
#import "ios/chrome/app/profile/post_restore_profile_agent.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/app/profile/search_engine_choice_profile_agent.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_buildflags.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_profile_agent.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service_factory.h"
#import "ios/chrome/browser/external_files/model/external_file_remover.h"
#import "ios/chrome/browser/external_files/model/external_file_remover_factory.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service_factory.h"
#import "ios/chrome/browser/profile_metrics/model/profile_activity_profile_agent.h"
#import "ios/chrome/browser/search_engines/model/extension_search_engine_data_updater.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/device_orientation/scoped_force_portrait_orientation.h"
#import "ios/components/cookie_util/cookie_util.h"

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
#import "ios/chrome/browser/credential_provider/model/credential_provider_util.h"  // nogncheck
#import "ios/chrome/browser/passwords/model/password_manager_util_ios.h"  // nogncheck
#import "ios/chrome/browser/sync/model/sync_service_factory.h"  // nogncheck
#endif

namespace {

// The delay for cleaning external files.
constexpr base::TimeDelta kExternalFilesCleanupDelay = base::Minutes(1);

// Name of the block deleting the leftover session state files.
NSString* const kStartupPurgeUnassociatedData = @"StartupPurgeUnassociatedData";

// Name of the block creating the MailtoHandlerService instance.
NSString* const kStartupCreateMailtoHandlerService =
    @"StartupCreateMailtoHandlerService";

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
// Name of the block cleaning up the favicons.
NSString* const kStartupCleanupFavicons = @"StartupCleanupFavicons";
#endif

#if !TARGET_IPHONE_SIMULATOR
// Name of the block logging the storage metrics.
NSString* const kStartupLogStorageMetrics = @"StartupLogStorageMetrics";

// The minimum amount of time between calculating and logging metrics about
// the amount of device storage space used by Chrome.
constexpr base::TimeDelta kMinimumTimeBetweenDocumentsSizeLogging =
    base::Days(14);

// Returns whether the storage metrics should be logged.
bool ShouldLogStorageMetrics(PrefService* pref_service) {
  const base::Time last_logged =
      pref_service->GetTime(prefs::kLastApplicationStorageMetricsLogTime);

  return last_logged == base::Time() ||
         base::Time::Now() - last_logged <
             kMinimumTimeBetweenDocumentsSizeLogging;
}
#endif

}  // namespace

@interface ProfileController () <ProfileStateObserver>
@end

@implementation ProfileController {
  // Used to force the device orientation in portrait mode on iPhone.
  std::unique_ptr<ScopedForcePortraitOrientation> _scopedForceOrientation;

  // The ExtensionSearchEngineDataUpdater that ensure the changes to the
  // default search engine are propagated to the extensions.
  std::unique_ptr<ExtensionSearchEngineDataUpdater> _searchEngineDataUpdater;
}

- (instancetype)initWithAppState:(AppState*)appState {
  if ((self = [super init])) {
    _state = [[ProfileState alloc] initWithAppState:appState];
    _scopedForceOrientation = ForcePortraitOrientationOnIphone(appState);
    [_state addObserver:self];
  }
  return self;
}

- (void)shutdown {
  // Delete the ExtensionSearchEngineDataUpdater if it has been created.
  _searchEngineDataUpdater.reset();

  // Under the UIScene API, -sceneDidDisconnect: notification is not sent to
  // the UISceneDelegate on app termination. Mark all connected scene states
  // as disconnected in order to allow the services to properly unregister
  // their observers and tear down the remaining UI.
  for (SceneState* sceneState in _state.connectedScenes) {
    sceneState.activationLevel = SceneActivationLevelDisconnected;
  }

  // Cancel any pending deferred startup tasks (the profile is shutting
  // down, so there is no point in running them).
  [_state.deferredRunner cancelAllBlocks];
}

#pragma mark ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  switch (nextInitStage) {
    case ProfileInitStage::kStart:
      NOTREACHED();

    case ProfileInitStage::kLoadProfile:
      break;

    case ProfileInitStage::kProfileLoaded:
      [self attachProfileAgents];
      break;

    case ProfileInitStage::kPrepareUI:
      break;

    case ProfileInitStage::kUIReady:
      // SceneController uses this stage to create the normal UI if needed.
      // There is no specific agent (other than SceneController) handling
      // this stage.
      [profileState queueTransitionToNextInitStage];
      break;

    case ProfileInitStage::kFirstRun:
      break;

    case ProfileInitStage::kChoiceScreen:
      break;

    case ProfileInitStage::kNormalUI:
      // Stop forcing the portrait orientation once the normal UI is presented.
      _scopedForceOrientation.reset();
      break;

    case ProfileInitStage::kFinal:
      break;
  }
}

- (void)profileState:(ProfileState*)profileState
    firstSceneHasInitializedUI:(SceneState*)sceneState {
  DCHECK_GE(profileState.initStage, ProfileInitStage::kUIReady);
  [self startUpAfterFirstWindowCreated];
}

#pragma mark Private methods

- (void)attachProfileAgents {
  // TODO(crbug.com/355142171): Remove the DiscoverFeedProfileAgent?
  [_state addAgent:[[DiscoverFeedProfileAgent alloc] init]];

  [_state addAgent:[[CertificatePolicyProfileAgent alloc] init]];
  [_state addAgent:[[FirstRunProfileAgent alloc] init]];
  [_state addAgent:[[IdentityConfirmationProfileAgent alloc] init]];
  [_state addAgent:[[ProfileActivityProfileAgent alloc] init]];
  [_state addAgent:[[PostRestoreProfileAgent alloc] init]];
  [_state addAgent:[[SearchEngineChoiceProfileAgent alloc] init]];

  if (IsDockingPromoEnabled()) {
    switch (DockingPromoExperimentTypeEnabled()) {
      case DockingPromoDisplayTriggerArm::kDuringFRE:
        break;
      case DockingPromoDisplayTriggerArm::kAfterFRE:
      case DockingPromoDisplayTriggerArm::kAppLaunch:
        [_state addAgent:[[DockingPromoProfileAgent alloc] init]];
        break;
    }
  }
}

- (void)startUpAfterFirstWindowCreated {
  [self scheduleRemoveExternalFiles];
  [self scheduleCreateSearchEngineDataUpdater];
  [self scheduleClearingSessionCookies];
  [self scheduleCleanupSessionStateCache];
  [self scheduleCreateMailtoHandlerService];
  [self scheduleCleanupFavicons];
  [self scheduleLogStorageMetrics];

  // The UI is created when a window is in the foreground and the Profile
  // initialisation has progressed enough. Since the window coming to the
  // foreground could have happened a while ago, notify of the foreground
  // event at this point.
  [self applicationWillEnterForeground];
}

- (void)applicationWillEnterForeground {
  DCHECK(_state.profile);
  enterprise_idle::IdleServiceFactory::GetForProfile(_state.profile)
      ->OnApplicationWillEnterForeground();
}

#pragma mark Deferred initialisation tasks scheduling

// Schedules external files removal.
- (void)scheduleRemoveExternalFiles {
  DCHECK(_state.profile);
  ExternalFileRemoverFactory::GetForProfile(_state.profile)
      ->RemoveAfterDelay(kExternalFilesCleanupDelay, base::DoNothing());
}

// Schedules installation of an ExtensionSearchEngineDataUpdater to track
// the changes to the default search engine.
- (void)scheduleCreateSearchEngineDataUpdater {
  DCHECK(_state.deferredRunner);
  __weak ProfileController* weakSelf = self;
  [_state.deferredRunner enqueueBlockNamed:kStartupInitPrefObservers
                                     block:^{
                                       [weakSelf createSearchEngineDataUpdater];
                                     }];
}

// Schedules clearing the session cookies.
- (void)scheduleClearingSessionCookies {
  DCHECK(_state.profile);
  ProfileIOS* profile = _state.profile;
  if (cookie_util::ShouldClearSessionCookies(profile->GetPrefs())) {
    cookie_util::ClearSessionCookies(profile);
    if (profile->HasOffTheRecordProfile()) {
      cookie_util::ClearSessionCookies(profile->GetOffTheRecordProfile());
    }
  }
}

// Schedules deleting leftover session state cache files.
- (void)scheduleCleanupSessionStateCache {
  DCHECK(_state.deferredRunner);
  __weak ProfileController* weakSelf = self;
  [_state.deferredRunner enqueueBlockNamed:kStartupPurgeUnassociatedData
                                     block:^{
                                       [weakSelf cleanupSessionStateCache];
                                     }];
}

// Schedules creating the MailtoHandlerService instance.
- (void)scheduleCreateMailtoHandlerService {
  DCHECK(_state.deferredRunner);
  __weak ProfileController* weakSelf = self;
  [_state.deferredRunner enqueueBlockNamed:kStartupCreateMailtoHandlerService
                                     block:^{
                                       [weakSelf createMailtoHandlerService];
                                     }];
}

// Schedules cleaning up favicons.
- (void)scheduleCleanupFavicons {
#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
  DCHECK(_state.deferredRunner);
  __weak ProfileController* weakSelf = self;
  [_state.deferredRunner enqueueBlockNamed:kStartupCleanupFavicons
                                     block:^{
                                       [weakSelf cleanupFavicons];
                                     }];
#endif
}

// Schedules logging the storage metrics.
- (void)scheduleLogStorageMetrics {
#if !TARGET_IPHONE_SIMULATOR
  if (!base::FeatureList::IsEnabled(kLogApplicationStorageSizeMetrics)) {
    return;
  }

  DCHECK(_state.deferredRunner);
  __weak ProfileController* weakSelf = self;
  [_state.deferredRunner enqueueBlockNamed:kStartupLogStorageMetrics
                                     block:^{
                                       [weakSelf logStorageMetrics];
                                     }];
#endif
}

#pragma mark Deferred initialisation tasks

// Ensures changes to the default search engine are propagated to extensions.
- (void)createSearchEngineDataUpdater {
  DCHECK(_state.profile);
  _searchEngineDataUpdater = std::make_unique<ExtensionSearchEngineDataUpdater>(
      ios::TemplateURLServiceFactory::GetForProfile(_state.profile));
}

// Ensures obsolete session state cache files are deleted.
- (void)cleanupSessionStateCache {
  DCHECK(_state.profile);
  SessionRestorationServiceFactory::GetForProfile(_state.profile)
      ->PurgeUnassociatedData(base::DoNothing());
}

// Ensures the MailtoHandlerService is instantiated.
- (void)createMailtoHandlerService {
  DCHECK(_state.profile);
  std::ignore = MailtoHandlerServiceFactory::GetForProfile(_state.profile);
}

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
// Ensures favicons are cleaned up.
- (void)cleanupFavicons {
  DCHECK(_state.profile);
  ProfileIOS* profile = _state.profile;

  // Only use the fallback to the Google server when fetching favicons for
  // normal encryption users saving to the account, because they are the only
  // users who consented to share data to Google.
  const bool fallbackToGoogleServer =
      password_manager_util::IsSavingPasswordsToAccountWithNormalEncryption(
          SyncServiceFactory::GetForProfile(profile));

  if (fallbackToGoogleServer) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&UpdateFaviconsStorageForProfile, profile->AsWeakPtr(),
                       fallbackToGoogleServer));
  }
}
#endif  // BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)

#if !TARGET_IPHONE_SIMULATOR
// Logs storage metrics.
- (void)logStorageMetrics {
  DCHECK(_state.profile);
  ProfileIOS* profile = _state.profile;
  PrefService* prefService = profile->GetPrefs();
  if (!ShouldLogStorageMetrics(prefService)) {
    return;
  }

  prefService->SetTime(prefs::kLastApplicationStorageMetricsLogTime,
                       base::Time::Now());
  LogApplicationStorageMetrics(profile->GetStatePath(),
                               profile->GetOffTheRecordStatePath());
}
#endif  // !TARGET_IPHONE_SIMULATOR

@end
