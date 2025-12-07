// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"

#import <memory>
#import <utility>

#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "base/time/time.h"
#import "components/affiliations/core/browser/affiliation_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#import "components/password_manager/core/browser/affiliation/password_affiliation_source_adapter.h"
#import "components/password_manager/core/browser/password_store/login_database.h"
#import "components/password_manager/core/browser/password_store/password_store.h"
#import "components/password_manager/core/browser/password_store/password_store_built_in_backend.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/browser/password_store_factory_util.h"
#import "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/model/credentials_cleaner_runner_factory.h"
#import "ios/chrome/browser/passwords/model/ios_password_store_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

using affiliations::AffiliationService;
using password_manager::AffiliatedMatchHelper;

// static
scoped_refptr<password_manager::PasswordStoreInterface>
IOSChromeAccountPasswordStoreFactory::GetForProfile(
    ProfileIOS* profile,
    ServiceAccessType access_type) {
  // `profile` gets always redirected to a non-Incognito one below, so
  // Incognito & IMPLICIT_ACCESS means that incognito browsing session would
  // result in traces in the normal Profile without the user knowing it.
  if (access_type == ServiceAccessType::IMPLICIT_ACCESS &&
      profile->IsOffTheRecord()) {
    return nullptr;
  }
  return GetInstance()
      ->GetServiceForProfileAs<password_manager::PasswordStoreInterface>(
          profile, /*create=*/true);
}

// static
IOSChromeAccountPasswordStoreFactory*
IOSChromeAccountPasswordStoreFactory::GetInstance() {
  static base::NoDestructor<IOSChromeAccountPasswordStoreFactory> instance;
  return instance.get();
}

IOSChromeAccountPasswordStoreFactory::IOSChromeAccountPasswordStoreFactory()
    : RefcountedProfileKeyedServiceFactoryIOS(
          "AccountPasswordStore",
          ProfileSelection::kRedirectedInIncognito,
          TestingCreation::kNoServiceForTests) {
  DependsOn(CredentialsCleanerRunnerFactory::GetInstance());
  DependsOn(IOSChromeAffiliationServiceFactory::GetInstance());
}

IOSChromeAccountPasswordStoreFactory::~IOSChromeAccountPasswordStoreFactory() =
    default;

scoped_refptr<RefcountedKeyedService>
IOSChromeAccountPasswordStoreFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabase(password_manager::kAccountStore,
                                            profile->GetStatePath(),
                                            profile->GetPrefs()));
  auto password_store = base::MakeRefCounted<password_manager::PasswordStore>(
      std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
          std::move(login_db),
          syncer::WipeModelUponSyncDisabledBehavior::kAlways,
          profile->GetPrefs(), GetApplicationContext()->GetOSCryptAsync()));
  AffiliationService* affiliation_service =
      IOSChromeAffiliationServiceFactory::GetForProfile(profile);
  password_store->Init(
      std::make_unique<AffiliatedMatchHelper>(affiliation_service));
  password_manager::SanitizeAndMigrateCredentials(
      CredentialsCleanerRunnerFactory::GetForProfile(profile), password_store,
      password_manager::kAccountStore, profile->GetPrefs(), base::Minutes(1),
      base::NullCallback());
  auto password_affiliation_adapter =
      std::make_unique<password_manager::PasswordAffiliationSourceAdapter>();
  password_affiliation_adapter->RegisterPasswordStore(password_store.get());
  affiliation_service->RegisterSource(std::move(password_affiliation_adapter));
  return password_store;
}
