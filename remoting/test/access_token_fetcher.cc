// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/access_token_fetcher.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/google_api_keys.h"
#include "net/url_request/url_fetcher.h"
#include "remoting/base/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace {
const int kMaxGetTokensRetries = 3;
const char kOauthRedirectUrl[] =
    "https://chromoting-oauth.talkgadget."
    "google.com/talkgadget/oauth/chrome-remote-desktop/dev";
}  // namespace

namespace remoting {
namespace test {

AccessTokenFetcher::AccessTokenFetcher() {
  oauth_client_info_ = {
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING),
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING),
      kOauthRedirectUrl};
}

AccessTokenFetcher::~AccessTokenFetcher() = default;

void AccessTokenFetcher::GetAccessTokenFromAuthCode(
    const std::string& auth_code,
    AccessTokenCallback callback) {
  DCHECK(!auth_code.empty());
  DCHECK(!callback.is_null());
  DCHECK(access_token_callback_.is_null());

  VLOG(2) << "Calling GetTokensFromAuthCode to exchange auth_code for token";

  access_token_.clear();
  refresh_token_.clear();
  access_token_callback_ = std::move(callback);

  // Create a new GaiaOAuthClient for each request to GAIA.
  CreateNewGaiaOAuthClientInstance();
  auth_client_->GetTokensFromAuthCode(oauth_client_info_, auth_code,
                                      kMaxGetTokensRetries,
                                      /*delegate=*/this);
}

void AccessTokenFetcher::GetAccessTokenFromRefreshToken(
    const std::string& refresh_token,
    AccessTokenCallback callback) {
  DCHECK(!refresh_token.empty());
  DCHECK(!callback.is_null());
  DCHECK(access_token_callback_.is_null());

  VLOG(2) << "Calling RefreshToken to generate a new access token";

  access_token_.clear();
  refresh_token_ = refresh_token;
  access_token_callback_ = std::move(callback);

  // Create a new GaiaOAuthClient for each request to GAIA.
  CreateNewGaiaOAuthClientInstance();
  auth_client_->RefreshToken(oauth_client_info_, refresh_token_,
                             /*scopes=*/std::vector<std::string>(),
                             kMaxGetTokensRetries,
                             /*delegate=*/this);
}

void AccessTokenFetcher::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory>
        url_loader_factory_for_testing) {
  url_loader_factory_for_testing_ = url_loader_factory_for_testing;
}

void AccessTokenFetcher::CreateNewGaiaOAuthClientInstance() {
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  if (url_loader_factory_for_testing_) {
    url_loader_factory = url_loader_factory_for_testing_;
  } else {
    scoped_refptr<remoting::URLRequestContextGetter> request_context_getter;
    request_context_getter = new remoting::URLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get());

    url_loader_factory_owner_.reset(
        new network::TransitionalURLLoaderFactoryOwner(request_context_getter));
    url_loader_factory = url_loader_factory_owner_->GetURLLoaderFactory();
  }

  auth_client_.reset(new gaia::GaiaOAuthClient(url_loader_factory));
}

void AccessTokenFetcher::OnGetTokensResponse(const std::string& refresh_token,
                                             const std::string& access_token,
                                             int expires_in_seconds) {
  VLOG(1) << "AccessTokenFetcher::OnGetTokensResponse() Called";
  VLOG(1) << "--refresh_token: " << refresh_token;
  VLOG(1) << "--access_token: " << access_token;
  VLOG(1) << "--expires_in_seconds: " << expires_in_seconds;

  refresh_token_ = refresh_token;
  access_token_ = access_token;

  ValidateAccessToken();
}

void AccessTokenFetcher::OnRefreshTokenResponse(const std::string& access_token,
                                                int expires_in_seconds) {
  VLOG(1) << "AccessTokenFetcher::OnRefreshTokenResponse() Called";
  VLOG(1) << "--access_token: " << access_token;
  VLOG(1) << "--expires_in_seconds: " << expires_in_seconds;

  access_token_ = access_token;

  ValidateAccessToken();
}

void AccessTokenFetcher::OnGetUserEmailResponse(const std::string& user_email) {
  // This callback should not be called as we do not request the user's email.
  NOTREACHED();
}

void AccessTokenFetcher::OnGetUserIdResponse(const std::string& user_id) {
  // This callback should not be called as we do not request the user's id.
  NOTREACHED();
}

void AccessTokenFetcher::OnGetUserInfoResponse(
    std::unique_ptr<base::DictionaryValue> user_info) {
  // This callback should not be called as we do not request user info.
  NOTREACHED();
}

void AccessTokenFetcher::OnGetTokenInfoResponse(
    std::unique_ptr<base::DictionaryValue> token_info) {
  VLOG(1) << "AccessTokenFetcher::OnGetTokenInfoResponse() Called";

  std::string error_string;
  std::string error_description;

  // Check to see if the token_info we received had any errors,
  // otherwise we will assume that it is valid for our purposes.
  if (token_info->HasKey("error")) {
    token_info->GetString("error", &error_string);
    token_info->GetString("error_description", &error_description);

    LOG(ERROR) << "OnGetTokenInfoResponse returned an error. "
               << "error: " << error_string << ", "
               << "description: " << error_description;
    access_token_.clear();
    refresh_token_.clear();
  } else {
    VLOG(1) << "Access Token has been validated";
  }

  std::move(access_token_callback_).Run(access_token_, refresh_token_);
}

void AccessTokenFetcher::OnOAuthError() {
  LOG(ERROR) << "AccessTokenFetcher::OnOAuthError() Called";

  access_token_.clear();
  refresh_token_.clear();

  std::move(access_token_callback_).Run(access_token_, refresh_token_);
}

void AccessTokenFetcher::OnNetworkError(int response_code) {
  LOG(ERROR) << "AccessTokenFetcher::OnNetworkError() Called";
  LOG(ERROR) << "response code: " << response_code;

  access_token_.clear();
  refresh_token_.clear();

  std::move(access_token_callback_).Run(access_token_, refresh_token_);
}

void AccessTokenFetcher::ValidateAccessToken() {
  VLOG(2) << "Calling GetTokenInfo to validate access token";

  // Create a new GaiaOAuthClient for each request to GAIA.
  CreateNewGaiaOAuthClientInstance();
  auth_client_->GetTokenInfo(access_token_, kMaxGetTokensRetries,
                             /*delegate=*/this);
}

}  // namespace test
}  // namespace remoting
