// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/multi_identity/switch_profile_settings_mediator.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/uuid.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/profile/model/constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/ui/settings/multi_identity/switch_profile_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/multi_identity/switch_profile_settings_item.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"

@interface SwitchProfileSettingsMediator () <
    ChromeAccountManagerServiceObserver>
@end

@implementation SwitchProfileSettingsMediator {
  NSString* _activeProfileName;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // List of all items to represent each existing profile and each managed
  // identity without a profile.
  NSMutableArray<SwitchProfileSettingsItem*>* _profileItems;
  // True if `-[SwitchProfileSettingsMediator updateConsumer]` task is pending.
  BOOL _updateConsumerWillBeCalled;
}

- (instancetype)initWithChromeAccountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                                  activeProfileName:
                                      (NSString*)activeProfileName {
  self = [super init];
  if (self) {
    _accountManagerService = accountManagerService;
    _activeProfileName = activeProfileName;
    _profileItems = [NSMutableArray array];
    [self loadAllProfiles];
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
    (SwitchProfileSettingsItem*)item {
  std::string profileName = base::SysNSStringToUTF8(item.profileName);
  if (!profileName.empty()) {
    // The profile already exists, just open it.
    [self openProfileInNewWindowWithProfileName:profileName];
    return;
  }
  ProfileManagerIOS* profileManager =
      GetApplicationContext()->GetProfileManager();
  ProfileAttributesStorageIOS* profileStorage =
      profileManager->GetProfileAttributesStorage();
  // The profile doesn't exist. Once the profile is created,
  // `item.attachedGaiaId` needs to be attached to the new profile.
  do {
    profileName = base::Uuid::GenerateRandomV4().AsLowercaseString();
  } while (profileStorage->HasProfileWithName(profileName));
  std::string attachedGaiaID = base::SysNSStringToUTF8(item.attachedGaiaId);
  CHECK(!attachedGaiaID.empty());
  __weak __typeof(self) weakSelf = self;
  ProfileManagerIOS::ProfileLoadedCallback createdCallback = base::BindOnce(
      [](__typeof(self) strongSelf, std::string attachedGaiaID,
         ProfileIOS* profile) {
        [strongSelf profileCreatedWithProfile:profile
                               attachedGaiaID:attachedGaiaID];
        [strongSelf
            openProfileInNewWindowWithProfileName:profile->GetProfileName()];
      },
      weakSelf, attachedGaiaID);
  // Create to be able to switch to it.
  profileManager->CreateProfileAsync(profileName, {},
                                     std::move(createdCallback));
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  [self loadAllProfiles];
}

- (void)identityUpdated:(id<SystemIdentity>)identity {
  [self fetchHostedDomainWithIdentity:identity];
}

- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService {
  _accountManagerService = nil;
}

#pragma mark - Private

// Create an item for each profile, and for each managed identity that isn't
// assigned to a profile yet.
- (void)loadAllProfiles {
  [_profileItems removeAllObjects];
  // Sets of all gaia IDs that are attached to an existing profile.
  std::set<std::string> gaiaIdsWithProfile;
  ProfileAttributesStorageIOS* profileStorage =
      GetApplicationContext()
          ->GetProfileManager()
          ->GetProfileAttributesStorage();
  size_t profile_count = profileStorage->GetNumberOfProfiles();
  // Create an item for each profile.
  for (size_t index = 0; index < profile_count; ++index) {
    ProfileAttributesIOS profileAttributes =
        profileStorage->GetAttributesForProfileAtIndex(index);
    std::string profileName = profileAttributes.GetProfileName();
    std::string firstGaiaID;
    for (const std::string& gaiaID : profileAttributes.GetAttachedGaiaIds()) {
      gaiaIdsWithProfile.insert(gaiaID);
      if (firstGaiaID.empty()) {
        firstGaiaID = gaiaID;
      }
    }
    SwitchProfileSettingsItem* item =
        [self profileItemWithProfileName:profileName gaiaID:firstGaiaID];
    [_profileItems addObject:item];
  }
  // For each identity that is not attached to a profile yet, the hosted
  // domain needs to be loaded.
  for (id<SystemIdentity> identity in _accountManagerService
           ->GetAllIdentities()) {
    std::string gaiaID = base::SysNSStringToUTF8(identity.gaiaID);
    if (gaiaIdsWithProfile.contains(gaiaID)) {
      continue;
    }
    // This identity doesn't have a profile yet. Need to check for its hosted
    // to know if it should be have a profile or not.
    [self fetchHostedDomainWithIdentity:identity];
  }
  [self updateConsumer];
}

// Fetches hosted domain asynchronously for `identity`.
- (void)fetchHostedDomainWithIdentity:(id<SystemIdentity>)identity {
  SystemIdentityManager* systemIdentityManager =
      GetApplicationContext()->GetSystemIdentityManager();
  __weak __typeof(self) weakSelf = self;
  SystemIdentityManager::HostedDomainCallback hostedDomainCalback =
      base::BindOnce(
          [](__typeof(self) strongSelf, id<SystemIdentity> identity,
             NSString* hostedDomain, NSError* error) {
            [strongSelf hostedDomainFetchedWithString:hostedDomain
                                             identity:identity];
          },
          weakSelf, identity);
  NSString* hostedDomain =
      systemIdentityManager->GetCachedHostedDomainForIdentity(identity);
  if (!hostedDomain) {
    // Hosted domain isn't cached yet; fetch it.
    systemIdentityManager->GetHostedDomain(identity,
                                           std::move(hostedDomainCalback));
  } else {
    std::move(hostedDomainCalback).Run(hostedDomain, /*error=*/nil);
  }
}

// Updates `_profileItemsPerProfileName` if the identity is a managed account.
- (void)hostedDomainFetchedWithString:(NSString*)hostedDomain
                             identity:(id<SystemIdentity>)identity {
  if (!hostedDomain) {
    // No hosted domain, let's try again.
    [self fetchHostedDomainWithIdentity:identity];
  } else if (hostedDomain.length == 0) {
    // The identity is not managed. No need profile needed.
    return;
  }
  // Create an item for this managed identity.
  std::string gaiaID = base::SysNSStringToUTF8(identity.gaiaID);
  SwitchProfileSettingsItem* item = [self profileItemWithProfileName:""
                                                              gaiaID:gaiaID];
  [_profileItems addObject:item];
  if (!_updateConsumerWillBeCalled) {
    _updateConsumerWillBeCalled = YES;
    // To avoid a blinking UI that reloads itself too often, 1 second delay
    // is added to aggregate the delegate refresh.
    __weak __typeof(self) weakSelf = self;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](__typeof(self) strongSelf) { [strongSelf updateConsumerTask]; },
            weakSelf),
        base::Seconds(1));
  }
}

