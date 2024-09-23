// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_H_

#import <string>
#import <vector>

#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"

namespace syncer {
class SyncService;
}

class AuthenticationServiceDelegate;
class AuthenticationServiceObserver;
class FakeAuthenticationService;
class PrefService;
@protocol RefreshAccessTokenError;
@protocol SystemIdentity;

// AuthenticationService is the Chrome interface to the iOS shared
// authentication library.
class AuthenticationService : public KeyedService,
                              public signin::IdentityManager::Observer,
                              public ChromeAccountManagerService::Observer {
 public:
  // The service status for AuthenticationService.
  enum class ServiceStatus {
    // Sign-in forced by enterprise policy.
    SigninForcedByPolicy = 0,
    // Sign-in is possible.
    SigninAllowed = 1,
    // Sign-in disabled by user.
    SigninDisabledByUser = 2,
    // Sign-in disabled by enterprise policy.
    SigninDisabledByPolicy = 3,
    // Sign-in disabled for internal reason (probably running Chromium).
    SigninDisabledByInternal = 4,
  };

  // Initializes the service.
  AuthenticationService(PrefService* pref_service,
                        ChromeAccountManagerService* account_manager_service,
                        signin::IdentityManager* identity_manager,
                        syncer::SyncService* sync_service);

  AuthenticationService(const AuthenticationService&) = delete;
  AuthenticationService& operator=(const AuthenticationService&) = delete;

  ~AuthenticationService() override;

  // Registers the preferences used by AuthenticationService;
  static void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns whether the AuthenticationService has been initialized. It is
  // a fatal error to invoke any method on this object except Initialize()
  // if this method returns false.
  bool initialized() const { return initialized_; }

  // Initializes the AuthenticationService.
  void Initialize(std::unique_ptr<AuthenticationServiceDelegate> delegate);

  // KeyedService
  void Shutdown() override;

  // Adds and removes observers.
  void AddObserver(AuthenticationServiceObserver* observer);
  void RemoveObserver(AuthenticationServiceObserver* observer);

  // Returns the service status, see ServiceStatus. This value can be observed
  // using AuthenticationServiceObserver::OnServiceStatusChanged().
  ServiceStatus GetServiceStatus();

  // Reminds user to Sign in and sync to Chrome when a new tab is opened.
  void SetReauthPromptForSignInAndSync();

  // Clears the reminder to Sign in and sync to Chrome when a new tab is opened.
  void ResetReauthPromptForSignInAndSync();

  // Returns whether user should be prompted to Sign in and sync to Chrome.
  bool ShouldReauthPromptForSignInAndSync() const;

  // SystemIdentity management

  // Returns true if the user is signed in.
  // While the AuthenticationService is in background, this will reload the
  // credentials to ensure the value is up to date.
  bool HasPrimaryIdentity(signin::ConsentLevel consent_level) const;

  // Returns true if the user is signed in and the identity is considered
  // managed.
  // Virtual for testing.
  virtual bool HasPrimaryIdentityManaged(
      signin::ConsentLevel consent_level) const;

  // Returns true if data should be cleared on sign-out.
  virtual bool ShouldClearDataForSignedInPeriodOnSignOut() const;

  // Retrieves the identity of the currently authenticated user or `nil` if
  // either the user is not authenticated, or is authenticated through
  // ClientLogin.
  // Virtual for testing.
  virtual id<SystemIdentity> GetPrimaryIdentity(
      signin::ConsentLevel consent_level) const;

  // Grants signin::ConsentLevel::kSignin to `identity` and records the signin
  // at `accessPoint`. This method does not set up Sync-the-feature for the
  // identity. Virtual for testing.
  virtual void SignIn(id<SystemIdentity> identity,
                      signin_metrics::AccessPoint access_point);

  // Grants signin::ConsentLevel::kSync to `identity` and records the event at
  // `access_point`. This starts setting up Sync-the-feature, but the setup will
  // only complete once SyncUserSettings::SetInitialSyncFeatureSetupComplete()
  // is called. This method is used for testing. Virtual for testing.
  // TODO(crbug.com/40067025): Delete this method after Phase 2 on iOS is
  // launched. See ConsentLevel::kSync documentation for details.
  virtual void GrantSyncConsent(id<SystemIdentity> identity,
                                signin_metrics::AccessPoint access_point);

  // Signs the authenticated user out of Chrome and clears the browsing
  // data if the account is managed. If force_clear_browsing_data is true,
  // clears the browsing data unconditionally.
  // Sync consent is automatically removed from all signed-out accounts.
  // `completion` is then executed asynchronously.
  // Virtual for testing.
  virtual void SignOut(signin_metrics::ProfileSignout signout_source,
                       bool force_clear_browsing_data,
                       ProceduralBlock completion);

  // Returns whether there is a cached associated MDM error for `identity`.
  bool HasCachedMDMErrorForIdentity(id<SystemIdentity> identity);

  // Shows the MDM Error dialog for `identity` if it has an associated MDM
  // error. Returns true if `identity` had an associated error, false otherwise.
  bool ShowMDMErrorDialogForIdentity(id<SystemIdentity> identity);

  // Returns a weak pointer of this.
  base::WeakPtr<AuthenticationService> GetWeakPtr();

  // This needs to be invoked when the application enters foreground to
  // sync the accounts between the IdentityManager and the SSO library.
  void OnApplicationWillEnterForeground();

  // Returns whether an account switch is in progress.
  bool IsAccountSwitchInProgress();

  // The account switch is considered to be in progress while the returned
  // object exists. Can only be called when no switch is in progress.
  base::ScopedClosureRunner DeclareAccountSwitchInProgress();

 private:
  friend class FakeAuthenticationService;
  friend class AuthenticationServiceTest;
  friend class FakeAuthenticationService;

  // Returns the cached MDM errors associated with `identity`. If the cache
  // is stale for `identity`, the entry might be removed.
  id<RefreshAccessTokenError> GetCachedMDMError(id<SystemIdentity> identity);

  // Handles an MDM error `error` associated with `identity`.
  // Returns whether the notification associated with `user_info` was fully
  // handled.
  bool HandleMDMError(id<SystemIdentity> identity,
                      id<RefreshAccessTokenError> error);

  // Invoked when the MDM error associated with `identity` has been handled.
  void MDMErrorHandled(id<SystemIdentity> identity, bool is_blocked);

  // Verifies that the authenticated user is still associated with a valid
  // SystemIdentity. This method must only be called when the user is
  // authenticated with the shared authentication library. If there is no valid
  // SystemIdentity associated with the currently authenticated user, or the
  // identity is `invalid_identity`, this method will sign the user out.
  //
  // `invalid_identity` is an additional identity to consider invalid. It can be
  // nil if there is no such additional identity to ignore.
  //
  // `device_restore` should be true only when called from `Initialize()` and
  // Chrome is started after a device restore.
  void HandleForgottenIdentity(id<SystemIdentity> invalid_identity,
                               bool device_restore);

  // Checks if the authenticated identity was removed by calling
  // `HandleForgottenIdentity`. Reloads the OAuth2 token service accounts if the
  // authenticated identity is still present.
  void ReloadCredentialsFromIdentities();

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  // ChromeAccountManagerService::Observer implementation.
  void OnIdentityListChanged() override;
  void OnAccessTokenRefreshFailed(id<SystemIdentity> identity,
                                  id<RefreshAccessTokenError> error) override;

  // Fires `OnPrimaryAccountRestricted` on all observers.
  void FirePrimaryAccountRestricted();

  // Notification for prefs::kSigninAllowed.
  void OnSigninAllowedChanged(const std::string& name);

  // Notification for prefs::kBrowserSigninPolicy.
  void OnBrowserSigninPolicyChanged(const std::string& name);

  // Fires `OnServiceStatusChanged` on all observers.
  void FireServiceStatusNotification();

  // Clears the account settings prefs of all removed accounts from device.
  void ClearAccountSettingsPrefsOfRemovedAccounts();

  // Returns the active identities for MDM.
  NSArray<id<SystemIdentity>>* ActiveIdentities();

  // The delegate for this AuthenticationService. It is invalid to call any
  // method on this object except Initialize() or Shutdown() if this pointer
  // is null.
  std::unique_ptr<AuthenticationServiceDelegate> delegate_;

  // Whether an account is currently switching.
  bool accountSwitchInProgress_ = false;

  // Pointer to the KeyedServices used by AuthenticationService.
  raw_ptr<PrefService> pref_service_ = nullptr;
  raw_ptr<ChromeAccountManagerService> account_manager_service_ = nullptr;
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;
  base::ObserverList<AuthenticationServiceObserver, true> observer_list_;
  // Whether Initialized has been called.
  bool initialized_ = false;

  // Whether the AuthenticationService is currently reloading credentials, used
  // to avoid an infinite reloading loop.
  bool is_reloading_credentials_ = false;

  // Whether the primary account was logged out because it became restricted.
  // It is used to respond to late observers.
  bool primary_account_was_restricted_ = false;

  // Map between account IDs and their associated MDM error.
  std::map<CoreAccountId, id<RefreshAccessTokenError>> cached_mdm_errors_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::ScopedObservation<ChromeAccountManagerService,
                          ChromeAccountManagerService::Observer>
      account_manager_service_observation_{this};

  // Registrar for prefs::kSigninAllowed.
  PrefChangeRegistrar pref_change_registrar_;
  // Registrar for prefs::kBrowserSigninPolicy.
  PrefChangeRegistrar local_pref_change_registrar_;

  base::WeakPtrFactory<AuthenticationService> weak_pointer_factory_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_H_
