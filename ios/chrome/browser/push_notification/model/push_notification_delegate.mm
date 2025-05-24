// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_delegate.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/check_is_test.h"
#import "base/files/file_path.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/send_tab_to_self/features.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/browser/content_notification/model/content_notification_nau_configuration.h"
#import "ios/chrome/browser/content_notification/model/content_notification_service.h"
#import "ios/chrome/browser/content_notification/model/content_notification_service_factory.h"
#import "ios/chrome/browser/content_notification/model/content_notification_settings_action.h"
#import "ios/chrome/browser/content_notification/model/content_notification_util.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/provisional_push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/provisional_push_notification_service_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_account_context_manager.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_manager.h"
#import "ios/chrome/browser/push_notification/model/push_notification_configuration.h"
#import "ios/chrome/browser/push_notification/model/push_notification_profile_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_profile_service_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/profile/scoped_profile_keep_alive_ios.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"

namespace {
// The time range's expected min and max values for custom histograms.
constexpr base::TimeDelta kTimeRangeIncomingNotificationHistogramMin =
    base::Milliseconds(1);
constexpr base::TimeDelta kTimeRangeIncomingNotificationHistogramMax =
    base::Seconds(30);
// Number of buckets for the time range histograms.
constexpr int kTimeRangeHistogramBucketCount = 30;

// The histogram used to record a push notification's current lifecycle state on
// the device.
const char kLifecycleEventsHistogram[] = "IOS.PushNotification.LifecyleEvents";

// This enum is used to represent a point along the push notification's
// lifecycle.
enum class PushNotificationLifecycleEvent {
  kNotificationReception,
  kNotificationForegroundPresentation,
  kNotificationInteraction,
  kMaxValue = kNotificationInteraction
};

// Extract the notification information from `attr`, and store them into
// `mapping`. Will also copy the notification permission from the profile's
// pref into the `attr` if the profile is loaded.
void ExtractNotificationInformation(ProfileManagerIOS* manager,
                                    NSMutableDictionary* mapping,
                                    ProfileAttributesIOS& attr) {
  const GaiaId& gaia_id = attr.GetGaiaId();
  if (gaia_id.empty()) {
    return;
  }

  // Get the permissions from `attr` but if they are missing, check if they
  // can be found in the profile (if it is loaded).
  const base::Value::Dict* permissions = attr.GetNotificationPermissions();
  if (!permissions) {
    ProfileIOS* profile = manager->GetProfileWithName(attr.GetProfileName());
    if (profile) {
      const base::Value::Dict& profile_permissions =
          profile->GetPrefs()->GetDict(
              prefs::kFeaturePushNotificationPermissions);
      attr.SetNotificationPermissions(profile_permissions.Clone());
      permissions = attr.GetNotificationPermissions();
    }
  }

  // There is no permissions for the profile in attr (or no permission could
  // be copied from the profile's pref, possibly because the profile is not
  // yet loaded).
  if (!permissions) {
    return;
  }

  NSMutableDictionary* permissions_map = [[NSMutableDictionary alloc] init];
  for (const auto pair : *permissions) {
    permissions_map[base::SysUTF8ToNSString(pair.first)] =
        [NSNumber numberWithBool:pair.second.GetBool()];
  }

  mapping[gaia_id.ToNSString()] = permissions_map;
}

// This function creates a dictionary that maps signed-in user's GAIA IDs to a
// map of each user's preferences for each push notification enabled feature.
GaiaIdToPushNotificationPreferenceMap*
GaiaIdToPushNotificationPreferenceMapFromCache() {
  ProfileManagerIOS* manager = GetApplicationContext()->GetProfileManager();
  ProfileAttributesStorageIOS* storage = manager->GetProfileAttributesStorage();

  NSMutableDictionary* account_preference_map =
      [[NSMutableDictionary alloc] init];

  storage->IterateOverProfileAttributes(base::BindRepeating(
      &ExtractNotificationInformation, manager, account_preference_map));

  return account_preference_map;
}

// Call ContentNotificationService::SendNAUForConfiguration() after fetching
// the notification settings if `weak_profile` is still valid.
void SendNAUFConfigurationForProfileWithSettings(
    base::WeakPtr<ProfileIOS> weak_profile,
    UNNotificationSettings* settings) {
  ProfileIOS* profile = weak_profile.get();
  if (!profile) {
    return;
  }

  UNAuthorizationStatus previousAuthStatus =
      [PushNotificationUtil getSavedPermissionSettings];
  ContentNotificationNAUConfiguration* config =
      [[ContentNotificationNAUConfiguration alloc] init];
  ContentNotificationSettingsAction* settingsAction =
      [[ContentNotificationSettingsAction alloc] init];
  settingsAction.previousAuthorizationStatus = previousAuthStatus;
  settingsAction.currentAuthorizationStatus = settings.authorizationStatus;
  config.settingsAction = settingsAction;
  ContentNotificationServiceFactory::GetForProfile(profile)
      ->SendNAUForConfiguration(config);
}

// Records a failure to access the PushNotificationClientManager at a specific
// point.
void RecordClientManagerAccessFailure(
    PushNotificationClientManagerFailurePoint failure_point) {
  base::UmaHistogramEnumeration(
      "IOS.PushNotification.ClientManagerAccessFailure", failure_point);
}

// Records the outcome of handling a multi-profile notification interaction.
void RecordPushNotificationTargetProfileHandlingResult(
    PushNotificationTargetProfileHandlingResult result) {
  base::UmaHistogramEnumeration("IOS.PushNotification.MultiProfile."
                                "PushNotificationTargetProfileHandlingResult",
                                result);
}

// Helper function to get the profile-specific PushNotificationClientManager
// directly from a ProfileIOS object. Returns nullptr if the manager cannot be
// retrieved.
PushNotificationClientManager* GetClientManagerForProfile(ProfileIOS* profile) {
  CHECK(IsMultiProfilePushNotificationHandlingEnabled());

  if (!profile) {
    RecordClientManagerAccessFailure(PushNotificationClientManagerFailurePoint::
                                         kGetClientManagerNullProfileInput);

    return nullptr;
  }

  PushNotificationProfileService* profile_service =
      PushNotificationProfileServiceFactory::GetForProfile(profile);

  if (!profile_service) {
    RecordClientManagerAccessFailure(
        PushNotificationClientManagerFailurePoint::
            kGetClientManagerMissingProfileService);

    return nullptr;
  }

  return profile_service->GetPushNotificationClientManager();
}

// Determines the associated Profile name using `user_info`.
//
// It first looks for `kOriginatingProfileNameKey`. If absent or otherwise
// invalid, it falls back to checking `kOriginatingGaiaIDKey` and uses
// `AccountProfileMapper` to map the Gaia ID to a Profile name.
//
// Returns the Profile name if found, otherwise returns an empty string. Logs
// specific reasons for failure to UMA.
//
// Note: This function should only be called when
// `IsMultiProfilePushNotificationHandlingEnabled()` is true.
std::string GetProfileNameFromUserInfo(NSDictionary* user_info) {
  CHECK(IsMultiProfilePushNotificationHandlingEnabled());

  ProfileManagerIOS* profile_manager =
      GetApplicationContext()->GetProfileManager();

  if (!profile_manager) {
    CHECK_IS_TEST();
    return "";
  }

  NSString* profile_name_ns = user_info[kOriginatingProfileNameKey];

  if (profile_name_ns) {
    if (profile_name_ns.length == 0) {
      RecordClientManagerAccessFailure(
          PushNotificationClientManagerFailurePoint::
              kGetProfileNameEmptyNameProvided);
      // Definite failure: An empty Profile name was explicitly provided. Cannot
      // proceed or fallback.
      return "";
    }

    std::string profile_name = base::SysNSStringToUTF8(profile_name_ns);

    if (!profile_manager->HasProfileWithName(profile_name)) {
      RecordClientManagerAccessFailure(
          PushNotificationClientManagerFailurePoint::
              kGetProfileNameDirectNameNotFoundInStorage);
      // Definite failure: An invalid Profile name was explicitly provided.
      // Cannot proceed or fallback.
      return "";
    }

    // Definite success: Found a valid, existing Profile name directly via
    // `kOriginatingProfileNameKey`.
    return profile_name;
  }

  NSString* gaia_id_ns = user_info[kOriginatingGaiaIDKey];
  GaiaId gaia_id = GaiaId(gaia_id_ns);

  if (gaia_id.empty()) {
    RecordClientManagerAccessFailure(PushNotificationClientManagerFailurePoint::
                                         kGetProfileNameMissingOrEmptyGaiaID);
    // Definite failure: The string provided for kOriginatingGaiaIDKey was
    // either missing or empty.
    return "";
  }

  std::optional<std::string> mapped_profile_name =
      GetApplicationContext()
          ->GetAccountProfileMapper()
          ->FindProfileNameForGaiaID(gaia_id);

  if (!mapped_profile_name.has_value()) {
    RecordClientManagerAccessFailure(PushNotificationClientManagerFailurePoint::
                                         kGetProfileNameGaiaIdNotMapped);
    // Definite failure: The Gaia ID was valid but is not associated with any
    // known Profile according to the AccountProfileMapper.
    return "";
  }

  if (!profile_manager->HasProfileWithName(mapped_profile_name.value())) {
    RecordClientManagerAccessFailure(
        PushNotificationClientManagerFailurePoint::
            kGetProfileNameMappedNameNotFoundInStorage);
    // Definite failure: Gaia ID mapped successfully to a Profile name, but that
    // Profile name does not exist in ProfileAttributesStorageIOS (e.g., stale
    // mapping or recently deleted profile).
    return "";
  }

  return mapped_profile_name.value();
}

// Callback used to asynchronously retrieve a `PushNotificationClientManager`.
using ClientManagerCallback =
    base::OnceCallback<void(PushNotificationClientManager*)>;

// Helper function invoked after an asynchronous Profile load attempt.
// It retrieves the `PushNotificationClientManager` for the loaded Profile (if
// successful) and runs the original callback with the result.
//
// TODO(crbug.com/418696432): Ensure that the ScopedProfileKeepAliveIOS is
// not destroyed before `original_callback` and any background processing,
// if any, is complete. Currently the profile will be unloaded as soon as
// the current function returns, even if `original_callback` starts any
// background processing.
void OnProfileLoadedForClientManager(ClientManagerCallback original_callback,
                                     ScopedProfileKeepAliveIOS keep_alive) {
  CHECK(IsMultiProfilePushNotificationHandlingEnabled());

  ProfileIOS* profile_after_load = keep_alive.profile();
  if (!profile_after_load) {
    RecordClientManagerAccessFailure(PushNotificationClientManagerFailurePoint::
                                         kGetClientManagerProfileLoadFailed);
    std::move(original_callback).Run(nullptr);
    return;
  }

  PushNotificationClientManager* manager =
      GetClientManagerForProfile(profile_after_load);

  std::move(original_callback).Run(manager);
}

// Gets the appropriate `PushNotificationClientManager` based on `user_info`.
// Checks for Profile-identifying keys (`kOriginatingProfileNameKey`,
// `kOriginatingGaiaIDKey`). If neither key is present, it synchronously returns
// the app-wide manager via the `callback`. If keys are present, it attempts to
// retrieve the Profile-specific manager, potentially loading the Profile
// asynchronously. The callback receives the retrieved manager or `nullptr` if
// the Profile-specific lookup or load fails.
void GetClientManagerForUserInfo(NSDictionary* user_info,
                                 ClientManagerCallback callback) {
  CHECK(IsMultiProfilePushNotificationHandlingEnabled());

  BOOL hasProfileKey = (user_info[kOriginatingProfileNameKey] != nil);
  BOOL hasGaiaKey = (user_info[kOriginatingGaiaIDKey] != nil);

  // If the notification payload contains neither the originating Profile name
  // key (`kOriginatingProfileNameKey`) nor the originating Gaia ID key
  // (`kOriginatingGaiaIDKey`), assume it's intended only for the app-wide
  // client manager.
  if (!hasProfileKey && !hasGaiaKey) {
    PushNotificationClientManager* app_wide_manager =
        GetApplicationContext()
            ->GetPushNotificationService()
            ->GetPushNotificationClientManager();

    std::move(callback).Run(app_wide_manager);

    return;
  }

  std::string profile_name = GetProfileNameFromUserInfo(user_info);

  if (profile_name.empty()) {
    RecordClientManagerAccessFailure(
        PushNotificationClientManagerFailurePoint::
            kGetClientManagerFailedToGetProfileName);
    std::move(callback).Run(nullptr);

    return;
  }

  ProfileManagerIOS* profile_manager =
      GetApplicationContext()->GetProfileManager();

  ProfileIOS* loaded_profile =
      profile_manager->GetProfileWithName(profile_name);

  if (loaded_profile) {
    PushNotificationClientManager* manager =
        GetClientManagerForProfile(loaded_profile);
    std::move(callback).Run(manager);

    return;
  }

  if (!profile_manager->HasProfileWithName(profile_name)) {
    RecordClientManagerAccessFailure(
        PushNotificationClientManagerFailurePoint::
            kGetClientManagerProfileNotFoundByName);
    std::move(callback).Run(nullptr);

    return;
  }

  profile_manager->LoadProfileAsync(
      profile_name,
      base::BindOnce(&OnProfileLoadedForClientManager, std::move(callback)));
}

// Callback executed after a Profile switch initiated by a notification
// interaction. This function validates the state of the `new_scene_state` and
// its associated Profile, retrieves the appropriate
// `PushNotificationClientManager` for the now-active `switched_profile`, and
// forwards the original `response` for handling by that manager. Logs failures
// to UMA.
void HandleNotificationInteractionAfterProfileSwitch(
    UNNotificationResponse* response,
    SceneState* new_scene_state,
    base::OnceClosure completion_closure) {
  base::ScopedClosureRunner run_completion_closure(
      std::move(completion_closure));

  CHECK(new_scene_state.profileState);
  CHECK_GE(new_scene_state.profileState.initStage,
           ProfileInitStage::kProfileLoaded);

  ProfileIOS* switched_profile = new_scene_state.profileState.profile;
  CHECK(switched_profile);

  PushNotificationClientManager* client_manager =
      GetClientManagerForProfile(switched_profile);

  if (!client_manager) {
    RecordClientManagerAccessFailure(
        PushNotificationClientManagerFailurePoint::
            kInteractionContinuationMissingClientManager);

    return;
  }

  client_manager->HandleNotificationInteraction(response);

  // Also allow the app-scoped clients the opportunity to handle interactions.
  GetApplicationContext()
      ->GetPushNotificationService()
      ->GetPushNotificationClientManager()
      ->HandleNotificationInteraction(response);
}

// Creates a `ChangeProfileContinuation` callback bound with the original
// notification `response`. `HandleNotificationInteractionAfterProfileSwitch()`
// will be invoked by the Profile switching mechanism if a switch occurs,
// allowing the notification interaction to be handled in the context of the
// newly switched Profile.
ChangeProfileContinuation CreateNotificationInteractionContinuation(
    UNNotificationResponse* response) {
  return base::BindOnce(&HandleNotificationInteractionAfterProfileSwitch,
                        response);
}

// Handles notification reception using the app-wide client manager and calls
// the final completion block.
void HandleNotificationReceptionWithAppWideManager(
    NSDictionary* user_info,
    void (^completion_block)(UIBackgroundFetchResult /* result */)) {
  PushNotificationClientManager* app_wide_manager =
      GetApplicationContext()
          ->GetPushNotificationService()
          ->GetPushNotificationClientManager();
  CHECK(app_wide_manager);

  UIBackgroundFetchResult result =
      app_wide_manager->HandleNotificationReception(user_info);

  if (completion_block) {
    completion_block(result);
  }
}

// Callback invoked after asynchronously attempting to retrieve the
// Profile-specific `PushNotificationClientManager`. Falls back to app-wide
// manager, if necessary.
void OnClientManagerReadyForReception(
    NSDictionary* user_info,
    PushNotificationClientManagerFailurePoint failure_point,
    void (^completion_block)(UIBackgroundFetchResult result),
    PushNotificationClientManager* client_manager) {
  if (!client_manager) {
    RecordClientManagerAccessFailure(failure_point);

    if (completion_block) {
      completion_block(UIBackgroundFetchResultNoData);
    }

    return;
  }

  UIBackgroundFetchResult result =
      client_manager->HandleNotificationReception(user_info);

  if (completion_block) {
    completion_block(result);
  }
}

// Processes an incoming notification by attempting to use a Profile-specific
// client manager, falling back to the app-wide manager, if necessary.
void ProcessIncomingNotification(
    NSDictionary* user_info,
    PushNotificationClientManagerFailurePoint failure_point,
    void (^completion_block)(UIBackgroundFetchResult result)) {
  CHECK(IsMultiProfilePushNotificationHandlingEnabled());

  ClientManagerCallback manager_ready_callback =
      base::BindOnce(&OnClientManagerReadyForReception, user_info,
                     failure_point, completion_block);

  // Start the async process to get the Profile-specific manager
  GetClientManagerForUserInfo(user_info, std::move(manager_ready_callback));
}

}  // anonymous namespace

