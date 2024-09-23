// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_access_token_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {
void RecordOAuth2TokenFetchResult(GoogleServiceAuthError::State state) {
  UMA_HISTOGRAM_ENUMERATION("Signin.OAuth2TokenGetResult", state,
                            GoogleServiceAuthError::NUM_STATES);
}
}  // namespace

int OAuth2AccessTokenManager::max_fetch_retry_num_ = 5;

OAuth2AccessTokenManager::Delegate::Delegate() = default;

OAuth2AccessTokenManager::Delegate::~Delegate() = default;

scoped_refptr<network::SharedURLLoaderFactory>
OAuth2AccessTokenManager::Delegate::GetURLLoaderFactory() const {
  return nullptr;
}

bool OAuth2AccessTokenManager::Delegate::HandleAccessTokenFetch(
    RequestImpl* request,
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes) {
  return false;
}

OAuth2AccessTokenManager::Request::Request() {}

OAuth2AccessTokenManager::Request::~Request() {}

OAuth2AccessTokenManager::Consumer::Consumer(const std::string& id) : id_(id) {}

OAuth2AccessTokenManager::Consumer::~Consumer() {}

OAuth2AccessTokenManager::RequestImpl::RequestImpl(
    const CoreAccountId& account_id,
    Consumer* consumer)
    : account_id_(account_id), consumer_(consumer) {}

OAuth2AccessTokenManager::RequestImpl::~RequestImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

CoreAccountId OAuth2AccessTokenManager::RequestImpl::GetAccountId() const {
  return account_id_;
}

std::string OAuth2AccessTokenManager::RequestImpl::GetConsumerId() const {
  return consumer_->id();
}

void OAuth2AccessTokenManager::RequestImpl::InformConsumer(
    const GoogleServiceAuthError& error,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error.state() == GoogleServiceAuthError::NONE) {
    consumer_->OnGetTokenSuccess(this, token_response);
  } else {
    consumer_->OnGetTokenFailure(this, error);
  }
}

OAuth2AccessTokenManager::RequestParameters::RequestParameters(
    const std::string& client_id,
    const CoreAccountId& account_id,
    const ScopeSet& scopes)
    : client_id(client_id), account_id(account_id), scopes(scopes) {}

OAuth2AccessTokenManager::RequestParameters::RequestParameters(
    const RequestParameters& other) = default;

OAuth2AccessTokenManager::RequestParameters::~RequestParameters() {}

bool OAuth2AccessTokenManager::RequestParameters::operator<(
    const RequestParameters& p) const {
  if (client_id < p.client_id) {
    return true;
  } else if (p.client_id < client_id) {
    return false;
  }

  if (account_id < p.account_id) {
    return true;
  } else if (p.account_id < account_id) {
    return false;
  }

  return scopes < p.scopes;
}

// Class that fetches an OAuth2 access token for a given account id and set of
// scopes.
//
// It aims to meet OAuth2AccessTokenManager's requirements on token fetching.
// Retry mechanism is used to handle failures.
//
// To use this class, call CreateAndStart() to create and start a Fetcher.
//
// The Fetcher will call back the token manager by calling
// OAuth2AccessTokenManager::OnFetchComplete() when it completes fetching, if
// it is not destroyed before it completes fetching; if the Fetcher is destroyed
// before it completes fetching, the token manager will never be called back.
// The Fetcher destroys itself after calling back the manager when it finishes
// fetching.
//
// Requests that are waiting for the fetching results of this Fetcher can be
// added to the Fetcher by calling
// OAuth2AccessTokenManager::Fetcher::AddWaitingRequest() before the Fetcher
// completes fetching.
//
// The waiting requests are taken as weak pointers and they can be deleted.
//
// The OAuth2AccessTokenManager and the waiting requests will never be called
// back on the same turn of the message loop as when the fetcher is started,
// even if an immediate error occurred.
class OAuth2AccessTokenManager::Fetcher : public OAuth2AccessTokenConsumer {
 public:
  // Creates a Fetcher and starts fetching an OAuth2 access token for
  // |account_id| and |scopes| in the request context obtained by |getter|.
  // The given |oauth2_access_token_manager| will be informed when fetching is
  // done.
  static std::unique_ptr<OAuth2AccessTokenManager::Fetcher> CreateAndStart(
      OAuth2AccessTokenManager* oauth2_access_token_manager,
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& client_id,
      const std::string& client_secret,
      const ScopeSet& scopes,
      const std::string& consumer_name,
      base::WeakPtr<RequestImpl> waiting_request);

