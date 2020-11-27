// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_access_token_fetcher.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// static
std::unique_ptr<GaiaAccessTokenFetcher>
GaiaAccessTokenFetcher::CreateExchangeRefreshTokenForAccessTokenInstance(
    OAuth2AccessTokenConsumer* consumer,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& refresh_token) {
  // Using `new` to access a non-public constructor.
  return base::WrapUnique(new GaiaAccessTokenFetcher(
      consumer, url_loader_factory, refresh_token, std::string()));
}

// static
std::unique_ptr<GaiaAccessTokenFetcher>
GaiaAccessTokenFetcher::CreateExchangeAuthCodeForRefeshTokenInstance(
    OAuth2AccessTokenConsumer* consumer,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& auth_code) {
  // Using `new` to access a non-public constructor.
  return base::WrapUnique(new GaiaAccessTokenFetcher(
      consumer, url_loader_factory, std::string(), auth_code));
}

GaiaAccessTokenFetcher::GaiaAccessTokenFetcher(
    OAuth2AccessTokenConsumer* consumer,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& refresh_token,
    const std::string& auth_code)
    : OAuth2AccessTokenFetcherImpl(consumer,
                                   url_loader_factory,
                                   refresh_token,
                                   auth_code) {}

GaiaAccessTokenFetcher::~GaiaAccessTokenFetcher() = default;

void GaiaAccessTokenFetcher::RecordResponseCodeUma(int error_value) const {
  base::UmaHistogramSparse("Gaia.ResponseCodesForOAuth2AccessToken",
                           error_value);
}

void GaiaAccessTokenFetcher::RecordBadRequestTypeUma(
    OAuth2ErrorCodesForHistogram access_error) const {
  UMA_HISTOGRAM_ENUMERATION("Gaia.BadRequestTypeForOAuth2AccessToken",
                            access_error, OAUTH2_ACCESS_ERROR_COUNT);
}

GURL GaiaAccessTokenFetcher::GetAccessTokenURL() const {
  return GaiaUrls::GetInstance()->oauth2_token_url();
}

net::NetworkTrafficAnnotationTag
GaiaAccessTokenFetcher::GetTrafficAnnotationTag() const {
  return net::DefineNetworkTrafficAnnotation("oauth2_access_token_fetcher", R"(
    semantics {
      sender: "OAuth 2.0 Access Token Fetcher"
      description:
        "This request is used by the Token Service to fetch an OAuth 2.0 "
        "access token for a known Google account."
      trigger:
        "This request can be triggered at any moment when any service "
        "requests an OAuth 2.0 access token from the Token Service."
      data:
        "Chrome OAuth 2.0 client id and secret, the set of OAuth 2.0 "
        "scopes and the OAuth 2.0 refresh token."
      destination: GOOGLE_OWNED_SERVICE
    }
    policy {
      cookies_allowed: NO
      setting:
        "This feature cannot be disabled in settings, but if user signs "
        "out of Chrome, this request would not be made."
      chrome_policy {
        SigninAllowed {
          policy_options {mode: MANDATORY}
          SigninAllowed: false
        }
      }
    })");
}