@interface PushNotificationDelegate () <AppStateObserver,
                                        ProfileStateObserver,
                                        SceneStateObserver>

// The first connected scene that is foreground active, or nil if there are
// none.
@property(nonatomic, readonly) SceneState* foregroundActiveScene;

// The client manager for notification clients that are app-scoped (rather than
// profile-scoped).
@property(nonatomic, readonly)
    PushNotificationClientManager* appWideClientManager;

@end

@implementation PushNotificationDelegate {
  __weak AppState* _appState;
  // Stores blocks to execute once the app is finished foregrounding.
  NSMutableArray<ProceduralBlock>* _runAfterForeground;
  // Storage for the lazy-loaded `appWideClientManager` property.
  raw_ptr<PushNotificationClientManager> _appWideClientManager;
}

- (instancetype)initWithAppState:(AppState*)appState {
  if ((self = [super init])) {
    _appState = appState;
    [_appState addObserver:self];
  }
  return self;
}

#pragma mark - UNUserNotificationCenterDelegate -

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
    didReceiveNotificationResponse:(UNNotificationResponse*)response
             withCompletionHandler:(void (^)(void))completionHandler {
  // This method is invoked by iOS to process the user's response to a delivered
  // notification.
  [self recordLifeCycleEvent:PushNotificationLifecycleEvent::
                                 kNotificationInteraction];
  __weak __typeof(self) weakSelf = self;
  [self executeWhenForeground:^{
    [weakSelf handleNotificationResponse:response];
  }];
  // TODO(crbug.com/401537165): Consider changing when completionHandler is
  // called.
  if (completionHandler) {
    completionHandler();
  }
  base::UmaHistogramEnumeration(kAppLaunchSource,
                                AppLaunchSource::NOTIFICATION);
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
       willPresentNotification:(UNNotification*)notification
         withCompletionHandler:
             (void (^)(UNNotificationPresentationOptions options))
                 completionHandler {
  [self recordLifeCycleEvent:PushNotificationLifecycleEvent::
                                 kNotificationForegroundPresentation];

  NSDictionary* userInfo = notification.request.content.userInfo;

  __weak __typeof(self) weakSelf = self;

  void (^presentationCompletionBlock)(UIBackgroundFetchResult result) =
      ^(UIBackgroundFetchResult /* result */) {
        [weakSelf handlePresentationCompletionWithUserInfo:userInfo
                                         completionHandler:completionHandler];
      };

  if (IsMultiProfilePushNotificationHandlingEnabled()) {
    ProcessIncomingNotification(
        userInfo,
        PushNotificationClientManagerFailurePoint::kWillPresentNotification,
        presentationCompletionBlock);
  } else {
    HandleNotificationReceptionWithAppWideManager(userInfo,
                                                  presentationCompletionBlock);
  }
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
    openSettingsForNotification:(UNNotification*)notification {
  __weak __typeof(self) weakSelf = self;
  if (IsMultiProfilePushNotificationHandlingEnabled()) {
    std::string profileName =
        GetProfileNameFromUserInfo(notification.request.content.userInfo);
    if (!profileName.empty()) {
      [self executeWhenForeground:^{
        [weakSelf openSettingsForNotification:notification
                                  profileName:profileName];
      }];
      return;
    }
  }
  [self executeWhenForeground:^{
    [weakSelf openSettingsForNotification:notification
                                    scene:weakSelf.foregroundActiveScene
                               completion:base::DoNothing()];
  }];
}

#pragma mark - PushNotificationDelegate

- (void)applicationWillProcessIncomingRemoteNotification:(NSDictionary*)userInfo
                                  fetchCompletionHandler:
                                      (void (^)(UIBackgroundFetchResult result))
                                          completionHandler {
  [self recordLifeCycleEvent:PushNotificationLifecycleEvent::
                                 kNotificationReception];

  double incomingNotificationTime =
      base::Time::Now().InSecondsFSinceUnixEpoch();

  auto recordMetricsAndComplete = ^(UIBackgroundFetchResult result) {
    double processingTime =
        base::Time::Now().InSecondsFSinceUnixEpoch() - incomingNotificationTime;

    UmaHistogramCustomTimes(
        "IOS.PushNotification.IncomingNotificationProcessingTime",
        base::Milliseconds(processingTime),
        kTimeRangeIncomingNotificationHistogramMin,
        kTimeRangeIncomingNotificationHistogramMax,
        kTimeRangeHistogramBucketCount);

    if (completionHandler) {
      completionHandler(result);
    }
  };

  if (IsMultiProfilePushNotificationHandlingEnabled()) {
    ProcessIncomingNotification(userInfo,
                                PushNotificationClientManagerFailurePoint::
                                    kWillProcessIncomingRemoteNotification,
                                recordMetricsAndComplete);
  } else {
    HandleNotificationReceptionWithAppWideManager(userInfo,
                                                  recordMetricsAndComplete);
  }
}

- (void)applicationDidRegisterWithAPNS:(NSData*)deviceToken
                               profile:(ProfileIOS*)profile {
  GaiaIdToPushNotificationPreferenceMap* accountPreferenceMap =
      GaiaIdToPushNotificationPreferenceMapFromCache();

  // Return early if no accounts are signed into Chrome.
  if (!accountPreferenceMap.count) {
    return;
  }

  if (IsMultiProfilePushNotificationHandlingEnabled()) {
    PushNotificationClientManager* clientManager =
        GetClientManagerForProfile(profile);

    // Gracefully handle the case where a clientManager couldn't be retrieved
    // (e.g., if the Profile is `nullptr` or its service isn't available).
    if (clientManager) {
      // Registers Chrome's PushNotificationClients' Actionable Notifications
      // with iOS.
      clientManager->RegisterActionableNotifications();
    } else {
      RecordClientManagerAccessFailure(
          PushNotificationClientManagerFailurePoint::kDidRegisterWithAPNS);
    }
  }

  // Registers Chrome's PushNotificationClients' Actionable Notifications with
  // iOS.
  self.appWideClientManager->RegisterActionableNotifications();

  PushNotificationConfiguration* config =
      [[PushNotificationConfiguration alloc] init];

  config.accountIDs = accountPreferenceMap.allKeys;
  config.preferenceMap = accountPreferenceMap;
  config.deviceToken = deviceToken;
  config.singleSignOnService =
      GetApplicationContext()->GetSingleSignOnService();

  if (profile) {
    config.shouldRegisterContentNotification =
        [self isContentNotificationAvailable:profile];
    if (config.shouldRegisterContentNotification) {
      AuthenticationService* authService =
          AuthenticationServiceFactory::GetForProfile(profile);
      id<SystemIdentity> identity =
          authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
      config.primaryAccount = identity;
      // Send an initial NAU to share the OS auth status and channel status with
      // the server. Send an NAU on every foreground to report the OS Auth
      // Settings.
      [self sendSettingsChangeNAUForProfile:profile];
    }
  }

  __weak __typeof(self) weakSelf = self;
  base::WeakPtr<ProfileIOS> weakProfile =
      profile ? profile->AsWeakPtr() : base::WeakPtr<ProfileIOS>{};
  PushNotificationService* notificationService =
      GetApplicationContext()->GetPushNotificationService();

  notificationService->RegisterDevice(config, ^(NSError* error) {
    [weakSelf deviceRegistrationForProfile:weakProfile withError:error];
  });
}

- (void)deviceRegistrationForProfile:(base::WeakPtr<ProfileIOS>)weakProfile
                           withError:(NSError*)error {
  base::UmaHistogramBoolean("IOS.PushNotification.ChimeDeviceRegistration",
                            !error);
  if (!error) {
    if (ProfileIOS* profile = weakProfile.get()) {
      if (base::FeatureList::IsEnabled(
              send_tab_to_self::kSendTabToSelfIOSPushNotifications)) {
        [self setUpAndEnableSendTabNotificationsWithProfile:profile];
      }
    }
  }
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    sceneDidBecomeActive:(SceneState*)sceneState {
  if (sceneState.profileState.initStage < ProfileInitStage::kFinal) {
    [sceneState.profileState addObserver:self];
    return;
  }

  if (sceneState.activationLevel < SceneActivationLevelForegroundActive) {
    return;
  }

  [self appDidEnterForeground:sceneState];
}

- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState {
  [sceneState addObserver:self];
  [self sceneState:sceneState
      transitionedToActivationLevel:sceneState.activationLevel];
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage < ProfileInitStage::kFinal) {
    return;
  }

  for (SceneState* sceneState in profileState.connectedScenes) {
    if (sceneState.activationLevel < SceneActivationLevelForegroundActive) {
      continue;
    }

    [self appDidEnterForeground:sceneState];
  }

  [profileState removeObserver:self];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level < SceneActivationLevelForegroundActive) {
    return;
  }

  if (sceneState.profileState.initStage < ProfileInitStage::kFinal) {
    [sceneState.profileState addObserver:self];
    return;
  }

  [self appDidEnterForeground:sceneState];
}

- (void)sceneState:(SceneState*)sceneState
    profileStateConnected:(ProfileState*)profileState {
  [profileState addObserver:self];
}

#pragma mark - Property accessors

- (SceneState*)foregroundActiveScene {
  for (SceneState* sceneState in _appState.connectedScenes) {
    if (sceneState.activationLevel < SceneActivationLevelForegroundActive) {
      continue;
    }

    if (sceneState.profileState.initStage < ProfileInitStage::kFinal) {
      continue;
    }

    return sceneState;
  }

  return nil;
}

- (PushNotificationClientManager*)appWideClientManager {
  if (!_appWideClientManager) {
    _appWideClientManager = GetApplicationContext()
                                ->GetPushNotificationService()
                                ->GetPushNotificationClientManager();
  }
  return _appWideClientManager;
}

#pragma mark - Private

// Determines how a notification should be presented when received while the app
// is in the foreground and invokes the system completion handler with the
// appropriate options. It also logs a histogram for the notification event.
- (void)handlePresentationCompletionWithUserInfo:(NSDictionary*)userInfo
                               completionHandler:
                                   (void (^)(UNNotificationPresentationOptions
                                                 options))completionHandler {
  if (completionHandler) {
    BOOL isSendTab = ([userInfo[kPushNotificationClientIdKey] intValue] ==
                      static_cast<int>(PushNotificationClientId::kSendTab));
    BOOL isForeground = (self.foregroundActiveScene != nil);

    UNNotificationPresentationOptions presentationOptions =
        (isSendTab && isForeground) ? UNNotificationPresentationOptionNone
                                    : UNNotificationPresentationOptionBanner;

    // TODO(crbug.com/408085973): Add PushNotificationDelegate unittest suite.
    // Cover critical paths and error cases.
    completionHandler(presentationOptions);
  }

  base::UmaHistogramEnumeration(kAppLaunchSource,
                                AppLaunchSource::NOTIFICATION);
}

// Executes blocks queued in _runAfterForeground. If multi-profile handling is
// enabled, also notifies the profile-specific PushNotificationClientManager
// for the given sceneState upon readiness.
- (void)handleQueuedBlocksWithSceneState:(SceneState*)sceneState {
  NSMutableArray<ProceduralBlock>* blocks = _runAfterForeground;
  _runAfterForeground = nil;

  for (ProceduralBlock block in blocks) {
    block();
  }

  if (IsMultiProfilePushNotificationHandlingEnabled()) {
    if (!sceneState || !sceneState.profileState.profile) {
      return;
    }

    PushNotificationClientManager* clientManager =
        GetClientManagerForProfile(sceneState.profileState.profile);

    if (!clientManager) {
      RecordClientManagerAccessFailure(
          PushNotificationClientManagerFailurePoint::kAppDidEnterForeground);
      return;
    }

    clientManager->OnSceneActiveForegroundBrowserReady();
  }
}

// Notifies the client manager that the scene is "foreground active".
- (void)appDidEnterForeground:(SceneState*)sceneState {
  DCHECK(self.appWideClientManager);
  self.appWideClientManager->OnSceneActiveForegroundBrowserReady();

  __weak PushNotificationDelegate* weakSelf = self;
  __weak SceneState* weakSceneState = sceneState;

  // Asynchronously processes any queued `_runAfterForeground` blocks.
  //
  // This is crucial for notification interactions (queued via
  // `-executeWhenForeground:`), as handling them might require tearing down the
  // current Browser/Profile to switch Profiles. Asynchronous processing ensures
  // that all current observers (potentially observing the Browser/Profile being
  // torn down) finish their work *before* these objects are destroyed. This
  // prevents state modification or destruction while observers are still active
  // in the original synchronous call stack.
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf handleQueuedBlocksWithSceneState:weakSceneState];
  });

  ProfileIOS* profile = sceneState.profileState.profile;
  if (IsContentNotificationEnabled(profile)) {
    ContentNotificationService* contentNotificationService =
        ContentNotificationServiceFactory::GetForProfile(profile);
    int maxNauSentPerSession = base::GetFieldTrialParamByFeatureAsInt(
        kContentNotificationDeliveredNAU, kDeliveredNAUMaxPerSession,
        kDeliveredNAUMaxSendsPerSession);
    // Check if there are notifications received in the background to send the
    // respective NAUs.
    NSUserDefaults* defaults = app_group::GetGroupUserDefaults();
    if ([defaults objectForKey:kContentNotificationContentArrayKey] != nil) {
      NSMutableArray* contentArray = [[defaults
          objectForKey:kContentNotificationContentArrayKey] mutableCopy];
      // Report in 5 item increments.
      NSMutableArray* uploadedItems = [NSMutableArray array];
      for (NSData* item in contentArray) {
        ContentNotificationNAUConfiguration* config =
            [[ContentNotificationNAUConfiguration alloc] init];
        config.actionType = NAUActionTypeDisplayed;
        UNNotificationContent* content = [NSKeyedUnarchiver
            unarchivedObjectOfClass:UNMutableNotificationContent.class
                           fromData:item
                              error:nil];
        config.content = content;
        contentNotificationService->SendNAUForConfiguration(config);
        [uploadedItems addObject:item];
        base::UmaHistogramEnumeration(
            kContentNotificationActionHistogramName,
            NotificationActionType::kNotificationActionTypeDisplayed);
        if ((int)uploadedItems.count == maxNauSentPerSession) {
          break;
        }
      }
      [contentArray removeObjectsInArray:uploadedItems];
      if (contentArray.count > 0) {
        [defaults setObject:contentArray
                     forKey:kContentNotificationContentArrayKey];
      } else {
        [defaults setObject:nil forKey:kContentNotificationContentArrayKey];
      }
    }
    // Send an NAU on every foreground to report the OS Auth Settings.
    [self sendSettingsChangeNAUForProfile:profile];
  }
  [PushNotificationUtil updateAuthorizationStatusPref];

  // For Reactivation Notifications, ask for provisional auth right away.
  if (IsFirstRunRecent(base::Days(28)) &&
      IsIOSReactivationNotificationsEnabled()) {
    UNAuthorizationStatus auth_status =
        [PushNotificationUtil getSavedPermissionSettings];
    if (auth_status == UNAuthorizationStatusNotDetermined) {
      [PushNotificationUtil enableProvisionalPushNotificationPermission:nil];
    }
  }
}

