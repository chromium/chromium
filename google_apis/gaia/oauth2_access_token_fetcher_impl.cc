// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {
constexpr char kGetAccessTokenBodyFormat[] =
    "client_id=%s&"
    "client_secret=%s&"
    "grant_type=refresh_token&"
    "refresh_token=%s";

constexpr char kGetAccessTokenBodyWithScopeFormat[] =
    "client_id=%s&"
    "client_secret=%s&"
    "grant_type=refresh_token&"
    "refresh_token=%s&"
    "scope=%s";

constexpr char kAccessTokenKey[] = "access_token";
constexpr char kExpiresInKey[] = "expires_in";
constexpr char kIdTokenKey[] = "id_token";
constexpr char kErrorKey[] = "error";

// Enumerated constants for logging server responses on 400 errors, matching
// RFC 6749.
enum OAuth2ErrorCodesForHistogram {
  OAUTH2_ACCESS_ERROR_INVALID_REQUEST = 0,
  OAUTH2_ACCESS_ERROR_INVALID_CLIENT,
  OAUTH2_ACCESS_ERROR_INVALID_GRANT,
  OAUTH2_ACCESS_ERROR_UNAUTHORIZED_CLIENT,
  OAUTH2_ACCESS_ERROR_UNSUPPORTED_GRANT_TYPE,
  OAUTH2_ACCESS_ERROR_INVALID_SCOPE,
  OAUTH2_ACCESS_ERROR_UNKNOWN,
  OAUTH2_ACCESS_ERROR_COUNT
};

OAuth2ErrorCodesForHistogram OAuth2ErrorToHistogramValue(
    const std::string& error) {
  if (error == "invalid_request")
    return OAUTH2_ACCESS_ERROR_INVALID_REQUEST;
  else if (error == "invalid_client")
    return OAUTH2_ACCESS_ERROR_INVALID_CLIENT;
  else if (error == "invalid_grant")
    return OAUTH2_ACCESS_ERROR_INVALID_GRANT;
  else if (error == "unauthorized_client")
    return OAUTH2_ACCESS_ERROR_UNAUTHORIZED_CLIENT;
  else if (error == "unsupported_grant_type")
    return OAUTH2_ACCESS_ERROR_UNSUPPORTED_GRANT_TYPE;
  else if (error == "invalid_scope")
    return OAUTH2_ACCESS_ERROR_INVALID_SCOPE;

  return OAUTH2_ACCESS_ERROR_UNKNOWN;
}

static GoogleServiceAuthError CreateAuthError(int net_error) {
  CHECK_NE(net_error, net::OK);
  DLOG(WARNING) << "Could not reach Google Accounts servers: errno "
                << net_error;
  return GoogleServiceAuthError::FromConnectionError(net_error);
}

static std::unique_ptr<network::SimpleURLLoader> CreateURLLoader(
    const GURL& url,
    const std::string& body) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("oauth2_access_token_fetcher", R"(
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

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  if (!body.empty())
    resource_request->method = "POST";

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  if (!body.empty())
    url_loader->AttachStringForUpload(body,
                                      "application/x-www-form-urlencoded");

  // We want to receive the body even on error, as it may contain the reason for
  // failure.
  url_loader->SetAllowHttpErrorResults(true);

  // Fetchers are sometimes cancelled because a network change was detected,
  // especially at startup and after sign-in on ChromeOS. Retrying once should
  // be enough in those cases; let the fetcher retry up to 3 times just in case.
  // http://crbug.com/163710
  url_loader->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  return url_loader;
}

std::unique_ptr<base::DictionaryValue> ParseGetAccessTokenResponse(
    std::unique_ptr<std::string> data) {
  if (!data)
    return nullptr;

  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(*data);
  if (!value.get() || value->type() != base::Value::Type::DICTIONARY)
    value.reset();

  return std::unique_ptr<base::DictionaryValue>(
      static_cast<base::DictionaryValue*>(value.release()));
}

}  // namespace

