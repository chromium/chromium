// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_access_token_consumer.h"

OAuth2AccessTokenConsumer::TokenResponse::TokenResponse() = default;

OAuth2AccessTokenConsumer::TokenResponse::TokenResponse(const TokenResponse&) =
    default;

OAuth2AccessTokenConsumer::TokenResponse::TokenResponse(TokenResponse&&) =
    default;

OAuth2AccessTokenConsumer::TokenResponse::TokenResponse(
    const std::string& access_token,
    const std::string& refresh_token,
    const base::Time& expiration_time,
    const std::string& id_token)
    : access_token(access_token),
      refresh_token(refresh_token),
      expiration_time(expiration_time),
      id_token(id_token) {}

OAuth2AccessTokenConsumer::TokenResponse::~TokenResponse() = default;

OAuth2AccessTokenConsumer::TokenResponse&
OAuth2AccessTokenConsumer::TokenResponse::operator=(
    const TokenResponse& response) = default;

OAuth2AccessTokenConsumer::TokenResponse&
OAuth2AccessTokenConsumer::TokenResponse::operator=(TokenResponse&& response) =
    default;

OAuth2AccessTokenConsumer::TokenResponse::Builder::Builder() = default;

OAuth2AccessTokenConsumer::TokenResponse::Builder::~Builder() = default;

OAuth2AccessTokenConsumer::TokenResponse::Builder&
OAuth2AccessTokenConsumer::TokenResponse::Builder::WithAccessToken(
    const std::string& token) {
  access_token_ = token;
  return *this;
}

OAuth2AccessTokenConsumer::TokenResponse::Builder&
OAuth2AccessTokenConsumer::TokenResponse::Builder::WithRefreshToken(
    const std::string& token) {
  refresh_token_ = token;
  return *this;
}

OAuth2AccessTokenConsumer::TokenResponse::Builder&
OAuth2AccessTokenConsumer::TokenResponse::Builder::WithExpirationTime(
    const base::Time& time) {
  expiration_time_ = time;
  return *this;
}

OAuth2AccessTokenConsumer::TokenResponse::Builder&
OAuth2AccessTokenConsumer::TokenResponse::Builder::WithIdToken(
    const std::string& token) {
  id_token_ = token;
  return *this;
}

OAuth2AccessTokenConsumer::TokenResponse
OAuth2AccessTokenConsumer::TokenResponse::Builder::build() {
  return TokenResponse(access_token_, refresh_token_, expiration_time_,
                       id_token_);
}

OAuth2AccessTokenConsumer::~OAuth2AccessTokenConsumer() = default;

void OAuth2AccessTokenConsumer::OnGetTokenSuccess(
    const TokenResponse& token_response) {}

void OAuth2AccessTokenConsumer::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {}
