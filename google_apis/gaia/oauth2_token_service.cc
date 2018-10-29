// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_token_service.h"

#include <stdint.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

int OAuth2TokenService::max_fetch_retry_num_ = 5;

OAuth2TokenService::RequestParameters::RequestParameters(
    const std::string& client_id,
    const std::string& account_id,
    const ScopeSet& scopes)
    : client_id(client_id),
      account_id(account_id),
      scopes(scopes) {
}

OAuth2TokenService::RequestParameters::RequestParameters(
    const RequestParameters& other) = default;

OAuth2TokenService::RequestParameters::~RequestParameters() {
}

bool OAuth2TokenService::RequestParameters::operator<(
    const RequestParameters& p) const {
  if (client_id < p.client_id)
    return true;
  else if (p.client_id < client_id)
    return false;

  if (account_id < p.account_id)
    return true;
  else if (p.account_id < account_id)
    return false;

  return scopes < p.scopes;
}

OAuth2TokenService::RequestImpl::RequestImpl(
    const std::string& account_id,
    OAuth2TokenService::Consumer* consumer)
    : account_id_(account_id),
      consumer_(consumer) {
}

OAuth2TokenService::RequestImpl::~RequestImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::string OAuth2TokenService::RequestImpl::GetAccountId() const {
  return account_id_;
}

std::string OAuth2TokenService::RequestImpl::GetConsumerId() const {
  return consumer_->id();
}

void OAuth2TokenService::RequestImpl::InformConsumer(
    const GoogleServiceAuthError& error,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error.state() == GoogleServiceAuthError::NONE)
    consumer_->OnGetTokenSuccess(this, token_response);
  else
    consumer_->OnGetTokenFailure(this, error);
}

// Class that fetches an OAuth2 access token for a given account id and set of
// scopes.
//
// It aims to meet OAuth2TokenService's requirements on token fetching. Retry
// mechanism is used to handle failures.
//
// To use this class, call CreateAndStart() to create and start a Fetcher.
//
// The Fetcher will call back the service by calling
// OAuth2TokenService::OnFetchComplete() when it completes fetching, if it is
// not destroyed before it completes fetching; if the Fetcher is destroyed
// before it completes fetching, the service will never be called back. The
// Fetcher destroys itself after calling back the service when it finishes
// fetching.
//
// Requests that are waiting for the fetching results of this Fetcher can be
// added to the Fetcher by calling
// OAuth2TokenService::Fetcher::AddWaitingRequest() before the Fetcher
// completes fetching.
//
// The waiting requests are taken as weak pointers and they can be deleted.
// They will be called back with the result when either the Fetcher completes
// fetching or is destroyed, whichever comes first. In the latter case, the
// waiting requests will be called back with an error.
//
// The OAuth2TokenService and the waiting requests will never be called back on
// the same turn of the message loop as when the fetcher is started, even if an
// immediate error occurred.
class OAuth2TokenService::Fetcher : public OAuth2AccessTokenConsumer {
 public:
  // Creates a Fetcher and starts fetching an OAuth2 access token for
  // |account_id| and |scopes| in the request context obtained by |getter|.
  // The given |oauth2_token_service| will be informed when fetching is done.
  static std::unique_ptr<OAuth2TokenService::Fetcher> CreateAndStart(
      OAuth2TokenService* oauth2_token_service,
      const std::string& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& client_id,
      const std::string& client_secret,
      const ScopeSet& scopes,
      base::WeakPtr<RequestImpl> waiting_request);
  ~Fetcher() override;

  // Add a request that is waiting for the result of this Fetcher.
  void AddWaitingRequest(base::WeakPtr<RequestImpl> waiting_request);

  // Returns count of waiting requests.
  size_t GetWaitingRequestCount() const;

  const std::vector<base::WeakPtr<RequestImpl> >& waiting_requests() const {
    return waiting_requests_;
  }

  void Cancel();

  const ScopeSet& GetScopeSet() const;
  const std::string& GetClientId() const;
  const std::string& GetAccountId() const;

  // The error result from this fetcher.
  const GoogleServiceAuthError& error() const { return error_; }

 protected:
  // OAuth2AccessTokenConsumer
  void OnGetTokenSuccess(
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const GoogleServiceAuthError& error) override;

