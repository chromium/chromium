// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/profile_controller.h"

#import <algorithm>
#import <memory>
#import <utility>

#import "base/critical_closure.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/functional/concurrent_closures.h"
#import "base/memory/scoped_refptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/time/time.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/language/core/browser/language_usage_metrics.h"
#import "components/language/core/browser/pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/translate/core/browser/translate_metrics_logger_impl.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
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
#import "ios/chrome/app/profile/session_metrics_profile_agent.h"
#import "ios/chrome/app/profile/welcome_back_screen_profile_agent.h"
#import "ios/chrome/app/spotlight/spotlight_manager.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_buildflags.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service_factory.h"
#import "ios/chrome/browser/external_files/model/external_file_remover.h"
#import "ios/chrome/browser/external_files/model/external_file_remover_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service_factory.h"
#import "ios/chrome/browser/profile_metrics/model/profile_activity_profile_agent.h"
#import "ios/chrome/browser/reading_list/model/reading_list_download_service.h"
#import "ios/chrome/browser/reading_list/model/reading_list_download_service_factory.h"
#import "ios/chrome/browser/search_engines/model/extension_search_engine_data_updater.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_constants.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/share_extension/model/share_extension_service.h"
#import "ios/chrome/browser/share_extension/model/share_extension_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/profile/scoped_profile_keep_alive_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/web_state_list/model/session_metrics.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/components/cookie_util/cookie_util.h"
#import "ios/public/provider/chrome/browser/raccoon/raccoon_api.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/cookies/cookie_store.h"
#import "net/url_request/url_request_context.h"
#import "net/url_request/url_request_context_getter.h"
#import "ui/base/device_form_factor.h"

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
#import "ios/chrome/browser/credential_provider/model/credential_provider_service_factory.h"  // nogncheck
#import "ios/chrome/browser/credential_provider/model/credential_provider_util.h"  // nogncheck
#import "ios/chrome/browser/passwords/model/password_manager_util_ios.h"  // nogncheck
#import "ios/chrome/browser/sync/model/sync_service_factory.h"  // nogncheck
#endif

namespace {

using SessionIds = ProfileAttributesIOS::SessionIds;

// The delay for cleaning external files.
constexpr base::TimeDelta kExternalFilesCleanupDelay = base::Minutes(1);

// Name of the block deleting the leftover session state files.
NSString* const kStartupPurgeUnassociatedData = @"StartupPurgeUnassociatedData";

// Name of the block creating the MailtoHandlerService instance.
NSString* const kStartupCreateMailtoHandlerService =
    @"StartupCreateMailtoHandlerService";

// Name of the block initializing the ReadingListDownloadService instance.
NSString* const kStartupInitReadingListDownloadService =
    @"StartupInitReadingListDownloadService";

// Name of the block that resynchronize the Spotlight index.
NSString* const kStartResyncSpotlightIndex = @"StartResyncSpotlightIndex";

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

// Flushes the CookieStore on the IO thread and invokes `closure` on completion
// on an unspecified sequence.
void FlushCookieStoreOnIOThread(
    scoped_refptr<net::URLRequestContextGetter> getter,
    base::OnceClosure closure) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  getter->GetURLRequestContext()->cookie_store()->FlushStore(
      std::move(closure));
}

// Purges data for discarded sessions `session_ids` relative to profile's
// storage paths (regulard and off-the-record).
void PurgeDataForSessions(const SessionIds& session_ids,
                          const std::array<base::FilePath, 2>& storage_paths) {
  const std::array<base::FilePath::StringViewType, 3> directories = {
      kLegacySessionsDirname,
      kSessionRestorationDirname,
      FILE_PATH_LITERAL("Snapshots"),
  };

  for (const base::FilePath& storage_path : storage_paths) {
    for (const std::string_view directory : directories) {
      const base::FilePath sub_directory = storage_path.Append(directory);
      for (const std::string& session : session_ids) {
        const base::FilePath path = sub_directory.Append(session);
        std::ignore = base::DeletePathRecursively(path);
      }
    }
  }
}

