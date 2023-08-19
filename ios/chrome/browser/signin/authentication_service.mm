// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/authentication_service.h"

#import "base/auto_reset.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/location.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/signin/ios/browser/features.h"
#import "components/signin/public/base/gaia_id_hash.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#import "components/signin/public/identity_manager/primary_account_mutator.h"
#import "components/sync/base/features.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#import "ios/chrome/browser/crash_report/crash_keys_helper.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/authentication_service_delegate.h"
#import "ios/chrome/browser/signin/authentication_service_observer.h"
#import "ios/chrome/browser/signin/refresh_access_token_error.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/signin/system_identity_manager.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"

namespace {

// Enum for Signin.IOSDeviceRestoreSignedInState histogram.
// Entries should not be renumbered and numeric values should never be reused.
enum class IOSDeviceRestoreSignedinState : int {
  // Case when the user is not signed in before the device restore.
  kUserNotSignedInBeforeDeviceRestore = 0,
  // Case when the user is signed in before the device restore but not after.
  kUserSignedInBeforeDeviceRestoreAndSignedOutAfterDeviceRestore = 1,
  // Case when the user is signed in before and after the device restore.
  kUserSignedInBeforeAndAfterDeviceRestore = 2,
  kMaxValue = kUserSignedInBeforeAndAfterDeviceRestore,
};

// Returns the account id associated with `identity`.
CoreAccountId SystemIdentityToAccountID(
    signin::IdentityManager* identity_manager,
    id<SystemIdentity> identity) {
  std::string gaia_id = base::SysNSStringToUTF8([identity gaiaID]);
  return identity_manager->FindExtendedAccountInfoByGaiaId(gaia_id).account_id;
}

}  // namespace

AuthenticationService::AuthenticationService(
    PrefService* pref_service,
    SyncSetupService* sync_setup_service,
    ChromeAccountManagerService* account_manager_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service)
    : pref_service_(pref_service),
      sync_setup_service_(sync_setup_service),
      account_manager_service_(account_manager_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      user_approved_account_list_manager_(pref_service),
      weak_pointer_factory_(this) {
  DCHECK(pref_service_);
  DCHECK(sync_setup_service_);
  DCHECK(identity_manager_);
  DCHECK(sync_service_);
}

AuthenticationService::~AuthenticationService() {
  DCHECK(!delegate_);
}

// static
void AuthenticationService::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kSigninShouldPromptForSigninAgain,
                                false);
  registry->RegisterListPref(prefs::kSigninLastAccounts);
  registry->RegisterBooleanPref(prefs::kSigninLastAccountsMigrated, false);
}