- (void)sendSettingsChangeNAUForProfile:(ProfileIOS*)profile {
  [PushNotificationUtil
      getPermissionSettings:base::CallbackToBlock(base::BindOnce(
                                &SendNAUFConfigurationForProfileWithSettings,
                                profile->AsWeakPtr()))];
}

- (void)recordLifeCycleEvent:(PushNotificationLifecycleEvent)event {
  base::UmaHistogramEnumeration(kLifecycleEventsHistogram, event);
}

- (BOOL)isContentNotificationAvailable:(ProfileIOS*)profile {
  return IsContentNotificationEnabled(profile) ||
         IsContentNotificationRegistered(profile);
}

// If user has not previously disabled Send Tab notifications, either 1) If user
// has authorized full notification permissions, enables Send Tab notifications
// OR 2) enrolls user in provisional notifications for Send Tab notification
// type.
- (void)setUpAndEnableSendTabNotificationsWithProfile:(ProfileIOS*)profile {
  // Refresh the local device info now that the client has a Chime
  // Representative Target ID.
  syncer::DeviceInfoSyncService* deviceInfoSyncService =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);
  deviceInfoSyncService->RefreshLocalDeviceInfo();

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  NSString* gaiaID =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin).gaiaID;

  // Early return if 1) the user has previously disabled Send Tab push
  // notifications, because in that case we don't want to automatically enable
  // the notification type or 2) if Send Tab notifications are already enabled.
  if (profile->GetPrefs()->GetBoolean(
          prefs::kSendTabNotificationsPreviouslyDisabled) ||
      push_notification_settings::
          GetMobileNotificationPermissionStatusForClient(
              PushNotificationClientId::kSendTab, GaiaId(gaiaID))) {
    return;
  }

  if ([PushNotificationUtil getSavedPermissionSettings] ==
      UNAuthorizationStatusAuthorized) {
    GetApplicationContext()->GetPushNotificationService()->SetPreference(
        gaiaID, PushNotificationClientId::kSendTab, true);
  } else {
    ProvisionalPushNotificationServiceFactory::GetForProfile(profile)
        ->EnrollUserToProvisionalNotifications(
            ProvisionalPushNotificationService::ClientIdState::kEnabled,
            {PushNotificationClientId::kSendTab});
  }
}