 private:
  Fetcher(OAuth2TokenService* oauth2_token_service,
          const std::string& account_id,
          scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
          const std::string& client_id,
          const std::string& client_secret,
          const OAuth2TokenService::ScopeSet& scopes,
          base::WeakPtr<RequestImpl> waiting_request);
  void Start();
  void InformWaitingRequests();
  void InformWaitingRequestsAndDelete();
  static bool ShouldRetry(const GoogleServiceAuthError& error);
  int64_t ComputeExponentialBackOffMilliseconds(int retry_num);

  // |oauth2_token_service_| remains valid for the life of this Fetcher, since
  // this Fetcher is destructed in the dtor of the OAuth2TokenService or is
  // scheduled for deletion at the end of OnGetTokenFailure/OnGetTokenSuccess
  // (whichever comes first).
  OAuth2TokenService* const oauth2_token_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::string account_id_;
  const ScopeSet scopes_;
  std::vector<base::WeakPtr<RequestImpl> > waiting_requests_;

  int retry_number_;
  base::OneShotTimer retry_timer_;
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher_;

  // Variables that store fetch results.
  // Initialized to be GoogleServiceAuthError::SERVICE_UNAVAILABLE to handle
  // destruction.
  GoogleServiceAuthError error_;
  OAuth2AccessTokenConsumer::TokenResponse token_response_;

  // OAuth2 client id and secret.
  std::string client_id_;
  std::string client_secret_;

  DISALLOW_COPY_AND_ASSIGN(Fetcher);
};

// static
std::unique_ptr<OAuth2TokenService::Fetcher>
OAuth2TokenService::Fetcher::CreateAndStart(
    OAuth2TokenService* oauth2_token_service,
    const std::string& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const OAuth2TokenService::ScopeSet& scopes,
    base::WeakPtr<RequestImpl> waiting_request) {
  std::unique_ptr<OAuth2TokenService::Fetcher> fetcher = base::WrapUnique(
      new Fetcher(oauth2_token_service, account_id, url_loader_factory,
                  client_id, client_secret, scopes, waiting_request));

  fetcher->Start();
  return fetcher;
}

OAuth2TokenService::Fetcher::Fetcher(
    OAuth2TokenService* oauth2_token_service,
    const std::string& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const OAuth2TokenService::ScopeSet& scopes,
    base::WeakPtr<RequestImpl> waiting_request)
    : oauth2_token_service_(oauth2_token_service),
      url_loader_factory_(url_loader_factory),
      account_id_(account_id),
      scopes_(scopes),
      retry_number_(0),
      error_(GoogleServiceAuthError::SERVICE_UNAVAILABLE),
      client_id_(client_id),
      client_secret_(client_secret) {
  DCHECK(oauth2_token_service_);
  waiting_requests_.push_back(waiting_request);
}

OAuth2TokenService::Fetcher::~Fetcher() {
  // Inform the waiting requests if it has not done so.
  if (waiting_requests_.size())
    InformWaitingRequests();
}

void OAuth2TokenService::Fetcher::Start() {
  fetcher_.reset(oauth2_token_service_->CreateAccessTokenFetcher(
      account_id_, url_loader_factory_, this));
  DCHECK(fetcher_);

  // Stop the timer before starting the fetch, as defense in depth against the
  // fetcher calling us back synchronously (which might restart the timer).
  retry_timer_.Stop();
  fetcher_->Start(client_id_,
                  client_secret_,
                  std::vector<std::string>(scopes_.begin(), scopes_.end()));
}

void OAuth2TokenService::Fetcher::OnGetTokenSuccess(
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  fetcher_.reset();

  // Fetch completes.
  error_ = GoogleServiceAuthError::AuthErrorNone();
  token_response_ = token_response;

  // Subclasses may override this method to skip caching in some cases, but
  // we still inform all waiting Consumers of a successful token fetch below.
  // This is intentional -- some consumers may need the token for cleanup
  // tasks. https://chromiumcodereview.appspot.com/11312124/
  oauth2_token_service_->RegisterTokenResponse(client_id_, account_id_, scopes_,
                                               token_response_);
  InformWaitingRequestsAndDelete();
}

