// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/oauth_token_getter_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/strings/string_util.h"
#include "google_apis/google_api_keys.h"
#include "remoting/base/logging.h"
#include "remoting/base/oauth_token_exchanger.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

// Maximum number of retries on network/500 errors.
const int kMaxRetries = 3;

// Time when we we try to update OAuth token before its expiration.
const int kTokenUpdateTimeBeforeExpirySeconds = 60;

}  // namespace

OAuthTokenGetterImpl::OAuthTokenGetterImpl(
    std::unique_ptr<OAuthIntermediateCredentials> intermediate_credentials,
    const OAuthTokenGetter::CredentialsUpdatedCallback& on_credentials_update,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    bool auto_refresh)
    : intermediate_credentials_(std::move(intermediate_credentials)),
      gaia_oauth_client_(new gaia::GaiaOAuthClient(url_loader_factory)),
      credentials_updated_callback_(on_credentials_update),
      token_exchanger_(url_loader_factory) {
  if (auto_refresh) {
    refresh_timer_.reset(new base::OneShotTimer());
  }
}

OAuthTokenGetterImpl::OAuthTokenGetterImpl(
    std::unique_ptr<OAuthAuthorizationCredentials> authorization_credentials,
    const OAuthTokenGetter::RefreshTokenUpdatedCallback&
        on_refresh_token_updated,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    bool auto_refresh)
    : authorization_credentials_(std::move(authorization_credentials)),
      gaia_oauth_client_(new gaia::GaiaOAuthClient(url_loader_factory)),
      refresh_token_updated_callback_(on_refresh_token_updated),
      token_exchanger_(url_loader_factory) {
  if (auto_refresh) {
    refresh_timer_.reset(new base::OneShotTimer());
  }
}

OAuthTokenGetterImpl::~OAuthTokenGetterImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OAuthTokenGetterImpl::OnGetTokensResponse(const std::string& refresh_token,
                                               const std::string& access_token,
                                               int expires_seconds) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(intermediate_credentials_);
  VLOG(1) << "Received OAuth tokens.";

  // Update the access token and any other auto-update timers.
  UpdateAccessToken(access_token, expires_seconds);

  // Keep the refresh token in the authorization_credentials.
  authorization_credentials_.reset(
      new OAuthTokenGetter::OAuthAuthorizationCredentials(
          std::string(), refresh_token,
          intermediate_credentials_->is_service_account));

  // Clear out the one time use token.
  intermediate_credentials_.reset();

  // At this point we don't know the email address so we need to fetch it.
  email_discovery_ = true;
  gaia_oauth_client_->GetUserEmail(access_token, kMaxRetries, this);
}

void OAuthTokenGetterImpl::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_seconds) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(authorization_credentials_);
  VLOG(1) << "Received OAuth token.";

  // Update the access token and any other auto-update timers.
  UpdateAccessToken(access_token, expires_seconds);

  if (!authorization_credentials_->is_service_account && !email_verified_) {
    gaia_oauth_client_->GetUserEmail(access_token, kMaxRetries, this);
  } else {
    ExchangeAccessToken();
  }
}

void OAuthTokenGetterImpl::OnGetUserEmailResponse(
    const std::string& user_email) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(authorization_credentials_);
  VLOG(1) << "Received user info.";

  if (email_discovery_) {
    authorization_credentials_->login = user_email;
    email_discovery_ = false;
    NotifyUpdatedCallbacks(authorization_credentials_->login,
                           authorization_credentials_->refresh_token);
  } else if (user_email != authorization_credentials_->login) {
    LOG(ERROR) << "OAuth token and email address do not refer to "
                  "the same account.";
    OnOAuthError();
    return;
  }

  email_verified_ = true;

  // Now that we've refreshed the token and verified that it's for the correct
  // user account, exchange the token if needed.
  ExchangeAccessToken();
}

void OAuthTokenGetterImpl::UpdateAccessToken(const std::string& access_token,
                                             int expires_seconds) {
  oauth_access_token_ = access_token;
  base::TimeDelta token_expiration =
      base::TimeDelta::FromSeconds(expires_seconds) -
      base::TimeDelta::FromSeconds(kTokenUpdateTimeBeforeExpirySeconds);
  access_token_expiry_time_ = base::Time::Now() + token_expiration;

  if (refresh_timer_) {
    refresh_timer_->Stop();
    refresh_timer_->Start(FROM_HERE, token_expiration, this,
                          &OAuthTokenGetterImpl::RefreshAccessToken);
  }
}

void OAuthTokenGetterImpl::NotifyTokenCallbacks(
    Status status,
    const std::string& user_email,
    const std::string& access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  response_pending_ = false;

  base::queue<TokenCallback> callbacks;
  callbacks.swap(pending_callbacks_);

  while (!callbacks.empty()) {
    std::move(callbacks.front()).Run(status, user_email, access_token);
    callbacks.pop();
  }
}

void OAuthTokenGetterImpl::NotifyUpdatedCallbacks(
    const std::string& user_email,
    const std::string& refresh_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (credentials_updated_callback_) {
    credentials_updated_callback_.Run(user_email, refresh_token);
  }
}

