// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/authentication_service.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#import "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ios/chrome/browser/crash_report/crash_keys_helper.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service_delegate.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/system_flags.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Enum describing the different sync states per login methods.
enum LoginMethodAndSyncState {
  // Legacy values retained to keep definitions in histograms.xml in sync.
  CLIENT_LOGIN_SYNC_OFF,
  CLIENT_LOGIN_SYNC_ON,
  SHARED_AUTHENTICATION_SYNC_OFF,
  SHARED_AUTHENTICATION_SYNC_ON,
  // NOTE: Add new login methods and sync states only immediately above this
  // line. Also, make sure the enum list in tools/histogram/histograms.xml is
  // updated with any change in here.
  LOGIN_METHOD_AND_SYNC_STATE_COUNT
};

// A fake account id used in the list of last signed in accounts when migrating
// an email for which the corresponding account was removed.
constexpr char kFakeAccountIdForRemovedAccount[] = "0000000000000";

// Returns the account id associated with |identity|.
CoreAccountId ChromeIdentityToAccountID(
    signin::IdentityManager* identity_manager,
    ChromeIdentity* identity) {
  std::string gaia_id = base::SysNSStringToUTF8([identity gaiaID]);
  auto maybe_account =
      identity_manager
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByGaiaId(gaia_id);
  AccountInfo account_info =
      maybe_account.has_value() ? maybe_account.value() : AccountInfo();
  return account_info.account_id;
}

}  // namespace

AuthenticationService::AuthenticationService(
    PrefService* pref_service,
    SyncSetupService* sync_setup_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service)
    : pref_service_(pref_service),
      sync_setup_service_(sync_setup_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      identity_service_observer_(this),
      identity_manager_observer_(this),
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
  delegate_ = std::move(delegate);
  initialized_ = true;

  MigrateAccountsStoredInPrefsIfNeeded();

  HandleForgottenIdentity(nil, true /* should_prompt */);

  crash_keys::SetCurrentlySignedIn(IsAuthenticated());

  identity_service_observer_.Add(
      ios::GetChromeBrowserProvider()->GetChromeIdentityService());

  OnApplicationWillEnterForeground();
}

void AuthenticationService::Shutdown() {
  identity_manager_observer_.RemoveAll();
  delegate_.reset();
}

void AuthenticationService::OnApplicationWillEnterForeground() {
  if (InForeground())
    return;

  identity_manager_observer_.Add(identity_manager_);

  // As the SSO library does not send notification when the app is in the
  // background, reload the credentials and check whether any accounts have
  // changed (both are done by |UpdateHaveAccountsChangedWhileInBackground|).
  // After that, save the current list of accounts.
  UpdateHaveAccountsChangedWhileInBackground();
  StoreKnownAccountsWhileInForeground();

  if (IsAuthenticated()) {
    bool sync_enabled = sync_setup_service_->IsSyncEnabled();
    LoginMethodAndSyncState loginMethodAndSyncState =
        sync_enabled ? SHARED_AUTHENTICATION_SYNC_ON
                     : SHARED_AUTHENTICATION_SYNC_OFF;
    UMA_HISTOGRAM_ENUMERATION("Signin.IOSLoginMethodAndSyncState",
                              loginMethodAndSyncState,
                              LOGIN_METHOD_AND_SYNC_STATE_COUNT);
  }
  UMA_HISTOGRAM_COUNTS_100("Signin.IOSNumberOfDeviceAccounts",
                           [ios::GetChromeBrowserProvider()
                                   ->GetChromeIdentityService()
                                   ->GetAllIdentities() count]);

  // Clear signin errors on the accounts that had a specific MDM device status.
  // This will trigger services to fetch data for these accounts again.
  using std::swap;
  std::map<CoreAccountId, NSDictionary*> cached_mdm_infos;
  swap(cached_mdm_infos_, cached_mdm_infos);

  if (!cached_mdm_infos.empty()) {
    signin::DeviceAccountsSynchronizer* device_accounts_synchronizer =
        identity_manager_->GetDeviceAccountsSynchronizer();
    for (const auto& cached_mdm_info : cached_mdm_infos) {
      device_accounts_synchronizer->ReloadAccountFromSystem(
          cached_mdm_info.first);
    }
  }
}

void AuthenticationService::OnApplicationDidEnterBackground() {
  if (!InForeground())
    return;

  // Stop observing |identity_manager_| when in the background. Note that
  // this allows checking whether the app is in background without having a
  // separate bool by using identity_manager_observer_.IsObservingSources().
  identity_manager_observer_.Remove(identity_manager_);

  // Reset the state |have_accounts_changed_while_in_background_| as the
  // application just entered background.
  have_accounts_changed_while_in_background_ = false;
}

bool AuthenticationService::InForeground() const {
  // The application is in foreground when |identity_manager_observer_| is
  // observing sources.
  return identity_manager_observer_.IsObservingSources();
}