// Removes `session_ids` from the set of sessions to discard from `attrs`.
void RemoveSessionsFromSessionsToDiscard(const SessionIds& session_ids,
                                         ProfileAttributesIOS& attrs) {
  SessionIds discarded_sessions;
  std::ranges::set_difference(
      attrs.GetDiscardedSessions(), session_ids,
      std::inserter(discarded_sessions, discarded_sessions.end()));
  attrs.SetDiscardedSessions(discarded_sessions);
}

// Record whether data has been purged for a scene with the same identifier.
//
// This method is only called once per-Scene, either when it is connected to
// the Profile (if InitStage is greater than  kPurgeDiscardedSessionsData) or
// after the data has been purged.
//
// It is used to detect if data is lost due to the possible bug in UIKit where
// the method -application:didDiscardSceneSessions: is called with references
// to scenes that are still connected.
//
// See https://crbug.com/392575873 for more details.
void RecordDiscardedSceneConnectedAfterBeingPurged(
    const SessionIds& purged_identifiers,
    std::string_view scene_identifier) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }

  const auto iterator = purged_identifiers.find(scene_identifier);
  base::UmaHistogramBoolean(
      "IOS.Sessions.DiscardedSceneConnectedAfterBeingPurged",
      iterator != purged_identifiers.end());
}

}  // namespace

@interface ProfileController () <ProfileStateObserver, SceneStateObserver>
@end

@implementation ProfileController {
  // The ExtensionSearchEngineDataUpdater that ensure the changes to the
  // default search engine are propagated to the extensions.
  std::unique_ptr<ExtensionSearchEngineDataUpdater> _searchEngineDataUpdater;

  // Responsible for indexing chrome links (such as bookmarks, ...) in system
  // Spotlight index for the given profile.
  SpotlightManager* _spotlightManager;

  // ProfileManager used to load the profile and its attributes.
  raw_ptr<ProfileManagerIOS> _profileManager;

  // Flag recording whether the cookies are currently being saved or not.
  BOOL _savingCookies;

  // For RecordDiscardedSceneConnectedAfterBeingPurged(), removed once the
  // investigation is complete (see https://crbug.com/392575873 for details).
  //
  // Contains the list of session identifiers whose data have been purged
  // during the current profile startup (used to detect whether data loss
  // occurred).
  SessionIds _purgedSessionIdentifiers;

  // Keep the loaded profile alive.
  ScopedProfileKeepAliveIOS _scopedProfileKeepAlive;
}

- (instancetype)initWithAppState:(AppState*)appState
                 metricsMediator:(MetricsMediator*)metricsMediator {
  if ((self = [super init])) {
    _state = [[ProfileState alloc] initWithAppState:appState];
    _metricsMediator = metricsMediator;
    [_state addObserver:self];

    // Inform the AppState of the ProfileState creation.
    [appState profileStateCreated:_state];
  }
  return self;
}

- (void)loadProfileNamed:(std::string_view)profileName
            usingManager:(ProfileManagerIOS*)manager {
  CHECK_EQ(_state.initStage, ProfileInitStage::kStart);

  // Store the pointer to the profile manager.
  _profileManager = manager;

  // Transition to the next init stage before loading the profile as the
  // load may be synchronous (if the profile has already been loaded for
  // background operation).
  [_state queueTransitionToNextInitStage];

  __weak ProfileController* weakSelf = self;
  _profileManager->CreateProfileAsync(
      profileName, base::BindOnce(^(ScopedProfileKeepAliveIOS keep_alive) {
        [weakSelf profileLoaded:std::move(keep_alive)];
      }));
}

