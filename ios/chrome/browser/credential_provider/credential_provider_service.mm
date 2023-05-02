// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/credential_provider_service.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "build/build_config.h"
#import "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#import "components/password_manager/core/browser/affiliation/affiliation_service.h"
#import "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/browser/password_store_change.h"
#import "components/password_manager/core/browser/password_store_interface.h"
#import "components/password_manager/core/browser/password_store_util.h"
#import "components/password_manager/core/browser/password_sync_util.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/driver/sync_service.h"
#import "components/sync/driver/sync_user_settings.h"
#import "ios/chrome/browser/credential_provider/archivable_credential+password_form.h"
#import "ios/chrome/browser/credential_provider/credential_provider_util.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/as_password_credential_identity+credential.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential_store.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using password_manager::PasswordForm;
using password_manager::AffiliatedMatchHelper;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;
using password_manager::PasswordStoreInterface;
using password_manager::AffiliationService;

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

void SyncASIdentityStore(id<CredentialStore> credential_store) {
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
    PrefService* prefs,
    scoped_refptr<PasswordStoreInterface> profile_password_store,
    scoped_refptr<PasswordStoreInterface> account_password_store,
    id<MutableCredentialStore> credential_store,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    password_manager::AffiliationService* affiliation_service,
    FaviconLoader* favicon_loader)
    : prefs_(prefs),
      profile_password_store_(profile_password_store),
      account_password_store_(account_password_store),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      affiliated_helper_(
          std::make_unique<AffiliatedMatchHelper>(affiliation_service)),
      favicon_loader_(favicon_loader),
      dual_credential_store_(credential_store) {
  CHECK(profile_password_store_);
  CHECK(identity_manager_);
  CHECK(sync_service_);
  CHECK(favicon_loader_);
  CHECK(dual_credential_store_);

  profile_password_store_->AddObserver(this);
  if (account_password_store_) {
    account_password_store_->AddObserver(this);
  }

  UpdateAccountId();
  UpdateUserEmail();

  identity_manager_->AddObserver(this);
  sync_service_->AddObserver(this);

  // This class should usually handle incremental PasswordStore updates in
  // OnLoginsChanged(), but there could be bugs. E.g. maybe an update is fired
  // before the observer is added. So re-write the data on startup as a
  // safeguard. Post a task for performance.
  // Note: in reality this re-write does the same IO work as saving a new
  // password. The implementations of MutableCredentialStore write *every*
  // password to disk, even in OnLoginsChanged().
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CredentialProviderService::RequestSyncAllCredentials,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(5));

  saving_passwords_enabled_.Init(
      password_manager::prefs::kCredentialsEnableService, prefs,
      base::BindRepeating(
          &CredentialProviderService::OnSavingPasswordsEnabledChanged,
          base::Unretained(this)));

  // Make sure the initial value of the pref is stored.
  OnSavingPasswordsEnabledChanged();

  // TODO(crbug.com/1441012): Remove after 04/2024.
  NSArray<NSString*>* obsolete_keys = @[
    @"UserDefaultsCredentialProviderASIdentityStoreSyncCompleted.V1",
    @"UserDefaultsCredentialProviderFirstTimeSyncCompleted.V1"
  ];
  for (NSString* key in obsolete_keys) {
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:key];
  }
}

CredentialProviderService::~CredentialProviderService() {}

void CredentialProviderService::Shutdown() {
  profile_password_store_->RemoveObserver(this);
  if (account_password_store_) {
    account_password_store_->RemoveObserver(this);
  }
  identity_manager_->RemoveObserver(this);
  sync_service_->RemoveObserver(this);
}

void CredentialProviderService::RequestSyncAllCredentials() {
  profile_password_store_->GetAutofillableLogins(
      weak_ptr_factory_.GetWeakPtr());
  if (account_password_store_) {
    account_password_store_->GetAutofillableLogins(
        weak_ptr_factory_.GetWeakPtr());
  }
}

void CredentialProviderService::SyncAllCredentials(
    password_manager::PasswordStoreInterface* store,
    absl::variant<std::vector<std::unique_ptr<PasswordForm>>,
                  password_manager::PasswordStoreBackendError> forms_or_error) {
  std::vector<std::unique_ptr<PasswordForm>> forms =
      password_manager::GetLoginsOrEmptyListOnFailure(
          std::move(forms_or_error));
  AddCredentials(GetCredentialStore(store), std::move(forms));
  SyncStore();
}

void CredentialProviderService::SyncStore() {
  [dual_credential_store_ removeAllCredentials];
  for (id<Credential> credential in profile_credential_store_.credentials) {
    [dual_credential_store_ addCredential:credential];
  }
  for (id<Credential> credential in account_credential_store_.credentials) {
    [dual_credential_store_ addCredential:credential];
  }

  __weak id<CredentialStore> weak_credential_store = dual_credential_store_;
  [dual_credential_store_ saveDataWithCompletion:^(NSError* error) {
    if (error) {
      return;
    }
    if (weak_credential_store) {
      SyncASIdentityStore(weak_credential_store);
    }
  }];
}

