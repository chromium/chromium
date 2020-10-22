// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/credential_provider/credential_provider_service.h"

#import <AuthenticationServices/AuthenticationServices.h>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/scoped_observer.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/android_affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/android_affiliation/android_affiliation_service.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/credential_provider/archivable_credential+password_form.h"
#import "ios/chrome/browser/credential_provider/credential_provider_util.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential_store.h"
#import "ios/chrome/common/credential_provider/as_password_credential_identity+credential.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using password_manager::PasswordForm;
using password_manager::AffiliatedMatchHelper;
using password_manager::PasswordStore;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;
using password_manager::AndroidAffiliationService;

// ASCredentialIdentityStoreError enum to report UMA metrics. Must be in sync
// with iOSCredentialIdentityStoreErrorForReporting in
// tools/metrics/histograms/enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CredentialIdentityStoreErrorForReporting {
  kUnknownError,
  kInternal,
  kDisabled,
  kBusy,
  kMaxValue = kBusy
};

// Converts a UIKit interface style to an interface style for reporting.
CredentialIdentityStoreErrorForReporting
ErrorForReportingForASCredentialIdentityStoreErrorCode(
    ASCredentialIdentityStoreErrorCode errorCode) {
  switch (errorCode) {
    case ASCredentialIdentityStoreErrorCodeInternalError:
      return CredentialIdentityStoreErrorForReporting::kInternal;
    case ASCredentialIdentityStoreErrorCodeStoreDisabled:
      return CredentialIdentityStoreErrorForReporting::kDisabled;
    case ASCredentialIdentityStoreErrorCodeStoreBusy:
      return CredentialIdentityStoreErrorForReporting::kBusy;
  }
  return CredentialIdentityStoreErrorForReporting::kUnknownError;
}

BOOL ShouldSyncAllCredentials() {
  NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
  DCHECK(user_defaults);
  return ![user_defaults
      boolForKey:kUserDefaultsCredentialProviderFirstTimeSyncCompleted];
}

BOOL ShouldSyncASIdentityStore() {
  NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
  DCHECK(user_defaults);
  BOOL isIdentityStoreSynced = [user_defaults
      boolForKey:kUserDefaultsCredentialProviderASIdentityStoreSyncCompleted];
  BOOL areCredentialsSynced = [user_defaults
      boolForKey:kUserDefaultsCredentialProviderFirstTimeSyncCompleted];
  return !isIdentityStoreSynced && areCredentialsSynced;
}

void SyncASIdentityStore(ArchivableCredentialStore* credential_store) {
  auto stateCompletion = ^(ASCredentialIdentityStoreState* state) {
#if !defined(NDEBUG)
    dispatch_assert_queue_not(dispatch_get_main_queue());
#endif  // !defined(NDEBUG)
    if (state.enabled) {
      NSArray<id<Credential>>* credentials = credential_store.credentials;
      NSMutableArray<ASPasswordCredentialIdentity*>* storeIdentities =
          [NSMutableArray arrayWithCapacity:credentials.count];
      for (id<Credential> credential in credentials) {
        [storeIdentities addObject:[[ASPasswordCredentialIdentity alloc]
                                       initWithCredential:credential]];
      }
      auto replaceCompletion = ^(BOOL success, NSError* error) {
        // Sometimes ASCredentialIdentityStore fails. Log this to measure the
        // impact of these failures and move on.
        if (!success) {
          ASCredentialIdentityStoreErrorCode code =
              static_cast<ASCredentialIdentityStoreErrorCode>(error.code);
          CredentialIdentityStoreErrorForReporting errorForReporting =
              ErrorForReportingForASCredentialIdentityStoreErrorCode(code);
          base::UmaHistogramEnumeration(
              "IOS.CredentialExtension.Service.Error."
              "ReplaceCredentialIdentitiesWithIdentities",
              errorForReporting);
        }
        NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
        NSString* key =
            kUserDefaultsCredentialProviderASIdentityStoreSyncCompleted;
        [user_defaults setBool:success forKey:key];
      };
      [ASCredentialIdentityStore.sharedStore
          replaceCredentialIdentitiesWithIdentities:storeIdentities
                                         completion:replaceCompletion];
    }
  };
  [ASCredentialIdentityStore.sharedStore
      getCredentialIdentityStoreStateWithCompletion:stateCompletion];
}

}  // namespace

CredentialProviderService::CredentialProviderService(
    scoped_refptr<PasswordStore> password_store,
    AuthenticationService* authentication_service,
    ArchivableCredentialStore* credential_store,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service)
    : password_store_(password_store),
      authentication_service_(authentication_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      archivable_credential_store_(credential_store) {
  DCHECK(password_store_);
  password_store_->AddObserver(this);

  DCHECK(authentication_service_);
  UpdateAccountValidationId();

  if (identity_manager_) {
    identity_manager_->AddObserver(this);
  }

  bool is_sync_active = false;
  if (sync_service_) {
    sync_service_->AddObserver(this);
    is_sync_active = sync_service_->IsSyncFeatureActive();
  }

  // If Sync is active, wait for the configuration to finish before syncing.
  // This will wait for affiliated_match_helper to be available.
  if (!is_sync_active) {
    RequestSyncAllCredentialsIfNeeded();
  }
}

CredentialProviderService::~CredentialProviderService() {}

void CredentialProviderService::Shutdown() {
  password_store_->RemoveObserver(this);
  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
  }
  if (sync_service_) {
    sync_service_->RemoveObserver(this);
  }
}

