// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_account_context_manager.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"

namespace {

// Contains the pref service and key to access the permissions dict, and the
// client key to access the bool field within the dict that holds the
// permission state for a client.
struct PermissionsPref {
  raw_ptr<PrefService> service;
  const std::string key;
  const std::string client_key;
  const std::string profile_name;
};

// Helper for iteration over all profile attributes when looking for a profile
// with a given gaia id.
ProfileAttributesStorageIOS::IterationResult FindProfileWithGaiaId(
    const GaiaId& gaia_id,
    std::string& profile_name,
    const ProfileAttributesIOS& attr) {
  if (gaia_id == attr.GetGaiaId()) {
    profile_name = attr.GetProfileName();
    return ProfileAttributesStorageIOS::IterationResult::kTerminate;
  }
  return ProfileAttributesStorageIOS::IterationResult::kContinue;
}

// Helper to add account to PushNotificationAccountContextManager* for all
// known profiles during iteration.
void AddAccountToManager(PushNotificationAccountContextManager* manager,
                         const ProfileAttributesIOS& attr) {
  [manager addAccount:attr.GetGaiaId()];
}

}  // namespace

@implementation PushNotificationAccountContextManager {
  // Used to retrieve Profiles located at a given path.
  raw_ptr<ProfileManagerIOS> _profileManager;

  // A dictionary that maps a user's GAIA ID to an unsigned integer representing
  // the number of times the account is signed in across Profiles.
  std::map<GaiaId, size_t> _contextMap;
}

- (instancetype)initWithProfileManager:(ProfileManagerIOS*)manager {
  self = [super init];

  if (self) {
    _profileManager = manager;
    ProfileAttributesStorageIOS* storage =
        manager->GetProfileAttributesStorage();
    storage->IterateOverProfileAttributes(
        base::BindRepeating(&AddAccountToManager, self));
  }

  return self;
}

- (BOOL)addAccount:(const GaiaId&)gaiaID {
  if (gaiaID.empty()) {
    return NO;
  }

  _contextMap[gaiaID] += 1;
  return YES;
}

- (BOOL)removeAccount:(const GaiaId&)gaiaID {
  auto iterator = _contextMap.find(gaiaID);
  if (iterator == _contextMap.end()) {
    // The account was unexpectedly not found, so return NO to indicate that
    // it was not removed.
    return NO;
  }
  size_t& occurrencesAcrossProfiles = iterator->second;

  occurrencesAcrossProfiles--;
  if (occurrencesAcrossProfiles > 0) {
    return NO;
  }

  _contextMap.erase(iterator);
  return YES;
}

- (void)enablePushNotification:(PushNotificationClientId)clientID
                    forAccount:(const GaiaId&)gaiaID {
  PermissionsPref pref = [self prefsForClient:clientID account:gaiaID];
  // TODO:(crbug.com/1445551) Restore to DCHECK when signing into Chrome via
  // ConsistencySigninPromo UI updates the ProfileAttributesStorageIOS.
  if (!pref.service) {
    return;
  }
  ScopedDictPrefUpdate update(pref.service, pref.key);
  update->Set(pref.client_key, true);
  [self setAttributesForProfile:pref.profile_name fromPrefs:pref.service];
  base::UmaHistogramEnumeration("IOS.PushNotification.Client.Enabled",
                                clientID);
}

- (void)disablePushNotification:(PushNotificationClientId)clientID
                     forAccount:(const GaiaId&)gaiaID {
  PermissionsPref pref = [self prefsForClient:clientID account:gaiaID];
  // TODO:(crbug.com/1445551) Restore to DCHECK when signing into Chrome via
  // ConsistencySigninPromo UI updates the ProfileAttributesStorageIOS.
  if (!pref.service) {
    return;
  }
  ScopedDictPrefUpdate update(pref.service, pref.key);
  update->Set(pref.client_key, false);
  [self setAttributesForProfile:pref.profile_name fromPrefs:pref.service];
  base::UmaHistogramEnumeration("IOS.PushNotification.Client.Disabled",
                                clientID);
}

- (BOOL)isPushNotificationEnabledForClient:(PushNotificationClientId)clientID
                                forAccount:(const GaiaId&)gaiaID {
  PermissionsPref pref = [self prefsForClient:clientID account:gaiaID];
  // TODO:(crbug.com/1445551) Restore to DCHECK when signing into Chrome via
  // ConsistencySigninPromo UI updates the ProfileAttributesStorageIOS.
  if (!pref.service) {
    return NO;
  }
  return pref.service->GetDict(pref.key)
      .FindBool(pref.client_key)
      .value_or(false);
}

