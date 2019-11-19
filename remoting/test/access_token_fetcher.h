// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_ACCESS_TOKEN_FETCHER_H_
#define REMOTING_TEST_ACCESS_TOKEN_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "google_apis/gaia/gaia_oauth_client.h"

namespace network {
class SharedURLLoaderFactory;
class TransitionalURLLoaderFactoryOwner;
}  // namespace network

namespace remoting {
namespace test {

// Supplied by the client for each request to GAIA and returns valid tokens on
// success or empty tokens on failure.
using AccessTokenCallback =
    base::OnceCallback<void(const std::string& access_token,
                            const std::string& refresh_token)>;

// Retrieves an access token from either an authorization code or a refresh
// token.  Destroying the AccessTokenFetcher while a request is outstanding will
// cancel the request. It is safe to delete the fetcher from within a completion
// callback.  Must be used from a thread running an IO message loop.
// The public methods are virtual to allow for mocking and fakes.
class AccessTokenFetcher : public gaia::GaiaOAuthClient::Delegate {
 public:
  AccessTokenFetcher();
  ~AccessTokenFetcher() override;

  // Retrieve an access token from a one time use authorization code.
  virtual void GetAccessTokenFromAuthCode(const std::string& auth_code,
                                          AccessTokenCallback callback);

  // Retrieve an access token from a refresh token.
  virtual void GetAccessTokenFromRefreshToken(const std::string& refresh_token,
                                              AccessTokenCallback callback);

  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory>
          url_loader_factory_for_testing);

 private:
  // gaia::GaiaOAuthClient::Delegate Interface.
  void OnGetTokensResponse(const std::string& refresh_token,
                           const std::string& access_token,
                           int expires_in_seconds) override;
  void OnRefreshTokenResponse(const std::string& access_token,
                              int expires_in_seconds) override;
  void OnGetUserEmailResponse(const std::string& user_email) override;
  void OnGetUserIdResponse(const std::string& user_id) override;
  void OnGetUserInfoResponse(
      std::unique_ptr<base::DictionaryValue> user_info) override;
  void OnGetTokenInfoResponse(
      std::unique_ptr<base::DictionaryValue> token_info) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

  // Updates |auth_client_| with a new GaiaOAuthClient instance.
  void CreateNewGaiaOAuthClientInstance();

  // Validates the retrieved access token.
  void ValidateAccessToken();

  // Caller-supplied callback used to return valid tokens on success or empty
  // tokens on failure.
  AccessTokenCallback access_token_callback_;

  // Retrieved based on the |refresh_token_|.
  std::string access_token_;

  // Supplied by the caller or retrieved from a caller-supplied auth token.
  std::string refresh_token_;

  // Holds the client id, secret, and redirect url used to make
  // the Gaia service request.
  gaia::OAuthClientInfo oauth_client_info_;

  // Used to feed network into |auth_client_|.
  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;

  scoped_refptr<network::SharedURLLoaderFactory>
      url_loader_factory_for_testing_;

  // Used to make token requests to GAIA.
  std::unique_ptr<gaia::GaiaOAuthClient> auth_client_;

  DISALLOW_COPY_AND_ASSIGN(AccessTokenFetcher);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_ACCESS_TOKEN_FETCHER_H_