void CredentialProviderService::AddCredentials(
    MemoryCredentialStore* store,
    std::vector<std::unique_ptr<PasswordForm>> forms) {
  // User is adding a password (not batch add from user login).
  const bool should_skip_max_verification = forms.size() == 1;
  const bool sync_enabled = sync_service_->IsSyncFeatureEnabled();

  for (const auto& form : forms) {
    NSString* favicon_key = GetFaviconFileKey(form->url);
    // Fetch the favicon and save it to the storage.
    // TODO(crbug.com/1441024): `sync_enabled` is not the correct check.
    FetchFaviconForURLToPath(favicon_loader_, form->url, favicon_key,
                             should_skip_max_verification, sync_enabled);

    ArchivableCredential* credential =
        [[ArchivableCredential alloc] initWithPasswordForm:*form
                                                   favicon:favicon_key
                                      validationIdentifier:account_id_];
    DCHECK(credential);
    [store addCredential:credential];
  }
}

void CredentialProviderService::RemoveCredentials(
    MemoryCredentialStore* store,
    std::vector<std::unique_ptr<PasswordForm>> forms) {
  for (const auto& form : forms) {
    NSString* recordID = RecordIdentifierForPasswordForm(*form);
    DCHECK(recordID);
    [store removeCredentialWithRecordIdentifier:recordID];
  }
}

void CredentialProviderService::UpdateAccountId() {
  CoreAccountInfo account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (!account.IsEmpty() &&
      identity_manager_->FindExtendedAccountInfo(account).IsManaged()) {
    account_id_ = base::SysUTF8ToNSString(account.gaia);
  } else {
    account_id_ = nil;
  }
  [app_group::GetGroupUserDefaults()
      setObject:account_id_
         forKey:AppGroupUserDefaultsCredentialProviderUserID()];
}

void CredentialProviderService::UpdateUserEmail() {
  absl::optional accountForSaving =
      password_manager::sync_util::GetAccountForSaving(prefs_, sync_service_);
  [app_group::GetGroupUserDefaults()
      setObject:accountForSaving ? base::SysUTF8ToNSString(*accountForSaving)
                                 : nil
         forKey:AppGroupUserDefaultsCredentialProviderUserEmail()];
}

void CredentialProviderService::OnGetPasswordStoreResultsFrom(
    password_manager::PasswordStoreInterface* store,
    std::vector<std::unique_ptr<PasswordForm>> results) {
  auto callback =
      base::BindOnce(&CredentialProviderService::SyncAllCredentials,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(store));
  affiliated_helper_->InjectAffiliationAndBrandingInformation(
      std::move(results), std::move(callback));
}

void CredentialProviderService::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // Not called because OnGetPasswordStoreResultsFrom() is overridden.
  NOTREACHED_NORETURN();
}

void CredentialProviderService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      UpdateAccountId();
      UpdateUserEmail();
      // The account id determines the validationIdentifier field in the
      // passwords, send them again.
      RequestSyncAllCredentials();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void CredentialProviderService::OnLoginsChanged(
    password_manager::PasswordStoreInterface* store,
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

  RemoveCredentials(GetCredentialStore(store), std::move(forms_to_remove));

  auto callback = base::BindOnce(
      &CredentialProviderService::OnInjectedAffiliationAfterLoginsChanged,
      weak_ptr_factory_.GetWeakPtr(), base::Unretained(store));

  affiliated_helper_->InjectAffiliationAndBrandingInformation(
      std::move(forms_to_add), std::move(callback));
}

void CredentialProviderService::OnLoginsRetained(
    password_manager::PasswordStoreInterface* /*store*/,
    const std::vector<password_manager::PasswordForm>& /*retained_passwords*/) {
}

void CredentialProviderService::OnInjectedAffiliationAfterLoginsChanged(
    password_manager::PasswordStoreInterface* store,
    absl::variant<std::vector<std::unique_ptr<PasswordForm>>,
                  password_manager::PasswordStoreBackendError> forms_or_error) {
  std::vector<std::unique_ptr<PasswordForm>> forms =
      password_manager::GetLoginsOrEmptyListOnFailure(
          std::move(forms_or_error));
  AddCredentials(GetCredentialStore(store), std::move(forms));
  SyncStore();
}

void CredentialProviderService::OnStateChanged(syncer::SyncService* sync) {
  // When the state changes, it's possible that password syncing has
  // started/stopped, so the user's email must be updated.
  UpdateUserEmail();
}

void CredentialProviderService::OnSavingPasswordsEnabledChanged() {
  [app_group::GetGroupUserDefaults()
      setObject:[NSNumber numberWithBool:saving_passwords_enabled_.GetValue()]
         forKey:AppGroupUserDefaulsCredentialProviderSavingPasswordsEnabled()];
}

MemoryCredentialStore* CredentialProviderService::GetCredentialStore(
    password_manager::PasswordStoreInterface* store) const {
  return store == profile_password_store_ ? profile_credential_store_
                                          : account_credential_store_;
}