void OAuthTokenGetterImpl::OnOAuthError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "OAuth: invalid credentials.";

  // Throw away invalid credentials and force a refresh.
  oauth_access_token_.clear();
  access_token_expiry_time_ = base::Time();
  email_verified_ = false;

  NotifyTokenCallbacks(OAuthTokenGetterImpl::AUTH_ERROR, std::string(),
                       std::string());
}

void OAuthTokenGetterImpl::OnNetworkError(int response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "Network error when trying to update OAuth token: "
             << response_code;
  NotifyTokenCallbacks(OAuthTokenGetterImpl::NETWORK_ERROR, std::string(),
                       std::string());
}

void OAuthTokenGetterImpl::CallWithToken(TokenCallback on_access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pending_callbacks_.push(std::move(on_access_token));

  if (intermediate_credentials_) {
    if (!response_pending_) {
      GetOauthTokensFromAuthCode();
    }
  } else {
    bool need_new_auth_token =
        access_token_expiry_time_.is_null() ||
        base::Time::Now() >= access_token_expiry_time_ ||
        (!authorization_credentials_->is_service_account && !email_verified_);

    if (need_new_auth_token) {
      if (!response_pending_) {
        RefreshAccessToken();
      }
    } else {
      // If |response_pending_| is true here, |oauth_access_token_| is
      // up-to-date but not yet exchanged (it might not have the needed scopes).
      // In that case, wait for token-exchange to complete before returning the
      // token.
      if (!response_pending_) {
        NotifyTokenCallbacks(OAuthTokenGetterImpl::SUCCESS,
                             authorization_credentials_->login,
                             oauth_access_token_);
      }
    }
  }
}

void OAuthTokenGetterImpl::InvalidateCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  access_token_expiry_time_ = base::Time();
}

base::WeakPtr<OAuthTokenGetterImpl> OAuthTokenGetterImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void OAuthTokenGetterImpl::GetOauthTokensFromAuthCode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Fetching OAuth token from Auth Code.";
  DCHECK(!response_pending_);

  // Service accounts use different API keys, as they use the client app flow.
  google_apis::OAuth2Client oauth2_client =
      intermediate_credentials_->is_service_account
          ? google_apis::CLIENT_REMOTING_HOST
          : google_apis::CLIENT_REMOTING;

  // For the case of fetching an OAuth token from a one-time-use code, the
  // caller should provide a redirect URI.
  std::string redirect_uri = intermediate_credentials_->oauth_redirect_uri;
  DCHECK(!redirect_uri.empty());

  gaia::OAuthClientInfo client_info = {
      google_apis::GetOAuth2ClientID(oauth2_client),
      google_apis::GetOAuth2ClientSecret(oauth2_client), redirect_uri};

  response_pending_ = true;

  gaia_oauth_client_->GetTokensFromAuthCode(
      client_info, intermediate_credentials_->authorization_code, kMaxRetries,
      this);
}

void OAuthTokenGetterImpl::RefreshAccessToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Refreshing OAuth Access token.";
  DCHECK(!response_pending_);

  // Service accounts use different API keys, as they use the client app flow.
  google_apis::OAuth2Client oauth2_client =
      authorization_credentials_->is_service_account
          ? google_apis::CLIENT_REMOTING_HOST
          : google_apis::CLIENT_REMOTING;

  gaia::OAuthClientInfo client_info = {
      google_apis::GetOAuth2ClientID(oauth2_client),
      google_apis::GetOAuth2ClientSecret(oauth2_client),
      // Redirect URL is only used when getting tokens from auth code. It
      // is not required when getting access tokens from refresh tokens.
      ""};

  response_pending_ = true;
  std::vector<std::string> empty_scope_list;  // Use scope from refresh token.
  gaia_oauth_client_->RefreshToken(client_info,
                                   authorization_credentials_->refresh_token,
                                   empty_scope_list, kMaxRetries, this);
}

void OAuthTokenGetterImpl::ExchangeAccessToken() {
  // Unretained() is safe because |this| owns its token-exchanger, which
  // owns its GaiaOAuthClient, which cancels callbacks on destruction.
  token_exchanger_.ExchangeToken(
      oauth_access_token_,
      base::BindOnce(&OAuthTokenGetterImpl::OnExchangeTokenResponse,
                     base::Unretained(this)));
}

void OAuthTokenGetterImpl::OnExchangeTokenResponse(
    Status status,
    const std::string& refresh_token,
    const std::string& access_token) {
  oauth_access_token_ = access_token;
  switch (status) {
    case AUTH_ERROR:
      OnOAuthError();
      break;
    case NETWORK_ERROR:
      NotifyTokenCallbacks(status, std::string(), std::string());
      break;
    case SUCCESS:
      if (!refresh_token.empty() &&
          refresh_token != authorization_credentials_->refresh_token) {
        authorization_credentials_->refresh_token = refresh_token;
        if (refresh_token_updated_callback_) {
          refresh_token_updated_callback_.Run(refresh_token);
        }
      }
      NotifyTokenCallbacks(status, authorization_credentials_->login,
                           oauth_access_token_);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace remoting
