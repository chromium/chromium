// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/credential_provider_service.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/check.h"
#import "base/check_is_test.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "build/build_config.h"
#import "components/affiliations/core/browser/affiliation_service.h"
#import "components/affiliations/core/browser/affiliation_utils.h"
#import "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/browser/password_store/password_form_converters.h"
#import "components/password_manager/core/browser/password_store/password_store_change.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/browser/password_store/password_store_util.h"
#import "components/password_manager/core/browser/password_sync_util.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/credential_provider/model/archivable_credential+password_form.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_util.h"
#import "ios/chrome/browser/credential_provider/model/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/ASPasskeyCredentialIdentity+credential.h"
#import "ios/chrome/common/credential_provider/ASPasswordCredentialIdentity+credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential+passkey.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential_store.h"
#import "ios/chrome/common/credential_provider/credential_store_util.h"
#import "ios/components/credential_provider_extension/password_util.h"

namespace {

using affiliations::AffiliationService;
using password_manager::AffiliatedMatchHelper;
using password_manager::PasswordForm;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;
using password_manager::PasswordStoreInterface;

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

// We can't sync store when the app is backgrounded, as it loses access to files
// and stores provided by iOS.
bool CanSyncStore() {
  return UIApplication.sharedApplication.applicationState !=
         UIApplicationStateBackground;
}

// Writes ASCredentialIdentity objects corresponding to `credentials` into the
// ASCredentialIdentityStore used for OS-initated credential lookups.
void SyncASIdentityStore(NSArray<id<Credential>>* credentials) {
  auto stateCompletion = ^(ASCredentialIdentityStoreState* state) {
    if (!state.enabled || !CanSyncStore()) {
      return;
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
    NSMutableArray<id<ASCredentialIdentity>>* storeIdentities =
        [NSMutableArray arrayWithCapacity:credentials.count];
    for (id<Credential> credential in credentials) {
      if (credential.isPasskey) {
        // Hidden passkeys shouldn't be surfaced in the sign-in suggestions.
        if (base::FeatureList::IsEnabled(kCredentialProviderSignalAPI) &&
            credential.hidden) {
          continue;
        }
        [storeIdentities addObject:[[ASPasskeyCredentialIdentity alloc]
                                       cr_initWithCredential:credential]];
      } else {
        [storeIdentities addObject:[[ASPasswordCredentialIdentity alloc]
                                       cr_initWithCredential:credential]];
      }
    }
    [ASCredentialIdentityStore.sharedStore
        replaceCredentialIdentityEntries:storeIdentities
                              completion:replaceCompletion];
  };
  [ASCredentialIdentityStore.sharedStore
      getCredentialIdentityStoreStateWithCompletion:stateCompletion];
}

bool CanSendHistoryData(syncer::SyncService* sync_service) {
  // SESSIONS and HISTORY both contain history-like data, so it's sufficient if
  // either of them is being uploaded.
  return syncer::GetUploadToGoogleState(sync_service,
                                        syncer::DataType::SESSIONS) ==
             syncer::UploadState::ACTIVE ||
         syncer::GetUploadToGoogleState(sync_service,
                                        syncer::DataType::HISTORY) ==
             syncer::UploadState::ACTIVE;
}

void RecordNumberFaviconsFetched(size_t fetched_favicon_count) {
  base::UmaHistogramCounts10000("IOS.CredentialExtension.NumberFaviconsFetched",
                                fetched_favicon_count);
}

}  // namespace

CredentialProviderService::CredentialProviderService(
    const std::string& profile_name,
    PrefService* prefs,
    PrefService* local_state,
    scoped_refptr<PasswordStoreInterface> profile_password_store,
    scoped_refptr<PasswordStoreInterface> account_password_store,
    webauthn::PasskeyModel* passkey_model,
    id<MutableCredentialStore> credential_store,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    affiliations::AffiliationService* affiliation_service,
    FaviconLoader* favicon_loader)
    : profile_name_(profile_name),
      local_state_(local_state),
      profile_password_store_(profile_password_store),
      account_password_store_(account_password_store),
      passkey_model_(passkey_model),
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