// Runs the given `block` immediately if the app has an active foreground
// scene connected, otherwise stores it to be called when the app is
// foregrounded.
- (void)executeWhenForeground:(ProceduralBlock)block {
  if (self.foregroundActiveScene) {
    block();
    return;
  }

  if (!_runAfterForeground) {
    _runAfterForeground = [[NSMutableArray alloc] init];
  }
  [_runAfterForeground addObject:block];
}

// Handles a notification response by sending it to the push notification
// client manager.
- (void)handleNotificationResponse:(UNNotificationResponse*)response {
  DCHECK_GE(_appState.initStage,
            AppInitStage::kBrowserObjectsForBackgroundHandlers);

  if (IsMultiProfilePushNotificationHandlingEnabled()) {
    [self handleProfileSpecificNotificationResponse:response];
    return;
  }

  // Notifications are intentionally passed on to the `appWideClientManager`
  // even when the profile specific one handles them. In a future refacor, if a
  // notification is properly handled in a profile specific manager it likely
  // should not be passed onto the app wide manager.

  self.appWideClientManager->HandleNotificationInteraction(response);
}

// Handles a notification interaction specifically for the multi-Profile case.
//
// It determines the Profile the notification originated from and the target
// scene where the interaction should occur. Based on whether the target scene's
// current Profile matches the notification's originating Profile, it either:
//
// 1. Handles the interaction directly using the current context if the Profiles
//    match.
// 2. Initiates a Profile switch for the target scene if the Profiles do not
//    match. A continuation callback is provided to the switching mechanism to
//    process the interaction once the correct Profile is active.
//
// Failures encountered during Profile validation, scene lookup, manager
// retrieval, or switch initiation are logged to UMA for monitoring.
- (void)handleProfileSpecificNotificationResponse:
    (UNNotificationResponse*)response {
  CHECK(IsMultiProfilePushNotificationHandlingEnabled());

  std::string profileName = GetProfileNameFromUserInfo(
      response.notification.request.content.userInfo);

  if (profileName.empty()) {
    // No profile name was found, so allow app-wide clients the opportunity to
    // handle interactions.
    self.appWideClientManager->HandleNotificationInteraction(response);

    RecordClientManagerAccessFailure(PushNotificationClientManagerFailurePoint::
                                         kHandleInteractionInvalidProfileName);

    RecordPushNotificationTargetProfileHandlingResult(
        PushNotificationTargetProfileHandlingResult::kProfileUnidentifiable);

    return;
  }

  SceneState* targetSceneState =
      [self notificationTargetSceneStateForResponse:response];

  if (!targetSceneState) {
    RecordClientManagerAccessFailure(PushNotificationClientManagerFailurePoint::
                                         kHandleInteractionMissingTargetScene);

    targetSceneState = self.foregroundActiveScene;
  }

  if (!targetSceneState) {
    RecordClientManagerAccessFailure(
        PushNotificationClientManagerFailurePoint::
            kHandleInteractionMissingFallbackScene);

    RecordPushNotificationTargetProfileHandlingResult(
        PushNotificationTargetProfileHandlingResult::kFailureSceneUnavailable);

    return;
  }

  ProfileIOS* sceneProfile = targetSceneState.profileState.profile;

  if (!sceneProfile || profileName != sceneProfile->GetProfileName()) {
    RecordPushNotificationTargetProfileHandlingResult(
        PushNotificationTargetProfileHandlingResult::
            kSwitchEnsuredCorrectProfile);
  } else {
    RecordPushNotificationTargetProfileHandlingResult(
        PushNotificationTargetProfileHandlingResult::kCorrectProfileActive);
  }

  id<ChangeProfileCommands> handler =
      HandlerForProtocol(_appState.appCommandDispatcher, ChangeProfileCommands);

  CHECK(handler);

  [handler changeProfile:profileName
                forScene:targetSceneState
                  reason:ChangeProfileReason::kHandlePushNotification
            continuation:CreateNotificationInteractionContinuation(response)];
}