  Fetcher(const Fetcher&) = delete;
  Fetcher& operator=(const Fetcher&) = delete;

  ~Fetcher() override;

  // Add a request that is waiting for the result of this Fetcher.
  void AddWaitingRequest(base::WeakPtr<RequestImpl> waiting_request);

  // Returns count of waiting requests.
  size_t GetWaitingRequestCount() const;

  const std::vector<base::WeakPtr<RequestImpl>>& waiting_requests() const {
    return waiting_requests_;
  }

  std::vector<base::WeakPtr<RequestImpl>> TakeWaitingRequests();

 protected:
  // OAuth2AccessTokenConsumer
  void OnGetTokenSuccess(
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const GoogleServiceAuthError& error) override;
  std::string GetConsumerName() const override;

 private:
  Fetcher(OAuth2AccessTokenManager* oauth2_access_token_manager,
          const CoreAccountId& account_id,
          scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
          const std::string& client_id,
          const std::string& client_secret,
          const ScopeSet& scopes,
          const std::string& consumer_name,
          base::WeakPtr<RequestImpl> waiting_request);
  void Start();
  void InformFetchCompleted(
      base::expected<OAuth2AccessTokenConsumer::TokenResponse,
                     GoogleServiceAuthError> response);
  int64_t ComputeExponentialBackOffMilliseconds(int retry_num);

  // Attempts to retry the fetch if possible.  This is possible if the retry
  // count has not been exceeded.  Returns true if a retry has been restarted
  // and false otherwise.
  bool RetryIfPossible(const GoogleServiceAuthError& error);

  const raw_ptr<OAuth2AccessTokenManager> oauth2_access_token_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const CoreAccountId account_id_;
  const ScopeSet scopes_;
  const std::string consumer_name_;
  std::vector<base::WeakPtr<RequestImpl>> waiting_requests_;

  int retry_number_;
  base::OneShotTimer retry_timer_;
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher_;

  // Token binding challenge from the last server response or an empty string if
  // the response didn't contain any challenge.
  bool seen_token_binding_challenge_ = false;
  std::string token_binding_challenge_;

  // OAuth2 client id and secret.
  std::string client_id_;
  std::string client_secret_;
};

// static
std::unique_ptr<OAuth2AccessTokenManager::Fetcher>
OAuth2AccessTokenManager::Fetcher::CreateAndStart(
    OAuth2AccessTokenManager* oauth2_access_token_manager,
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes,
    const std::string& consumer_name,
    base::WeakPtr<RequestImpl> waiting_request) {
  std::unique_ptr<OAuth2AccessTokenManager::Fetcher> fetcher =
      base::WrapUnique(new Fetcher(oauth2_access_token_manager, account_id,
                                   url_loader_factory, client_id, client_secret,
                                   scopes, consumer_name, waiting_request));

  fetcher->Start();
  return fetcher;
}

OAuth2AccessTokenManager::Fetcher::Fetcher(
    OAuth2AccessTokenManager* oauth2_access_token_manager,
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes,
    const std::string& consumer_name,
    base::WeakPtr<RequestImpl> waiting_request)
    : oauth2_access_token_manager_(oauth2_access_token_manager),
      url_loader_factory_(url_loader_factory),
      account_id_(account_id),
      scopes_(scopes),
      consumer_name_(consumer_name),
      retry_number_(0),
      client_id_(client_id),
      client_secret_(client_secret) {
  DCHECK(oauth2_access_token_manager_);
  waiting_requests_.push_back(waiting_request);
}

OAuth2AccessTokenManager::Fetcher::~Fetcher() {
  // waiting requests should have been consumed.
  CHECK(waiting_requests_.empty());
  if (fetcher_) {
    fetcher_->CancelRequest();
  }
}

void OAuth2AccessTokenManager::Fetcher::Start() {
  fetcher_ = oauth2_access_token_manager_->CreateAccessTokenFetcher(
      account_id_, url_loader_factory_, this, token_binding_challenge_);
  DCHECK(fetcher_);

  // Stop the timer before starting the fetch, as defense in depth against the
  // fetcher calling us back synchronously (which might restart the timer).
  retry_timer_.Stop();
  fetcher_->Start(client_id_, client_secret_,
                  std::vector<std::string>(scopes_.begin(), scopes_.end()));
}

