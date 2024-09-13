// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/multi_identity/switch_profile_settings_mediator.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/ui/settings/multi_identity/switch_profile_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/multi_identity/switch_profile_settings_item.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"

@implementation SwitchProfileSettingsMediator {
  NSString* _activeProfileName;
}

- (instancetype)initWithActiveProfileName:(NSString*)activeProfileName {
  self = [super init];
  if (self) {
    _activeProfileName = activeProfileName;
  }
  return self;
}

#pragma mark - Properties

- (void)setConsumer:(id)consumer {
  if (consumer == _consumer) {
    return;
  }
  _consumer = consumer;
  [self updateConsumer];
}

#pragma mark - SwitchProfileSettingsDelegate

- (void)openProfileInNewWindowWithSwitchProfileSettingsItem:
    (SwitchProfileSettingsItem*)switchProfileSettingsItem {
  NSString* profileName = switchProfileSettingsItem.profileName;
  // Update the last used profile so that the newly created scene is linked
  // to the selected profile (and not to the old one).
  GetApplicationContext()->GetLocalState()->SetString(
      prefs::kLastUsedProfile, base::SysNSStringToUTF8(profileName));
  // TODO(crbug.com/333520714): Add logic to open the profile in the same window
  // once the API is available.
  // Open the selected profile in a new window.
  if (@available(iOS 17, *)) {
    UISceneSessionActivationRequest* activationRequest =
        [UISceneSessionActivationRequest request];
    [UIApplication.sharedApplication
        activateSceneSessionForRequest:activationRequest
                          errorHandler:nil];
  }
}

#pragma mark - Private

// Updates the consumer by settings the list of profiles.
- (void)updateConsumer {
  NSMutableArray<SwitchProfileSettingsItem*>* items = [NSMutableArray array];
  ProfileAttributesStorageIOS* profileStorage =
      GetApplicationContext()
          ->GetProfileManager()
          ->GetProfileAttributesStorage();
  size_t profile_count = profileStorage->GetNumberOfProfiles();
  for (size_t index = 0; index < profile_count; ++index) {
    ProfileAttributesIOS profileAttribute =
        profileStorage->GetAttributesForProfileAtIndex(index);
    SwitchProfileSettingsItem* item = [[SwitchProfileSettingsItem alloc] init];
    item.avatar = ios::provider::GetSigninDefaultAvatar();
    item.profileName =
        base::SysUTF8ToNSString(profileAttribute.GetProfileName());
    item.active = [item.profileName isEqualToString:_activeProfileName];
    [items addObject:item];
  }
  [self.consumer setSwitchProfileSettingsItem:[items copy]];
}

@end
