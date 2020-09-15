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
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/signin/public/identity_manager/identity_manager.h"
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

using autofill::PasswordForm;
using password_manager::PasswordStore;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;

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

ArchivableCredential* CredentialFromForm(const PasswordForm& form,
                                         NSString* validation_id) {
  ArchivableCredential* credential =
      [[ArchivableCredential alloc] initWithPasswordForm:form
                                                 favicon:nil
                                    validationIdentifier:validation_id];
  if (!credential) {
    // Verify that the credential is nil because it's an Android one or
    // blacklisted.
    DCHECK(password_manager::IsValidAndroidFacetURI(form.signon_realm) ||
           form.blocked_by_user);
  }
  return credential;
}

}  // namespace

CredentialProviderService::CredentialProviderService(
    scoped_refptr<PasswordStore> password_store,
    AuthenticationService* authentication_service,
    ArchivableCredentialStore* credential_store,
    signin::IdentityManager* identity_manager)
    : password_store_(password_store),
      authentication_service_(authentication_service),
      identity_manager_(identity_manager),
      archivable_credential_store_(credential_store) {
  DCHECK(password_store_);
  password_store_->AddObserver(this);

  DCHECK(authentication_service_);
  UpdateAccountValidationId();

  if (identity_manager_) {
    identity_manager_->AddObserver(this);
  }

  // TODO(crbug.com/1066803): Wait for things to settle down before
  // syncs, and sync credentials after Sync finishes or some
  // seconds in the future.
  if (ShouldSyncASIdentityStore()) {
    SyncASIdentityStore(credential_store);
  }
  if (ShouldSyncAllCredentials()) {
    RequestSyncAllCredentials();
  }
}

CredentialProviderService::~CredentialProviderService() {}

void CredentialProviderService::Shutdown() {
  password_store_->RemoveObserver(this);
  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
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

void CredentialProviderService::RequestSyncAllCredentials() {
  UpdateAccountValidationId();
  password_store_->GetAutofillableLogins(this);
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

void CredentialProviderService::SyncStore(void (^completion)(NSError*)) const {
  [archivable_credential_store_ saveDataWithCompletion:^(NSError* error) {
    DCHECK(!error) << "An error occurred while saving to disk";
    if (completion) {
      completion(error);
    }
  }];
}

void CredentialProviderService::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  [archivable_credential_store_ removeAllCredentials];
  for (const auto& form : results) {
    ArchivableCredential* credential =
        CredentialFromForm(*form, account_validation_id_);
    if (credential) {
      [archivable_credential_store_ addCredential:credential];
    }
  }
  SyncStore(^(NSError* error) {
    if (!error) {
      NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
      NSString* key = kUserDefaultsCredentialProviderFirstTimeSyncCompleted;
      [user_defaults setBool:YES forKey:key];
      SyncASIdentityStore(archivable_credential_store_);
    }
  });
}

void CredentialProviderService::OnLoginsChanged(
    const PasswordStoreChangeList& changes) {
  for (const PasswordStoreChange& change : changes) {
    ArchivableCredential* credential =
        CredentialFromForm(change.form(), account_validation_id_);
    if (change.form().blocked_by_user) {
      continue;
    }
    switch (change.type()) {
      case PasswordStoreChange::ADD:
        [archivable_credential_store_ addCredential:credential];
        break;
      case PasswordStoreChange::UPDATE:
        [archivable_credential_store_ updateCredential:credential];
        break;
      case PasswordStoreChange::REMOVE:
        // Using the record identifier from the form, as the credential might
        // not be valid anymore.
        [archivable_credential_store_
            removeCredentialWithRecordIdentifier:
                RecordIdentifierForPasswordForm(change.form())];
        break;

      default:
        NOTREACHED();
        break;
    }
  }
  SyncStore(^(NSError* error) {
    if (!error) {
      // TODO(crbug.com/1077747): Support ASCredentialIdentityStore incremental
      // updates. Currently calling multiple methods on it to save and remove
      // causes it to crash. This needs to be further investigated.
      SyncASIdentityStore(archivable_credential_store_);
    }
  });
}