// Shows the app's notification settings, switching to the given `profileName`
// profile if needed.
- (void)openSettingsForNotification:(UNNotification*)notification
                        profileName:(std::string_view)profileName {
  SceneState* sceneState = self.foregroundActiveScene;
  CHECK(sceneState);
  CHECK(IsMultiProfilePushNotificationHandlingEnabled());

  id<ChangeProfileCommands> handler =
      HandlerForProtocol(_appState.appCommandDispatcher, ChangeProfileCommands);

  __weak __typeof(self) weakSelf = self;
  ChangeProfileContinuation continuation = base::BindOnce(
      [](__typeof(self) strong_self, UNNotification* notification,
         SceneState* new_scene_state, base::OnceClosure completion_closure) {
        [strong_self openSettingsForNotification:notification
                                           scene:new_scene_state
                                      completion:std::move(completion_closure)];
      },
      weakSelf, notification);

  [handler changeProfile:profileName
                forScene:sceneState
                  reason:ChangeProfileReason::kHandlePushNotification
            continuation:std::move(continuation)];
}

// Shows the app's notification settings in the given `sceneState`, and calls
// `completion` when finished.
- (void)openSettingsForNotification:(UNNotification*)notification
                              scene:(SceneState*)sceneState
                         completion:(base::OnceClosure)completion {
  CHECK(sceneState);
  Browser* browser =
      sceneState.browserProviderInterface.mainBrowserProvider.browser;
  ProfileIOS* profile = browser->GetProfile();

  // Get the clientID for the client that is associated with this notification.
  PushNotificationClient* client = [self clientForNotification:notification
                                                       profile:profile];
  std::optional<PushNotificationClientId> clientID =
      (client) ? std::make_optional(client->GetClientId()) : std::nullopt;

  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  id<ApplicationCommands> applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  id<SettingsCommands> settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  __block base::OnceClosure completion2 = std::move(completion);
  [applicationHandler
      prepareToPresentModalWithSnackbarDismissal:YES
                                      completion:^{
                                        [settingsHandler
                                            showNotificationsSettingsAndHighlightClient:
                                                clientID];
                                        std::move(completion2).Run();
                                      }];
}