void OAuth2TokenService::Fetcher::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  fetcher_.reset();

  if (ShouldRetry(error) && retry_number_ < max_fetch_retry_num_) {
    base::TimeDelta backoff = base::TimeDelta::FromMilliseconds(
        ComputeExponentialBackOffMilliseconds(retry_number_));
    ++retry_number_;
    UMA_HISTOGRAM_ENUMERATION("Signin.OAuth2TokenGetRetry",
        error.state(), GoogleServiceAuthError::NUM_STATES);
    retry_timer_.Stop();
    retry_timer_.Start(FROM_HERE,
                       backoff,
                       this,
                       &OAuth2TokenService::Fetcher::Start);
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("Signin.OAuth2TokenGetFailure",
      error.state(), GoogleServiceAuthError::NUM_STATES);
  error_ = error;
  InformWaitingRequestsAndDelete();
}

// Returns an exponential backoff in milliseconds including randomness less than
// 1000 ms when retrying fetching an OAuth2 access token.
int64_t OAuth2TokenService::Fetcher::ComputeExponentialBackOffMilliseconds(
    int retry_num) {
  DCHECK(retry_num < max_fetch_retry_num_);
  int exponential_backoff_in_seconds = 1 << retry_num;
  // Returns a backoff with randomness < 1000ms
  return (exponential_backoff_in_seconds + base::RandDouble()) * 1000;
}

// static
bool OAuth2TokenService::Fetcher::ShouldRetry(
    const GoogleServiceAuthError& error) {
  GoogleServiceAuthError::State error_state = error.state();
  return error_state == GoogleServiceAuthError::CONNECTION_FAILED ||
         error_state == GoogleServiceAuthError::REQUEST_CANCELED ||
         error_state == GoogleServiceAuthError::SERVICE_UNAVAILABLE;
}

void OAuth2TokenService::Fetcher::InformWaitingRequests() {
  for (const base::WeakPtr<RequestImpl>& request : waiting_requests_) {
    if (request)
      request->InformConsumer(error_, token_response_);
  }
  waiting_requests_.clear();
}

void OAuth2TokenService::Fetcher::InformWaitingRequestsAndDelete() {
  // Deregisters itself from the service to prevent more waiting requests to
  // be added when it calls back the waiting requests.
  oauth2_token_service_->OnFetchComplete(this);
  InformWaitingRequests();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void OAuth2TokenService::Fetcher::AddWaitingRequest(
    base::WeakPtr<OAuth2TokenService::RequestImpl> waiting_request) {
  waiting_requests_.push_back(waiting_request);
}

size_t OAuth2TokenService::Fetcher::GetWaitingRequestCount() const {
  return waiting_requests_.size();
}

void OAuth2TokenService::Fetcher::Cancel() {
  if (fetcher_)
    fetcher_->CancelRequest();
  fetcher_.reset();
  retry_timer_.Stop();
  error_ = GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
  InformWaitingRequestsAndDelete();
}

const OAuth2TokenService::ScopeSet& OAuth2TokenService::Fetcher::GetScopeSet()
    const {
  return scopes_;
}

const std::string& OAuth2TokenService::Fetcher::GetClientId() const {
  return client_id_;
}

const std::string& OAuth2TokenService::Fetcher::GetAccountId() const {
  return account_id_;
}

OAuth2TokenService::Request::Request() {
}

OAuth2TokenService::Request::~Request() {
}

OAuth2TokenService::Consumer::Consumer(const std::string& id)
    : id_(id) {}

OAuth2TokenService::Consumer::~Consumer() {
}

OAuth2TokenService::OAuth2TokenService(
    std::unique_ptr<OAuth2TokenServiceDelegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK(delegate_);
}

OAuth2TokenService::~OAuth2TokenService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Release all the pending fetchers.
  pending_fetchers_.clear();
}

OAuth2TokenServiceDelegate* OAuth2TokenService::GetDelegate() {
  return delegate_.get();
}

const OAuth2TokenServiceDelegate* OAuth2TokenService::GetDelegate() const {
  return delegate_.get();
}

void OAuth2TokenService::AddObserver(Observer* observer) {
  delegate_->AddObserver(observer);
}

void OAuth2TokenService::RemoveObserver(Observer* observer) {
  delegate_->RemoveObserver(observer);
}

void OAuth2TokenService::AddDiagnosticsObserver(DiagnosticsObserver* observer) {
  diagnostics_observer_list_.AddObserver(observer);
}