void CredentialProviderService::RequestSyncAllCredentials() {
  UpdateAccountValidationId();
  password_store_->GetAutofillableLogins(this);
}

void CredentialProviderService::RequestSyncAllCredentialsIfNeeded() {
  if (ShouldSyncASIdentityStore()) {
    SyncASIdentityStore(archivable_credential_store_);
  }
  if (ShouldSyncAllCredentials()) {
    RequestSyncAllCredentials();
  }
}

void CredentialProviderService::SyncAllCredentials(
    std::vector<std::unique_ptr<PasswordForm>> forms) {
  [archivable_credential_store_ removeAllCredentials];
  AddCredentials(std::move(forms));
  SyncStore(true);
}

void CredentialProviderService::SyncStore(bool set_first_time_sync_flag) {
  __weak ArchivableCredentialStore* weak_archivable_credential_store =
      archivable_credential_store_;
  [archivable_credential_store_ saveDataWithCompletion:^(NSError* error) {
    if (error) {
      return;
    }
    if (set_first_time_sync_flag) {
      NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
      for (NSString* key in UnusedUserDefaultsCredentialProviderKeys()) {
        [user_defaults removeObjectForKey:key];
      }
      NSString* key = kUserDefaultsCredentialProviderFirstTimeSyncCompleted;
      [user_defaults setBool:YES forKey:key];
    }
    if (weak_archivable_credential_store) {
      SyncASIdentityStore(weak_archivable_credential_store);
    }
  }];
}

void CredentialProviderService::AddCredentials(
    std::vector<std::unique_ptr<PasswordForm>> forms) {
  for (const auto& form : forms) {
    ArchivableCredential* credential = [[ArchivableCredential alloc]
        initWithPasswordForm:*form
                     favicon:nil
        validationIdentifier:account_validation_id_];
    DCHECK(credential);
    [archivable_credential_store_ addCredential:credential];
  }
}

void CredentialProviderService::RemoveCredentials(
    std::vector<std::unique_ptr<PasswordForm>> forms) {
  for (const auto& form : forms) {
    NSString* recordID = RecordIdentifierForPasswordForm(*form);
    DCHECK(recordID);
    [archivable_credential_store_
        removeCredentialWithRecordIdentifier:recordID];
  }
}

void CredentialProviderService::UpdateAccountValidationId() {
  if (authentication_service_->IsAuthenticatedIdentityManaged()) {
    account_validation_id_ =
        authentication_service_->GetAuthenticatedIdentity().gaiaID;
  } else {
    account_validation_id_ = nil;
  }
  [app_group::GetGroupUserDefaults()
      setObject:account_validation_id_
         forKey:AppGroupUserDefaultsCredentialProviderManagedUserID()];
}

void CredentialProviderService::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  auto callback = base::BindOnce(&CredentialProviderService::SyncAllCredentials,
                                 weak_factory_.GetWeakPtr());
  AffiliatedMatchHelper* matcher = password_store_->affiliated_match_helper();
  if (matcher) {
    matcher->InjectAffiliationAndBrandingInformation(
        std::move(results),
        AndroidAffiliationService::StrategyOnCacheMiss::FETCH_OVER_NETWORK,
        std::move(callback));
  } else {
    std::move(callback).Run(std::move(results));
  }
}

void CredentialProviderService::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  RequestSyncAllCredentials();
}

void CredentialProviderService::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  RequestSyncAllCredentials();
}

void CredentialProviderService::OnLoginsChanged(
    const PasswordStoreChangeList& changes) {
  std::vector<std::unique_ptr<PasswordForm>> forms_to_add;
  std::vector<std::unique_ptr<PasswordForm>> forms_to_remove;
  for (const PasswordStoreChange& change : changes) {
    if (change.form().blocked_by_user) {
      continue;
    }
    switch (change.type()) {
      case PasswordStoreChange::ADD:
        forms_to_add.push_back(std::make_unique<PasswordForm>(change.form()));
        break;
      case PasswordStoreChange::UPDATE:
        forms_to_remove.push_back(
            std::make_unique<PasswordForm>(change.form()));
        forms_to_add.push_back(std::make_unique<PasswordForm>(change.form()));
        break;
      case PasswordStoreChange::REMOVE:
        forms_to_remove.push_back(
            std::make_unique<PasswordForm>(change.form()));
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  RemoveCredentials(std::move(forms_to_remove));

  auto callback = base::BindOnce(
      &CredentialProviderService::OnInjectedAffiliationAfterLoginsChanged,
      weak_factory_.GetWeakPtr());

  AffiliatedMatchHelper* matcher = password_store_->affiliated_match_helper();
  if (matcher) {
    matcher->InjectAffiliationAndBrandingInformation(
        std::move(forms_to_add),
        AndroidAffiliationService::StrategyOnCacheMiss::FETCH_OVER_NETWORK,
        std::move(callback));
  } else {
    std::move(callback).Run(std::move(forms_to_add));
  }
}

void CredentialProviderService::OnInjectedAffiliationAfterLoginsChanged(
    std::vector<std::unique_ptr<PasswordForm>> forms) {
  AddCredentials(std::move(forms));
  SyncStore(false);
}

void CredentialProviderService::OnSyncConfigurationCompleted(
    syncer::SyncService* sync) {
  RequestSyncAllCredentialsIfNeeded();
}
