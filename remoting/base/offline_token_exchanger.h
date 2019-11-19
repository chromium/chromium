// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_OFFLINE_TOKEN_EXCHANGER_H_
#define REMOTING_BASE_OFFLINE_TOKEN_EXCHANGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "remoting/base/oauth_token_exchanger.h"

namespace remoting {

// This class exchanges an OAuth refresh token (read from the host
// config) for a new refresh token with required scopes. This can be
// used to upgrade and write a new host config if needed. This is a
// simple wrapper around OAuthTokenExchanger - it uses the input refresh
// token to get an access token, then passes it to OAuthTokenExchanger (with
// offline mode) to maybe get a new refresh/access token pair.
class OfflineTokenExchanger : public gaia::GaiaOAuthClient::Delegate {
 public:
  enum Status {
    // New refresh token provided.
    SUCCESS,
    // No token exchange needed.
    NO_EXCHANGE,
    // Failed to test the token's scopes, or to get a new token.
    FAILURE,
  };

  typedef base::OnceCallback<void(Status status,
                                  const std::string& refresh_token)>
      TokenCallback;

  explicit OfflineTokenExchanger(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~OfflineTokenExchanger() override;

  // |refresh_token| is the OAuth token from the host config.
  // |callback| will be notified with the new refresh token if exchange took
  // place, or NO_EXCHANGE if current token is good, or FAILURE.
  void ExchangeRefreshToken(const std::string& refresh_token,
                            TokenCallback callback);

 private:
  // gaia::GaiaOAuthClient::Delegate interface.
  void OnRefreshTokenResponse(const std::string& access_token,
                              int expires_in_seconds) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

  void OnExchangeTokenResponse(OAuthTokenGetter::Status status,
                               const std::string& refresh_token,
                               const std::string& access_token);

  std::unique_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;
  OAuthTokenExchanger token_exchanger_;
  TokenCallback callback_;

  // Store the access token, in order to determine whether token-exchange
  // actually occurred.
  std::string access_token_;

  DISALLOW_COPY_AND_ASSIGN(OfflineTokenExchanger);
};

}  // namespace remoting

#endif  // REMOTING_BASE_OFFLINE_TOKEN_EXCHANGER_H_