  // Favicon folder availability check involves disk I/O or IPC to get the app
  // group container URL. Move to background task to avoid blocking the main
  // thread.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce([]() {
        base::UmaHistogramBoolean(
            "IOS.CredentialExtension.FaviconFolderAvailable",
            IsFaviconFolderAvailable());
      }));

  profile_password_store_->AddObserver(this);
  if (account_password_store_) {
    account_password_store_->AddObserver(this);
  }
  if (passkey_model_) {
    passkey_model_->AddObserver(this);
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
          &CredentialProviderService::OnPrefOrPolicyStatusChanged,
          base::Unretained(this)));

  saving_passkeys_enabled_.Init(
      password_manager::prefs::kCredentialsEnablePasskeys, prefs,
      base::BindRepeating(
          &CredentialProviderService::OnPrefOrPolicyStatusChanged,
          base::Unretained(this)));

  automatic_passkey_upgrades_enabled_.Init(
      password_manager::prefs::kAutomaticPasskeyUpgrades, prefs,
      base::BindRepeating(
          &CredentialProviderService::OnPrefOrPolicyStatusChanged,
          base::Unretained(this)));

  // Make sure the initial value of the pref is stored.
  OnPrefOrPolicyStatusChanged();
  UpdatePasswordSyncSetting();
  UpdateAutomaticPasskeyUpgradeSetting();
  UpdatePasskeyLargeBlobSetting();
  UpdateSignalAPISetting();
}

CredentialProviderService::~CredentialProviderService() {}

void CredentialProviderService::Shutdown() {
  profile_password_store_->RemoveObserver(this);
  if (account_password_store_) {
    account_password_store_->RemoveObserver(this);
  }
  if (passkey_model_) {
    passkey_model_->RemoveObserver(this);
  }
  identity_manager_->RemoveObserver(this);
  if (sync_service_) {
    sync_service_->RemoveObserver(this);
    sync_service_ = nullptr;
  }
}

void CredentialProviderService::OnLoginsChanged(
    password_manager::PasswordStoreInterface* store,
    const PasswordStoreChangeList& changes) {
  std::vector<password_manager::StoredCredential> forms_to_add, forms_to_remove;
  for (const PasswordStoreChange& change : changes) {
    if (change.credential().blocked_by_user) {
      continue;
    }
    switch (change.type()) {
      case PasswordStoreChange::ADD:
        forms_to_add.push_back(
            password_manager::CloneStoredCredential(change.credential()));
        break;
      case PasswordStoreChange::UPDATE:
        // Using a password triggers this code path, since it updates
        // the use count and use date. Ideally we shouldn't care about this, but
        // for now the whole password file is re-written on every change, which
        // is inefficient. Username changes are not considered updates, but
        // instead treated as a new credential (REMOVE then ADD).
        forms_to_remove.push_back(
            password_manager::CloneStoredCredential(change.credential()));
        forms_to_add.push_back(
            password_manager::CloneStoredCredential(change.credential()));
        break;
      case PasswordStoreChange::REMOVE:
        forms_to_remove.push_back(
            password_manager::CloneStoredCredential(change.credential()));
        break;
      default:
        NOTREACHED();
    }
  }

  RemoveCredentials(GetCredentialStore(store), std::move(forms_to_remove));

  auto callback = base::BindOnce(
      &CredentialProviderService::OnInjectedAffiliationAfterLoginsChanged,
      weak_ptr_factory_.GetWeakPtr(), base::Unretained(store));

  affiliated_helper_->InjectAffiliationAndBrandingInformation(
      std::move(forms_to_add), std::move(callback));
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
    password_manager::LoginsResultOrError forms_or_error) {
  std::vector<password_manager::StoredCredential> forms =
      password_manager::GetLoginsOrEmptyListOnFailure(
          std::move(forms_or_error));

  MemoryCredentialStore* memory_credential_store = GetCredentialStore(store);

  auto completion = base::BindOnce(
      &CredentialProviderService::CompleteSyncAllCredentials,
      weak_ptr_factory_.GetWeakPtr(), base::Unretained(memory_credential_store),
      base::Unretained(store));

  AddCredentials(memory_credential_store, std::move(forms),
                 std::move(completion));
}