void OAuth2AccessTokenManager::Fetcher::OnGetTokenSuccess(
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  CHECK(fetcher_);
  OAuth2AccessTokenConsumer::TokenResponse fetcher_token_response =
      token_response;
  fetcher_.reset();

  RecordOAuth2TokenFetchResult(GoogleServiceAuthError::NONE);

  // Delegates may override this method to skip caching in some cases, but
  // we still inform all waiting Consumers of a successful token fetch below.
  // This is intentional -- some consumers may need the token for cleanup
  // tasks. https://chromiumcodereview.appspot.com/11312124/
  InformFetchCompleted(fetcher_token_response);
}

void OAuth2AccessTokenManager::Fetcher::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  CHECK(fetcher_);
  GoogleServiceAuthError fetcher_error = error;
  fetcher_.reset();

  if (fetcher_error.IsTransientError() && RetryIfPossible(fetcher_error)) {
    return;
  }

  RecordOAuth2TokenFetchResult(fetcher_error.state());
  InformFetchCompleted(base::unexpected(fetcher_error));
}

std::string OAuth2AccessTokenManager::Fetcher::GetConsumerName() const {
  return consumer_name_;
}

// Returns an exponential backoff in milliseconds including randomness less than
// 1000 ms when retrying fetching an OAuth2 access token.
int64_t
OAuth2AccessTokenManager::Fetcher::ComputeExponentialBackOffMilliseconds(
    int retry_num) {
  DCHECK(retry_num < oauth2_access_token_manager_->max_fetch_retry_num_);
  int exponential_backoff_in_seconds = 1 << retry_num;
  // Returns a backoff with randomness < 1000ms
  return (exponential_backoff_in_seconds + base::RandDouble()) * 1000;
}

bool OAuth2AccessTokenManager::Fetcher::RetryIfPossible(
    const GoogleServiceAuthError& error) {
  if (error.state() == GoogleServiceAuthError::CHALLENGE_RESPONSE_REQUIRED) {
    token_binding_challenge_ = error.GetTokenBindingChallenge();
    if (!seen_token_binding_challenge_) {
      seen_token_binding_challenge_ = true;
      // The server wants us to sign a challenge. Retry immediately if this is
      // the first attempt to pass a challenge.
      Start();
      return true;
    }
  } else {
    token_binding_challenge_.clear();
  }

  if (retry_number_ < oauth2_access_token_manager_->max_fetch_retry_num_) {
    base::TimeDelta backoff = base::Milliseconds(
        ComputeExponentialBackOffMilliseconds(retry_number_));
    ++retry_number_;
    UMA_HISTOGRAM_ENUMERATION("Signin.OAuth2TokenGetRetry", error.state(),
                              GoogleServiceAuthError::NUM_STATES);
    retry_timer_.Stop();
    retry_timer_.Start(FROM_HERE, backoff, this,
                       &OAuth2AccessTokenManager::Fetcher::Start);
    return true;
  }

  return false;
}

void OAuth2AccessTokenManager::Fetcher::InformFetchCompleted(
    base::expected<OAuth2AccessTokenConsumer::TokenResponse,
                   GoogleServiceAuthError> response) {
  oauth2_access_token_manager_->OnFetchComplete(client_id_, account_id_,
                                                scopes_, response);
  // `this` might be deleted.
}

std::vector<base::WeakPtr<OAuth2AccessTokenManager::RequestImpl>>
OAuth2AccessTokenManager::Fetcher::TakeWaitingRequests() {
  std::vector<base::WeakPtr<RequestImpl>> requests;
  std::swap(requests, waiting_requests_);
  return requests;
}

void OAuth2AccessTokenManager::Fetcher::AddWaitingRequest(
    base::WeakPtr<RequestImpl> waiting_request) {
  waiting_requests_.push_back(waiting_request);
}

size_t OAuth2AccessTokenManager::Fetcher::GetWaitingRequestCount() const {
  return waiting_requests_.size();
}

