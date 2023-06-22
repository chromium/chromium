// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_mint_access_token_fetcher_adapter.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

OAuth2MintAccessTokenFetcherAdapter::OAuth2MintAccessTokenFetcherAdapter(
    OAuth2AccessTokenConsumer* consumer,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& refresh_token,
    const std::string& device_id,
    const std::string& client_version,
    const std::string& client_channel)
    : OAuth2AccessTokenFetcher(consumer),
      url_loader_factory_(std::move(url_loader_factory)),
      refresh_token_(refresh_token),
      device_id_(device_id),
      client_version_(client_version),
      client_channel_(client_channel) {}

OAuth2MintAccessTokenFetcherAdapter::~OAuth2MintAccessTokenFetcherAdapter() =
    default;

void OAuth2MintAccessTokenFetcherAdapter::Start(
    const std::string& client_id,
    const std::string& client_secret,
    const std::vector<std::string>& scopes) {
  auto params = OAuth2MintTokenFlow::Parameters(
      /*eid=*/std::string(), client_id, scopes,
      /*enable_granular_permissions=*/false, device_id_,
      /*selected_user_id=*/std::string(),
      /*consent_result=*/std::string(), client_version_, client_channel_,
      OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
  if (mint_token_flow_factory_for_testing_) {
    mint_token_flow_ = mint_token_flow_factory_for_testing_.Run(this, params);
  } else {
    mint_token_flow_ = std::make_unique<OAuth2MintTokenFlow>(this, params);
  }
  mint_token_flow_->Start(url_loader_factory_, refresh_token_);
}

void OAuth2MintAccessTokenFetcherAdapter::CancelRequest() {
  mint_token_flow_.reset();
}

void OAuth2MintAccessTokenFetcherAdapter::
    SetOAuth2MintTokenFlowFactoryForTesting(
        OAuth2MintTokenFlowFactory factory) {
  mint_token_flow_factory_for_testing_ = std::move(factory);
}

void OAuth2MintAccessTokenFetcherAdapter::OnMintTokenSuccess(
    const std::string& access_token,
    const std::set<std::string>& granted_scopes,
    int time_to_live) {
  // The token will expire in `time_to_live` seconds. Take a 10% error margin to
  // prevent reusing a token too close to its expiration date.
  base::Time expiration_time =
      base::Time::Now() + base::Seconds(9 * time_to_live / 10);
  OAuth2AccessTokenConsumer::TokenResponse::Builder response_builder;
  response_builder.WithAccessToken(access_token)
      .WithExpirationTime(expiration_time);
  FireOnGetTokenSuccess(response_builder.build());
}
void OAuth2MintAccessTokenFetcherAdapter::OnMintTokenFailure(
    const GoogleServiceAuthError& error) {
  FireOnGetTokenFailure(error);
}
void OAuth2MintAccessTokenFetcherAdapter::OnRemoteConsentSuccess(
    const RemoteConsentResolutionData& resolution_data) {
  FireOnGetTokenFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
      "Unexpected remote consent response received."));
}