void AuthenticationService::Initialize(
    std::unique_ptr<AuthenticationServiceDelegate> delegate) {
  CHECK(delegate);
  CHECK(!initialized());
  bool has_primary_account_before_initialize =
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  int account_count_before_initialize =
      identity_manager_->GetAccountsWithRefreshTokens().size();
  delegate_ = std::move(delegate);
  signin::Tribool device_restore_session = IsFirstSessionAfterDeviceRestore();
  initialized_ = true;

  identity_manager_observation_.Observe(identity_manager_);
  HandleForgottenIdentity(nil, /*should_prompt=*/true,
                          device_restore_session == signin::Tribool::kTrue);
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // If the user is signed out, the list is supposed to be empty. To avoid
    // any issue if a crash happened between the sign-out and clearing out
    // the list, it is better to clear out this list.
    // See crbug.com/3862523.
    user_approved_account_list_manager_.ClearApprovedAccountList();
  }

  // Clean up account-scoped settings, in case any accounts were removed from
  // the device while Chrome wasn't running.
  ClearAccountSettingsPrefsOfRemovedAccounts();

  crash_keys::SetCurrentlySignedIn(
      HasPrimaryIdentity(signin::ConsentLevel::kSignin));

  account_manager_service_observation_.Observe(account_manager_service_);

  // Register for prefs::kSigninAllowed.
  pref_change_registrar_.Init(pref_service_);
  PrefChangeRegistrar::NamedChangeCallback signin_allowed_callback =
      base::BindRepeating(&AuthenticationService::OnSigninAllowedChanged,
                          base::Unretained(this));
  pref_change_registrar_.Add(prefs::kSigninAllowed, signin_allowed_callback);

  // Register for prefs::kBrowserSigninPolicy.
  PrefService* local_pref_service = GetApplicationContext()->GetLocalState();
  local_pref_change_registrar_.Init(local_pref_service);
  PrefChangeRegistrar::NamedChangeCallback browser_signin_policy_callback =
      base::BindRepeating(&AuthenticationService::OnBrowserSigninPolicyChanged,
                          base::Unretained(this));
  local_pref_change_registrar_.Add(prefs::kBrowserSigninPolicy,
                                   browser_signin_policy_callback);

  // Reload credentials to ensure the accounts from the token service are
  // up-to-date.
  // As UpdateHaveAccountsChangedAtColdStart is only called while the
  // application is cold starting, `keychain_reload` must be set to true.
  ReloadCredentialsFromIdentities(/*keychain_reload=*/true);

  OnApplicationWillEnterForeground();
  bool has_primary_account_after_initialize =
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  DCHECK(!has_primary_account_after_initialize ||
         has_primary_account_before_initialize);
  if (device_restore_session == signin::Tribool::kTrue) {
    // Records device restore histograms.
    if (has_primary_account_before_initialize) {
      base::UmaHistogramCounts100("Signin.IOSDeviceRestoreIdentityCountBefore",
                                  account_count_before_initialize);
      int account_count_after_initialize =
          identity_manager_->GetAccountsWithRefreshTokens().size();
      base::UmaHistogramCounts100("Signin.IOSDeviceRestoreIdentityCountAfter",
                                  account_count_after_initialize);
    }
    IOSDeviceRestoreSignedinState signed_in_state =
        IOSDeviceRestoreSignedinState::kUserNotSignedInBeforeDeviceRestore;
    if (has_primary_account_before_initialize) {
      signed_in_state =
          has_primary_account_after_initialize
              ? IOSDeviceRestoreSignedinState::
                    kUserSignedInBeforeAndAfterDeviceRestore
              : IOSDeviceRestoreSignedinState::
                    kUserSignedInBeforeDeviceRestoreAndSignedOutAfterDeviceRestore;
    }
    base::UmaHistogramEnumeration("Signin.IOSDeviceRestoreSignedInState",
                                  signed_in_state);
  }
}

void AuthenticationService::Shutdown() {
  user_approved_account_list_manager_.Shutdown();
  identity_manager_observation_.Reset();
  account_manager_service_observation_.Reset();
  delegate_.reset();
}

void AuthenticationService::AddObserver(
    AuthenticationServiceObserver* observer) {
  observer_list_.AddObserver(observer);
  // Handle messages for late observers.
  if (primary_account_was_restricted_) {
    observer->OnPrimaryAccountRestricted();
  }
}