// Returns the client to handle the given `notification`. The client can be
// either profile-scoped (and associated with the given `profile`), or
// app-scoped.
- (PushNotificationClient*)clientForNotification:(UNNotification*)notification
                                         profile:(ProfileIOS*)profile {
  if (notification == nil) {
    return nullptr;
  }
  PushNotificationClient* client = nullptr;
  if (IsMultiProfilePushNotificationHandlingEnabled()) {
    PushNotificationClientManager* clientManager =
        GetClientManagerForProfile(profile);
    client = clientManager->GetClientForNotification(notification);
  }
  if (!client) {
    client = self.appWideClientManager->GetClientForNotification(notification);
  }
  return client;
}

// Returns the `SceneState` matching the notification response's target scene,
// if any.
- (SceneState*)notificationTargetSceneStateForResponse:
    (UNNotificationResponse*)response {
  UIScene* targetScene = response.targetScene;

  if (!targetScene) {
    RecordClientManagerAccessFailure(
        PushNotificationClientManagerFailurePoint::kGetResponseTargetSceneNil);

    return nil;
  }

  std::string targetSceneSessionIdentifier =
      SessionIdentifierForScene(targetScene);

  for (SceneState* scene in _appState.connectedScenes) {
    if (scene.sceneSessionID == targetSceneSessionIdentifier) {
      return scene;
    }
  }

  // No matching scene found
  return nil;
}

@end