void AuthenticationService::SetPromptForSignIn() {
  pref_service_->SetBoolean(prefs::kSigninShouldPromptForSigninAgain, true);
}

void AuthenticationService::ResetPromptForSignIn() {
  pref_service_->SetBoolean(prefs::kSigninShouldPromptForSigninAgain, false);
}

bool AuthenticationService::ShouldPromptForSignIn() const {
  return pref_service_->GetBoolean(prefs::kSigninShouldPromptForSigninAgain);
}

void AuthenticationService::UpdateHaveAccountsChangedWhileInBackground() {
  // Load accounts from preference before synchronizing the accounts with
  // the system, otherwiser we would never detect any changes to the list
  // of accounts.
  std::vector<CoreAccountId> last_foreground_accounts =
      GetLastKnownAccountsFromForeground();
  std::sort(last_foreground_accounts.begin(), last_foreground_accounts.end());

  // Reload credentials to ensure the accounts from the token service are
  // up-to-date.
  // As UpdateHaveAccountsChangedWhileInBackground is only called while the
  // application is in background or when it enters foreground, |should_prompt|
  // must be set to true.
  ReloadCredentialsFromIdentities(/*should_prompt=*/true);

  std::vector<CoreAccountInfo> current_accounts_info =
      identity_manager_->GetAccountsWithRefreshTokens();
  std::vector<CoreAccountId> current_accounts;
  for (const CoreAccountInfo& account_info : current_accounts_info)
    current_accounts.push_back(account_info.account_id);
  std::sort(current_accounts.begin(), current_accounts.end());

  have_accounts_changed_while_in_background_ =
      last_foreground_accounts != current_accounts;
}

bool AuthenticationService::HaveAccountsChangedWhileInBackground() const {
  return have_accounts_changed_while_in_background_;
}

void AuthenticationService::MigrateAccountsStoredInPrefsIfNeeded() {
  if (identity_manager_->GetAccountIdMigrationState() ==
      signin::IdentityManager::AccountIdMigrationState::MIGRATION_NOT_STARTED) {
    return;
  }
  DCHECK_EQ(signin::IdentityManager::AccountIdMigrationState::MIGRATION_DONE,
            identity_manager_->GetAccountIdMigrationState());
  if (pref_service_->GetBoolean(prefs::kSigninLastAccountsMigrated)) {
    // Already migrated.
    return;
  }

  std::vector<CoreAccountId> account_ids = GetLastKnownAccountsFromForeground();
  std::vector<base::Value> accounts_pref_value;
  for (const auto& account_id : account_ids) {
    if (identity_manager_->HasAccountWithRefreshToken(account_id)) {
      accounts_pref_value.emplace_back(account_id.ToString());
    } else {
      // The account for |email| was removed since the last application cold
      // start. Insert |kFakeAccountIdForRemovedAccount| to ensure
      // |have_accounts_changed_while_in_background_| will be set to true and
      // the removal won't be silently ignored.
      accounts_pref_value.emplace_back(kFakeAccountIdForRemovedAccount);
    }
  }
  pref_service_->Set(prefs::kSigninLastAccounts,
                     base::Value(std::move(accounts_pref_value)));
  pref_service_->SetBoolean(prefs::kSigninLastAccountsMigrated, true);
}

void AuthenticationService::StoreKnownAccountsWhileInForeground() {
  DCHECK(InForeground());
  std::vector<CoreAccountInfo> accounts(
      identity_manager_->GetAccountsWithRefreshTokens());
  std::vector<base::Value> accounts_pref_value;
  for (const CoreAccountInfo& account_info : accounts)
    accounts_pref_value.emplace_back(account_info.account_id.ToString());
  pref_service_->Set(prefs::kSigninLastAccounts,
                     base::Value(std::move(accounts_pref_value)));
}

std::vector<CoreAccountId>
AuthenticationService::GetLastKnownAccountsFromForeground() {
  const base::Value* accounts_pref =
      pref_service_->GetList(prefs::kSigninLastAccounts);

  std::vector<CoreAccountId> accounts;
  for (const auto& value : accounts_pref->GetList()) {
    DCHECK(value.is_string());
    DCHECK(!value.GetString().empty());
    accounts.push_back(CoreAccountId::FromString(value.GetString()));
  }
  return accounts;
}

ChromeIdentity* AuthenticationService::GetAuthenticatedIdentity() const {
  // There is no authenticated identity if there is no signed in user or if the
  // user signed in via the client login flow.
  if (!identity_manager_->HasPrimaryAccount()) {
    return nil;
  }

  std::string authenticated_gaia_id =
      identity_manager_->GetPrimaryAccountInfo().gaia;
  if (authenticated_gaia_id.empty())
    return nil;

  return ios::GetChromeBrowserProvider()
      ->GetChromeIdentityService()
      ->GetIdentityWithGaiaID(authenticated_gaia_id);
}