void AuthenticationService::RemoveObserver(
    AuthenticationServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

AuthenticationService::ServiceStatus AuthenticationService::GetServiceStatus() {
  if (!account_manager_service_->IsServiceSupported()) {
    return ServiceStatus::SigninDisabledByInternal;
  }
  BrowserSigninMode policy_mode = static_cast<BrowserSigninMode>(
      GetApplicationContext()->GetLocalState()->GetInteger(
          prefs::kBrowserSigninPolicy));
  switch (policy_mode) {
    case BrowserSigninMode::kDisabled:
      return ServiceStatus::SigninDisabledByPolicy;
    case BrowserSigninMode::kForced:
      return ServiceStatus::SigninForcedByPolicy;
    case BrowserSigninMode::kEnabled:
      break;
  }
  if (!pref_service_->GetBoolean(prefs::kSigninAllowed)) {
    return ServiceStatus::SigninDisabledByUser;
  }
  return ServiceStatus::SigninAllowed;
}

void AuthenticationService::OnApplicationWillEnterForeground() {
  if (GetServiceStatus() !=
      AuthenticationService::ServiceStatus::SigninDisabledByInternal) {
    UMA_HISTOGRAM_COUNTS_100(
        "Signin.IOSNumberOfDeviceAccounts",
        [account_manager_service_->GetAllIdentities() count]);
  }

  // Clear signin errors on the accounts that had a specific MDM device status.
  // This will trigger services to fetch data for these accounts again.
  using std::swap;
  std::map<CoreAccountId, id<RefreshAccessTokenError>> cached_mdm_errors;
  swap(cached_mdm_errors_, cached_mdm_errors);

  if (!cached_mdm_errors.empty()) {
    signin::DeviceAccountsSynchronizer* device_accounts_synchronizer =
        identity_manager_->GetDeviceAccountsSynchronizer();
    for (const auto& pair : cached_mdm_errors) {
      device_accounts_synchronizer->ReloadAccountFromSystem(pair.first);
    }
  }
}

void AuthenticationService::SetReauthPromptForSignInAndSync() {
  pref_service_->SetBoolean(prefs::kSigninShouldPromptForSigninAgain, true);
}

void AuthenticationService::ResetReauthPromptForSignInAndSync() {
  pref_service_->SetBoolean(prefs::kSigninShouldPromptForSigninAgain, false);
}

bool AuthenticationService::ShouldReauthPromptForSignInAndSync() const {
  return pref_service_->GetBoolean(prefs::kSigninShouldPromptForSigninAgain);
}

bool AuthenticationService::IsAccountListApprovedByUser() const {
  DCHECK(HasPrimaryIdentity(signin::ConsentLevel::kSignin));
  std::vector<CoreAccountInfo> accounts_info =
      identity_manager_->GetAccountsWithRefreshTokens();
  return user_approved_account_list_manager_.IsAccountListApprouvedByUser(
      accounts_info);
}

void AuthenticationService::ApproveAccountList() {
  DCHECK(HasPrimaryIdentity(signin::ConsentLevel::kSignin));
  if (IsAccountListApprovedByUser())
    return;
  std::vector<CoreAccountInfo> current_accounts_info =
      identity_manager_->GetAccountsWithRefreshTokens();
  user_approved_account_list_manager_.SetApprovedAccountList(
      current_accounts_info);
}

bool AuthenticationService::HasPrimaryIdentity(
    signin::ConsentLevel consent_level) const {
  return GetPrimaryIdentity(consent_level) != nil;
}

bool AuthenticationService::HasPrimaryIdentityManaged(
    signin::ConsentLevel consent_level) const {
  return identity_manager_
      ->FindExtendedAccountInfo(
          identity_manager_->GetPrimaryAccountInfo(consent_level))
      .IsManaged();
}

id<SystemIdentity> AuthenticationService::GetPrimaryIdentity(
    signin::ConsentLevel consent_level) const {
  // There is no authenticated identity if there is no signed in user or if the
  // user signed in via the client login flow.
  if (!identity_manager_->HasPrimaryAccount(consent_level)) {
    return nil;
  }

  std::string authenticated_gaia_id =
      identity_manager_->GetPrimaryAccountInfo(consent_level).gaia;
  if (authenticated_gaia_id.empty())
    return nil;

  return account_manager_service_->GetIdentityWithGaiaID(authenticated_gaia_id);
}

void AuthenticationService::SignIn(id<SystemIdentity> identity,
                                   signin_metrics::AccessPoint access_point) {
  ServiceStatus status = GetServiceStatus();
  CHECK(status == ServiceStatus::SigninAllowed ||
        status == ServiceStatus::SigninForcedByPolicy)
      << "Service status " << static_cast<int>(status);
  DCHECK(account_manager_service_->IsValidIdentity(identity));

  primary_account_was_restricted_ = false;

  ResetReauthPromptForSignInAndSync();

  // TODO(crbug.com/1442202): Move this reset to a place more consistent with
  // bookmarks.
  ResetLastUsedBookmarkFolder(pref_service_);

  // Load all credentials from SSO library. This must load the credentials
  // for the primary account too.
  identity_manager_->GetDeviceAccountsSynchronizer()
      ->ReloadAllAccountsFromSystemWithPrimaryAccount(CoreAccountId());

  const CoreAccountId account_id = identity_manager_->PickAccountIdForAccount(
      base::SysNSStringToUTF8(identity.gaiaID),
      base::SysNSStringToUTF8(identity.userEmail));

  // Ensure that the account the user is trying to sign into has been loaded
  // from the SSO library and that hosted_domain is set (should be the proper
  // hosted domain or kNoHostedDomainFound that are both non-empty strings).
  CHECK(identity_manager_->HasAccountWithRefreshToken(account_id));
  const AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
  CHECK(!account_info.IsEmpty());

  // `PrimaryAccountManager::SetAuthenticatedAccountId` simply ignores the call
  // if there is already a signed in user. Check that there is no signed in
  // account or that the new signed in account matches the old one to avoid a
  // mismatch between the old and the new authenticated accounts.
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    DCHECK(identity_manager_->GetPrimaryAccountMutator());
    // Initial sign-in to Chrome does not automatically turn on Sync features.
    // The Sync service will be enabled in a separate request to
    // `GrantSyncConsent`.
    signin::PrimaryAccountMutator::PrimaryAccountError error =
        identity_manager_->GetPrimaryAccountMutator()->SetPrimaryAccount(
            account_id, signin::ConsentLevel::kSignin, access_point);
    CHECK_EQ(signin::PrimaryAccountMutator::PrimaryAccountError::kNoError,
             error);
  }

  // The primary account should now be set to the expected account_id.
  // If CHECK_EQ() fails, having the CHECK() before would help to understand if
  // the primary account is empty or different that `account_id`.
  // Related to crbug.com/1308448.
  CoreAccountId primary_account =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  CHECK(!primary_account.empty());
  CHECK_EQ(account_id, primary_account);
  crash_keys::SetCurrentlySignedIn(true);
}