- (void)shutdown {
  // Stop and destroy the profile specific service helpers (SpotlightManager,
  // ExtensionSearchEngineDataUpdater, ...) if they have been created.
  _searchEngineDataUpdater.reset();
  [_spotlightManager shutdown];
  _spotlightManager = nil;

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

  // Inform the AppState of the ProfileState destruction.
  [_state.appState profileStateDestroyed:_state];

  // Clear the -profile property of ProfileState before unloading the object.
  [_state setProfile:nullptr];

  // Destroy the ScopedProfileKeepAlive which will allow the ProfileManagerIOS
  // to unload the profile (if this was the last object keeping it alive).
  _scopedProfileKeepAlive.Reset();
}

#pragma mark ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    willTransitionToInitStage:(ProfileInitStage)nextInitStage
                fromInitStage:(ProfileInitStage)fromInitStage {
  switch (nextInitStage) {
    case ProfileInitStage::kStart:
      NOTREACHED();

    case ProfileInitStage::kLoadProfile:
    case ProfileInitStage::kMigrateStorage:
    case ProfileInitStage::kPurgeDiscardedSessionsData:
    case ProfileInitStage::kProfileLoaded:
    case ProfileInitStage::kPrepareUI:
      // Nothing to do.
      break;

    case ProfileInitStage::kUIReady:
      [self startUpBeforeFirstWindowCreated];
      break;

    case ProfileInitStage::kFirstRun:
    case ProfileInitStage::kChoiceScreen:
    case ProfileInitStage::kNormalUI:
    case ProfileInitStage::kFinal:
      // Nothing to do.
      break;
  }
}

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  switch (nextInitStage) {
    case ProfileInitStage::kStart:
      NOTREACHED();

    case ProfileInitStage::kLoadProfile:
      // Nothing to do.
      break;

    case ProfileInitStage::kMigrateStorage:
      [self migrateSessionStorageIfNeeded];
      break;

    case ProfileInitStage::kPurgeDiscardedSessionsData:
      [self purgeDiscardedSessionsData];
      break;

    case ProfileInitStage::kProfileLoaded:
      [self startUpBrowserBackgroundInitialization];
      [profileState queueTransitionToNextInitStage];
      break;

    case ProfileInitStage::kPrepareUI:
      [self maybeContinueForegroundInitialization];
      break;

    case ProfileInitStage::kUIReady:
      // SceneController uses this stage to create the normal UI if needed.
      // There is no specific agent (other than SceneController) handling
      // this stage.
      [profileState queueTransitionToNextInitStage];
      break;

    case ProfileInitStage::kFirstRun:
    case ProfileInitStage::kChoiceScreen:
      // Nothing to do.
      break;

    case ProfileInitStage::kNormalUI:
      [profileState queueTransitionToNextInitStage];
      break;

    case ProfileInitStage::kFinal:
      // Nothing to do.
      break;
  }
}

- (void)profileState:(ProfileState*)profileState
    firstSceneHasInitializedUI:(SceneState*)sceneState {
  DCHECK_GE(profileState.initStage, ProfileInitStage::kUIReady);
  [self startUpAfterFirstWindowCreated];
}

- (void)profileState:(ProfileState*)profileState
      sceneConnected:(SceneState*)sceneState {
  const ProfileInitStage initStage = _state.initStage;
  if (initStage > ProfileInitStage::kPurgeDiscardedSessionsData) {
    RecordDiscardedSceneConnectedAfterBeingPurged(_purgedSessionIdentifiers,
                                                  sceneState.sceneSessionID);
  }

  if (initStage >= ProfileInitStage::kUIReady) {
    return;
  }

  [sceneState addObserver:self];
}

#pragma mark SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  switch (level) {
    case SceneActivationLevelUnattached:
      break;

    case SceneActivationLevelDisconnected:
      [sceneState removeObserver:self];
      break;

    case SceneActivationLevelBackground:
      break;

    case SceneActivationLevelForegroundInactive:
    case SceneActivationLevelForegroundActive:
      [self maybeContinueForegroundInitialization];
      break;
  }
}