OAuth2AccessTokenFetcherImpl::OAuth2AccessTokenFetcherImpl(
    OAuth2AccessTokenConsumer* consumer,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& refresh_token)
    : OAuth2AccessTokenFetcher(consumer),
      url_loader_factory_(url_loader_factory),
      refresh_token_(refresh_token),
      state_(INITIAL) {}

OAuth2AccessTokenFetcherImpl::~OAuth2AccessTokenFetcherImpl() {}

void OAuth2AccessTokenFetcherImpl::CancelRequest() {
  url_loader_.reset();
}

void OAuth2AccessTokenFetcherImpl::Start(
    const std::string& client_id,
    const std::string& client_secret,
    const std::vector<std::string>& scopes) {
  client_id_ = client_id;
  client_secret_ = client_secret;
  scopes_ = scopes;
  StartGetAccessToken();
}

void OAuth2AccessTokenFetcherImpl::StartGetAccessToken() {
  CHECK_EQ(INITIAL, state_);
  state_ = GET_ACCESS_TOKEN_STARTED;
  url_loader_ =
      CreateURLLoader(MakeGetAccessTokenUrl(),
                      MakeGetAccessTokenBody(client_id_, client_secret_,
                                             refresh_token_, scopes_));
  // It's safe to use Unretained below as the |url_loader_| is owned by |this|.
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&OAuth2AccessTokenFetcherImpl::OnURLLoadComplete,
                     base::Unretained(this)),
      1024 * 1024);
}

void OAuth2AccessTokenFetcherImpl::EndGetAccessToken(
    std::unique_ptr<std::string> response_body) {
  CHECK_EQ(GET_ACCESS_TOKEN_STARTED, state_);
  state_ = GET_ACCESS_TOKEN_DONE;

  bool net_failure = false;
  int histogram_value;
  if (url_loader_->NetError() == net::OK && url_loader_->ResponseInfo() &&
      url_loader_->ResponseInfo()->headers) {
    histogram_value = url_loader_->ResponseInfo()->headers->response_code();
  } else {
    histogram_value = url_loader_->NetError();
    net_failure = true;
  }
  base::UmaHistogramSparse("Gaia.ResponseCodesForOAuth2AccessToken",
                           histogram_value);
  if (net_failure) {
    OnGetTokenFailure(CreateAuthError(histogram_value));
    return;
  }

  int response_code = url_loader_->ResponseInfo()->headers->response_code();
  switch (response_code) {
    case net::HTTP_OK:
      break;
    case net::HTTP_PROXY_AUTHENTICATION_REQUIRED:
      NOTREACHED() << "HTTP 407 should be treated as a network error.";
      // If this ever happens in production, we treat it as a temporary error as
      // it is similar to a network error.
      OnGetTokenFailure(
          GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
      return;
    case net::HTTP_FORBIDDEN:
      // HTTP_FORBIDDEN (403) is treated as temporary error, because it may be
      // '403 Rate Limit Exeeded.'
      OnGetTokenFailure(
          GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
      return;
    case net::HTTP_BAD_REQUEST: {
      // HTTP_BAD_REQUEST (400) usually contains error as per
      // http://tools.ietf.org/html/rfc6749#section-5.2.
      std::string gaia_error;
      if (!ParseGetAccessTokenFailureResponse(std::move(response_body),
                                              &gaia_error)) {
        OnGetTokenFailure(
            GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR));
        return;
      }

      OAuth2ErrorCodesForHistogram access_error(
          OAuth2ErrorToHistogramValue(gaia_error));
      UMA_HISTOGRAM_ENUMERATION("Gaia.BadRequestTypeForOAuth2AccessToken",
                                access_error,
                                OAUTH2_ACCESS_ERROR_COUNT);

      OnGetTokenFailure(
          access_error == OAUTH2_ACCESS_ERROR_INVALID_GRANT
              ? GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                    GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                        CREDENTIALS_REJECTED_BY_SERVER)
              : GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR));
      return;
    }
    default: {
      if (response_code >= net::HTTP_INTERNAL_SERVER_ERROR) {
        // 5xx is always treated as transient.
        OnGetTokenFailure(GoogleServiceAuthError(
            GoogleServiceAuthError::SERVICE_UNAVAILABLE));
      } else {
        // The other errors are treated as permanent error.
        DLOG(ERROR) << "Unexpected persistent error: http_status="
                    << response_code;
        OnGetTokenFailure(
            GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                    CREDENTIALS_REJECTED_BY_SERVER));
      }
      return;
    }
  }

  // The request was successfully fetched and it returned OK.
  // Parse out the access token and the expiration time.
  std::string access_token;
  int expires_in;
  std::string id_token;
  if (!ParseGetAccessTokenSuccessResponse(
          std::move(response_body), &access_token, &expires_in, &id_token)) {
    DLOG(WARNING) << "Response doesn't match expected format";
    OnGetTokenFailure(
        GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
    return;
  }
  // The token will expire in |expires_in| seconds. Take a 10% error margin to
  // prevent reusing a token too close to its expiration date.
  OnGetTokenSuccess(OAuth2AccessTokenConsumer::TokenResponse(
      access_token,
      base::Time::Now() + base::TimeDelta::FromSeconds(9 * expires_in / 10),
      id_token));
}