OAuth2AccessTokenManager::OAuth2AccessTokenManager(
    OAuth2AccessTokenManager::Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

OAuth2AccessTokenManager::~OAuth2AccessTokenManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CancelAllRequests(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
}

OAuth2AccessTokenManager::Delegate* OAuth2AccessTokenManager::GetDelegate() {
  return delegate_;
}

const OAuth2AccessTokenManager::Delegate*
OAuth2AccessTokenManager::GetDelegate() const {
  return delegate_;
}

void OAuth2AccessTokenManager::AddDiagnosticsObserver(
    DiagnosticsObserver* observer) {
  diagnostics_observer_list_.AddObserver(observer);
}

void OAuth2AccessTokenManager::RemoveDiagnosticsObserver(
    DiagnosticsObserver* observer) {
  diagnostics_observer_list_.RemoveObserver(observer);
}

std::unique_ptr<OAuth2AccessTokenManager::Request>
OAuth2AccessTokenManager::StartRequest(const CoreAccountId& account_id,
                                       const ScopeSet& scopes,
                                       Consumer* consumer) {
  return StartRequestForClientWithContext(
      account_id, delegate_->GetURLLoaderFactory(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(), scopes, consumer);
}

std::unique_ptr<OAuth2AccessTokenManager::Request>
OAuth2AccessTokenManager::StartRequestForClient(
    const CoreAccountId& account_id,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes,
    Consumer* consumer) {
  return StartRequestForClientWithContext(
      account_id, delegate_->GetURLLoaderFactory(), client_id, client_secret,
      scopes, consumer);
}

std::unique_ptr<OAuth2AccessTokenManager::Request>
OAuth2AccessTokenManager::StartRequestWithContext(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const ScopeSet& scopes,
    Consumer* consumer) {
  return StartRequestForClientWithContext(
      account_id, url_loader_factory,
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(), scopes, consumer);
}

void OAuth2AccessTokenManager::FetchOAuth2Token(
    RequestImpl* request,
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const std::string& consumer_name,
    const ScopeSet& scopes) {
  // If there is already a pending fetcher for |scopes| and |account_id|,
  // simply register this |request| for those results rather than starting
  // a new fetcher.
  RequestParameters request_parameters =
      RequestParameters(client_id, account_id, scopes);
  auto iter = pending_fetchers_.find(request_parameters);
  if (iter != pending_fetchers_.end()) {
    iter->second->AddWaitingRequest(request->AsWeakPtr());
    return;
  }

  pending_fetchers_[request_parameters] = Fetcher::CreateAndStart(
      this, account_id, url_loader_factory, client_id, client_secret, scopes,
      consumer_name, request->AsWeakPtr());
}

const OAuth2AccessTokenConsumer::TokenResponse*
OAuth2AccessTokenManager::GetCachedTokenResponse(
    const RequestParameters& request_parameters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TokenCache::iterator token_iterator = token_cache_.find(request_parameters);
  if (token_iterator == token_cache_.end()) {
    return nullptr;
  }
  if (token_iterator->second.expiration_time <= base::Time::Now()) {
    token_cache_.erase(token_iterator);
    return nullptr;
  }
  return &token_iterator->second;
}

void OAuth2AccessTokenManager::ClearCacheForAccount(
    const CoreAccountId& account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (TokenCache::iterator iter = token_cache_.begin();
       iter != token_cache_.end();
       /* iter incremented in body */) {
    if (iter->first.account_id == account_id) {
      for (auto& observer : diagnostics_observer_list_) {
        observer.OnAccessTokenRemoved(account_id, iter->first.scopes);
      }
      token_cache_.erase(iter++);
    } else {
      ++iter;
    }
  }
}

void OAuth2AccessTokenManager::CancelAllRequests(
    const GoogleServiceAuthError& error) {
  // Match all requests.
  CancelRequestIfMatch(
      error, base::BindRepeating(
                 [](const RequestParameters& params) -> bool { return true; }));
}

void OAuth2AccessTokenManager::CancelRequestsForAccount(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error) {
  CancelRequestIfMatch(error, base::BindRepeating(
                                  [](const CoreAccountId& account_id,
                                     const RequestParameters& params) -> bool {
                                    return params.account_id == account_id;
                                  },
                                  account_id));
}

void OAuth2AccessTokenManager::CancelRequestIfMatch(
    const GoogleServiceAuthError& error,
    base::RepeatingCallback<bool(const RequestParameters&)> match_request) {
  for (auto it = pending_fetchers_.begin(); it != pending_fetchers_.end();) {
    if (match_request.Run(it->first)) {
      RequestParameters request_parameters = it->first;
      auto waiting_requests = it->second->TakeWaitingRequests();
      it = pending_fetchers_.erase(it);
      ProcessOnFetchComplete(request_parameters, base::unexpected(error),
                             waiting_requests);
    } else {
      ++it;
    }
  }
}

void OAuth2AccessTokenManager::InvalidateAccessToken(
    const CoreAccountId& account_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  InvalidateAccessTokenImpl(account_id,
                            GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
                            scopes, access_token);
}

void OAuth2AccessTokenManager::InvalidateAccessTokenImpl(
    const CoreAccountId& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveCachedTokenResponse(RequestParameters(client_id, account_id, scopes),
                            access_token);
  delegate_->OnAccessTokenInvalidated(account_id, client_id, scopes,
                                      access_token);
}

void OAuth2AccessTokenManager::
    set_max_authorization_token_fetch_retries_for_testing(int max_retries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  max_fetch_retry_num_ = max_retries;
}

size_t OAuth2AccessTokenManager::GetNumPendingRequestsForTesting(
    const std::string& client_id,
    const CoreAccountId& account_id,
    const ScopeSet& scopes) const {
  auto iter =
      pending_fetchers_.find(RequestParameters(client_id, account_id, scopes));
  return iter == pending_fetchers_.end()
             ? 0
             : iter->second->GetWaitingRequestCount();
}

const base::ObserverList<OAuth2AccessTokenManager::DiagnosticsObserver,
                         true>::Unchecked&
OAuth2AccessTokenManager::GetDiagnosticsObserversForTesting() {
  return diagnostics_observer_list_;
}

std::unique_ptr<OAuth2AccessTokenFetcher>
OAuth2AccessTokenManager::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer,
    const std::string& token_binding_challenge) {
  return delegate_->CreateAccessTokenFetcher(account_id, url_loader_factory,
                                             consumer, token_binding_challenge);
}

std::unique_ptr<OAuth2AccessTokenManager::Request>
OAuth2AccessTokenManager::StartRequestForClientWithContext(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes,
    Consumer* consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<RequestImpl> request(new RequestImpl(account_id, consumer));
  for (auto& observer : diagnostics_observer_list_) {
    observer.OnAccessTokenRequested(account_id, consumer->id(), scopes);
  }

  if (!delegate_->HasRefreshToken(account_id)) {
    GoogleServiceAuthError error(GoogleServiceAuthError::USER_NOT_SIGNED_UP);

    for (auto& observer : diagnostics_observer_list_) {
      observer.OnFetchAccessTokenComplete(account_id, consumer->id(), scopes,
                                          error, base::Time());
    }

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&RequestImpl::InformConsumer, request->AsWeakPtr(),
                       error, OAuth2AccessTokenConsumer::TokenResponse()));
    return std::move(request);
  }

  RequestParameters request_parameters(client_id, account_id, scopes);
  const OAuth2AccessTokenConsumer::TokenResponse* token_response =
      GetCachedTokenResponse(request_parameters);
  if (token_response && token_response->access_token.length()) {
    InformConsumerWithCachedTokenResponse(token_response, request.get(),
                                          request_parameters);
  } else if (delegate_->HandleAccessTokenFetch(request.get(), account_id,
                                               url_loader_factory, client_id,
                                               client_secret, scopes)) {
    // The delegate handling the fetch request means that we *don't* perform a
    // fetch.
  } else {
    // The token isn't in the cache and the delegate isn't fetching it: fetch it
    // ourselves!
    FetchOAuth2Token(request.get(), account_id, url_loader_factory, client_id,
                     client_secret, consumer->id(), scopes);
  }
  return std::move(request);
}

