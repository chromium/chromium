// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/push_notification_account_context_manager.h"

#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/browser_state/browser_state_info_cache.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/prefs/pref_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PushNotificationAccountContext ()

@property(nonatomic, assign) NSUInteger occurrencesAcrossBrowserStates;

@end

@implementation PushNotificationAccountContext
@end

@implementation PushNotificationAccountContextManager {
  // Used to retrieve BrowserStates located at a given path.
  ios::ChromeBrowserStateManager* _chromeBrowserStateManager;
  // Maps a GaiaID to the given account's AccountContext object.
  NSMutableDictionary<NSString*, PushNotificationAccountContext*>* _contextMap;
}

- (instancetype)initWithChromeBrowserStateManager:
    (ios::ChromeBrowserStateManager*)manager {
  self = [super init];

  if (self) {
    _contextMap = [[NSMutableDictionary alloc] init];
    _chromeBrowserStateManager = manager;
    BrowserStateInfoCache* infoCache = manager->GetBrowserStateInfoCache();

    const size_t numberOfBrowserStates = infoCache->GetNumberOfBrowserStates();
    for (size_t i = 0; i < numberOfBrowserStates; i++) {
      NSString* gaiaID =
          base::SysUTF8ToNSString(infoCache->GetGAIAIdOfBrowserStateAtIndex(i));
      base::FilePath path = infoCache->GetPathOfBrowserStateAtIndex(i);
      ChromeBrowserState* chromeBrowserState =
          _chromeBrowserStateManager->GetBrowserState(path);
      [self addAccount:gaiaID withBrowserState:chromeBrowserState];
    }
  }

  return self;
}

- (BOOL)addAccount:(NSString*)gaiaID {
  ChromeBrowserState* chromeBrowserState = [self chromeBrowserStateFrom:gaiaID];
  if (!chromeBrowserState)
    return NO;

  return [self addAccount:gaiaID
         withBrowserState:[self chromeBrowserStateFrom:gaiaID]];
}

- (BOOL)removeAccount:(NSString*)gaiaID {
  PushNotificationAccountContext* context = _contextMap[gaiaID];
  DCHECK(context);

  context.occurrencesAcrossBrowserStates--;
  if (context.occurrencesAcrossBrowserStates > 0)
    return NO;

  [_contextMap removeObjectForKey:gaiaID];
  return YES;
}

#pragma mark - Properties

- (NSDictionary<NSString*, PushNotificationAccountContext*>*)contextMap {
  return _contextMap;
}

#pragma mark - Private

// Create a new AccountContext object for the given gaiaID, if the gaia id does
// not already exist in the dictionary, and maps the gaiaID to the new context
// object.
- (BOOL)addAccount:(NSString*)gaiaID
    withBrowserState:(ChromeBrowserState*)browserState {
  DCHECK(browserState);

  if (!gaiaID.length)
    return NO;

  PushNotificationAccountContext* context = _contextMap[gaiaID];
  if (context) {
    context.occurrencesAcrossBrowserStates++;
    return NO;
  }

  PrefService* prefService = browserState->GetPrefs();
  NSMutableDictionary<NSString*, NSNumber*>* preferenceMap =
      [[NSMutableDictionary alloc] init];
  const base::Value::Dict& permissions =
      prefService->GetDict(prefs::kFeaturePushNotificationPermissions);

  for (const auto pair : permissions) {
    preferenceMap[base::SysUTF8ToNSString(pair.first)] =
        [NSNumber numberWithBool:pair.second.GetBool()];
  }

  context = [[PushNotificationAccountContext alloc] init];
  context.preferenceMap = preferenceMap;
  context.occurrencesAcrossBrowserStates = 1;
  _contextMap[gaiaID] = context;

  return YES;
}

// Returns the ChromeBrowserState that has the given gaiaID set as the primary
// account. TODO(crbug.com/1400732) Implement policy that computes correct
// permission set. This function naively chooses the first ChromeBrowserState
// that is associated with the given gaiaID. In a multi-profile environment
// where the given gaiaID is signed into multiple profiles, it is possible that
// the push notification enabled features' permissions may be incorrectly
// applied.
- (ChromeBrowserState*)chromeBrowserStateFrom:(NSString*)gaiaID {
  BrowserStateInfoCache* infoCache =
      _chromeBrowserStateManager->GetBrowserStateInfoCache();
  const size_t numberOfBrowserStates = infoCache->GetNumberOfBrowserStates();
  for (size_t i = 0; i < numberOfBrowserStates; i++) {
    NSString* browserStateGaiaID =
        base::SysUTF8ToNSString(infoCache->GetGAIAIdOfBrowserStateAtIndex(i));

    if (gaiaID == browserStateGaiaID) {
      base::FilePath path = infoCache->GetPathOfBrowserStateAtIndex(i);
      return _chromeBrowserStateManager->GetBrowserState(path);
    }
  }

  return nil;
}

@end