void CredentialProviderService::CompleteSyncAllCredentials(
    MemoryCredentialStore* memory_credential_store,
    password_manager::PasswordStoreInterface* store) {
  // We only sync passkeys into the account store.
  if (passkey_model_ && (store == account_password_store_)) {
    AddCredentials(memory_credential_store,
                   passkey_model_->GetPasskeys(
                       webauthn::PasskeyModel::AnyRp(),
                       webauthn::PasskeyModel::ShadowedCredentials::kExclude));
  }
  SyncStore();
}

void CredentialProviderService::SyncStore() {
  if (!IsLastUsedProfile() || !CanSyncStore()) {
    return;
  }

  base::UmaHistogramBoolean(kSyncStoreHistogramName, true);

  // Create a callback to process the read credentials, matching the signature
  // required by `ReadFromMultipleCredentialStoresAsync`.
  //
  // Use `BindPostTask` along with the current thread's task runner to ensure
  // that we run on the current thread. This class is not thread-safe, and
  // `ReadFromMultipleCredentialStoresAsync` does not guarantee that the
  // completion will run on the same thread.
  //
  // By first binding CompleteSync with a WeakPtr to `this`, we ensure that the
  // completion is a no-op if `this` goes out of scope during the read. This
  // style of binding also requires that the callback be run on the same thread
  // where the WeakPtr was created (i.e., the current thread).
  auto update_store_on_current_thread = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&CredentialProviderService::CompleteSync,
                     weak_ptr_factory_.GetWeakPtr()));

  credential_store_util::ReadFromMultipleCredentialStoresAsync(
      @[ account_credential_store_, profile_credential_store_ ],
      std::move(update_store_on_current_thread));
}

void CredentialProviderService::CompleteSync(
    NSArray<id<Credential>>* credentials) {
  if (!CanSyncStore()) {
    return;
  }

  [dual_credential_store_ removeAllCredentials];
  for (id<Credential> credential in credentials) {
    [dual_credential_store_ addCredential:credential];
  }
  [dual_credential_store_ saveDataWithCompletion:nil];
  SyncASIdentityStore(credentials);
}

void CredentialProviderService::AddCredentials(
    MemoryCredentialStore* store,
    std::vector<password_manager::StoredCredential> forms,
    base::OnceClosure completion) {
  if (base::FeatureList::IsEnabled(
          kCredentialProviderRefactoredAddCredentials)) {
    AddCredentialsRefactored(store, std::move(forms), std::move(completion));
  } else {
    AddCredentialsLegacy(store, std::move(forms));
    std::move(completion).Run();
  }
}

NSString* CredentialProviderService::PrimaryAccountId() const {
  CoreAccountInfo account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  return account.gaia.ToNSString();
}

void CredentialProviderService::AddCredentialsLegacy(
    MemoryCredentialStore* store,
    std::vector<password_manager::StoredCredential> forms) {
  // User is adding a password (not batch add from user login).
  const bool should_skip_max_verification = forms.size() == 1;
  const bool fallback_to_google_server_allowed =
      CanSendHistoryData(sync_service_);
  NSString* gaia = PrimaryAccountId();

  int fetched_favicon_count = 0;

  for (const auto& form : forms) {
    NSString* favicon_key;
    // Only fetch favicon for valid URL. FaviconLoader::FaviconForPageUrl does
    // not take Android facet URI.
    if (form.url.is_valid()) {
      ++fetched_favicon_count;
      favicon_key = GetFaviconFileKey(form.url);

      // Fetch the favicon and save it to the storage.
      FetchFaviconForURLToPath(favicon_loader_, form.url, favicon_key,
                               should_skip_max_verification,
                               fallback_to_google_server_allowed);
    }

    // Only store password with valid Android facet URI or valid URL.
    if (affiliations::IsValidAndroidFacetURI(form.signon_realm) ||
        form.url.is_valid()) {
      ArchivableCredential* credential = [[ArchivableCredential alloc]
          initWithPasswordForm:password_manager::ToPasswordForm(form)
                       favicon:favicon_key
                          gaia:gaia];
      DCHECK(credential);
      [store addCredential:credential];
    }
  }

  RecordNumberFaviconsFetched(fetched_favicon_count);
}

