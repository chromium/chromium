// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/host_starter_oauth_helper.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "remoting/host/setup/host_starter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

using Result = HostStarter::Result;

constexpr int kMaxGetTokensRetries = 3;

}  // namespace

HostStarterOAuthHelper::HostStarterOAuthHelper(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : oauth_client_(
          std::make_unique<gaia::GaiaOAuthClient>(url_loader_factory)) {}

HostStarterOAuthHelper::~HostStarterOAuthHelper() = default;

void HostStarterOAuthHelper::FetchTokens(
    const std::string& user_email,
    const std::string& authorization_code,
    gaia::OAuthClientInfo oauth_client_info,
    OnDoneCallback done_callback,
    OnErrorCallback error_callback) {
  // An instance of this class should only be used once.
  CHECK(expected_user_email_.empty());
  CHECK(!done_callback_);
  CHECK(!error_callback_);

  expected_user_email_ = user_email;
  done_callback_ = std::move(done_callback);
  error_callback_ = std::move(error_callback);

  oauth_client_->GetTokensFromAuthCode(std::move(oauth_client_info),
                                       authorization_code, kMaxGetTokensRetries,
                                       this);
}

void HostStarterOAuthHelper::OnGetTokensResponse(
    const std::string& refresh_token,
    const std::string& access_token,
    int expires_in_seconds) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ignore |expires_in_seconds| since the setup process will complete well
  // before the tokens expire.
  access_token_ = access_token;
  refresh_token_ = refresh_token;

  // Get the email corresponding to the access token.
  oauth_client_->GetUserEmail(access_token, 1, this);
}

void HostStarterOAuthHelper::OnGetUserEmailResponse(
    const std::string& user_email) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!expected_user_email_.empty() &&
      !base::EqualsCaseInsensitiveASCII(expected_user_email_, user_email)) {
    // Verify that the token retrieved matches the expected user email.
    std::move(error_callback_)
        .Run(base::StringPrintf(
                 "authorization_code was created for `%s` which does not "
                 "match the expected account: `%s`",
                 user_email.c_str(), expected_user_email_.c_str()),
             Result::PERMISSION_DENIED);
    return;
  }

  std::move(done_callback_).Run(user_email, access_token_, refresh_token_, "");
}

void HostStarterOAuthHelper::OnOAuthError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(error_callback_)
      .Run("Failed to exchange the authorization_code due to an OAuth error.",
           Result::OAUTH_ERROR);
}

void HostStarterOAuthHelper::OnNetworkError(int response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(error_callback_)
      .Run(base::StringPrintf("Failed to exchange the authorization_code due "
                              "to a network error: %d.",
                              response_code),
           Result::NETWORK_ERROR);
}

}  // namespace remoting