#pragma mark AppLifetimeObserver

- (void)applicationWillResignActive:(UIApplication*)application {
  // Nothing to do if the profile is not yet fully loaded.
  if (_state.initStage < ProfileInitStage::kPrepareUI) {
    return;
  }

  DCHECK(_state.profile);
  ProfileIOS* profile = _state.profile;

  // Record session metrics for the regular profile and off-the-record profile
  // (if it exists, do not force its creation).
  SessionMetrics::FromProfile(profile)->RecordAndClearSessionMetrics(
      MetricsToRecordFlags::kActivatedTabCount);
  if (profile->HasOffTheRecordProfile()) {
    SessionMetrics::FromProfile(profile->GetOffTheRecordProfile())
        ->RecordAndClearSessionMetrics(
            MetricsToRecordFlags::kActivatedTabCount);
  }
}

- (void)applicationWillTerminate:(UIApplication*)application {
  // Nothing to do if the profile is not yet fully loaded.
  if (_state.initStage < ProfileInitStage::kPrepareUI) {
    return;
  }

  DCHECK(_state.profile);

  // Halt the tabs, so any outstanding requests get cleaned up, without actually
  // closing the tabs. Set the BVC to inactive to cancel all the dialogs.
  for (Browser* browser :
       BrowserListFactory::GetForProfile(_state.profile)
           ->BrowsersOfType(BrowserList::BrowserType::kAll)) {
    if (auto* agent = WebUsageEnablerBrowserAgent::FromBrowser(browser)) {
      agent->SetWebUsageEnabled(false);
    }
  }
}

- (void)applicationDidEnterBackground:(UIApplication*)application
                         memoryHelper:(MemoryWarningHelper*)memoryHelper {
  // Nothing to do if the profile is not yet fully loaded.
  if (_state.initStage < ProfileInitStage::kPrepareUI) {
    return;
  }

  DCHECK(_state.profile);
  ProfileIOS* profile = _state.profile;

  enterprise_idle::IdleServiceFactory::GetForProfile(profile)
      ->OnApplicationWillEnterBackground();

  // Save the cookies unless there is already a save in progress. This avoid
  // posting multiple tasks if the user switch rapidly between multiple apps.
  if (!_savingCookies) {
    _savingCookies = YES;

    // Save the cookie while ensuring the application will be given time for
    // the operation to complete by marking it as a critical closure. Since
    // the closure passed to FlushCookieStoreOnIOThread(...) may be invoked
    // on an arbitrary sequence, wrap it in base::BindPostTask(...) so that
    // it executes on the current sequence.
    __weak ProfileController* weakSelf = self;
    web::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FlushCookieStoreOnIOThread,
            base::WrapRefCounted(profile->GetRequestContext()),
            base::BindPostTask(
                base::SequencedTaskRunner::GetCurrentDefault(),
                base::MakeCriticalClosure("-[ProfileController saveCookies]",
                                          base::BindOnce(^{
                                            [weakSelf cookiesSaved];
                                          }),
                                          /*is_immediate=*/true))));
  }
}

- (void)applicationWillEnterForeground:(UIApplication*)application
                          memoryHelper:(MemoryWarningHelper*)memoryHelper {
  // Nothing to do if the profile is not yet fully loaded.
  if (_state.initStage < ProfileInitStage::kPrepareUI) {
    return;
  }

  DCHECK(_state.profile);
  ProfileIOS* profile = _state.profile;

  AuthenticationServiceFactory::GetForProfile(profile)
      ->OnApplicationWillEnterForeground();

  enterprise_idle::IdleServiceFactory::GetForProfile(profile)
      ->OnApplicationWillEnterForeground();

  // Send the "Chrome opened" event to the feature engagement tracker on a
  // warm start.
  [self sendChromeOpenedEvent];
}