void AuthenticationService::GrantSyncConsent(
    id<SystemIdentity> identity,
    signin_metrics::AccessPoint access_point) {
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    // TODO(crbug.com/1462858): Turn sync on was deprecated. Remove
    // `GrantSyncConsent()` as it is obsolete.
    DUMP_WILL_BE_CHECK(access_point !=
                       signin_metrics::AccessPoint::
                           ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO)
        << "Turn sync on should not be available as sync promos are deprecated "
           "[access point = "
        << int(access_point) << "]";
  }
  DCHECK(account_manager_service_->IsValidIdentity(identity));
  DCHECK(identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  const CoreAccountId account_id = identity_manager_->PickAccountIdForAccount(
      base::SysNSStringToUTF8(identity.gaiaID),
      base::SysNSStringToUTF8(identity.userEmail));
  const AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
  CHECK(!account_info.IsEmpty());
  CHECK(!account_info.hosted_domain.empty());

  // When sync is disabled by enterprise, sync consent is not removed.
  // Consent can be skipped.
  // TODO(crbug.com/1259054): Remove this if once the sync consent is removed
  // when enteprise disable sync.
  if (!HasPrimaryIdentity(signin::ConsentLevel::kSync)) {
    const signin::PrimaryAccountMutator::PrimaryAccountError error =
        identity_manager_->GetPrimaryAccountMutator()->SetPrimaryAccount(
            account_id, signin::ConsentLevel::kSync, access_point);
    CHECK_EQ(signin::PrimaryAccountMutator::PrimaryAccountError::kNoError,
             error)
        << "SetPrimaryAccount error: " << static_cast<int>(error);
  }
  CHECK_EQ(account_id,
           identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSync));

  // Sets the Sync setup handle to prepare for configuring the Sync data types
  // before Sync-the-feature actually starts.
  // TODO(crbug.com/1206680): Add EarlGrey tests to ensure that the Sync feature
  // only starts after GrantSyncConsent is called.
  sync_setup_service_->PrepareForFirstSyncSetup();

  // Kick-off sync: The authentication error UI (sign in infobar and warning
  // badge in settings screen) check the sync auth error state. Sync
  // needs to be kicked off so that it resets the auth error quickly once
  // `identity` is reauthenticated.
  sync_service_->SetSyncFeatureRequested();
}

