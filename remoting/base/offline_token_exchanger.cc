// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/offline_token_exchanger.h"

#include <utility>

#include "base/logging.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

// Maximum number of retries on network/500 errors.
const int kMaxRetries = 3;

}  // namespace

OfflineTokenExchanger::OfflineTokenExchanger(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : gaia_oauth_client_(
          std::make_unique<gaia::GaiaOAuthClient>(url_loader_factory)),
      token_exchanger_(url_loader_factory) {
  token_exchanger_.set_offline_mode(true);
}

OfflineTokenExchanger::~OfflineTokenExchanger() = default;

void OfflineTokenExchanger::ExchangeRefreshToken(
    const std::string& refresh_token,
    TokenCallback callback) {
  DCHECK(callback_.is_null());
  callback_ = std::move(callback);

  // Get access token from refresh token, needed by OAuthTokenExchanger.
  gaia::OAuthClientInfo client_info = {
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST),
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST),
      // Redirect URL is only used when getting tokens from auth code. It
      // is not required when getting access tokens from refresh tokens.
      ""};
  std::vector<std::string> empty_scope_list;  // Use scope from refresh token.
  gaia_oauth_client_->RefreshToken(client_info, refresh_token, empty_scope_list,
                                   kMaxRetries, this);
}

void OfflineTokenExchanger::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_in_seconds) {
  access_token_ = access_token;
  token_exchanger_.ExchangeToken(
      access_token,
      base::BindOnce(&OfflineTokenExchanger::OnExchangeTokenResponse,
                     base::Unretained(this)));
}

void OfflineTokenExchanger::OnOAuthError() {
  LOG(ERROR) << "OAuth error.";
  std::move(callback_).Run(FAILURE, std::string());
}

void OfflineTokenExchanger::OnNetworkError(int response_code) {
  LOG(ERROR) << "Network error: " << response_code;
  std::move(callback_).Run(FAILURE, std::string());
}

void OfflineTokenExchanger::OnExchangeTokenResponse(
    OAuthTokenGetter::Status status,
    const std::string& refresh_token,
    const std::string& access_token) {
  if (status == OAuthTokenGetter::SUCCESS) {
    if (access_token_ == access_token) {
      std::move(callback_).Run(NO_EXCHANGE, std::string());
    } else {
      std::move(callback_).Run(SUCCESS, refresh_token);
    }
  } else {
    LOG(ERROR) << "Error exchanging token.";
    std::move(callback_).Run(FAILURE, std::string());
  }
}

}  // namespace remoting
