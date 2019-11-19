// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_MANAGER_H_
#define GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_MANAGER_H_

#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"

namespace network {
class SharedURLLoaderFactory;
}

class OAuth2AccessTokenFetcher;

// Class that manages requests for OAuth2 access tokens.
class OAuth2AccessTokenManager {
 public:
  // A set of scopes in OAuth2 authentication.
  typedef std::set<std::string> ScopeSet;
  class RequestImpl;

  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    // Creates and returns an OAuth2AccessTokenFetcher.
    virtual std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
        const CoreAccountId& account_id,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        OAuth2AccessTokenConsumer* consumer) WARN_UNUSED_RESULT = 0;

    // Returns |true| if a refresh token is available for |account_id|, and
    // |false| otherwise.
    virtual bool HasRefreshToken(const CoreAccountId& account_id) const = 0;

    // Attempts to fix the error if possible.  Returns true if the error was
    // fixed and false otherwise. Default implementation returns false.
    virtual bool FixRequestErrorIfPossible();

    // Returns a SharedURLLoaderFactory object that will be used as part of
    // fetching access tokens. Default implementation returns nullptr.
    virtual scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
        const;

    // Gives the delegate a chance to handle the access token request before
    // the manager sends the request over the network. Returns true if the
    // request was handled by the delegate (in which case the manager will not
    // send the request) and false otherwise.
    virtual bool HandleAccessTokenFetch(
        RequestImpl* request,
        const CoreAccountId& account_id,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        const std::string& client_id,
        const std::string& client_secret,
        const ScopeSet& scopes);

    // Called when an access token is invalidated.
    virtual void OnAccessTokenInvalidated(const CoreAccountId& account_id,
                                          const std::string& client_id,
                                          const ScopeSet& scopes,
                                          const std::string& access_token) {}

    // Called when an access token is fetched.
    virtual void OnAccessTokenFetched(const CoreAccountId& account_id,
                                      const GoogleServiceAuthError& error) {}
  };

  // Class representing a request that fetches an OAuth2 access token.
  class Request {
   public:
    virtual ~Request();
    virtual CoreAccountId GetAccountId() const = 0;

   protected:
    Request();
  };

  // Class representing the consumer of a Request passed to |StartRequest|,
  // which will be called back when the request completes.
  class Consumer {
   public:
    explicit Consumer(const std::string& id);
    virtual ~Consumer();

    std::string id() const { return id_; }

    // |request| is a Request that is started by this consumer and has
    // completed.
    virtual void OnGetTokenSuccess(
        const Request* request,
        const OAuth2AccessTokenConsumer::TokenResponse& token_response) = 0;
    virtual void OnGetTokenFailure(const Request* request,
                                   const GoogleServiceAuthError& error) = 0;

   private:
    std::string id_;
  };

  // Implements a cancelable |OAuth2AccessTokenManager::Request|, which should
  // be operated on the UI thread.
  // TODO(davidroche): move this out of header file.
  class RequestImpl : public base::SupportsWeakPtr<RequestImpl>,
                      public Request {
   public:
    // |consumer| is required to outlive this.
    RequestImpl(const CoreAccountId& account_id, Consumer* consumer);
    ~RequestImpl() override;

    // Overridden from Request:
    CoreAccountId GetAccountId() const override;

    std::string GetConsumerId() const;

    // Informs |consumer_| that this request is completed.
    void InformConsumer(
        const GoogleServiceAuthError& error,
        const OAuth2AccessTokenConsumer::TokenResponse& token_response);

   private:
    const CoreAccountId account_id_;
    // |consumer_| to call back when this request completes.
    Consumer* const consumer_;

    SEQUENCE_CHECKER(sequence_checker_);
  };

  // Classes that want to monitor status of access token and access token
  // request should implement this interface and register with the
  // AddDiagnosticsObserver() call.
  class DiagnosticsObserver {
   public:
    // Called when receiving request for access token.
    virtual void OnAccessTokenRequested(const CoreAccountId& account_id,
                                        const std::string& consumer_id,
                                        const ScopeSet& scopes) {}

    // Called when access token fetching finished successfully or
    // unsuccessfully. |expiration_time| are only valid with
    // successful completion.
    virtual void OnFetchAccessTokenComplete(const CoreAccountId& account_id,
                                            const std::string& consumer_id,
                                            const ScopeSet& scopes,
                                            GoogleServiceAuthError error,
                                            base::Time expiration_time) {}

    // Called when an access token was removed.
    virtual void OnAccessTokenRemoved(const CoreAccountId& account_id,
                                      const ScopeSet& scopes) {}
  };

  // The parameters used to fetch an OAuth2 access token.
  struct RequestParameters {
    RequestParameters(const std::string& client_id,
                      const CoreAccountId& account_id,
                      const ScopeSet& scopes);
    RequestParameters(const RequestParameters& other);
    ~RequestParameters();
    bool operator<(const RequestParameters& params) const;

    // OAuth2 client id.
    std::string client_id;
    // Account id for which the request is made.
    CoreAccountId account_id;
    // URL scopes for the requested access token.
    ScopeSet scopes;
  };
  typedef std::map<RequestParameters, OAuth2AccessTokenConsumer::TokenResponse>
      TokenCache;

  explicit OAuth2AccessTokenManager(
      OAuth2AccessTokenManager::Delegate* delegate);
  virtual ~OAuth2AccessTokenManager();

  OAuth2AccessTokenManager::Delegate* GetDelegate();
  const OAuth2AccessTokenManager::Delegate* GetDelegate() const;

  // Add or remove observers of this token manager.
  void AddDiagnosticsObserver(DiagnosticsObserver* observer);
  void RemoveDiagnosticsObserver(DiagnosticsObserver* observer);

  // Checks in the cache for a valid access token for a specified |account_id|
  // and |scopes|, and if not found starts a request for an OAuth2 access token
  // using the OAuth2 refresh token maintained by this instance for that
  // |account_id|. The caller owns the returned Request.
  // |scopes| is the set of scopes to get an access token for, |consumer| is
  // the object that will be called back with results if the returned request
  // is not deleted.
  std::unique_ptr<Request> StartRequest(const CoreAccountId& account_id,
                                        const ScopeSet& scopes,
                                        Consumer* consumer);

  // This method does the same as |StartRequest| except it uses |client_id| and
  // |client_secret| to identify OAuth client app instead of using
  // Chrome's default values.
  std::unique_ptr<Request> StartRequestForClient(
      const CoreAccountId& account_id,
      const std::string& client_id,
      const std::string& client_secret,
      const ScopeSet& scopes,
      Consumer* consumer);

  // This method does the same as |StartRequest| except it uses the
  // URLLoaderfactory given by |url_loader_factory| instead of using the one
  // returned by |GetURLLoaderFactory| implemented by the delegate.
  std::unique_ptr<Request> StartRequestWithContext(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const ScopeSet& scopes,
      Consumer* consumer);

  // Fetches an OAuth token for the specified client/scopes. Virtual so it can
  // be overridden for tests.
  virtual void FetchOAuth2Token(
      RequestImpl* request,
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& client_id,
      const std::string& client_secret,
      const ScopeSet& scopes);

  // Returns a currently valid OAuth2 access token for the given set of scopes,
  // or NULL if none have been cached. Note the user of this method should
  // ensure no entry with the same |client_scopes| is added before the usage of
  // the returned entry is done.
  const OAuth2AccessTokenConsumer::TokenResponse* GetCachedTokenResponse(
      const RequestParameters& client_scopes);

  // Clears the internal token cache.
  void ClearCache();

  // Clears all of the tokens belonging to |account_id| from the internal token
  // cache. It does not matter what other parameters, like |client_id| were
  // used to request the tokens.
  void ClearCacheForAccount(const CoreAccountId& account_id);

  // Cancels all requests that are currently in progress. Virtual so it can be
  // overridden for tests.
  virtual void CancelAllRequests();

  // Cancels all requests related to a given |account_id|. Virtual so it can be
  // overridden for tests.
  virtual void CancelRequestsForAccount(const CoreAccountId& account_id);

  // Mark an OAuth2 |access_token| issued for |account_id| and |scopes| as
  // invalid. This should be done if the token was received from this class,
  // but was not accepted by the server (e.g., the server returned
  // 401 Unauthorized). The token will be removed from the cache for the given
  // scopes.
  void InvalidateAccessToken(const CoreAccountId& account_id,
                             const ScopeSet& scopes,
                             const std::string& access_token);

  void set_max_authorization_token_fetch_retries_for_testing(int max_retries);

  // Returns the current number of pending fetchers matching given params.
  size_t GetNumPendingRequestsForTesting(const std::string& client_id,
                                         const CoreAccountId& account_id,
                                         const ScopeSet& scopes) const;

  // Returns a list of DiagnosticsObservers.
  const base::ObserverList<DiagnosticsObserver, true>::Unchecked&
  GetDiagnosticsObserversForTesting();

 protected:
  // Invalidates the |access_token| issued for |account_id|, |client_id| and
  // |scopes|. Virtual so it can be overridden for tests.
  virtual void InvalidateAccessTokenImpl(const CoreAccountId& account_id,
                                         const std::string& client_id,
                                         const ScopeSet& scopes,
                                         const std::string& access_token);

 private:
  class Fetcher;
  friend class Fetcher;

  TokenCache& token_cache() { return token_cache_; }

  // Create an access token fetcher for the given account id.
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer);

  // This method does the same as |StartRequestWithContext| except it
  // uses |client_id| and |client_secret| to identify OAuth
  // client app instead of using Chrome's default values.
  std::unique_ptr<Request> StartRequestForClientWithContext(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& client_id,
      const std::string& client_secret,
      const ScopeSet& scopes,
      Consumer* consumer);

  // Posts a task to fire the Consumer callback with the cached token response.
  void InformConsumerWithCachedTokenResponse(
      const OAuth2AccessTokenConsumer::TokenResponse* token_response,
      RequestImpl* request,
      const RequestParameters& client_scopes);

  // Add a new entry to the cache.
  void RegisterTokenResponse(
      const std::string& client_id,
      const CoreAccountId& account_id,
      const ScopeSet& scopes,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response);

  // Removes an access token for the given set of scopes from the cache.
  // Returns true if the entry was removed, otherwise false.
  bool RemoveCachedTokenResponse(const RequestParameters& client_scopes,
                                 const std::string& token_to_remove);

  // Called when |fetcher| finishes fetching.
  void OnFetchComplete(Fetcher* fetcher);

  // Called when a number of fetchers need to be canceled.
  void CancelFetchers(std::vector<Fetcher*> fetchers_to_cancel);

  // The cache of currently valid tokens.
  TokenCache token_cache_;
  // List of observers to notify when access token status changes.
  base::ObserverList<DiagnosticsObserver, true>::Unchecked
      diagnostics_observer_list_;
  Delegate* delegate_;
  // A map from fetch parameters to a fetcher that is fetching an OAuth2 access
  // token using these parameters.
  std::map<RequestParameters, std::unique_ptr<Fetcher>> pending_fetchers_;
  // Maximum number of retries in fetching an OAuth2 access token.
  static int max_fetch_retry_num_;

  SEQUENCE_CHECKER(sequence_checker_);

  FRIEND_TEST_ALL_PREFIXES(OAuth2AccessTokenManagerTest, ClearCache);
  FRIEND_TEST_ALL_PREFIXES(OAuth2AccessTokenManagerTest, ClearCacheForAccount);
  FRIEND_TEST_ALL_PREFIXES(OAuth2AccessTokenManagerTest, OnAccessTokenRemoved);

  DISALLOW_COPY_AND_ASSIGN(OAuth2AccessTokenManager);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_MANAGER_H_
