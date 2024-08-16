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
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"

namespace {

// Contains the pref service and key to access the permissions dict, and the
// client key to access the bool field within the dict that holds the
// permission state for a client.
struct PermissionsPref {
  raw_ptr<PrefService> service;
  const std::string key;
  const std::string client_key;
};

}  // namespace

@implementation PushNotificationAccountContextManager {
  // Used to retrieve BrowserStates located at a given path.
  raw_ptr<ChromeBrowserStateManager> _chromeBrowserStateManager;

  // A dictionary that maps a user's GAIA ID to an unsigned integer representing
  // the number of times the account is signed in across BrowserStates.
  std::map<std::string, size_t> _contextMap;
}

- (instancetype)initWithChromeBrowserStateManager:
    (ChromeBrowserStateManager*)manager {
  self = [super init];

  if (self) {
    _chromeBrowserStateManager = manager;
    BrowserStateInfoCache* infoCache = manager->GetBrowserStateInfoCache();
    const size_t numberOfBrowserStates = infoCache->GetNumberOfBrowserStates();
    for (size_t i = 0; i < numberOfBrowserStates; i++) {
      const std::string& gaiaID = infoCache->GetGAIAIdOfBrowserStateAtIndex(i);
      [self addAccount:gaiaID];
    }
  }

  return self;
}

- (BOOL)addAccount:(const std::string&)gaiaID {
  if (gaiaID.empty()) {
    return NO;
  }

  _contextMap[gaiaID] += 1;
  return YES;
}

- (BOOL)removeAccount:(const std::string&)gaiaID {
  auto iterator = _contextMap.find(gaiaID);
  DCHECK(iterator != _contextMap.end());
  if (iterator == _contextMap.end()) {
    // The account was unexpectedly not found, so return NO to indicate that
    // it was not removed.
    return NO;
  }
  size_t& occurrencesAcrossBrowserStates = iterator->second;

  occurrencesAcrossBrowserStates--;
  if (occurrencesAcrossBrowserStates > 0) {
    return NO;
  }

  _contextMap.erase(iterator);
  return YES;
}

- (void)enablePushNotification:(PushNotificationClientId)clientID
                    forAccount:(const std::string&)gaiaID {
  PermissionsPref pref = [self prefsForClient:clientID account:gaiaID];
  // TODO:(crbug.com/1445551) Restore to DCHECK when signing into Chrome via
  // ConsistencySigninPromo UI updates the BrowserStateInfoCache.
  if (!pref.service) {
    return;
  }
  ScopedDictPrefUpdate update(pref.service, pref.key);
  update->Set(pref.client_key, true);
  base::UmaHistogramEnumeration("IOS.PushNotification.Client.Enabled",
                                clientID);
}

- (void)disablePushNotification:(PushNotificationClientId)clientID
                     forAccount:(const std::string&)gaiaID {
  PermissionsPref pref = [self prefsForClient:clientID account:gaiaID];
  // TODO:(crbug.com/1445551) Restore to DCHECK when signing into Chrome via
  // ConsistencySigninPromo UI updates the BrowserStateInfoCache.
  if (!pref.service) {
    return;
  }
  ScopedDictPrefUpdate update(pref.service, pref.key);
  update->Set(pref.client_key, false);
  base::UmaHistogramEnumeration("IOS.PushNotification.Client.Disabled",
                                clientID);
}

- (BOOL)isPushNotificationEnabledForClient:(PushNotificationClientId)clientID
                                forAccount:(const std::string&)gaiaID {
  PermissionsPref pref = [self prefsForClient:clientID account:gaiaID];
  // TODO:(crbug.com/1445551) Restore to DCHECK when signing into Chrome via
  // ConsistencySigninPromo UI updates the BrowserStateInfoCache.
  if (!pref.service) {
    return NO;
  }
  return pref.service->GetDict(pref.key)
      .FindBool(pref.client_key)
      .value_or(false);
}

- (NSDictionary<NSString*, NSNumber*>*)preferenceMapForAccount:
    (const std::string&)gaiaID {
  ChromeBrowserState* browserState = [self chromeBrowserStateFrom:gaiaID];
  NSMutableDictionary<NSString*, NSNumber*>* result =
      [[NSMutableDictionary alloc] init];
  if (!browserState) {
    return result;
  }

  const base::Value::Dict& pref = browserState->GetPrefs()->GetDict(
      prefs::kFeaturePushNotificationPermissions);
  for (const auto&& [key, value] : pref) {
    [result setObject:@(value.GetBool()) forKey:base::SysUTF8ToNSString(key)];
  }
  return result;
}

- (NSArray<NSString*>*)accountIDs {
  NSMutableArray<NSString*>* keys =
      [[NSMutableArray alloc] initWithCapacity:_contextMap.size()];
  for (auto const& context : _contextMap) {
    [keys addObject:base::SysUTF8ToNSString(context.first)];
  }
  return keys;
}

- (NSUInteger)registrationCountForAccount:(const std::string&)gaiaID {
  DCHECK(base::Contains(_contextMap, gaiaID));
  return _contextMap[gaiaID];
}

#pragma mark - Private

// Returns the ChromeBrowserState that has the given gaiaID set as the primary
// account. TODO(crbug.com/40250402) Implement policy that computes correct
// permission set. This function naively chooses the first ChromeBrowserState
// that is associated with the given gaiaID. In a multi-profile environment
// where the given gaiaID is signed into multiple profiles, it is possible that
// the push notification enabled features' permissions may be incorrectly
// applied.
- (ChromeBrowserState*)chromeBrowserStateFrom:(const std::string&)gaiaID {
  BrowserStateInfoCache* infoCache =
      _chromeBrowserStateManager->GetBrowserStateInfoCache();
  const size_t numberOfBrowserStates = infoCache->GetNumberOfBrowserStates();

  for (size_t i = 0; i < numberOfBrowserStates; i++) {
    const std::string& browserStateGaiaID =
        infoCache->GetGAIAIdOfBrowserStateAtIndex(i);
    if (gaiaID == browserStateGaiaID) {
      const std::string& name = infoCache->GetNameOfBrowserStateAtIndex(i);
      return _chromeBrowserStateManager->GetBrowserStateByName(name);
    }
  }

  return nil;
}

// Returns the appropriate `PermissionsPref` for the given `clientID` and
// `gaiaID`. This can be either BrowserState prefs or LocalState prefs.
- (PermissionsPref)prefsForClient:(PushNotificationClientId)clientID
                          account:(const std::string&)gaiaID {
  std::string clientKey =
      PushNotificationClientManager::PushNotificationClientIdToString(clientID);
  switch (clientID) {
    case PushNotificationClientId::kCommerce:
    case PushNotificationClientId::kContent:
    case PushNotificationClientId::kSports: {
      ChromeBrowserState* browserState = [self chromeBrowserStateFrom:gaiaID];
      if (!browserState) {
        // TODO:(crbug.com/1445551) Restore to DCHECK when signing into Chrome
        // via ConsistencySigninPromo UI updates the BrowserStateInfoCache.
        return {nullptr, prefs::kFeaturePushNotificationPermissions, clientKey};
      }
      return {browserState->GetPrefs(),
              prefs::kFeaturePushNotificationPermissions, clientKey};
    }
    case PushNotificationClientId::kTips:
    case PushNotificationClientId::kSafetyCheck:
      return {GetApplicationContext()->GetLocalState(),
              prefs::kAppLevelPushNotificationPermissions, clientKey};
  }
}

@end