- (void)application:(UIApplication*)application
    didDiscardSceneSessions:(NSSet<UISceneSession*>*)sceneSessions {
  NOTREACHED();
}

#pragma mark Private methods

- (void)profileLoaded:(ScopedProfileKeepAliveIOS)keepAlive {
  CHECK(!_scopedProfileKeepAlive.profile());
  _scopedProfileKeepAlive = std::move(keepAlive);
  ProfileIOS* profile = _scopedProfileKeepAlive.profile();
  CHECK(profile);

  [_state setProfile:profile];
  [_state queueTransitionToNextInitStage];
}

- (void)migrateSessionStorageIfNeeded {
  DCHECK(_state.profile);

  __weak ProfileController* weakSelf = self;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      _state.profile, SessionRestorationServiceFactory::kOptimized,
      base::BindOnce(^{
        [weakSelf.state queueTransitionToNextInitStage];
      }));
}

- (void)purgeDiscardedSessionsData {
  DCHECK(_state.profile);
  DCHECK(_profileManager);
  ProfileIOS* profile = _state.profile;

  SessionIds sessionIDs =
      _profileManager->GetProfileAttributesStorage()
          ->GetAttributesForProfileWithName(profile->GetProfileName())
          .GetDiscardedSessions();

  if (sessionIDs.empty() || tests_hook::NeverPurgeDiscardedSessionsData()) {
    // No data to purge since there is no discarded sessions, advance stage.
    [self dataPurgedForDiscardedSessions:sessionIDs];
    return;
  }

  std::array<base::FilePath, 2> storagePaths = {
      profile->GetStatePath(),
      profile->GetOffTheRecordStatePath(),
  };

  __weak ProfileController* weakSelf = self;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&PurgeDataForSessions, sessionIDs, storagePaths),
      base::BindOnce(^{
        [weakSelf dataPurgedForDiscardedSessions:sessionIDs];
      }));
}

- (void)dataPurgedForDiscardedSessions:(const SessionIds&)sessions {
  DCHECK(_state.profile);
  DCHECK(_profileManager);
  ProfileIOS* profile = _state.profile;
  _purgedSessionIdentifiers = sessions;

  if (!sessions.empty()) {
    _profileManager->GetProfileAttributesStorage()
        ->UpdateAttributesForProfileWithName(
            profile->GetProfileName(),
            base::BindOnce(&RemoveSessionsFromSessionsToDiscard, sessions));
  }

  for (SceneState* sceneState in _state.connectedScenes) {
    RecordDiscardedSceneConnectedAfterBeingPurged(_purgedSessionIdentifiers,
                                                  sceneState.sceneSessionID);
  }

  // The profile manager is no longer used, clear the pointer so that it
  // does not dangle.
  _profileManager = nullptr;

  [_state queueTransitionToNextInitStage];
}

- (void)startUpBrowserBackgroundInitialization {
  DCHECK(_state.profile);
  ProfileIOS* profile = _state.profile;
  PrefService* prefs = profile->GetPrefs();

  // Record initial translation metrics.
  language::LanguageUsageMetrics::RecordAcceptLanguages(
      prefs->GetString(language::prefs::kAcceptLanguages));
  translate::TranslateMetricsLoggerImpl::LogApplicationStartMetrics(
      ChromeIOSTranslateClient::CreateTranslatePrefs(prefs));

  search_engines::UpdateSearchEngineCountryCodeIfNeeded(prefs);

  // Force desktop mode when racoon is enabled.
  if (ios::provider::IsRaccoonEnabled()) {
    if (!prefs->GetBoolean(prefs::kUserAgentWasChanged)) {
      prefs->SetBoolean(prefs::kUserAgentWasChanged, true);
      ios::HostContentSettingsMapFactory::GetForProfile(profile)
          ->SetDefaultContentSetting(ContentSettingsType::REQUEST_DESKTOP_SITE,
                                     CONTENT_SETTING_ALLOW);
    }
  }

  [self attachProfileAgents];
}