void CredentialProviderService::AddCredentialsRefactored(
    MemoryCredentialStore* store,
    std::vector<password_manager::StoredCredential> forms,
    base::OnceClosure completion) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::ThreadPolicy::PREFER_BACKGROUND,
       base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetFaviconsListAndFreshness),
      base::BindOnce(
          &CredentialProviderService::ContinueAddCredentialsRefactored,
          weak_ptr_factory_.GetWeakPtr(), base::Unretained(store),
          std::move(forms), std::move(completion)));
}

void CredentialProviderService::ContinueAddCredentialsRefactored(
    MemoryCredentialStore* store,
    std::vector<password_manager::StoredCredential> forms,
    base::OnceClosure completion,
    NSDictionary<NSString*, NSDate*>* favicon_dict) {
  // Don't rate limit the favicon fetch when adding a single password.
  const bool should_skip_max_verification = forms.size() == 1;
  const bool fallback_to_google_server_allowed =
      CanSendHistoryData(sync_service_);
  NSString* gaia = PrimaryAccountId();

  int fetched_favicon_count = 0;
  NSMutableSet<NSString*>* fetched_in_batch = [NSMutableSet set];

  for (const auto& form : forms) {
    NSString* favicon_key;
    if (form.url.is_valid()) {
      favicon_key = GetFaviconFileKey(form.url);

      if (ShouldFetchFavicon(favicon_key, favicon_dict) &&
          ![fetched_in_batch containsObject:favicon_key]) {
        ++fetched_favicon_count;
        [fetched_in_batch addObject:favicon_key];

        // Fetch the favicon and save it to the storage.
        FetchFaviconForURLToPath(favicon_loader_, form.url, favicon_key,
                                 should_skip_max_verification,
                                 fallback_to_google_server_allowed);
      }
    }

    // Only store password with valid Android facet URI or valid URL.
    if (affiliations::IsValidAndroidFacetURI(form.signon_realm) ||
        form.url.is_valid()) {
      ArchivableCredential* credential = [[ArchivableCredential alloc]
          initWithPasswordForm:password_manager::ToPasswordForm(form)
                       favicon:favicon_key
                          gaia:gaia];
      DCHECK(credential);
      [store addCredential:credential];
    }
  }

  RecordNumberFaviconsFetched(fetched_favicon_count);
  std::move(completion).Run();
}

void CredentialProviderService::AddCredentials(
    MemoryCredentialStore* store,
    std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys) {
  // User is adding a passkey (not batch add from user login).
  const bool should_skip_max_verification = passkeys.size() == 1;
  const bool fallback_to_google_server = CanSendHistoryData(sync_service_);
  NSString* gaia = PrimaryAccountId();

  for (const sync_pb::WebauthnCredentialSpecifics& passkey : passkeys) {
    // With the feature enabled, hidden passkeys are only filtered out before
    // being added to ASCredentialIdentityStore, they should still be added to
    // `store`.
    if (!base::FeatureList::IsEnabled(kCredentialProviderSignalAPI) &&
        passkey.hidden()) {
      continue;
    }

    GURL url(base::StrCat(
        {url::kHttpsScheme, url::kStandardSchemeSeparator, passkey.rp_id()}));
    // Only fetch favicon for valid URL.
    NSString* favicon_key;
    if (url.is_valid()) {
      favicon_key = GetFaviconFileKey(url);

      // Fetch the favicon and save it to the storage.
      FetchFaviconForURLToPath(favicon_loader_, url, favicon_key,
                               should_skip_max_verification,
                               fallback_to_google_server);
    }

    ArchivableCredential* credential =
        [[ArchivableCredential alloc] initWithFavicon:favicon_key
                                                 gaia:gaia
                                              passkey:passkey];
    DCHECK(credential);
    [store addCredential:credential];
  }
}

void CredentialProviderService::RemoveCredentials(
    MemoryCredentialStore* store,
    std::vector<password_manager::StoredCredential> forms) {
  for (const auto& form : forms) {
    NSString* recordID =
        RecordIdentifierForPasswordForm(password_manager::ToPasswordForm(form));
    DCHECK(recordID);
    [store removeCredentialWithRecordIdentifier:recordID];
  }
}

