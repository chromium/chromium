// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_H_
#define IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_H_

#include <string>
#include <vector>

#import "base/ios/block_types.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/signin/user_approved_account_list_manager.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

namespace syncer {
class SyncService;
}

class AuthenticationServiceDelegate;
class AuthenticationServiceFake;
class ChromeAccountManagerService;
@class ChromeIdentity;
class PrefService;
class SyncSetupService;

// AuthenticationService is the Chrome interface to the iOS shared
// authentication library.
class AuthenticationService : public KeyedService,
                              public signin::IdentityManager::Observer,
                              public ios::ChromeIdentityService::Observer {
 public:
  AuthenticationService(PrefService* pref_service,
                        SyncSetupService* sync_setup_service,
                        ChromeAccountManagerService* account_manager_service,
                        signin::IdentityManager* identity_manager,
                        syncer::SyncService* sync_service);
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

  // Reminds user to Sign in and sync to Chrome when a new tab is opened.
  void SetReauthPromptForSignInAndSync();

  // Clears the reminder to Sign in and sync to Chrome when a new tab is opened.
  void ResetReauthPromptForSignInAndSync();

  // Returns whether user should be prompted to Sign in and sync to Chrome.
  bool ShouldReauthPromptForSignInAndSync() const;

  // Returns whether the current account list has been approved by the user.
  // This method should only be called when there is a primary account.
  //
  // TODO(crbug.com/1084491): To make sure IsAccountListApprovedByUser()
  // a non-stale value, an notification need to be implemented when the SSO
  // keychain is reloaded. The user can add/remove an account while Chrome
  // is in foreground (with split screen on iPad).
  bool IsAccountListApprovedByUser() const;

  // Saves the current account list in Chrome as being approved by the user.
  // This method should only be called when there is a primary account.
  void ApproveAccountList();

  // ChromeIdentity management

  // Returns true if the user is signed in.
  // While the AuthenticationService is in background, this will reload the
  // credentials to ensure the value is up to date.
  bool HasPrimaryIdentity(signin::ConsentLevel consent_level) const;

  // Returns true if the user is signed in and the identity is considered
  // managed.
  // Virtual for testing.
  virtual bool HasPrimaryIdentityManaged(
      signin::ConsentLevel consent_level) const;

  // Retrieves the identity of the currently authenticated user or |nil| if
  // either the user is not authenticated, or is authenticated through
  // ClientLogin.
  // Virtual for testing.
  virtual ChromeIdentity* GetPrimaryIdentity(
      signin::ConsentLevel consent_level) const;

  // Grants signin::ConsentLevel::kSignin to |identity|.
  // This method does not set up Sync-the-feature for the identity.
  // Virtual for testing.
  virtual void SignIn(ChromeIdentity* identity);

  // Grants signin::ConsentLevel::kSync to |identity|.
  // This starts setting up Sync-the-feature, but the setup will only complete
  // once SyncUserSettings::SetFirstSetupComplete() is called.
  // Virtual for testing.
  virtual void GrantSyncConsent(ChromeIdentity* identity);

  // Signs the authenticated user out of Chrome and clears the browsing
  // data if the account is managed. If force_clear_browsing_data is true,
  // clears the browsing data unconditionally.
  // Sync consent is automatically removed from all signed-out accounts.
  // Virtual for testing.
  virtual void SignOut(signin_metrics::ProfileSignout signout_source,
                       bool force_clear_browsing_data,
                       ProceduralBlock completion);

  // Returns whether there is a cached associated MDM error for |identity|.
  bool HasCachedMDMErrorForIdentity(ChromeIdentity* identity) const;

  // Shows the MDM Error dialog for |identity| if it has an associated MDM
  // error. Returns true if |identity| had an associated error, false otherwise.
  bool ShowMDMErrorDialogForIdentity(ChromeIdentity* identity);

  // Resets the ChromeIdentityService observer to the one available in the
  // ChromeBrowserProvider. Used for testing when changing the
  // ChromeIdentityService to or from a fake one.
  void ResetChromeIdentityServiceObserverForTesting();

  // Returns a weak pointer of this.
  base::WeakPtr<AuthenticationService> GetWeakPtr();

  // This needs to be invoked when the application enters foreground to
  // sync the accounts between the IdentityManager and the SSO library.
  void OnApplicationWillEnterForeground();

 private:
  friend class AuthenticationServiceTest;
  friend class AuthenticationServiceFake;

  // Migrates the token service accounts stored in prefs from emails to account
  // ids.
  void MigrateAccountsStoredInPrefsIfNeeded();

  // Returns the cached MDM infos associated with |identity|. If the cache
  // is stale for |identity|, the entry might be removed.
  NSDictionary* GetCachedMDMInfo(ChromeIdentity* identity) const;

  // Handles an MDM notification |user_info| associated with |identity|.
  // Returns whether the notification associated with |user_info| was fully
  // handled.
  bool HandleMDMNotification(ChromeIdentity* identity, NSDictionary* user_info);

  // Verifies that the authenticated user is still associated with a valid
  // ChromeIdentity. This method must only be called when the user is
  // authenticated with the shared authentication library. If there is no valid
  // ChromeIdentity associated with the currently authenticated user, or the
  // identity is |invalid_identity|, this method will sign the user out.
  //
  // |invalid_identity| is an additional identity to consider invalid. It can be
  // nil if there is no such additional identity to ignore.
  //
  // |should_prompt| indicates whether the user should be prompted with the
  // resign-in infobar if the method signs out.
  void HandleForgottenIdentity(ChromeIdentity* invalid_identity,
                               bool should_prompt);

  // Checks if the authenticated identity was removed by calling
  // |HandleForgottenIdentity|. Reloads the OAuth2 token service accounts if the
  // authenticated identity is still present.
  // |keychain_reload| indicates if the identity list has to be reloaded because
  // the keychain has changed.
  void ReloadCredentialsFromIdentities(bool keychain_reload);

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  // ChromeIdentityServiceObserver implementation.
  void OnIdentityListChanged(bool keychainReload) override;
  void OnAccessTokenRefreshFailed(ChromeIdentity* identity,
                                  NSDictionary* user_info) override;
  void OnChromeIdentityServiceWillBeDestroyed() override;

  // The delegate for this AuthenticationService. It is invalid to call any
  // method on this object except Initialize() or Shutdown() if this pointer
  // is null.
  std::unique_ptr<AuthenticationServiceDelegate> delegate_;

  // Pointer to the KeyedServices used by AuthenticationService.
  PrefService* pref_service_ = nullptr;
  SyncSetupService* sync_setup_service_ = nullptr;
  ChromeAccountManagerService* account_manager_service_ = nullptr;
  signin::IdentityManager* identity_manager_ = nullptr;
  syncer::SyncService* sync_service_ = nullptr;

  // Whether Initialized has been called.
  bool initialized_ = false;

  // Manager for the approved account list.
  UserApprovedAccountListManager user_approved_account_list_manager_;

  // Whether the AuthenticationService is currently reloading credentials, used
  // to avoid an infinite reloading loop.
  bool is_reloading_credentials_ = false;

  // Map between account IDs and their associated MDM error.
  mutable std::map<CoreAccountId, NSDictionary*> cached_mdm_infos_;

  base::ScopedObservation<ios::ChromeIdentityService,
                          ios::ChromeIdentityService::Observer>
      identity_service_observation_{this};

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::WeakPtrFactory<AuthenticationService> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticationService);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_H_
