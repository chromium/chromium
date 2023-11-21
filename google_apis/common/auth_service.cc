// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/common/auth_service.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/common/auth_service_observer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace google_apis {

namespace {

// OAuth2 authorization token retrieval request.
class AuthRequest {
 public:
  AuthRequest(signin::IdentityManager* identity_manager,
              const CoreAccountId& account_id,
              scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
              AuthStatusCallback callback,
              const std::vector<std::string>& scopes);
  AuthRequest(const AuthRequest&) = delete;
  AuthRequest& operator=(const AuthRequest&) = delete;
  ~AuthRequest();

 private:
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo token_info);

  AuthStatusCallback callback_;
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
  base::ThreadChecker thread_checker_;
};

AuthRequest::AuthRequest(
    signin::IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    AuthStatusCallback callback,
    const std::vector<std::string>& scopes)
    : callback_(std::move(callback)) {
  DCHECK(identity_manager);
  DCHECK(callback_);

  access_token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      account_id, "auth_service", url_loader_factory,
      signin::ScopeSet(scopes.begin(), scopes.end()),
      base::BindOnce(&AuthRequest::OnAccessTokenFetchComplete,
                     base::Unretained(this)),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

AuthRequest::~AuthRequest() {}

void AuthRequest::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (error.state() == GoogleServiceAuthError::NONE) {
    std::move(callback_).Run(HTTP_SUCCESS, token_info.token);
  } else {
    LOG(WARNING) << "AuthRequest: token request using refresh token failed: "
                 << error.ToString();

    // There are many ways to fail, but if the failure is due to connection,
    // it's likely that the device is off-line. We treat the error differently
    // so that the file manager works while off-line.
    if (error.state() == GoogleServiceAuthError::CONNECTION_FAILED) {
      std::move(callback_).Run(NO_CONNECTION, std::string());
    } else if (error.state() == GoogleServiceAuthError::SERVICE_UNAVAILABLE) {
      std::move(callback_).Run(HTTP_FORBIDDEN, std::string());
    } else {
      // Permanent auth error.
      std::move(callback_).Run(HTTP_UNAUTHORIZED, std::string());
    }
  }

  delete this;
}

}  // namespace

// This class is separate from AuthService itself so that AuthService doesn't
// need a public dependency on signin::IdentityManager::Observer, and therefore
// doesn't need to pull that dependency into all of its client classes.
class AuthService::IdentityManagerObserver
    : public signin::IdentityManager::Observer {
 public:
  explicit IdentityManagerObserver(AuthService* service) : service_(service) {
    manager_observation_.Observe(service->identity_manager_.get());
  }
  ~IdentityManagerObserver() override = default;

  // signin::IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override {
    service_->OnHandleRefreshToken(account_info.account_id, true);
  }

  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override {
    service_->OnHandleRefreshToken(account_id, false);
  }

 private:
  raw_ptr<AuthService> service_ = nullptr;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      manager_observation_{this};
};

AuthService::AuthService(
    signin::IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::vector<std::string>& scopes)
    : identity_manager_(identity_manager),
      identity_manager_observer_(
          std::make_unique<IdentityManagerObserver>(this)),
      account_id_(account_id),
      url_loader_factory_(url_loader_factory),
      scopes_(scopes) {
  DCHECK(identity_manager_);

  has_refresh_token_ =
      identity_manager_->HasAccountWithRefreshToken(account_id_);
}

AuthService::~AuthService() = default;

void AuthService::StartAuthentication(AuthStatusCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (HasAccessToken()) {
    // We already have access token. Give it back to the caller asynchronously.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), HTTP_SUCCESS, access_token_));
  } else if (HasRefreshToken()) {
    // We have refresh token, let's get an access token.
    new AuthRequest(
        identity_manager_, account_id_, url_loader_factory_,
        base::BindOnce(&AuthService::OnAuthCompleted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        scopes_);
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), NOT_READY, std::string()));
  }
}

bool AuthService::HasAccessToken() const {
  return !access_token_.empty();
}

bool AuthService::HasRefreshToken() const {
  return has_refresh_token_;
}

const std::string& AuthService::access_token() const {
  return access_token_;
}

void AuthService::ClearAccessToken() {
  access_token_.clear();
}

void AuthService::ClearRefreshToken() {
  OnHandleRefreshToken(account_id_, false);
}

void AuthService::OnAuthCompleted(AuthStatusCallback callback,
                                  ApiErrorCode error,
                                  const std::string& access_token) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  if (error == HTTP_SUCCESS) {
    access_token_ = access_token;
  } else if (error == HTTP_UNAUTHORIZED) {
    // Refreshing access token using the refresh token is failed with 401 error
    // (HTTP_UNAUTHORIZED). This means the current refresh token is invalid for
    // the current scope,  hence we clear the refresh token here to make
    // HasRefreshToken() false, thus the invalidness is clearly observable.
    // This is not for triggering refetch of the refresh token. UI should
    // show some message to encourage user to log-off and log-in again in order
    // to fetch new valid refresh token.
    ClearRefreshToken();
  }

  std::move(callback).Run(error, access_token);
}

void AuthService::AddObserver(AuthServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void AuthService::RemoveObserver(AuthServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AuthService::OnHandleRefreshToken(const CoreAccountId& account_id,
                                       bool has_refresh_token) {
  if (account_id != account_id_)
    return;

  access_token_.clear();
  has_refresh_token_ = has_refresh_token;

  for (auto& observer : observers_)
    observer.OnOAuth2RefreshTokenChanged();
}

}  // namespace google_apis