void CredentialProviderService::RemoveCredentials(
    MemoryCredentialStore* store,
    std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys) {
  for (const auto& passkey : passkeys) {
    NSString* recordID = RecordIdentifierForPasskey(passkey);
    DCHECK(recordID);
    [store removeCredentialWithRecordIdentifier:recordID];
  }
}

bool CredentialProviderService::IsLastUsedProfile() const {
  return profile_name_ == local_state_->GetString(prefs::kLastUsedProfile);
}

void CredentialProviderService::UpdateAccountId() {
  if (!IsLastUsedProfile()) {
    return;
  }

  CoreAccountInfo account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  NSString* account_id = account.gaia.ToNSString();
  BOOL is_valid_account = !account.IsEmpty();
  BOOL is_managed_account =
      is_valid_account &&
      identity_manager_->FindExtendedAccountInfo(account).IsManaged() ==
          signin::Tribool::kTrue;
  [app_group::GetGroupUserDefaults()
      setObject:is_managed_account ? account_id : nil
         forKey:AppGroupUserDefaultsCredentialProviderManagedUserID()];

  [app_group::GetGroupUserDefaults()
      setObject:is_valid_account ? account_id : nil
         forKey:AppGroupUserDefaultsCredentialProviderUserID()];
}

void CredentialProviderService::UpdateUserEmail() {
  if (!IsLastUsedProfile()) {
    return;
  }

  std::optional accountForSaving =
      password_manager::sync_util::GetAccountForSaving(sync_service_);
  [app_group::GetGroupUserDefaults()
      setObject:accountForSaving ? base::SysUTF8ToNSString(*accountForSaving)
                                 : nil
         forKey:AppGroupUserDefaultsCredentialProviderUserEmail()];
}

void CredentialProviderService::UpdatePasswordSyncSetting() {
  if (!IsLastUsedProfile()) {
    return;
  }

  BOOL is_syncing =
      password_manager::sync_util::HasChosenToSyncPasswords(sync_service_);
  [app_group::GetGroupUserDefaults()
      setObject:[NSNumber numberWithBool:is_syncing]
         forKey:AppGroupUserDefaultsCredentialProviderPasswordSyncSetting()];
}

void CredentialProviderService::UpdateAutomaticPasskeyUpgradeSetting() {
  if (!IsLastUsedProfile()) {
    return;
  }

  BOOL is_enabled = saving_passwords_enabled_.GetValue() &&
                    saving_passkeys_enabled_.GetValue() &&
                    automatic_passkey_upgrades_enabled_.GetValue();
  [app_group::GetGroupUserDefaults()
      setObject:[NSNumber numberWithBool:is_enabled]
         forKey:
             AppGroupUserDefaulsCredentialProviderAutomaticPasskeyUpgradeEnabled()];
}


void CredentialProviderService::UpdatePasskeyLargeBlobSetting() {
  if (!IsLastUsedProfile()) {
    return;
  }

  BOOL is_enabled =
      base::FeatureList::IsEnabled(kCredentialProviderPasskeyLargeBlob);
  [app_group::GetGroupUserDefaults()
      setObject:[NSNumber numberWithBool:is_enabled]
         forKey:AppGroupUserDefaulsCredentialProviderPasskeyLargeBlobEnabled()];
}

void CredentialProviderService::UpdateSignalAPISetting() {
  if (!IsLastUsedProfile()) {
    return;
  }

  BOOL is_enabled = base::FeatureList::IsEnabled(kCredentialProviderSignalAPI);
  [app_group::GetGroupUserDefaults()
      setObject:[NSNumber numberWithBool:is_enabled]
         forKey:AppGroupUserDefaulsCredentialProviderSignalAPIEnabled()];
}

void CredentialProviderService::OnGetPasswordStoreResultsOrErrorFrom(
    password_manager::PasswordStoreInterface* store,
    password_manager::LoginsResultOrError results) {
  auto callback =
      base::BindOnce(&CredentialProviderService::SyncAllCredentials,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(store));
  affiliated_helper_->InjectAffiliationAndBrandingInformation(
      password_manager::GetLoginsOrEmptyListOnFailure(std::move(results)),
      std::move(callback));
}

