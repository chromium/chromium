// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_MANAGER_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_MANAGER_H_

#include "base/observer_list.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "services/identity/public/cpp/access_token_fetcher.h"
#include "services/identity/public/cpp/scope_set.h"

#if !defined(OS_CHROMEOS)
#include "components/signin/core/browser/signin_manager.h"
#endif

// Necessary to declare this class as a friend.
namespace arc {
class ArcTermsOfServiceDefaultNegotiatorTest;
}

// Necessary to declare these classes as friends.
namespace chromeos {
class ChromeSessionManager;
class UserSessionManager;
}

// Necessary to declare this class as a friend.
namespace file_manager {
class MultiProfileFilesAppBrowserTest;
}

namespace network {
class SharedURLLoaderFactory;
}

// Necessary to declare these classes as friends.
class ArcSupportHostTest;
class MultiProfileDownloadNotificationTest;

namespace identity {

class PrimaryAccountMutator;

// Gives access to information about the user's Google identities. See
// ./README.md for detailed documentation.
class IdentityManager : public SigninManagerBase::Observer,
                        public OAuth2TokenService::DiagnosticsObserver,
                        public OAuth2TokenService::Observer,
                        public GaiaCookieManagerService::Observer {
 public:
  class Observer {
   public:
    Observer() = default;
    virtual ~Observer() = default;

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    // Called when an account becomes the user's primary account.
    // This method is not called during a reauth.
    virtual void OnPrimaryAccountSet(const AccountInfo& primary_account_info) {}

    // Called when when the user moves from having a primary account to no
    // longer having a primary account.
    virtual void OnPrimaryAccountCleared(
        const AccountInfo& previous_primary_account_info) {}

    // Called when the user attempts but fails to set their primary
    // account. |error| gives the reason for the failure.
    virtual void OnPrimaryAccountSigninFailed(
        const GoogleServiceAuthError& error) {}

    // Called when a new refresh token is associated with |account_info|.
    // |is_valid| indicates whether the new refresh token is valid.
    // NOTE: On a signin event, the ordering of this callback wrt the
    // OnPrimaryAccountSet() callback is undefined. If you as a client are
    // interested in both callbacks, PrimaryAccountAccessTokenFetcher will
    // likely meet your needs. Otherwise, if this lack of ordering is
    // problematic for your use case, please contact blundell@chromium.org.
    virtual void OnRefreshTokenUpdatedForAccount(
        const AccountInfo& account_info,
        bool is_valid) {}

    // Called when the refresh token previously associated with |account_id|
    // has been removed. At the time that this callback is invoked, there is
    // no longer guaranteed to be any AccountInfo associated with
    // |account_id|.
    // NOTE: It is not guaranteed that a call to
    // OnRefreshTokenUpdatedForAccount() has previously occurred for this
    // account due to corner cases.
    // TODO(https://crbug.com/884731): Eliminate these corner cases.
    // NOTE: On a signout event, the ordering of this callback wrt the
    // OnPrimaryAccountCleared() callback is undefined.If this lack of ordering
    // is problematic for your use case, please contact blundell@chromium.org.
    virtual void OnRefreshTokenRemovedForAccount(
        const std::string& account_id) {}

    // Called after refresh tokens are loaded.
    // CAVEAT: On ChromeOS, this callback is not invoked during
    // startup in all cases. See https://crbug.com/749535, which
    // details the cases where it's not invoked.
    virtual void OnRefreshTokensLoaded() {}

    // Called whenever the list of Gaia accounts in the cookie jar has changed.
    // |accounts| is ordered by the order of the accounts in the cookie.
    virtual void OnAccountsInCookieUpdated(
        const std::vector<AccountInfo>& accounts) {}

    // Called before a batch of refresh token state changes is started.
    virtual void OnStartBatchOfRefreshTokenStateChanges() {}

    // Called after a batch of refresh token state chagnes is completed.
    virtual void OnEndBatchOfRefreshTokenStateChanges() {}
  };

  // Observer interface for classes that want to monitor status of various
  // requests. Mostly useful in tests and debugging contexts (e.g., WebUI).
  class DiagnosticsObserver {
   public:
    DiagnosticsObserver() = default;
    virtual ~DiagnosticsObserver() = default;

    DiagnosticsObserver(const DiagnosticsObserver&) = delete;
    DiagnosticsObserver& operator=(const DiagnosticsObserver&) = delete;

    // Called when receiving request for access token.
    virtual void OnAccessTokenRequested(const std::string& account_id,
                                        const std::string& consumer_id,
                                        const identity::ScopeSet& scopes) {}
  };

  IdentityManager(
      SigninManagerBase* signin_manager,
      ProfileOAuth2TokenService* token_service,
      AccountTrackerService* account_tracker_service,
      GaiaCookieManagerService* gaia_cookie_manager_service,
      std::unique_ptr<PrimaryAccountMutator> primary_account_mutator);
  ~IdentityManager() override;

  // Provides access to the extended information of the user's primary account.
  // Returns an empty struct if no such info is available, either because there
  // is no primary account or because the extended information for the primary
  // account has been removed (this happens when the refresh token is revoked,
  // for example).
  AccountInfo GetPrimaryAccountInfo() const;

  // Provides access to the account ID of the user's primary account. Note that
  // this may return a valid string even in cases where GetPrimaryAccountInfo()
  // returns an empty struct, as the extended information for the primary
  // account is removed on certain events (e.g., when its refresh token is
  // revoked).
  const std::string& GetPrimaryAccountId() const;

  // Returns whether the primary account is available. Simple convenience
  // wrapper over checking whether GetPrimaryAccountId() returns a non-empty
  // string.
  bool HasPrimaryAccount() const;

// For ChromeOS, mutation of primary account state is not managed externally.
#if !defined(OS_CHROMEOS)
  // Describes options for handling of tokens upon calling
  // ClearPrimaryAccount().
  enum class ClearAccountTokensAction{
      // Default action (keep or remove tokens) based on internal policy.
      kDefault,
      // Keeps all account tokens for all accounts.
      kKeepAll,
      // Removes all accounts tokens for all accounts.
      kRemoveAll,
  };

  // Clears the primary account, removing the preferences, and canceling all
  // auth in progress. May optionally remove account tokens - see
  // ClearAccountTokensAction. See definitions of signin_metrics::ProfileSignout
  // and signin_metrics::SignoutDelete for usage. Observers will be notified via
  // OnPrimaryAccountCleared() when complete.
  void ClearPrimaryAccount(ClearAccountTokensAction token_action,
                           signin_metrics::ProfileSignout signout_source_metric,
                           signin_metrics::SignoutDelete signout_delete_metric);
#endif  // defined(OS_CHROMEOS)

  // Provides access to the latest cached information of all accounts that have
  // refresh tokens.
  // NOTE: The accounts should not be assumed to be in any particular order; in
  // particular, they are not guaranteed to be in the order in which the
  // refresh tokens were added.
  std::vector<AccountInfo> GetAccountsWithRefreshTokens() const;

  // Provides access to the latest cached information of all accounts that are
  // present in the Gaia cookie in the cookie jar, ordered by their order in
  // the cookie. If the cached state is known to be stale by the underlying
  // implementation, a call to this method will trigger an internal update and
  // subsequent invocation of
  // IdentityManager::Observer::OnAccountsInCookieJarChanged().
  // |source| is supplied as the source of any network requests that are made as
  // part of an internal update.
  // NOTE: The information of whether the cached state is known to be stale by
  // the underlying implementation is not currently exposed. The design for
  // exposing it if necessary is tracked by https://crbug.com/859882. If the
  // lack of this exposure is a blocker for you in using this API, contact
  // blundell@chromium.org.
  std::vector<AccountInfo> GetAccountsInCookieJar(
      const std::string& source) const;

  // Returns true if a refresh token exists for |account_id|.
  bool HasAccountWithRefreshToken(const std::string& account_id) const;

  // Returns true if (a) a refresh token exists for |account_id|, and (b) the
  // refresh token is in a persistent error state (defined as
  // GoogleServiceAuthError::IsPersistentError() returning true for the error
  // returned by GetErrorStateOfRefreshTokenForAccount(account_id)).
  bool HasAccountWithRefreshTokenInPersistentErrorState(
      const std::string& account_id) const;

  // Returns the error state of the refresh token associated with |account_id|.
  // In particular: Returns GoogleServiceAuthError::AuthErrorNone() if either
  // (a) no refresh token exists for |account_id|, or (b) the refresh token is
  // not in a persistent error state. Otherwise, returns the last persistent
  // error that was detected when using the refresh token.
  GoogleServiceAuthError GetErrorStateOfRefreshTokenForAccount(
      const std::string& account_id) const;

  // Returns true if (a) the primary account exists, and (b) a refresh token
  // exists for the primary account.
  bool HasPrimaryAccountWithRefreshToken() const;

  // Creates an AccessTokenFetcher given the passed-in information.
  std::unique_ptr<AccessTokenFetcher> CreateAccessTokenFetcherForAccount(
      const std::string& account_id,
      const std::string& oauth_consumer_name,
      const identity::ScopeSet& scopes,
      AccessTokenFetcher::TokenCallback callback,
      AccessTokenFetcher::Mode mode);

  // Creates an AccessTokenFetcher given the passed-in information, allowing
  // to specify a custom |url_loader_factory| as well.
  std::unique_ptr<AccessTokenFetcher> CreateAccessTokenFetcherForAccount(
      const std::string& account_id,
      const std::string& oauth_consumer_name,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const identity::ScopeSet& scopes,
      AccessTokenFetcher::TokenCallback callback,
      AccessTokenFetcher::Mode mode);

  // If an entry exists in the Identity Service's cache corresponding to the
  // given information, removes that entry; in this case, the next access token
  // request for |account_id| and |scopes| will fetch a new token from the
  // network. Otherwise, is a no-op.
  void RemoveAccessTokenFromCache(const std::string& account_id,
                                  const identity::ScopeSet& scopes,
                                  const std::string& access_token);

  // Returns pointer to the object used to change the signed-in state of the
  // primary account, if supported on the current platform. Otherwise, returns
  // null.
  PrimaryAccountMutator* GetPrimaryAccountMutator();

  // Methods to register or remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void AddDiagnosticsObserver(DiagnosticsObserver* observer);
  void RemoveDiagnosticsObserver(DiagnosticsObserver* observer);

 private:
  // These clients need to call SetPrimaryAccountSynchronouslyForTests().
  friend AccountInfo SetPrimaryAccount(SigninManagerBase* signin_manager,
                                       IdentityManager* identity_manager,
                                       const std::string& email);
  friend MultiProfileDownloadNotificationTest;
  friend file_manager::MultiProfileFilesAppBrowserTest;

  // These clients needs to call SetPrimaryAccountSynchronously().
  friend ArcSupportHostTest;
  friend arc::ArcTermsOfServiceDefaultNegotiatorTest;
  friend chromeos::ChromeSessionManager;
  friend chromeos::UserSessionManager;

  // Sets the primary account info synchronously with both the IdentityManager
  // and its backing SigninManager/ProfileOAuth2TokenService instances.
  // Prefer using the methods in identity_test_{environment, utils}.h to using
  // this method directly.
  void SetPrimaryAccountSynchronouslyForTests(const std::string& gaia_id,
                                              const std::string& email_address,
                                              const std::string& refresh_token);

  // Sets the primary account info synchronously with both the IdentityManager
  // and its backing SigninManager instance. If |refresh_token| is not empty,
  // sets the refresh token with the backing ProfileOAuth2TokenService
  // instance. This method should not be used directly; it exists only to serve
  // one legacy use case at this point.
  // TODO(https://crbug.com/814787): Eliminate the need for this method.
  void SetPrimaryAccountSynchronously(const std::string& gaia_id,
                                      const std::string& email_address,
                                      const std::string& refresh_token);

  // Populates and returns an AccountInfo object corresponding to |account_id|,
  // which must be an account with a refresh token.
  AccountInfo GetAccountInfoForAccountWithRefreshToken(
      std::string account_id) const;

  // SigninManagerBase::Observer:
  void GoogleSigninSucceeded(const AccountInfo& account_info) override;
  void GoogleSignedOut(const AccountInfo& account_info) override;
  void GoogleSigninFailed(const GoogleServiceAuthError& error) override;

  // OAuth2TokenService::Observer:
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokenRevoked(const std::string& account_id) override;
  void OnRefreshTokensLoaded() override;
  void OnStartBatchChanges() override;
  void OnEndBatchChanges() override;

  // GaiaCookieManagerService::Observer:
  void OnGaiaAccountsInCookieUpdated(
      const std::vector<gaia::ListedAccount>& accounts,
      const std::vector<gaia::ListedAccount>& signed_out_accounts,
      const GoogleServiceAuthError& error) override;

  // OAuth2TokenService::DiagnosticsObserver:
  void OnAccessTokenRequested(
      const std::string& account_id,
      const std::string& consumer_id,
      const OAuth2TokenService::ScopeSet& scopes) override;

  // Backing signin classes. NOTE: We strive to limit synchronous access to
  // these classes in the IdentityManager implementation, as all such
  // synchronous access will become impossible when IdentityManager is backed by
  // the Identity Service.
  SigninManagerBase* signin_manager_;
  ProfileOAuth2TokenService* token_service_;
  AccountTrackerService* account_tracker_service_;
  GaiaCookieManagerService* gaia_cookie_manager_service_;

  // PrimaryAccountMutator instance. May be null if mutation of the primary
  // account state is not supported on the current platform.
  std::unique_ptr<PrimaryAccountMutator> primary_account_mutator_;

  // Lists of observers.
  // Makes sure lists are empty on destruction.
  base::ObserverList<Observer, true>::Unchecked observer_list_;
  base::ObserverList<DiagnosticsObserver, true>::Unchecked
      diagnostics_observer_list_;

  DISALLOW_COPY_AND_ASSIGN(IdentityManager);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_MANAGER_H_