void AuthenticationService::SignOut(
    signin_metrics::ProfileSignout signout_source,
    bool force_clear_browsing_data,
    ProceduralBlock completion) {
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    if (completion)
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(completion));
    return;
  }

  // TODO(crbug.com/1442202): Move this reset to a place more consistent with
  // bookmarks.
  ResetLastUsedBookmarkFolder(pref_service_);

  const bool is_managed =
      HasPrimaryIdentityManaged(signin::ConsentLevel::kSignin);
  // Get first setup complete value before to stop the sync service.
  const bool is_initial_sync_feature_setup_complete =
      sync_setup_service_->IsInitialSyncFeatureSetupComplete();

  auto* account_mutator = identity_manager_->GetPrimaryAccountMutator();
  // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
  DCHECK(account_mutator);

  account_mutator->ClearPrimaryAccount(
      signout_source, signin_metrics::SignoutDelete::kIgnoreMetric);
  crash_keys::SetCurrentlySignedIn(false);
  cached_mdm_errors_.clear();

  // Browsing data for managed account needs to be cleared only if sync has
  // started at least once.
  if (force_clear_browsing_data ||
      (is_managed && is_initial_sync_feature_setup_complete)) {
    delegate_->ClearBrowsingData(completion);
  } else if (completion) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(completion));
  }
}

id<RefreshAccessTokenError> AuthenticationService::GetCachedMDMError(
    id<SystemIdentity> identity) {
  auto it = cached_mdm_errors_.find(
      SystemIdentityToAccountID(identity_manager_, identity));

  if (it == cached_mdm_errors_.end()) {
    return nil;
  }

  if (!identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          it->first)) {
    // Account has no error, invalidate the cache.
    cached_mdm_errors_.erase(it);
    return nil;
  }

  return it->second;
}

bool AuthenticationService::HasCachedMDMErrorForIdentity(
    id<SystemIdentity> identity) {
  return GetCachedMDMError(identity) != nil;
}

bool AuthenticationService::ShowMDMErrorDialogForIdentity(
    id<SystemIdentity> identity) {
  id<RefreshAccessTokenError> cached_error = GetCachedMDMError(identity);
  if (!cached_error) {
    return false;
  }

  GetApplicationContext()->GetSystemIdentityManager()->HandleMDMNotification(
      identity, cached_error, base::DoNothing());

  return true;
}

base::WeakPtr<AuthenticationService> AuthenticationService::GetWeakPtr() {
  return weak_pointer_factory_.GetWeakPtr();
}

void AuthenticationService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      DCHECK(user_approved_account_list_manager_.GetApprovedAccountIDList()
                 .empty());
      ApproveAccountList();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      user_approved_account_list_manager_.ClearApprovedAccountList();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }

  for (auto& observer : observer_list_) {
    observer.OnPrimaryIdentityChanged();
  }
}