- (void)attachProfileAgents {
  [_state addAgent:[[CertificatePolicyProfileAgent alloc] init]];
  [_state addAgent:[[FirstRunProfileAgent alloc] init]];
  [_state addAgent:[[IdentityConfirmationProfileAgent alloc] init]];
  [_state addAgent:[[ProfileActivityProfileAgent alloc] init]];
  [_state addAgent:[[PostRestoreProfileAgent alloc] init]];
  [_state addAgent:[[SearchEngineChoiceProfileAgent alloc] init]];
  [_state addAgent:[[SessionMetricsProfileAgent alloc] init]];

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

  if (first_run::IsWelcomeBackInFirstRunEnabled()) {
    [_state addAgent:[[WelcomeBackScreenProfileAgent alloc] init]];
  }
}

- (void)maybeContinueForegroundInitialization {
  if (_state.initStage != ProfileInitStage::kPrepareUI) {
    return;
  }

  if (_state.foregroundScenes.count == 0) {
    return;
  }

  // Stop listening to the SceneStates, as there is no need anymore once
  // the transition to the next stage is scheduled. This avoids a crash
  // if a SceneState reaches foreground in reaction to the ProfileState
  // reaching the PrepareUI stage.
  for (SceneState* sceneState in _state.connectedScenes) {
    [sceneState removeObserver:self];
  }

  [_state queueTransitionToNextInitStage];
}

- (void)startUpBeforeFirstWindowCreated {
  DCHECK(_state.profile);
  ProfileIOS* profile = _state.profile;

  // Send "Chrome Opened" event to the feature engagement tracker on
  // cold start.
  [self sendChromeOpenedEvent];

  _spotlightManager = [SpotlightManager spotlightManagerWithProfile:profile];
  ShareExtensionServiceFactory::GetForProfile(profile)->Initialize();

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
  CredentialProviderServiceFactory::GetForProfile(profile);
#endif
}

- (void)startUpAfterFirstWindowCreated {
  [self scheduleRemoveExternalFiles];
  [self scheduleCreateSearchEngineDataUpdater];
  [self scheduleClearingSessionCookies];
  [self scheduleCleanupSessionStateCache];
  [self scheduleCreateMailtoHandlerService];
  [self scheduleInitializeReadingListDownloadService];
  [self scheduleResyncSpotlightIndex];
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

- (void)sendChromeOpenedEvent {
  DCHECK(_state.profile);
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(_state.profile);

  tracker->NotifyEvent(feature_engagement::events::kChromeOpened);
  [_metricsMediator notifyCredentialProviderWasUsed:tracker];
}

- (void)cookiesSaved {
  _savingCookies = NO;
}

#pragma mark Deferred initialization tasks scheduling

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

// Schedules initialization of the ReadingList download service.
- (void)scheduleInitializeReadingListDownloadService {
  DCHECK(_state.deferredRunner);
  __weak ProfileController* weakSelf = self;
  [_state.deferredRunner
      enqueueBlockNamed:kStartupInitReadingListDownloadService
                  block:^{
                    [weakSelf initializeReadingListDownloadService];
                  }];
}

// Schedules resynchronisation of the Spotlight index.
- (void)scheduleResyncSpotlightIndex {
  DCHECK(_state.deferredRunner);
  __weak ProfileController* weakSelf = self;
  [_state.deferredRunner enqueueBlockNamed:kStartResyncSpotlightIndex
                                     block:^{
                                       [weakSelf resyncSpotlightIndex];
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

// Initializes the ReadingListDownloadService.
- (void)initializeReadingListDownloadService {
  DCHECK(_state.profile);
  ReadingListDownloadServiceFactory::GetForProfile(_state.profile)
      ->Initialize();
}

// Resynchronizes the spotlight index.
- (void)resyncSpotlightIndex {
  [_spotlightManager resyncIndex];
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