void OAuth2TokenService::RemoveDiagnosticsObserver(
    DiagnosticsObserver* observer) {
  diagnostics_observer_list_.RemoveObserver(observer);
}

std::unique_ptr<OAuth2TokenService::Request>
OAuth2TokenService::StartRequestForMultilogin(
    const std::string& account_id,
    OAuth2TokenService::Consumer* consumer) {
  const std::string refresh_token =
      delegate_->GetTokenForMultilogin(account_id);
  if (refresh_token.empty()) {
    // If we can't get refresh token from the delegate, start request for access
    // token.
    OAuth2TokenService::ScopeSet scopes;
    scopes.insert(GaiaConstants::kOAuth1LoginScope);
    return StartRequest(account_id, scopes, consumer);
  }
  std::unique_ptr<RequestImpl> request(new RequestImpl(account_id, consumer));
  // Create token response from token. Expiration time and id token do not
  // matter and should not be accessed.
  OAuth2AccessTokenConsumer::TokenResponse token_response(
      refresh_token, base::Time(), std::string());
  // If we can get refresh token from the delegate, inform cosumer right away.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&RequestImpl::InformConsumer, request.get()->AsWeakPtr(),
                 GoogleServiceAuthError(GoogleServiceAuthError::NONE),
                 token_response));
  return std::move(request);
}

std::unique_ptr<OAuth2TokenService::Request> OAuth2TokenService::StartRequest(
    const std::string& account_id,
    const OAuth2TokenService::ScopeSet& scopes,
    OAuth2TokenService::Consumer* consumer) {
  return StartRequestForClientWithContext(
      account_id, delegate_->GetURLLoaderFactory(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(), scopes, consumer);
}

std::unique_ptr<OAuth2TokenService::Request>
OAuth2TokenService::StartRequestForClient(
    const std::string& account_id,
    const std::string& client_id,
    const std::string& client_secret,
    const OAuth2TokenService::ScopeSet& scopes,
    OAuth2TokenService::Consumer* consumer) {
  return StartRequestForClientWithContext(account_id, GetURLLoaderFactory(),
                                          client_id, client_secret, scopes,
                                          consumer);
}

scoped_refptr<network::SharedURLLoaderFactory>
OAuth2TokenService::GetURLLoaderFactory() const {
  return delegate_->GetURLLoaderFactory();
}

std::unique_ptr<OAuth2TokenService::Request>
OAuth2TokenService::StartRequestWithContext(
    const std::string& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const ScopeSet& scopes,
    Consumer* consumer) {
  return StartRequestForClientWithContext(
      account_id, url_loader_factory,
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(), scopes, consumer);
}

std::unique_ptr<OAuth2TokenService::Request>
OAuth2TokenService::StartRequestForClientWithContext(
    const std::string& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes,
    Consumer* consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<RequestImpl> request(new RequestImpl(account_id, consumer));
  for (auto& observer : diagnostics_observer_list_)
    observer.OnAccessTokenRequested(account_id, consumer->id(), scopes);

  if (!RefreshTokenIsAvailable(account_id)) {
    GoogleServiceAuthError error(GoogleServiceAuthError::USER_NOT_SIGNED_UP);

    for (auto& observer : diagnostics_observer_list_) {
      observer.OnFetchAccessTokenComplete(account_id, consumer->id(), scopes,
                                          error, base::Time());
    }

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&RequestImpl::InformConsumer, request->AsWeakPtr(), error,
                   OAuth2AccessTokenConsumer::TokenResponse()));
    return std::move(request);
  }

  RequestParameters request_parameters(client_id,
                                       account_id,
                                       scopes);
  const OAuth2AccessTokenConsumer::TokenResponse* token_response =
      GetCachedTokenResponse(request_parameters);
  if (token_response && token_response->access_token.length()) {
    InformConsumerWithCachedTokenResponse(token_response, request.get(),
                                          request_parameters);
  } else {
    FetchOAuth2Token(request.get(), account_id, url_loader_factory, client_id,
                     client_secret, scopes);
  }
  return std::move(request);
}

