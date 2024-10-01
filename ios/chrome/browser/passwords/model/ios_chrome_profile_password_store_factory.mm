// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"

#import <memory>
#import <utility>

#import "base/command_line.h"
#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "components/affiliations/core/browser/affiliation_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/password_manager/core/browser/affiliation/password_affiliation_source_adapter.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/browser/password_store/login_database.h"
#import "components/password_manager/core/browser/password_store/password_store_built_in_backend.h"
#import "components/password_manager/core/browser/password_store_factory_util.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/model/credentials_cleaner_runner_factory.h"
#import "ios/chrome/browser/passwords/model/ios_password_store_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

using affiliations::AffiliationService;
using password_manager::AffiliatedMatchHelper;

// Returns what the profile store should do when sync is disabled, that is,
// whether passwords might need to be deleted.
syncer::WipeModelUponSyncDisabledBehavior
GetWipeModelUponSyncDisabledBehaviorForProfileStore() {
  if (IsFirstSessionAfterDeviceRestore() != signin::Tribool::kTrue) {
    return syncer::WipeModelUponSyncDisabledBehavior::kNever;
  }

  return syncer::WipeModelUponSyncDisabledBehavior::kOnceIfTrackingMetadata;
}

}  // namespace

// static
scoped_refptr<password_manager::PasswordStoreInterface>
IOSChromeProfilePasswordStoreFactory::GetForBrowserState(
    ProfileIOS* profile,
    ServiceAccessType access_type) {
  return GetForProfile(profile, access_type);
}

// static
scoped_refptr<password_manager::PasswordStoreInterface>
IOSChromeProfilePasswordStoreFactory::GetForProfile(
    ProfileIOS* profile,
    ServiceAccessType access_type) {
  // `profile` gets always redirected to a non-Incognito profile below, so
  // Incognito & IMPLICIT_ACCESS means that incognito browsing session would
  // result in traces in the normal profile without the user knowing it.
  if (access_type == ServiceAccessType::IMPLICIT_ACCESS &&
      profile->IsOffTheRecord()) {
    return nullptr;
  }
  return base::WrapRefCounted(
      static_cast<password_manager::PasswordStoreInterface*>(
          GetInstance()->GetServiceForBrowserState(profile, true).get()));
}

// static
IOSChromeProfilePasswordStoreFactory*
IOSChromeProfilePasswordStoreFactory::GetInstance() {
  static base::NoDestructor<IOSChromeProfilePasswordStoreFactory> instance;
  return instance.get();
}

IOSChromeProfilePasswordStoreFactory::IOSChromeProfilePasswordStoreFactory()
    : RefcountedBrowserStateKeyedServiceFactory(
          "PasswordStore",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(CredentialsCleanerRunnerFactory::GetInstance());
  DependsOn(IOSChromeAffiliationServiceFactory::GetInstance());
}

IOSChromeProfilePasswordStoreFactory::~IOSChromeProfilePasswordStoreFactory() {}

scoped_refptr<RefcountedKeyedService>
IOSChromeProfilePasswordStoreFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabaseForProfileStorage(
          profile->GetStatePath(), profile->GetPrefs()));

  os_crypt_async::OSCryptAsync* os_crypt_async =
      base::FeatureList::IsEnabled(
          password_manager::features::kUseAsyncOsCryptInLoginDatabase)
          ? GetApplicationContext()->GetOSCryptAsync()
          : nullptr;

  scoped_refptr<password_manager::PasswordStore> store =
      base::MakeRefCounted<password_manager::PasswordStore>(
          std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
              std::move(login_db),
              GetWipeModelUponSyncDisabledBehaviorForProfileStore(),
              profile->GetPrefs(), os_crypt_async));

  AffiliationService* affiliation_service =
      IOSChromeAffiliationServiceFactory::GetForProfile(profile);
  std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper =
      std::make_unique<AffiliatedMatchHelper>(affiliation_service);
  std::unique_ptr<password_manager::PasswordAffiliationSourceAdapter>
      password_affiliation_adapter = std::make_unique<
          password_manager::PasswordAffiliationSourceAdapter>();

  store->Init(profile->GetPrefs(), std::move(affiliated_match_helper));

  password_manager::SanitizeAndMigrateCredentials(
      CredentialsCleanerRunnerFactory::GetForProfile(profile), store,
      password_manager::kProfileStore, profile->GetPrefs(), base::Seconds(60),
      base::NullCallback());
  if (!profile->IsOffTheRecord()) {
    DelayReportingPasswordStoreMetrics(profile);
  }

  password_affiliation_adapter->RegisterPasswordStore(store.get());
  affiliation_service->RegisterSource(std::move(password_affiliation_adapter));
  return store;
}

web::BrowserState* IOSChromeProfilePasswordStoreFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool IOSChromeProfilePasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
