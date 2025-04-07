// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_delegate.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/files/file_path.h"
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
#import "ios/chrome/browser/push_notification/model/provisional_push_notification_util.h"
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
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
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

// Helper function to get the profile-specific PushNotificationClientManager
// directly from a ProfileIOS object. Returns nullptr if the manager cannot be
// retrieved.
PushNotificationClientManager* GetClientManagerForProfile(ProfileIOS* profile) {
  CHECK(IsIOSMultiProfilePushNotificationHandlingEnabled());

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

// Helper function to get the profile-specific PushNotificationClientManager
// using userInfo containing the profile name. Returns nullptr if the profile
// cannot be found or the manager cannot be retrieved.
PushNotificationClientManager* GetClientManagerForUserInfo(
    NSDictionary* user_info) {
  CHECK(IsIOSMultiProfilePushNotificationHandlingEnabled());

  NSString* profile_name_ns = user_info[kOriginatingProfileNameKey];

  if (!profile_name_ns) {
    RecordClientManagerAccessFailure(
        PushNotificationClientManagerFailurePoint::
            kGetClientManagerMissingProfileNameInUserInfo);

    return nullptr;
  }

  std::string profile_name = base::SysNSStringToUTF8(profile_name_ns);

  ProfileManagerIOS* profile_manager =
      GetApplicationContext()->GetProfileManager();

  ProfileIOS* profile = profile_manager->GetProfileWithName(profile_name);

  if (!profile) {
    // TODO(crbug.com/407999350): Enable PushNotificationClientManager to switch
    // to potentially unloaded Profiles for proper notification handling.
    // Replace this nullptr return with Profile loading functionality once
    // implemented.
    //
    // Note: Currently, this metric is logged when the Profile matching
    // `profile_name` is not already loaded. After the refactor described
    // in the TODO above (to handle unloaded Profiles), this metric will
    // signify that the Profile truly could not be found by name.
    RecordClientManagerAccessFailure(
        PushNotificationClientManagerFailurePoint::
            kGetClientManagerProfileNotFoundByName);

    return nullptr;
  }

  // Now that we have the profile, delegate to the other helper.
  return GetClientManagerForProfile(profile);
}

}  // anonymous namespace

@interface PushNotificationDelegate () <AppStateObserver,
                                        ProfileStateObserver,
                                        SceneStateObserver>

// The first connected scene that is foreground active, or nil if there are
// none.
@property(nonatomic, readonly) SceneState* foregroundActiveScene;

@end