void OAuth2TokenService::FetchOAuth2Token(
    RequestImpl* request,
    const std::string& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes) {
  // If there is already a pending fetcher for |scopes| and |account_id|,
  // simply register this |request| for those results rather than starting
  // a new fetcher.
  RequestParameters request_parameters = RequestParameters(client_id,
                                                           account_id,
                                                           scopes);
  auto iter = pending_fetchers_.find(request_parameters);
  if (iter != pending_fetchers_.end()) {
    iter->second->AddWaitingRequest(request->AsWeakPtr());
    return;
  }

  pending_fetchers_[request_parameters] =
      Fetcher::CreateAndStart(this, account_id, url_loader_factory, client_id,
                              client_secret, scopes, request->AsWeakPtr());
}

OAuth2AccessTokenFetcher* OAuth2TokenService::CreateAccessTokenFetcher(
    const std::string& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) {
  return delegate_->CreateAccessTokenFetcher(account_id, url_loader_factory,
                                             consumer);
}

void OAuth2TokenService::InformConsumerWithCachedTokenResponse(
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&RequestImpl::InformConsumer, request->AsWeakPtr(),
                 GoogleServiceAuthError(GoogleServiceAuthError::NONE),
                 *cache_token_response));
}

std::vector<std::string> OAuth2TokenService::GetAccounts() const {
  return delegate_->GetAccounts();
}

bool OAuth2TokenService::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  return delegate_->RefreshTokenIsAvailable(account_id);
}

bool OAuth2TokenService::RefreshTokenHasError(
    const std::string& account_id) const {
  return GetAuthError(account_id) != GoogleServiceAuthError::AuthErrorNone();
}

GoogleServiceAuthError OAuth2TokenService::GetAuthError(
    const std::string& account_id) const {
  GoogleServiceAuthError error = delegate_->GetAuthError(account_id);
  DCHECK(!error.IsTransientError());
  return error;
}

void OAuth2TokenService::RevokeAllCredentials() {
  CancelAllRequests();
  ClearCache();
  delegate_->RevokeAllCredentials();
}

void OAuth2TokenService::InvalidateAccessToken(
    const std::string& account_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  InvalidateAccessTokenImpl(account_id,
                            GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
                            scopes, access_token);
}

void OAuth2TokenService::InvalidateTokenForMultilogin(
    const std::string& failed_account,
    const std::string& token) {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);
  // Remove from cache. This will have no effect on desktop since token is a
  // refresh token and is not in cache.
  InvalidateAccessToken(failed_account, scopes, token);
  // For desktop refresh tokens can be invalidated directly in delegate. This
  // will have no effect on mobile.
  delegate_->InvalidateTokenForMultilogin(failed_account);
}

void OAuth2TokenService::InvalidateAccessTokenForClient(
    const std::string& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  InvalidateAccessTokenImpl(account_id, client_id, scopes, access_token);
}

void OAuth2TokenService::InvalidateAccessTokenImpl(
    const std::string& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveCachedTokenResponse(RequestParameters(client_id, account_id, scopes),
                            access_token);
  delegate_->InvalidateAccessToken(account_id, client_id, scopes, access_token);
}

void OAuth2TokenService::OnFetchComplete(Fetcher* fetcher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Update the auth error state so auth errors are appropriately communicated
  // to the user.
  UpdateAuthError(fetcher->GetAccountId(), fetcher->error());

  // Note |fetcher| is recorded in |pending_fetcher_| mapped to its refresh
  // token and scope set. This is guaranteed as follows; here a Fetcher is said
  // to be uncompleted if it has not finished calling back
  // OAuth2TokenService::OnFetchComplete().
  //
  // (1) All the live Fetchers are created by this service.
  //     This is because (1) all the live Fetchers are created by a live
  //     service, as all the fetchers created by a service are destructed in the
  //     service's dtor.
  //
  // (2) All the uncompleted Fetchers created by this service are recorded in
  //     |pending_fetchers_|.
  //     This is because (1) all the created Fetchers are added to
  //     |pending_fetchers_| (in method StartRequest()) and (2) method
  //     OnFetchComplete() is the only place where a Fetcher is erased from
  //     |pending_fetchers_|. Note no Fetcher is erased in method
  //     StartRequest().
  //
  // (3) Each of the Fetchers recorded in |pending_fetchers_| is mapped to its
  //     refresh token and ScopeSet. This is guaranteed by Fetcher creation in
  //     method StartRequest().
  //
  // When this method is called, |fetcher| is alive and uncompleted.
  // By (1), |fetcher| is created by this service.
  // Then by (2), |fetcher| is recorded in |pending_fetchers_|.
  // Then by (3), |fetcher_| is mapped to its refresh token and ScopeSet.
  RequestParameters request_param(fetcher->GetClientId(),
                                  fetcher->GetAccountId(),
                                  fetcher->GetScopeSet());

  const OAuth2AccessTokenConsumer::TokenResponse* entry =
      GetCachedTokenResponse(request_param);
  for (const base::WeakPtr<RequestImpl>& req : fetcher->waiting_requests()) {
    if (req) {
      for (auto& observer : diagnostics_observer_list_) {
        observer.OnFetchAccessTokenComplete(
            req->GetAccountId(), req->GetConsumerId(), fetcher->GetScopeSet(),
            fetcher->error(), entry ? entry->expiration_time : base::Time());
      }
    }
  }

  auto iter = pending_fetchers_.find(request_param);
  DCHECK(iter != pending_fetchers_.end());
  DCHECK_EQ(fetcher, iter->second.get());

  // The Fetcher deletes itself.
  iter->second.release();
  pending_fetchers_.erase(iter);
}

