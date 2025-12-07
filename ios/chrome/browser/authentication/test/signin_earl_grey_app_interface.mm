// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/test/signin_earl_grey_app_interface.h"

#import <map>
#import <string>

#import "base/apple/foundation_util.h"
#import "base/functional/callback_helpers.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/titled_url_match.h"
#import "components/policy/core/browser/signin/profile_separation_policies.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/primary_account_mutator.h"
#import "components/supervised_user/core/browser/supervised_user_preferences.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "google_apis/gaia/core_account_id.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_identity_cell.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_interaction_manager.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/signin_test_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

@implementation SigninEarlGreyAppInterface

+ (void)addFakeIdentity:(FakeSystemIdentity*)fakeIdentity
    withUnknownCapabilities:(BOOL)usingUnknownCapabilities {
  FakeSystemIdentityManager* systemIdentityManager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  if (usingUnknownCapabilities) {
    systemIdentityManager->AddIdentityWithUnknownCapabilities(fakeIdentity);
  } else {
    systemIdentityManager->AddIdentity(fakeIdentity);
  }
}

+ (void)addFakeIdentity:(FakeSystemIdentity*)fakeIdentity
       withCapabilities:(NSDictionary<NSString*, NSNumber*>*)capabilities {
  FakeSystemIdentityManager* systemIdentityManager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  systemIdentityManager->AddIdentityWithCapabilities(fakeIdentity,
                                                     capabilities);
}

+ (void)addFakeIdentityForSSOAuthAddAccountFlow:
            (FakeSystemIdentity*)fakeIdentity
                        withUnknownCapabilities:(BOOL)usingUnknownCapabilities {
  [FakeSystemIdentityInteractionManager setIdentity:fakeIdentity
                            withUnknownCapabilities:usingUnknownCapabilities];
}

+ (void)forgetFakeIdentity:(FakeSystemIdentity*)fakeIdentity {
  FakeSystemIdentityManager* systemIdentityManager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  systemIdentityManager->ForgetIdentity(fakeIdentity, base::DoNothing());
}

+ (BOOL)isIdentityAdded:(FakeSystemIdentity*)fakeIdentity {
  FakeSystemIdentityManager* systemIdentityManager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  return systemIdentityManager->ContainsIdentity(fakeIdentity);
}

+ (void)setPersistentAuthErrorForAccount:(NSString*)accountGaiaId {
  CoreAccountId accountId = CoreAccountId::FromGaiaId(GaiaId(accountGaiaId));
  FakeSystemIdentityManager* systemIdentityManager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  systemIdentityManager->SetPersistentAuthErrorForAccount(accountId);
}

+ (NSString*)primaryAccountGaiaIDString {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  CoreAccountInfo info =
      IdentityManagerFactory::GetForProfile(profile)->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);

  return info.gaia.ToNSString();
}

+ (NSString*)primaryAccountEmail {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  CoreAccountInfo info =
      IdentityManagerFactory::GetForProfile(profile)->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);

  return base::SysUTF8ToNSString(info.email);
}

+ (NSSet<NSString*>*)accountsInProfileGaiaIDs {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  std::vector<CoreAccountInfo> infos =
      IdentityManagerFactory::GetForProfile(profile)
          ->GetAccountsWithRefreshTokens();

  NSMutableSet<NSString*>* gaias = [[NSMutableSet alloc] init];
  for (const CoreAccountInfo& info : infos) {
    [gaias addObject:info.gaia.ToNSString()];
  }
  return gaias;
}

+ (BOOL)isSignedOut {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();

  return !IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount(
      signin::ConsentLevel::kSignin);
}

+ (void)signOut {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  authentication_service->SignOut(signin_metrics::ProfileSignout::kTest, nil);
}

