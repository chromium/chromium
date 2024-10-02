// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_HOST_STARTER_OAUTH_HELPER_H_
#define REMOTING_HOST_SETUP_HOST_STARTER_OAUTH_HELPER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "remoting/host/setup/host_starter.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace remoting {

class HostStarterOAuthHelper : public gaia::GaiaOAuthClient::Delegate {
 public:
  using OnDoneCallback =
      base::OnceCallback<void(const std::string& user_email,
                              const std::string& access_token,
                              const std::string& refresh_token,
                              const std::string& scopes)>;
  using OnErrorCallback =
      base::OnceCallback<void(const std::string& error_message,
                              HostStarter::Result result)>;

  explicit HostStarterOAuthHelper(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  HostStarterOAuthHelper(const HostStarterOAuthHelper&) = delete;
  HostStarterOAuthHelper& operator=(const HostStarterOAuthHelper&) = delete;
  ~HostStarterOAuthHelper() override;

  // Fetches OAuth tokens using |authorization_code|. If |user_email| is
  // provided, it will be compared against the email which generated the auth
  // code to ensure they match.
  void FetchTokens(const std::string& user_email,
                   const std::string& authorization_code,
                   gaia::OAuthClientInfo oauth_client_info,
                   OnDoneCallback done_callback,
                   OnErrorCallback error_callback);

  // gaia::GaiaOAuthClient::Delegate
  void OnGetTokensResponse(const std::string& refresh_token,
                           const std::string& access_token,
                           int expires_in_seconds) override;
  void OnGetUserEmailResponse(const std::string& user_email) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

 private:
  std::string access_token_;
  std::string refresh_token_;
  std::string expected_user_email_;

  OnDoneCallback done_callback_;
  OnErrorCallback error_callback_;
  gaia::OAuthClientInfo oauth_client_info_;
  std::unique_ptr<gaia::GaiaOAuthClient> oauth_client_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_HOST_STARTER_OAUTH_HELPER_H_