const OAuth2AccessTokenConsumer::TokenResponse*
OAuth2TokenService::GetCachedTokenResponse(
    const RequestParameters& request_parameters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TokenCache::iterator token_iterator = token_cache_.find(request_parameters);
  if (token_iterator == token_cache_.end())
    return NULL;
  if (token_iterator->second.expiration_time <= base::Time::Now()) {
    token_cache_.erase(token_iterator);
    return NULL;
  }
  return &token_iterator->second;
}

bool OAuth2TokenService::RemoveCachedTokenResponse(
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
void OAuth2TokenService::UpdateAuthError(const std::string& account_id,
                                         const GoogleServiceAuthError& error) {
  delegate_->UpdateAuthError(account_id, error);
}

void OAuth2TokenService::RegisterTokenResponse(
    const std::string& client_id,
    const std::string& account_id,
    const ScopeSet& scopes,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  token_cache_[RequestParameters(client_id, account_id, scopes)] =
      token_response;
}

void OAuth2TokenService::ClearCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& entry : token_cache_) {
    for (auto& observer : diagnostics_observer_list_)
      observer.OnAccessTokenRemoved(entry.first.account_id, entry.first.scopes);
  }

  token_cache_.clear();
}

void OAuth2TokenService::ClearCacheForAccount(const std::string& account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (TokenCache::iterator iter = token_cache_.begin();
       iter != token_cache_.end();
       /* iter incremented in body */) {
    if (iter->first.account_id == account_id) {
      for (auto& observer : diagnostics_observer_list_)
        observer.OnAccessTokenRemoved(account_id, iter->first.scopes);
      token_cache_.erase(iter++);
    } else {
      ++iter;
    }
  }
}

void OAuth2TokenService::CancelAllRequests() {
  std::vector<Fetcher*> fetchers_to_cancel;
  for (const auto& pending_fetcher : pending_fetchers_)
    fetchers_to_cancel.push_back(pending_fetcher.second.get());
  CancelFetchers(fetchers_to_cancel);
}

void OAuth2TokenService::CancelRequestsForAccount(
    const std::string& account_id) {
  std::vector<Fetcher*> fetchers_to_cancel;
  for (const auto& pending_fetcher : pending_fetchers_) {
    if (pending_fetcher.first.account_id == account_id)
      fetchers_to_cancel.push_back(pending_fetcher.second.get());
  }
  CancelFetchers(fetchers_to_cancel);
}

void OAuth2TokenService::CancelFetchers(
    std::vector<Fetcher*> fetchers_to_cancel) {
  for (Fetcher* pending_fetcher : fetchers_to_cancel)
    pending_fetcher->Cancel();
}

void OAuth2TokenService::set_max_authorization_token_fetch_retries_for_testing(
    int max_retries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  max_fetch_retry_num_ = max_retries;
}

size_t OAuth2TokenService::GetNumPendingRequestsForTesting(
    const std::string& client_id,
    const std::string& account_id,
    const ScopeSet& scopes) const {
  auto iter = pending_fetchers_.find(
      OAuth2TokenService::RequestParameters(client_id, account_id, scopes));
  return iter == pending_fetchers_.end() ?
             0 : iter->second->GetWaitingRequestCount();
}