@implementation PushNotificationDelegate {
  __weak AppState* _appState;
  // Stores blocks to execute once the app has reached init stage "final".
  NSMutableArray<ProceduralBlock>* _runAfterInit;
  // Stores blocks to execute once the app is finished foregrounding.
  NSMutableArray<ProceduralBlock>* _runAfterForeground;
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
  [self executeWhenInitStageFinal:^{
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

  if (IsIOSMultiProfilePushNotificationHandlingEnabled()) {
    PushNotificationClientManager* clientManager =
        GetClientManagerForUserInfo(notification.request.content.userInfo);

    if (clientManager) {
      clientManager->HandleNotificationReception(
          notification.request.content.userInfo);
    } else {
      RecordClientManagerAccessFailure(
          PushNotificationClientManagerFailurePoint::kWillPresentNotification);
    }
  }

  // This method is invoked by iOS to process a notification that arrived
  // while the app was running in the foreground.
  auto* appWideClientManager = GetApplicationContext()
                                   ->GetPushNotificationService()
                                   ->GetPushNotificationClientManager();
  DCHECK(appWideClientManager);
  appWideClientManager->HandleNotificationReception(
      notification.request.content.userInfo);

  // Per Apple's guidance for delegate methods handling notifications: "You
  // must execute [completionHandler] at some point…to let the system know that
  // you are done." Therefore, `completionHandler` is always invoked below, even
  // if a `PushNotificationClientManager` could not be found for the `Profile`,
  // to avoid leaving the system in an indeterminate state.
  if (completionHandler) {
    // If the app is foregrounded, Send Tab push notifications should not be
    // displayed.
    if ([notification.request.content.userInfo[kPushNotificationClientIdKey]
            intValue] == static_cast<int>(PushNotificationClientId::kSendTab) &&
        self.foregroundActiveScene) {
      completionHandler(UNNotificationPresentationOptionNone);
    } else {
      // TODO(crbug.com/408085973): Add PushNotificationDelegate unittest suite.
      // Cover critical paths and error cases.
      completionHandler(UNNotificationPresentationOptionBanner);
    }
  }

  base::UmaHistogramEnumeration(kAppLaunchSource,
                                AppLaunchSource::NOTIFICATION);
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
    openSettingsForNotification:(UNNotification*)notification {
  __weak __typeof(self) weakSelf = self;
  [self executeWhenForeground:^{
    [weakSelf openSettingsForNotification:notification];
  }];
}

#pragma mark - PushNotificationDelegate

- (UIBackgroundFetchResult)applicationWillProcessIncomingRemoteNotification:
    (NSDictionary*)userInfo {
  [self recordLifeCycleEvent:PushNotificationLifecycleEvent::
                                 kNotificationReception];

  double incomingNotificationTime =
      base::Time::Now().InSecondsFSinceUnixEpoch();

  UIBackgroundFetchResult profileResult = UIBackgroundFetchResultFailed;
  UIBackgroundFetchResult appWideResult = UIBackgroundFetchResultFailed;
  bool profileManagerCalledAndSuccessful = false;

  if (IsIOSMultiProfilePushNotificationHandlingEnabled()) {
    PushNotificationClientManager* clientManager =
        GetClientManagerForUserInfo(userInfo);

    if (clientManager) {
      profileResult = clientManager->HandleNotificationReception(userInfo);

      profileManagerCalledAndSuccessful = true;
    } else {
      RecordClientManagerAccessFailure(
          PushNotificationClientManagerFailurePoint::
              kWillProcessIncomingRemoteNotification);
    }
  }

  // Always notify the app-wide client manager.
  PushNotificationClientManager* appWideClientManager =
      GetApplicationContext()
          ->GetPushNotificationService()
          ->GetPushNotificationClientManager();
  DCHECK(appWideClientManager);
  appWideResult = appWideClientManager->HandleNotificationReception(userInfo);

  // Determine the final result to return.
  // Prioritize the profile manager's result if it was found and called.
  // Otherwise, use the app-wide manager's result.
  UIBackgroundFetchResult result =
      profileManagerCalledAndSuccessful ? profileResult : appWideResult;

  double processingTime =
      base::Time::Now().InSecondsFSinceUnixEpoch() - incomingNotificationTime;

  UmaHistogramCustomTimes(
      "IOS.PushNotification.IncomingNotificationProcessingTime",
      base::Milliseconds(processingTime),
      kTimeRangeIncomingNotificationHistogramMin,
      kTimeRangeIncomingNotificationHistogramMax,
      kTimeRangeHistogramBucketCount);

  return result;
}

- (void)applicationDidRegisterWithAPNS:(NSData*)deviceToken
                               profile:(ProfileIOS*)profile {
  GaiaIdToPushNotificationPreferenceMap* accountPreferenceMap =
      GaiaIdToPushNotificationPreferenceMapFromCache();

  // Return early if no accounts are signed into Chrome.
  if (!accountPreferenceMap.count) {
    return;
  }

  if (IsIOSMultiProfilePushNotificationHandlingEnabled()) {
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

  PushNotificationService* notificationService =
      GetApplicationContext()->GetPushNotificationService();

  // Registers Chrome's PushNotificationClients' Actionable Notifications with
  // iOS.
  PushNotificationClientManager* appWideClientManager =
      notificationService->GetPushNotificationClientManager();
  appWideClientManager->RegisterActionableNotifications();

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
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  if (appState.initStage == AppInitStage::kFinal && _runAfterInit) {
    for (ProceduralBlock block in _runAfterInit) {
      block();
    }
    _runAfterInit = nil;
  }
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

#pragma mark - Private

// Notifies the client manager that the scene is "foreground active".
- (void)appDidEnterForeground:(SceneState*)sceneState {
  if (IsIOSMultiProfilePushNotificationHandlingEnabled()) {
    PushNotificationClientManager* clientManager =
        GetClientManagerForProfile(sceneState.profileState.profile);

    if (clientManager) {
      clientManager->OnSceneActiveForegroundBrowserReady();
    } else {
      RecordClientManagerAccessFailure(
          PushNotificationClientManagerFailurePoint::kAppDidEnterForeground);
    }
  }

  PushNotificationClientManager* appWideClientManager =
      GetApplicationContext()
          ->GetPushNotificationService()
          ->GetPushNotificationClientManager();
  DCHECK(appWideClientManager);
  appWideClientManager->OnSceneActiveForegroundBrowserReady();

  for (ProceduralBlock block in _runAfterForeground) {
    block();
  }
  _runAfterForeground = nil;
  ProfileIOS* profile = sceneState.browserProviderInterface.mainBrowserProvider
                            .browser->GetProfile();
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

// Returns the first connected foreground active `SceneState`, or nil if there
// isn't one.
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
    [ProvisionalPushNotificationUtil
        enrollUserToProvisionalNotificationsForClientIds:
            {PushNotificationClientId::kSendTab}
                             clientEnabledForProvisional:YES
                                         withAuthService:authService
                                   deviceInfoSyncService:deviceInfoSyncService];
  }
}

// Runs the given `block` immediately if the app's `initStage` is already
// final, otherwise stores it to be called when the `initStage is final.
- (void)executeWhenInitStageFinal:(ProceduralBlock)block {
  if (_appState.initStage == AppInitStage::kFinal) {
    block();
    return;
  }

  if (!_runAfterInit) {
    _runAfterInit = [[NSMutableArray alloc] init];
  }
  [_runAfterInit addObject:block];
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

  if (IsIOSMultiProfilePushNotificationHandlingEnabled()) {
    PushNotificationClientManager* clientManager = GetClientManagerForUserInfo(
        response.notification.request.content.userInfo);

    if (clientManager) {
      clientManager->HandleNotificationInteraction(response);
    } else {
      RecordClientManagerAccessFailure(
          PushNotificationClientManagerFailurePoint::
              kHandleNotificationResponse);
    }
  }

  PushNotificationClientManager* appWideClientManager =
      GetApplicationContext()
          ->GetPushNotificationService()
          ->GetPushNotificationClientManager();
  appWideClientManager->HandleNotificationInteraction(response);
}

// Shows the app's notification settings in the first foreground active
// connected scene. Must only be called when the app has a foreground active
// scene.
- (void)openSettingsForNotification:(UNNotification*)notification {
  SceneState* sceneState = self.foregroundActiveScene;
  CHECK(sceneState);
  Browser* browser =
      sceneState.browserProviderInterface.mainBrowserProvider.browser;
  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  id<ApplicationCommands> applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  id<SettingsCommands> settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  [applicationHandler prepareToPresentModal:^{
    [settingsHandler showNotificationsSettings];
  }];
}

@end
