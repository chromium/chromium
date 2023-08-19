// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/push_notification_account_context_manager.h"

#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/push_notification/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/push_notification_client_manager.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

@implementation PushNotificationAccountContextManager {
  // Used to retrieve BrowserStates located at a given path.
  ios::ChromeBrowserStateManager* _chromeBrowserStateManager;

  // A dictionary that maps a user's GAIA ID to an unsigned integer representing
  // the number of times the account is signed in across BrowserStates.
  std::map<std::string, size_t> _contextMap;
}

- (instancetype)initWithChromeBrowserStateManager:
    (ios::ChromeBrowserStateManager*)manager {
  self = [super init];

  if (self) {
    _chromeBrowserStateManager = manager;
    BrowserStateInfoCache* infoCache = manager->GetBrowserStateInfoCache();
    const size_t numberOfBrowserStates = infoCache->GetNumberOfBrowserStates();
    for (size_t i = 0; i < numberOfBrowserStates; i++) {
      std::string gaiaID = infoCache->GetGAIAIdOfBrowserStateAtIndex(i);
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
  ChromeBrowserState* browserState = [self chromeBrowserStateFrom:gaiaID];
  DCHECK(browserState);
  ScopedDictPrefUpdate update(browserState->GetPrefs(),
                              prefs::kFeaturePushNotificationPermissions);
  std::string key =
      PushNotificationClientManager::PushNotificationClientIdToString(clientID);
  update->Set(key, true);
}

- (void)disablePushNotification:(PushNotificationClientId)clientID
                     forAccount:(const std::string&)gaiaID {
  ChromeBrowserState* browserState = [self chromeBrowserStateFrom:gaiaID];
  DCHECK(browserState);
  ScopedDictPrefUpdate update(browserState->GetPrefs(),
                              prefs::kFeaturePushNotificationPermissions);
  std::string key =
      PushNotificationClientManager::PushNotificationClientIdToString(clientID);
  update->Set(key, false);
}

- (BOOL)isPushNotificationEnabledForClient:(PushNotificationClientId)clientID
                                forAccount:(const std::string&)gaiaID {
  ChromeBrowserState* browserState = [self chromeBrowserStateFrom:gaiaID];
  // TODO:(crbug.com/1445551) Restore to DCHECK when signing into Chrome via
  // ConsistencySigninPromo UI updates the BrowserStateInfoCache.
  if (!browserState) {
    return false;
  }

  std::string key =
      PushNotificationClientManager::PushNotificationClientIdToString(clientID);
  return browserState->GetPrefs()
      ->GetDict(prefs::kFeaturePushNotificationPermissions)
      .FindBool(key)
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
// account. TODO(crbug.com/1400732) Implement policy that computes correct
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
    std::string browserStateGaiaID =
        infoCache->GetGAIAIdOfBrowserStateAtIndex(i);
    if (gaiaID == browserStateGaiaID) {
      base::FilePath path = infoCache->GetPathOfBrowserStateAtIndex(i);
      return _chromeBrowserStateManager->GetBrowserState(path);
    }
  }

  return nil;
}

@end