void AuthenticationService::SignIn(ChromeIdentity* identity) {
  DCHECK(ios::GetChromeBrowserProvider()
             ->GetChromeIdentityService()
             ->IsValidIdentity(identity));

  ResetPromptForSignIn();
  sync_setup_service_->PrepareForFirstSyncSetup();

  const CoreAccountId account_id = identity_manager_->PickAccountIdForAccount(
      base::SysNSStringToUTF8(identity.gaiaID),
      base::SysNSStringToUTF8(identity.userEmail));

  // Load all credentials from SSO library. This must load the credentials
  // for the primary account too.
  identity_manager_->GetDeviceAccountsSynchronizer()
      ->ReloadAllAccountsFromSystem();

  // Ensure that the account the user is trying to sign into has been loaded
  // from the SSO library and that hosted_domain is set (should be the proper
  // hosted domain or kNoHostedDomainFound that are both non-empty strings).
  const base::Optional<AccountInfo> account_info =
      identity_manager_
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              account_id);
  CHECK(account_info.has_value());
  CHECK(!account_info->hosted_domain.empty());

  // |PrimaryAccountManager::SetAuthenticatedAccountId| simply ignores the call
  // if there is already a signed in user. Check that there is no signed in
  // account or that the new signed in account matches the old one to avoid a
  // mismatch between the old and the new authenticated accounts.
  if (!identity_manager_->HasPrimaryAccount()) {
    DCHECK(identity_manager_->GetPrimaryAccountMutator());
    const bool success =
        identity_manager_->GetPrimaryAccountMutator()->SetPrimaryAccount(
            account_id);
    CHECK(success);
  }

  // The primary account should now be set to the expected account_id.
  CHECK_EQ(account_id, identity_manager_->GetPrimaryAccountId());

  // Kick-off sync: The authentication error UI (sign in infobar and warning
  // badge in settings screen) check the sync auth error state. Sync
  // needs to be kicked off so that it resets the auth error quickly once
  // |identity| is reauthenticated.
  // TODO(msarda): Remove this code once the authentication error UI checks
  // SigninGlobalError instead of the sync auth error state.
  // crbug.com/289493
  sync_service_->GetUserSettings()->SetSyncRequested(true);
  crash_keys::SetCurrentlySignedIn(true);
}

void AuthenticationService::SignOut(
    signin_metrics::ProfileSignout signout_source,
    bool force_clear_browsing_data,
    ProceduralBlock completion) {
  if (!identity_manager_->HasPrimaryAccount()) {
    if (completion)
      completion();
    return;
  }

  bool is_managed = IsAuthenticatedIdentityManaged();

  sync_service_->StopAndClear();

  auto* account_mutator = identity_manager_->GetPrimaryAccountMutator();

  // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
  DCHECK(account_mutator);
  account_mutator->ClearPrimaryAccount(
      signout_source, signin_metrics::SignoutDelete::IGNORE_METRIC);
  crash_keys::SetCurrentlySignedIn(false);
  cached_mdm_infos_.clear();
  if (force_clear_browsing_data || is_managed) {
    delegate_->ClearBrowsingData(completion);
  } else if (completion) {
    completion();
  }
}

NSDictionary* AuthenticationService::GetCachedMDMInfo(
    ChromeIdentity* identity) const {
  auto it = cached_mdm_infos_.find(
      ChromeIdentityToAccountID(identity_manager_, identity));

  if (it == cached_mdm_infos_.end()) {
    return nil;
  }

  if (!identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          it->first)) {
    // Account has no error, invalidate the cache.
    cached_mdm_infos_.erase(it);
    return nil;
  }

  return it->second;
}

bool AuthenticationService::HasCachedMDMErrorForIdentity(
    ChromeIdentity* identity) const {
  return GetCachedMDMInfo(identity) != nil;
}

bool AuthenticationService::ShowMDMErrorDialogForIdentity(
    ChromeIdentity* identity) {
  NSDictionary* cached_info = GetCachedMDMInfo(identity);
  if (!cached_info) {
    return false;
  }

  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();
  identity_service->HandleMDMNotification(identity, cached_info, ^(bool){
                                                    });
  return true;
}

void AuthenticationService::ResetChromeIdentityServiceObserverForTesting() {
  DCHECK(!identity_service_observer_.IsObservingSources());
  identity_service_observer_.Add(
      ios::GetChromeBrowserProvider()->GetChromeIdentityService());
}

base::WeakPtr<AuthenticationService> AuthenticationService::GetWeakPtr() {
  return weak_pointer_factory_.GetWeakPtr();
}