void AuthenticationService::OnIdentityListChanged(bool need_user_approval) {
  ClearAccountSettingsPrefsOfRemovedAccounts();

  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // IdentityManager::HasPrimaryAccount() needs to be called instead of
    // AuthenticationService::HasPrimaryIdentity() or
    // AuthenticationService::GetPrimaryIdentity().
    // If the primary identity has just been removed, GetPrimaryIdentity()
    // would return NO (since this method tests if the primary identity exists
    // in ChromeIdentityService).
    // In this case, we do need to call ReloadCredentialsFromIdentities().
    return;
  }
  // The list of identities may change while in an authorized call. Signing out
  // the authenticated user at this time may lead to crashes (e.g.
  // http://crbug.com/398431 ).
  // Handle the change of the identity list on the next message loop cycle.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&AuthenticationService::ReloadCredentialsFromIdentities,
                     GetWeakPtr(), need_user_approval));
}

bool AuthenticationService::HandleMDMError(id<SystemIdentity> identity,
                                           id<RefreshAccessTokenError> error) {
  id<RefreshAccessTokenError> cached_error = GetCachedMDMError(identity);
  if (cached_error && [cached_error isEqualToError:error]) {
    // Same status as the last error, ignore it to avoid spamming users.
    return false;
  }

  SystemIdentityManager* system_identity_manager =
      GetApplicationContext()->GetSystemIdentityManager();

  if (system_identity_manager->HandleMDMNotification(
          identity, error,
          base::BindOnce(&AuthenticationService::MDMErrorHandled,
                         weak_pointer_factory_.GetWeakPtr(), identity))) {
    const CoreAccountId key =
        SystemIdentityToAccountID(identity_manager_, identity);
    cached_mdm_errors_[key] = error;
    return true;
  }

  return false;
}

void AuthenticationService::MDMErrorHandled(id<SystemIdentity> identity,
                                            bool is_blocked) {
  // If the identity is blocked, sign-out of the account. As only managed
  // account can be blocked, this will clear the associated browsing data.
  if (!is_blocked) {
    return;
  }

  if (![identity isEqual:GetPrimaryIdentity(signin::ConsentLevel::kSignin)]) {
    return;
  }

  SignOut(signin_metrics::ProfileSignout::kAbortSignin,
          /*force_clear_browsing_data*/ false, nil);
}

void AuthenticationService::OnAccessTokenRefreshFailed(
    id<SystemIdentity> identity,
    id<RefreshAccessTokenError> error) {
  if (HandleMDMError(identity, error)) {
    return;
  }

  if (!error.isInvalidGrantError) {
    // If the failure is not due to an invalid grant, the identity is not
    // invalid and there is nothing to do.
    return;
  }

  // Handle the failure of access token refresh on the next message loop cycle.
  // `identity` is now invalid and the authentication service might need to
  // react to this loss of identity.
  // Note that no reload of the credentials is necessary here, as `identity`
  // might still be accessible in SSO, and `OnIdentityListChanged` will handle
  // this when `identity` will actually disappear from SSO.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AuthenticationService::HandleForgottenIdentity,
                                GetWeakPtr(), identity, /*should_prompt=*/true,
                                /*device_restore=*/false));
}

