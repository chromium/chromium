// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_OAUTH_TOKEN_EXCHANGER_H_
#define REMOTING_BASE_OAUTH_TOKEN_EXCHANGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "remoting/base/oauth_token_getter.h"

namespace grpc {
class Status;
}  // namespace grpc

namespace remoting {

namespace apis {
namespace v1 {
class UpdateRobotTokenResponse;
}  // namespace v1
}  // namespace apis

class OAuthTokenExchanger : public gaia::GaiaOAuthClient::Delegate {
 public:
  typedef base::OnceCallback<void(OAuthTokenGetter::Status status,
                                  const std::string& refresh_token,
                                  const std::string& access_token)>
      TokenCallback;

  explicit OAuthTokenExchanger(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~OAuthTokenExchanger() override;

  void set_offline_mode(bool offline_mode) { offline_mode_ = offline_mode; }

  // |access_token| should be an OAuth access token derived from the refresh
  // token from the host's config. This will test the token's scopes to see if
  // an exchange is needed. The test only happens once - the result is
  // cached for subsequent calls.
  // If exchange occurred, the new access and refresh tokens (fetched
  // from OAuth "token" endpoint) are returned to |on_new_token| callback.
  // If exchange did not occur, the provided |access_token| and an empty string
  // for the refresh token are returned.
  // A caller can determine if exchange occurred by comparing the returned
  // access token with the input |access_token| for equality.
  void ExchangeToken(const std::string& access_token,
                     TokenCallback on_new_token);

  // gaia::GaiaOAuthClient::Delegate interface.
  void OnGetTokensResponse(const std::string& refresh_token,
                           const std::string& access_token,
                           int expires_in_seconds) override;
  void OnGetTokenInfoResponse(
      std::unique_ptr<base::DictionaryValue> token_info) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

 private:
  class DirectoryServiceClient;

  void NotifyCallbacks(OAuthTokenGetter::Status status,
                       const std::string& refresh_token,
                       const std::string& access_token);
  void RequestNewToken();
  void OnRobotTokenResponse(const grpc::Status& status,
                            const apis::v1::UpdateRobotTokenResponse& response);

  std::unique_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;
  base::queue<TokenCallback> pending_callbacks_;
  std::string oauth_access_token_;

  bool offline_mode_ = true;

  // True if the OAuth refresh token is lacking required scopes and the
  // token-exchange service is needed to provide a new access-token.
  // False if the refresh token is up-to-date with required scopes.
  // Unset if the scopes are unknown and the tokeninfo endpoint needs to be
  // queried.
  base::Optional<bool> need_token_exchange_;

  std::unique_ptr<DirectoryServiceClient> directory_service_client_;

  DISALLOW_COPY_AND_ASSIGN(OAuthTokenExchanger);
};

}  // namespace remoting

#endif  // REMOTING_BASE_OAUTH_TOKEN_EXCHANGER_H_