void OAuth2AccessTokenFetcherImpl::OnGetTokenSuccess(
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  FireOnGetTokenSuccess(token_response);
}

void OAuth2AccessTokenFetcherImpl::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  state_ = ERROR_STATE;
  FireOnGetTokenFailure(error);
}

void OAuth2AccessTokenFetcherImpl::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  CHECK(state_ == GET_ACCESS_TOKEN_STARTED);
  EndGetAccessToken(std::move(response_body));
}

// static
GURL OAuth2AccessTokenFetcherImpl::MakeGetAccessTokenUrl() {
  return GaiaUrls::GetInstance()->oauth2_token_url();
}

// static
std::string OAuth2AccessTokenFetcherImpl::MakeGetAccessTokenBody(
    const std::string& client_id,
    const std::string& client_secret,
    const std::string& refresh_token,
    const std::vector<std::string>& scopes) {
  std::string enc_client_id = net::EscapeUrlEncodedData(client_id, true);
  std::string enc_client_secret =
      net::EscapeUrlEncodedData(client_secret, true);
  std::string enc_refresh_token =
      net::EscapeUrlEncodedData(refresh_token, true);
  if (scopes.empty()) {
    return base::StringPrintf(kGetAccessTokenBodyFormat,
                              enc_client_id.c_str(),
                              enc_client_secret.c_str(),
                              enc_refresh_token.c_str());
  } else {
    std::string scopes_string = base::JoinString(scopes, " ");
    return base::StringPrintf(
        kGetAccessTokenBodyWithScopeFormat,
        enc_client_id.c_str(),
        enc_client_secret.c_str(),
        enc_refresh_token.c_str(),
        net::EscapeUrlEncodedData(scopes_string, true).c_str());
  }
}

// static
bool OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenSuccessResponse(
    std::unique_ptr<std::string> response_body,
    std::string* access_token,
    int* expires_in,
    std::string* id_token) {
  CHECK(access_token);
  std::unique_ptr<base::DictionaryValue> value =
      ParseGetAccessTokenResponse(std::move(response_body));
  if (!value)
    return false;
  // ID token field is optional.
  value->GetString(kIdTokenKey, id_token);
  return value->GetString(kAccessTokenKey, access_token) &&
         value->GetInteger(kExpiresInKey, expires_in);
}

// static
bool OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenFailureResponse(
    std::unique_ptr<std::string> response_body,
    std::string* error) {
  CHECK(error);
  std::unique_ptr<base::DictionaryValue> value =
      ParseGetAccessTokenResponse(std::move(response_body));
  return value ? value->GetString(kErrorKey, error) : false;
}