void CredentialProviderService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      UpdateAccountId();
      UpdateUserEmail();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void CredentialProviderService::OnLoginsRetained(
    password_manager::PasswordStoreInterface* /*store*/,
    const std::vector<
        password_manager::StoredCredential>& /*retained_credentials*/) {}

void CredentialProviderService::OnInjectedAffiliationAfterLoginsChanged(
    password_manager::PasswordStoreInterface* store,
    password_manager::LoginsResultOrError results_or_error) {
  AddCredentials(GetCredentialStore(store),
                 password_manager::GetLoginsOrEmptyListOnFailure(
                     std::move(results_or_error)),
                 base::BindOnce(&CredentialProviderService::SyncStore,
                                weak_ptr_factory_.GetWeakPtr()));
}

void CredentialProviderService::OnStateChanged(syncer::SyncService* sync) {
  // When the state changes, it's possible that password syncing has
  // started/stopped, so the user's email must be updated.
  UpdateAccountId();
  UpdateUserEmail();
  UpdatePasswordSyncSetting();
}

void CredentialProviderService::OnSyncShutdown(syncer::SyncService* sync) {
  // Unreachable, since this service is Shutdown() before the SyncService.
  NOTREACHED();
}

// PasskeyModel::Observer:
void CredentialProviderService::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {
  // Passkeys get saved only into the account store.
  if (!account_password_store_) {
    return;
  }

  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys_to_add;
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys_to_remove;
  for (const webauthn::PasskeyModelChange& change : changes) {
    const sync_pb::WebauthnCredentialSpecifics& passkey = change.passkey();
    switch (change.type()) {
      case webauthn::PasskeyModelChange::ChangeType::ADD:
        passkeys_to_add.push_back(passkey);
        break;
      case webauthn::PasskeyModelChange::ChangeType::REMOVE:
        passkeys_to_remove.push_back(passkey);
        break;
      case webauthn::PasskeyModelChange::ChangeType::UPDATE:
        // TODO(crbug.com/458784354): do something more optimal than this.
        passkeys_to_add.push_back(passkey);
        passkeys_to_remove.push_back(passkey);
        break;
      default:
        NOTREACHED();
    }
  }

  if (passkeys_to_add.empty() && passkeys_to_remove.empty()) {
    return;
  }

  if (!passkeys_to_remove.empty()) {
    RemoveCredentials(account_credential_store_, passkeys_to_remove);
  }

  if (!passkeys_to_add.empty()) {
    AddCredentials(account_credential_store_, passkeys_to_add);
  }

  SyncStore();
}

void CredentialProviderService::OnPasskeyModelShuttingDown() {
  if (passkey_model_) {
    passkey_model_->RemoveObserver(this);
  }
  passkey_model_ = nullptr;
}

void CredentialProviderService::OnPasskeyModelIsReady(bool is_ready) {}

void CredentialProviderService::OnPrefOrPolicyStatusChanged() {
  if (!IsLastUsedProfile()) {
    return;
  }

  [app_group::GetGroupUserDefaults()
      setObject:[NSNumber numberWithBool:saving_passwords_enabled_.GetValue()]
         forKey:AppGroupUserDefaultsCredentialProviderSavingPasswordsEnabled()];
  [app_group::GetGroupUserDefaults()
      setObject:[NSNumber numberWithBool:saving_passwords_enabled_.IsManaged()]
         forKey:AppGroupUserDefaultsCredentialProviderSavingPasswordsManaged()];
  [app_group::GetGroupUserDefaults()
      setObject:[NSNumber numberWithBool:saving_passkeys_enabled_.GetValue()]
         forKey:AppGroupUserDefaultsCredentialProviderSavingPasskeysEnabled()];
  UpdateAutomaticPasskeyUpgradeSetting();
}

MemoryCredentialStore* CredentialProviderService::GetCredentialStore(
    password_manager::PasswordStoreInterface* store) const {
  return store == profile_password_store_ ? profile_credential_store_
                                          : account_credential_store_;
}