void AuthenticationService::OnEndBatchOfRefreshTokenStateChanges() {
  // Accounts maybe have been excluded or included from the current browser
  // state, without any change to the identity list.
  // Store the current list of accounts to make sure it is up-to-date.
  StoreKnownAccountsWhileInForeground();
}

void AuthenticationService::OnIdentityListChanged() {
  // The list of identities may change while in an authorized call. Signing out
  // the authenticated user at this time may lead to crashes (e.g.
  // http://crbug.com/398431 ).
  // Handle the change of the identity list on the next message loop cycle.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&AuthenticationService::HandleIdentityListChanged,
                     GetWeakPtr()));
}

bool AuthenticationService::HandleMDMNotification(ChromeIdentity* identity,
                                                  NSDictionary* user_info) {
  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();
  ios::MDMDeviceStatus status = identity_service->GetMDMDeviceStatus(user_info);
  NSDictionary* cached_info = GetCachedMDMInfo(identity);

  if (cached_info &&
      identity_service->GetMDMDeviceStatus(cached_info) == status) {
    // Same status as the last error, ignore it to avoid spamming users.
    return false;
  }

  base::WeakPtr<AuthenticationService> weak_ptr = GetWeakPtr();
  ios::MDMStatusCallback callback = ^(bool is_blocked) {
    if (is_blocked && weak_ptr.get()) {
      // If the identity is blocked, sign out of the account. As only managed
      // account can be blocked, this will clear the associated browsing data.
      if (identity == weak_ptr->GetAuthenticatedIdentity()) {
        weak_ptr->SignOut(signin_metrics::ABORT_SIGNIN,
                          /*force_clear_browsing_data=*/false, nil);
      }
    }
  };
  if (identity_service->HandleMDMNotification(identity, user_info, callback)) {
    cached_mdm_infos_[ChromeIdentityToAccountID(identity_manager_, identity)] =
        user_info;
    return true;
  }
  return false;
}

void AuthenticationService::OnAccessTokenRefreshFailed(
    ChromeIdentity* identity,
    NSDictionary* user_info) {
  if (HandleMDMNotification(identity, user_info)) {
    return;
  }

  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();
  if (!identity_service->IsInvalidGrantError(user_info)) {
    // If the failure is not due to an invalid grant, the identity is not
    // invalid and there is nothing to do.
    return;
  }

  // Handle the failure of access token refresh on the next message loop cycle.
  // |identity| is now invalid and the authentication service might need to
  // react to this loss of identity.
  // Note that no reload of the credentials is necessary here, as |identity|
  // might still be accessible in SSO, and |OnIdentityListChanged| will handle
  // this when |identity| will actually disappear from SSO.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AuthenticationService::HandleForgottenIdentity,
                                base::Unretained(this), identity, true));
}

void AuthenticationService::OnChromeIdentityServiceWillBeDestroyed() {
  identity_service_observer_.RemoveAll();
}

void AuthenticationService::HandleIdentityListChanged() {
  // Only notify the user about an identity change notification if the
  // application was in background.
  if (InForeground()) {
    // Do not update the have accounts change state when in foreground.
    ReloadCredentialsFromIdentities(/*should_prompt=*/false);
    return;
  }

  UpdateHaveAccountsChangedWhileInBackground();
}

void AuthenticationService::HandleForgottenIdentity(
    ChromeIdentity* invalid_identity,
    bool should_prompt) {
  if (!identity_manager_->HasPrimaryAccount()) {
    // User is not signed in. Nothing to do here.
    return;
  }

  ChromeIdentity* authenticated_identity = GetAuthenticatedIdentity();
  if (authenticated_identity && authenticated_identity != invalid_identity) {
    // |authenticated_identity| exists and is a valid identity. Nothing to do
    // here.
    return;
  }

  // Sign the user out.
  SignOut(signin_metrics::ABORT_SIGNIN, /*force_clear_browsing_data=*/false,
          nil);
  if (should_prompt)
    SetPromptForSignIn();
}

void AuthenticationService::ReloadCredentialsFromIdentities(
    bool should_prompt) {
  if (is_reloading_credentials_) {
    return;
  }

  base::AutoReset<bool> auto_reset(&is_reloading_credentials_, true);

  HandleForgottenIdentity(nil, should_prompt);
  if (IsAuthenticated()) {
    identity_manager_->GetDeviceAccountsSynchronizer()
        ->ReloadAllAccountsFromSystem();
  }
}

bool AuthenticationService::IsAuthenticated() const {
  return GetAuthenticatedIdentity() != nil;
}

bool AuthenticationService::IsAuthenticatedIdentityManaged() const {
  base::Optional<AccountInfo> primary_account_info =
      identity_manager_->FindExtendedAccountInfoForAccountWithRefreshToken(
          identity_manager_->GetPrimaryAccountInfo());
  if (!primary_account_info)
    return false;

  return primary_account_info->IsManaged();
}