+ (void)signinWithFakeIdentity:(FakeSystemIdentity*)identity {
  if (![self isIdentityAdded:identity]) {
    // For convenience, add the identity, if it was not added yet.
    [self addFakeIdentity:identity withUnknownCapabilities:NO];
  }
  chrome_test_util::SignIn(identity);
}

+ (void)signinWithFakeManagedIdentityInPersonalProfile:
    (FakeSystemIdentity*)identity {
  CHECK(identity);
  CHECK(IsIdentityManaged(identity).value_or(NO));
  if (![self isIdentityAdded:identity]) {
    // For convenience, add the identity, if it was not added yet.
    [self addFakeIdentity:identity withUnknownCapabilities:NO];
  }

  if (AreSeparateProfilesForManagedAccountsEnabled()) {
    GetApplicationContext()
        ->GetAccountProfileMapper()
        ->MakePersonalProfileManagedWithGaiaID(identity.gaiaId);
  }

  chrome_test_util::SignIn(identity);
}

+ (void)triggerReauthDialogWithFakeIdentity:(FakeSystemIdentity*)identity {
  [FakeSystemIdentityInteractionManager setIdentity:identity
                            withUnknownCapabilities:NO];
  std::string emailAddress = base::SysNSStringToUTF8(identity.userEmail);
  PrefService* prefService = chrome_test_util::GetOriginalProfile()->GetPrefs();
  prefService->SetString(prefs::kGoogleServicesLastSyncingUsername,
                         emailAddress);
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kResignin
            accessPoint:signin_metrics::AccessPoint::kResigninInfobar];
  UIViewController* baseViewController =
      chrome_test_util::GetActiveViewController();
  SceneController* sceneController =
      chrome_test_util::GetForegroundActiveSceneController();
  [sceneController showSignin:command baseViewController:baseViewController];
}

+ (void)triggerConsistencyPromoSigninDialogWithURL:(NSURL*)url {
  const GURL gURL = net::GURLWithNSURL(url);
  UIViewController* baseViewController =
      chrome_test_util::GetActiveViewController();
  SceneController* sceneController =
      chrome_test_util::GetForegroundActiveSceneController();
  [sceneController showWebSigninPromoFromViewController:baseViewController
                                                    URL:gURL];
}

+ (void)presentSignInAccountsViewControllerIfNecessary {
  chrome_test_util::PresentSignInAccountsViewControllerIfNecessary();
}

+ (void)setSelectedType:(syncer::UserSelectableType)type enabled:(BOOL)enabled {
  syncer::SyncUserSettings* settings =
      SyncServiceFactory::GetForProfile(chrome_test_util::GetOriginalProfile())
          ->GetUserSettings();
  settings->SetSelectedTypes(/*sync_everything=*/false,
                             settings->GetSelectedTypes());
  settings->SetSelectedType(type, enabled);
}

+ (BOOL)isSelectedTypeEnabled:(syncer::UserSelectableType)type {
  syncer::SyncUserSettings* settings =
      SyncServiceFactory::GetForProfile(chrome_test_util::GetOriginalProfile())
          ->GetUserSettings();
  return settings->GetSelectedTypes().Has(type) ? YES : NO;
}

+ (void)setUseFakeResponsesForProfileSeparationPolicyRequests {
  chrome_test_util::SetUseFakeResponsesForProfileSeparationPolicyRequests();
}

+ (void)clearUseFakeResponsesForProfileSeparationPolicyRequests {
  chrome_test_util::ClearUseFakeResponsesForProfileSeparationPolicyRequests();
}

+ (void)setPolicyResponseForNextProfileSeparationPolicyRequest:
    (policy::ProfileSeparationDataMigrationSettings)
        profileSeparationDataMigrationSettings {
  chrome_test_util::SetPolicyResponseForNextProfileSeparationPolicyRequest(
      profileSeparationDataMigrationSettings);
}

+ (BOOL)areSeparateProfilesForManagedAccountsEnabled {
  return AreSeparateProfilesForManagedAccountsEnabled();
}

@end