void OAuth2AccessTokenManager::InformConsumerWithCachedTokenResponse(
    const OAuth2AccessTokenConsumer::TokenResponse* cache_token_response,
    RequestImpl* request,
    const RequestParameters& request_parameters) {
  DCHECK(cache_token_response && cache_token_response->access_token.length());
  for (auto& observer : diagnostics_observer_list_) {
    observer.OnFetchAccessTokenComplete(
        request_parameters.account_id, request->GetConsumerId(),
        request_parameters.scopes, GoogleServiceAuthError::AuthErrorNone(),
        cache_token_response->expiration_time);
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&RequestImpl::InformConsumer, request->AsWeakPtr(),
                     GoogleServiceAuthError(GoogleServiceAuthError::NONE),
                     *cache_token_response));
}

bool OAuth2AccessTokenManager::RemoveCachedTokenResponse(
    const RequestParameters& request_parameters,
    const std::string& token_to_remove) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TokenCache::iterator token_iterator = token_cache_.find(request_parameters);
  if (token_iterator != token_cache_.end() &&
      token_iterator->second.access_token == token_to_remove) {
    for (auto& observer : diagnostics_observer_list_) {
      observer.OnAccessTokenRemoved(request_parameters.account_id,
                                    request_parameters.scopes);
    }
    token_cache_.erase(token_iterator);
    return true;
  }
  return false;
}