void AuthenticationService::HandleForgottenIdentity(
    id<SystemIdentity> invalid_identity,
    bool should_prompt,
    bool device_restore) {
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // User is not signed in. Nothing to do here.
    return;
  }

  // YES if the primary identity should be ignore to simulate a backup/restore
  // of the device.
  bool simulate_identity_lost_for_restore =
      device_restore && experimental_flags::SimulatePostDeviceRestore();
  // If the restore shorty is needs to be simulated, the primary identity should
  // not found.
  id<SystemIdentity> authenticated_identity =
      simulate_identity_lost_for_restore
          ? nil
          : GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (authenticated_identity &&
      ![authenticated_identity isEqual:invalid_identity]) {
    // `authenticated_identity` exists and is a valid identity. Nothing to do
    // here.
    return;
  }

  const CoreAccountInfo account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  const bool account_filtered_out =
      account_manager_service_->IsEmailRestricted(account_info.email);

  // Reauth prompt should only be set when the user is syncing, since reauth
  // turns on sync by default.
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    should_prompt = should_prompt && identity_manager_->HasPrimaryAccount(
                                         signin::ConsentLevel::kSignin);
  } else {
    should_prompt = should_prompt && identity_manager_->HasPrimaryAccount(
                                         signin::ConsentLevel::kSync);
  }

  // Metrics.
  signin_metrics::ProfileSignout signout_source;
  if (account_filtered_out) {
    // Account filtered out by enterprise policy.
    signout_source = signin_metrics::ProfileSignout::kPrefChanged;
  } else if (device_restore) {
    // Account removed from the device after a device restore.
    signout_source = signin_metrics::ProfileSignout::
        kIosAccountRemovedFromDeviceAfterRestore;
  } else {
    // Account removed from the device by another app or the token being
    // invalid.
    signout_source = signin_metrics::ProfileSignout::kAccountRemovedFromDevice;
  }

  // Store the pre-device-restore identity in-memory in order to prompt user
  // to sign-in again later.
  if (device_restore) {
    AccountInfo extended_account_info =
        identity_manager_->FindExtendedAccountInfoByAccountId(
            account_info.account_id);
    StorePreRestoreIdentity(GetApplicationContext()->GetLocalState(),
                            extended_account_info);
  }

  // Sign the user out.
  SignOut(signout_source, /*force_clear_browsing_data=*/false, nil);

  if (should_prompt && account_filtered_out) {
    FirePrimaryAccountRestricted();
  } else if (should_prompt &&
             IsFirstSessionAfterDeviceRestore() != signin::Tribool::kTrue) {
    // If the device is restored, the restore shorty UI will be shown.
    // Therefore, the reauth UI should be skipped.
    SetReauthPromptForSignInAndSync();
  }
}

void AuthenticationService::ReloadCredentialsFromIdentities(
    bool keychain_reload) {
  if (is_reloading_credentials_)
    return;

  base::AutoReset<bool> auto_reset(&is_reloading_credentials_, true);

  HandleForgottenIdentity(nil, keychain_reload, /*device_restore=*/false);
  if (!HasPrimaryIdentity(signin::ConsentLevel::kSignin))
    return;

  DCHECK(
      !user_approved_account_list_manager_.GetApprovedAccountIDList().empty());
  identity_manager_->GetDeviceAccountsSynchronizer()
      ->ReloadAllAccountsFromSystemWithPrimaryAccount(
          identity_manager_->GetPrimaryAccountId(
              signin::ConsentLevel::kSignin));
  if (!keychain_reload) {
    // The changes come from Chrome, so we can approve this new account list,
    // since this change comes from the user.
    ApproveAccountList();
  }
}

void AuthenticationService::FirePrimaryAccountRestricted() {
  primary_account_was_restricted_ = true;
  for (auto& observer : observer_list_) {
    observer.OnPrimaryAccountRestricted();
  }
}

void AuthenticationService::OnSigninAllowedChanged(const std::string& name) {
  DCHECK_EQ(prefs::kSigninAllowed, name);
  FireServiceStatusNotification();
}

void AuthenticationService::OnBrowserSigninPolicyChanged(
    const std::string& name) {
  DCHECK_EQ(prefs::kBrowserSigninPolicy, name);
  FireServiceStatusNotification();
}

void AuthenticationService::FireServiceStatusNotification() {
  for (auto& observer : observer_list_) {
    observer.OnServiceStatusChanged();
  }
}

void AuthenticationService::ClearAccountSettingsPrefsOfRemovedAccounts() {
  std::vector<signin::GaiaIdHash> available_gaia_ids;
  for (id<SystemIdentity> identity in account_manager_service_
           ->GetAllIdentities()) {
    signin::GaiaIdHash gaia_id_hash = signin::GaiaIdHash::FromGaiaId(
        base::SysNSStringToUTF8(identity.gaiaID));
    available_gaia_ids.push_back(gaia_id_hash);
  }
  sync_service_->GetUserSettings()->KeepAccountSettingsPrefsOnlyForUsers(
      available_gaia_ids);
}
