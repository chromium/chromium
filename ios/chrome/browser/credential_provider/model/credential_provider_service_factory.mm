// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/credential_provider_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_service.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/chrome/common/credential_provider/archivable_credential_store.h"
#import "ios/chrome/common/credential_provider/constants.h"

// static
CredentialProviderService* CredentialProviderServiceFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
CredentialProviderService* CredentialProviderServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<CredentialProviderService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
CredentialProviderServiceFactory*
CredentialProviderServiceFactory::GetInstance() {
  static base::NoDestructor<CredentialProviderServiceFactory> instance;
  return instance.get();
}

CredentialProviderServiceFactory::CredentialProviderServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "CredentialProviderService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IOSChromeAffiliationServiceFactory::GetInstance());
  DependsOn(IOSChromeAccountPasswordStoreFactory::GetInstance());
  DependsOn(IOSChromeProfilePasswordStoreFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(IOSChromeFaviconLoaderFactory::GetInstance());
}

CredentialProviderServiceFactory::~CredentialProviderServiceFactory() = default;

std::unique_ptr<KeyedService>
CredentialProviderServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  scoped_refptr<password_manager::PasswordStoreInterface>
      profile_password_store =
          IOSChromeProfilePasswordStoreFactory::GetForProfile(
              profile, ServiceAccessType::IMPLICIT_ACCESS);
  scoped_refptr<password_manager::PasswordStoreInterface>
      account_password_store =
          IOSChromeAccountPasswordStoreFactory::GetForProfile(
              profile, ServiceAccessType::IMPLICIT_ACCESS);
  webauthn::PasskeyModel* passkeyModel =
      base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials)
          ? IOSPasskeyModelFactory::GetForProfile(profile)
          : nullptr;
  ArchivableCredentialStore* credential_store =
      [[ArchivableCredentialStore alloc]
          initWithFileURL:CredentialProviderSharedArchivableStoreURL()];
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  affiliations::AffiliationService* affiliation_service =
      IOSChromeAffiliationServiceFactory::GetForProfile(profile);
  FaviconLoader* favicon_loader =
      IOSChromeFaviconLoaderFactory::GetForProfile(profile);

  return std::make_unique<CredentialProviderService>(
      profile->GetPrefs(), profile_password_store, account_password_store,
      passkeyModel, credential_store, identity_manager, sync_service,
      affiliation_service, favicon_loader);
}