- (NSDictionary<NSString*, NSNumber*>*)preferenceMapForAccount:
    (const GaiaId&)gaiaID {
  ProfileIOS* profile = [self profileFrom:gaiaID];
  NSMutableDictionary<NSString*, NSNumber*>* result =
      [[NSMutableDictionary alloc] init];
  if (!profile) {
    return result;
  }

  const base::Value::Dict& pref =
      profile->GetPrefs()->GetDict(prefs::kFeaturePushNotificationPermissions);
  for (const auto&& [key, value] : pref) {
    [result setObject:@(value.GetBool()) forKey:base::SysUTF8ToNSString(key)];
  }
  return result;
}

- (NSArray<NSString*>*)accountIDs {
  NSMutableArray<NSString*>* keys =
      [[NSMutableArray alloc] initWithCapacity:_contextMap.size()];
  for (auto const& context : _contextMap) {
    [keys addObject:context.first.ToNSString()];
  }
  return keys;
}

- (NSUInteger)registrationCountForAccount:(const GaiaId&)gaiaID {
  DCHECK(base::Contains(_contextMap, gaiaID));
  return _contextMap[gaiaID];
}

#pragma mark - Private

// Returns the ProfileIOS that has the given gaiaID set as the primary
// account. TODO(crbug.com/40250402) Implement policy that computes correct
// permission set. This function naively chooses the first ProfileIOS
// that is associated with the given gaiaID. In a multi-profile environment
// where the given gaiaID is signed into multiple profiles, it is possible that
// the push notification enabled features' permissions may be incorrectly
// applied.
- (ProfileIOS*)profileFrom:(const GaiaId&)gaiaID {
  ProfileAttributesStorageIOS* storage =
      _profileManager->GetProfileAttributesStorage();

  std::string profileName;
  storage->IterateOverProfileAttributes(base::BindRepeating(
      &FindProfileWithGaiaId, gaiaID, std::ref(profileName)));

  if (!profileName.empty()) {
    return _profileManager->GetProfileWithName(profileName);
  }

  return nullptr;
}

// Returns the appropriate `PermissionsPref` for the given `clientID` and
// `gaiaID`. This can be either profile prefs or LocalState prefs.
- (PermissionsPref)prefsForClient:(PushNotificationClientId)clientID
                          account:(const GaiaId&)gaiaID {
  std::string clientKey = PushNotificationClientIdToString(clientID);
  switch (clientID) {
    case PushNotificationClientId::kCommerce:
    case PushNotificationClientId::kContent:
    case PushNotificationClientId::kSports:
    case PushNotificationClientId::kSendTab:
    case PushNotificationClientId::kReminders: {
      ProfileIOS* profile = [self profileFrom:gaiaID];
      if (!profile) {
        // TODO:(crbug.com/1445551) Restore to DCHECK when signing into Chrome
        // via ConsistencySigninPromo UI updates the
        // ProfileAttributesStorageIOS.
        return {nullptr, prefs::kFeaturePushNotificationPermissions, clientKey,
                ""};
      }
      return {profile->GetPrefs(), prefs::kFeaturePushNotificationPermissions,
              clientKey, profile->GetProfileName()};
    }
    case PushNotificationClientId::kTips:
    case PushNotificationClientId::kSafetyCheck:
      return {GetApplicationContext()->GetLocalState(),
              prefs::kAppLevelPushNotificationPermissions, clientKey, ""};
  }
}

// Copies the permissions dictionary from profile prefs to ProfileAttributesIOS
// in order to make them accessible when the profile isn't loaded.
- (void)setAttributesForProfile:(std::string_view)profileName
                      fromPrefs:(PrefService*)prefs {
  if (profileName.empty()) {
    return;
  }

  const base::Value::Dict& permissions =
      prefs->GetDict(prefs::kFeaturePushNotificationPermissions);
  GetApplicationContext()
      ->GetProfileManager()
      ->GetProfileAttributesStorage()
      ->UpdateAttributesForProfileWithName(
          profileName,
          base::BindOnce(
              [](base::Value::Dict permissions, ProfileAttributesIOS& attr) {
                attr.SetNotificationPermissions(std::move(permissions));
              },
              permissions.Clone()));
}

@end