void OAuth2AccessTokenManager::OnFetchComplete(
    const std::string& client_id,
    const CoreAccountId& account_id,
    const ScopeSet& scopes,
    base::expected<OAuth2AccessTokenConsumer::TokenResponse,
                   GoogleServiceAuthError> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note |fetcher| is recorded in |pending_fetcher_| mapped from its
  // combination of client ID, account ID, and scope set. This is guaranteed as
  // follows; here a Fetcher is said to be uncompleted if it has not finished
  // calling back
  // OAuth2AccessTokenManager::OnFetchComplete().
  //
  // (1) All the live Fetchers are created by this manager.
  //     This is because (1) all the live Fetchers are created by a live
  //     manager, as all the fetchers created by a manager are destructed in the
  //     manager's dtor.
  //
  // (2) All the uncompleted Fetchers created by this manager are recorded in
  //     |pending_fetchers_|.
  //     This is because (1) all the created Fetchers are added to
  //     |pending_fetchers_| (in method StartRequest()) and (2) method
  //     OnFetchComplete() is the only place where a Fetcher is erased from
  //     |pending_fetchers_|. Note no Fetcher is erased in method
  //     StartRequest().
  //
  // (3) Each of the Fetchers recorded in |pending_fetchers_| is mapped from
  //     its combination of client ID, account ID, and scope set. This is
  //     guaranteed by Fetcher creation in method StartRequest().
  //
  // When this method is called, |fetcher| is alive and uncompleted.
  // By (1), |fetcher| is created by this manager.
  // Then by (2), |fetcher| is recorded in |pending_fetchers_|.
  // Then by (3), |fetcher_| is mapped from its combination of client ID,
  // account ID, and scope set.
  RequestParameters request_parameters(client_id, account_id, scopes);
  auto iter = pending_fetchers_.find(request_parameters);
  CHECK(iter != pending_fetchers_.end());

  auto waiting_requests = iter->second->TakeWaitingRequests();
  pending_fetchers_.erase(iter);
  ProcessOnFetchComplete(request_parameters, response, waiting_requests);
}

void OAuth2AccessTokenManager::ProcessOnFetchComplete(
    const RequestParameters& request_parameters,
    base::expected<OAuth2AccessTokenConsumer::TokenResponse,
                   GoogleServiceAuthError> response,
    const std::vector<base::WeakPtr<OAuth2AccessTokenManager::RequestImpl>>&
        waiting_requests) {
  GoogleServiceAuthError error;
  OAuth2AccessTokenConsumer::TokenResponse token_response;
  if (response.has_value()) {
    token_response = std::move(response).value();
    token_cache_[request_parameters] = token_response;
    error = GoogleServiceAuthError::AuthErrorNone();
  } else {
    error = std::move(response).error();
  }

  delegate_->OnAccessTokenFetched(request_parameters.account_id, error);

  const OAuth2AccessTokenConsumer::TokenResponse* entry =
      GetCachedTokenResponse(request_parameters);
  for (const base::WeakPtr<RequestImpl>& req : waiting_requests) {
    if (req) {
      for (auto& observer : diagnostics_observer_list_) {
        observer.OnFetchAccessTokenComplete(
            request_parameters.account_id, req->GetConsumerId(),
            request_parameters.scopes, error,
            entry ? entry->expiration_time : base::Time());
      }
    }
  }

  for (const base::WeakPtr<RequestImpl>& request : waiting_requests) {
    if (request) {
      request->InformConsumer(error, token_response);
    }
  }
}