- (void)updateConsumerTask {
  _updateConsumerWillBeCalled = NO;
  [self updateConsumer];
}

// Updates the consumer by settings the list of profiles.
- (void)updateConsumer {
  [_profileItems sortUsingComparator:^(SwitchProfileSettingsItem* item1,
                                       SwitchProfileSettingsItem* item2) {
    return [item1.displayName compare:item2.displayName];
  }];
  [self.consumer setSwitchProfileSettingsItem:[_profileItems copy]];
}

// Opens a new window for `profile`.
- (void)openProfileInNewWindowWithProfileName:(std::string)profileName {
  // Update the last used profile so that the newly created scene is linked
  // to the selected profile (and not to the old one).
  GetApplicationContext()->GetLocalState()->SetString(prefs::kLastUsedProfile,
                                                      profileName);
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

// Creates an item with `profileName` and for `gaiaID`.
// `profileName` is empty if the profile doesn't exist yet, but then gaiaID must
// not be empty.
// `gaiaID` is empty for the personal profile or for the test profiles created
// with experimental flags, but then `profileName` must not be empty.
- (SwitchProfileSettingsItem*)profileItemWithProfileName:
                                  (const std::string&)profileName
                                                  gaiaID:(std::string)gaiaID {
  CHECK(!profileName.empty() || !gaiaID.empty());
  SwitchProfileSettingsItem* item = [[SwitchProfileSettingsItem alloc] init];
  item.profileName = base::SysUTF8ToNSString(profileName);
  item.attachedGaiaId = base::SysUTF8ToNSString(gaiaID);
  item.active = [item.profileName isEqualToString:_activeProfileName];
  if (profileName == kIOSChromeInitialProfile) {
    // TODO(crbug.com/331783685): Remove assumption that "Default" is the
    // personal profile.
    item.displayName = @"Personal profile";
    item.avatar = ios::provider::GetSigninDefaultAvatar();
    return item;
  }
  id<SystemIdentity> identity =
      _accountManagerService->GetIdentityWithGaiaID(gaiaID);
  if (identity) {
    item.displayName = identity.userEmail;
    item.avatar = _accountManagerService->GetIdentityAvatarWithIdentity(
        identity, IdentityAvatarSize::SmallSize);
  } else {
    item.displayName = item.profileName;
    item.avatar = ios::provider::GetSigninDefaultAvatar();
  }
  return item;
}

// Attach `attachedGaiaID` to `profile`.
- (void)profileCreatedWithProfile:(ProfileIOS*)profile
                   attachedGaiaID:(const std::string&)attachedGaiaID {
  CHECK(!attachedGaiaID.empty());
  ProfileAttributesIOS::GaiaIdSet gaiaIDs;
  gaiaIDs.insert(attachedGaiaID);
  std::string profileName = profile->GetProfileName();
  GetApplicationContext()
      ->GetProfileManager()
      ->GetProfileAttributesStorage()
      ->UpdateAttributesForProfileWithName(
          profileName, base::BindOnce(
                           [](const ProfileAttributesIOS::GaiaIdSet& gaiaIDs,
                              ProfileAttributesIOS attr) {
                             attr.SetAttachedGaiaIds(gaiaIDs);
                             return attr;
                           },
                           gaiaIDs));
}

@end
