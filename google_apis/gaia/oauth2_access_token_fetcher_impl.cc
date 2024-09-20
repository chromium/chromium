// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

constexpr char kGetAccessTokenBodyFormat[] =
    "client_id=%s&"
    "client_secret=%s&"
    "grant_type=%s&"
    "%s=%s";

constexpr char kGetAccessTokenBodyWithScopeFormat[] =
    "client_id=%s&"
    "client_secret=%s&"
    "grant_type=%s&"
    "%s=%s&"
    "scope=%s";

constexpr char kGrantTypeAuthCode[] = "authorization_code";
constexpr char kGrantTypeRefreshToken[] = "refresh_token";

constexpr char kKeyAuthCode[] = "code";
constexpr char kKeyRefreshToken[] = "refresh_token";

constexpr char kAccessTokenKey[] = "access_token";
constexpr char krefreshTokenKey[] = "refresh_token";
constexpr char kExpiresInKey[] = "expires_in";
constexpr char kIdTokenKey[] = "id_token";
constexpr char kErrorKey[] = "error";
constexpr char kErrorSubTypeKey[] = "error_subtype";
constexpr char kErrorDescriptionKey[] = "error_description";

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kRaptRequiredError[] = "rapt_required";
constexpr char kInvalidRaptError[] = "invalid_rapt";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

OAuth2AccessTokenFetcherImpl::OAuth2Response
OAuth2ResponseErrorToOAuth2Response(const std::string& error) {
  if (error.empty())
    return OAuth2AccessTokenFetcherImpl::kErrorUnexpectedFormat;

  if (error == "invalid_request")
    return OAuth2AccessTokenFetcherImpl::kInvalidRequest;

  if (error == "invalid_client")
    return OAuth2AccessTokenFetcherImpl::kInvalidClient;

  if (error == "invalid_grant")
    return OAuth2AccessTokenFetcherImpl::kInvalidGrant;

  if (error == "unauthorized_client")
    return OAuth2AccessTokenFetcherImpl::kUnauthorizedClient;

  if (error == "unsupported_grant_type")
    return OAuth2AccessTokenFetcherImpl::kUnsuportedGrantType;

  if (error == "invalid_scope")
    return OAuth2AccessTokenFetcherImpl::kInvalidScope;

  if (error == "restricted_client")
    return OAuth2AccessTokenFetcherImpl::kRestrictedClient;

  if (error == "rate_limit_exceeded")
    return OAuth2AccessTokenFetcherImpl::kRateLimitExceeded;

  if (error == "internal_failure")
    return OAuth2AccessTokenFetcherImpl::kInternalFailure;

  return OAuth2AccessTokenFetcherImpl::kUnknownError;
}

static std::unique_ptr<network::SimpleURLLoader> CreateURLLoader(
    const GURL& url,
    const std::string& body,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();
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

GoogleServiceAuthError CreateErrorForInvalidGrant(
    const std::string& error_subtype,
    const std::string& error_description) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS cannot handle RAPT-type re-authentication requests and is
  // supposed to be excluded from RAPT re-authentication on the server side.
  // Just to be safe we need to handle this anyways. If we do not handle this,
  // any service requesting a RAPT re-auth protected OAuth scope can
  // potentially invalidate the entire ChromeOS session and send the user into
  // a never ending re-authentication loop.
  std::string error_subtype_lowercase = base::ToLowerASCII(error_subtype);
  if (error_subtype_lowercase == kRaptRequiredError ||
      error_subtype_lowercase == kInvalidRaptError) {
    return GoogleServiceAuthError::FromScopeLimitedUnrecoverableError(
        error_description);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Persistent error requiring the user to sign in again.
  return GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
      GoogleServiceAuthError::InvalidGaiaCredentialsReason::
          CREDENTIALS_REJECTED_BY_SERVER);
}

}  // namespace

OAuth2AccessTokenFetcherImpl::OAuth2AccessTokenFetcherImpl(
    OAuth2AccessTokenConsumer* consumer,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& refresh_token,
    const std::string& auth_code)
    : OAuth2AccessTokenFetcher(consumer),
      url_loader_factory_(url_loader_factory),
      refresh_token_(refresh_token),
      auth_code_(auth_code),
      state_(INITIAL) {
  // It's an error to specify neither a refresh token nor an auth code, or
  // to specify both at the same time.
  CHECK_NE(refresh_token_.empty(), auth_code_.empty());
}

OAuth2AccessTokenFetcherImpl::~OAuth2AccessTokenFetcherImpl() = default;

void OAuth2AccessTokenFetcherImpl::CancelRequest() {
  state_ = GET_ACCESS_TOKEN_CANCELED;
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
  url_loader_ = CreateURLLoader(
      GetAccessTokenURL(),
      MakeGetAccessTokenBody(client_id_, client_secret_, refresh_token_,
                             auth_code_, scopes_),
      GetTrafficAnnotationTag());
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

  bool net_failure = url_loader_->NetError() != net::OK ||
                     !url_loader_->ResponseInfo() ||
                     !url_loader_->ResponseInfo()->headers;

  if (net_failure) {
    int net_error = url_loader_->NetError();
    DLOG(WARNING) << "Could not reach Authorization servers: errno "
                  << net_error;
    RecordResponseCodeUma(net_error);
    OnGetTokenFailure(GoogleServiceAuthError::FromConnectionError(net_error));
    return;
  }

  int response_code = url_loader_->ResponseInfo()->headers->response_code();
  RecordResponseCodeUma(response_code);
  std::string response_str = response_body ? *response_body : "";

  if (response_code == net::HTTP_OK) {
    OAuth2AccessTokenConsumer::TokenResponse token_response;
    if (ParseGetAccessTokenSuccessResponse(response_str, &token_response)) {
      RecordOAuth2Response(OAuth2Response::kOk);
      OnGetTokenSuccess(token_response);
    } else {
      // Successful (net::HTTP_OK) unexpected format is considered as a
      // transient error.
      DLOG(WARNING) << "Response doesn't match expected format";
      RecordOAuth2Response(OAuth2Response::kOkUnexpectedFormat);
      OnGetTokenFailure(
          GoogleServiceAuthError::FromServiceUnavailable(response_str));
    }
    return;
  }

  // Request failed
  std::string oauth2_error, error_subtype, error_description;
  ParseGetAccessTokenFailureResponse(response_str, &oauth2_error,
                                     &error_subtype, &error_description);
  OAuth2Response response = OAuth2ResponseErrorToOAuth2Response(oauth2_error);
  RecordOAuth2Response(response);
  std::optional<GoogleServiceAuthError> error;

  switch (response) {
    case kOk:
    case kOkUnexpectedFormat:
      NOTREACHED();

    case kRateLimitExceeded:
    case kInternalFailure:
      // Transient error.
      error = GoogleServiceAuthError::FromServiceUnavailable(response_str);
      break;

    case kInvalidGrant:
      error = CreateErrorForInvalidGrant(error_subtype, error_description);
      break;

    case kInvalidScope:
    case kRestrictedClient:
      // Scope persistent error that can't be fixed by user action.
      error = GoogleServiceAuthError::FromScopeLimitedUnrecoverableError(
          response_str);
      break;

    case kInvalidRequest:
    case kInvalidClient:
    case kUnauthorizedClient:
    case kUnsuportedGrantType:
      DLOG(ERROR) << "Unexpected persistent error: error code = "
                  << oauth2_error;
      error = GoogleServiceAuthError::FromServiceError(response_str);
      break;

    case kUnknownError:
    case kErrorUnexpectedFormat:
      // Failed request with unknown error code or unexpected format is
      // treated as a persistent error case.
      DLOG(ERROR) << "Unexpected error/format: error code = " << oauth2_error;
      break;
  }

  if (!error.has_value()) {
    // Fallback to http status code.
    if (response_code == net::HTTP_OK) {
      NOTREACHED();
    } else if (response_code == net::HTTP_FORBIDDEN ||
               response_code == net::HTTP_PROXY_AUTHENTICATION_REQUIRED ||
               response_code >= net::HTTP_INTERNAL_SERVER_ERROR) {
      // HTTP_FORBIDDEN (403): is treated as transient error, because it may be
      //                       '403 Rate Limit Exeeded.'
      // HTTP_PROXY_AUTHENTICATION_REQUIRED (407): is treated as a network error
      // HTTP_INTERNAL_SERVER_ERROR: 5xx is always treated as transient.
      error = GoogleServiceAuthError::FromServiceUnavailable(response_str);
    } else {
      // HTTP_BAD_REQUEST (400) or other response codes are treated as
      // persistent errors.
      // HTTP_BAD_REQUEST errors usually contains errors as per
      // http://tools.ietf.org/html/rfc6749#section-5.2.
      if (response == kInvalidGrant) {
        error = CreateErrorForInvalidGrant(error_subtype, error_description);
      } else {
        error = GoogleServiceAuthError::FromServiceError(response_str);
      }
    }
  }

  if (error.has_value())
    OnGetTokenFailure(error.value());
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
  CHECK_EQ(state_, GET_ACCESS_TOKEN_STARTED);
  EndGetAccessToken(std::move(response_body));
}

// static
std::string OAuth2AccessTokenFetcherImpl::MakeGetAccessTokenBody(
    const std::string& client_id,
    const std::string& client_secret,
    const std::string& refresh_token,
    const std::string& auth_code,
    const std::vector<std::string>& scopes) {
  // It's an error to specify neither a refresh token nor an auth code, or
  // to specify both at the same time.
  CHECK_NE(refresh_token.empty(), auth_code.empty());

  std::string enc_client_id = base::EscapeUrlEncodedData(client_id, true);
  std::string enc_client_secret =
      base::EscapeUrlEncodedData(client_secret, true);

  const char* key = nullptr;
  const char* grant_type = nullptr;
  std::string enc_value;
  if (refresh_token.empty()) {
    key = kKeyAuthCode;
    grant_type = kGrantTypeAuthCode;
    enc_value = base::EscapeUrlEncodedData(auth_code, true);
  } else {
    key = kKeyRefreshToken;
    grant_type = kGrantTypeRefreshToken;
    enc_value = base::EscapeUrlEncodedData(refresh_token, true);
  }

  if (scopes.empty()) {
    return base::StringPrintf(kGetAccessTokenBodyFormat, enc_client_id.c_str(),
                              enc_client_secret.c_str(), grant_type, key,
                              enc_value.c_str());
  } else {
    std::string scopes_string = base::JoinString(scopes, " ");
    return base::StringPrintf(
        kGetAccessTokenBodyWithScopeFormat, enc_client_id.c_str(),
        enc_client_secret.c_str(), grant_type, key, enc_value.c_str(),
        base::EscapeUrlEncodedData(scopes_string, true).c_str());
  }
}

// static
bool OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenSuccessResponse(
    const std::string& response_body,
    OAuth2AccessTokenConsumer::TokenResponse* token_response) {
  CHECK(token_response);
  auto value = base::JSONReader::Read(response_body);
  if (!value.has_value() || !value->is_dict())
    return false;

  const base::Value::Dict* dict = value->GetIfDict();
  // Refresh and id token are optional and don't cause an error if missing.
  const std::string* refresh_token = dict->FindString(krefreshTokenKey);
  if (refresh_token)
    token_response->refresh_token = *refresh_token;

  const std::string* id_token = dict->FindString(kIdTokenKey);
  if (id_token)
    token_response->id_token = *id_token;

  const std::string* access_token = dict->FindString(kAccessTokenKey);
  if (access_token)
    token_response->access_token = *access_token;

  std::optional<int> expires_in = dict->FindInt(kExpiresInKey);
  bool ok = access_token && expires_in.has_value();
  if (ok) {
    // The token will expire in |expires_in| seconds. Take a 10% error margin to
    // prevent reusing a token too close to its expiration date.
    token_response->expiration_time =
        base::Time::Now() + base::Seconds(9 * expires_in.value() / 10);
  }
  return ok;
}

// static
bool OAuth2AccessTokenFetcherImpl::ParseGetAccessTokenFailureResponse(
    const std::string& response_body,
    std::string* error,
    std::string* error_subtype,
    std::string* error_description) {
  CHECK(error);
  CHECK(error_subtype);
  CHECK(error_description);
  auto value = base::JSONReader::Read(response_body);
  if (!value.has_value() || !value->is_dict())
    return false;

  const base::Value::Dict* dict = value->GetIfDict();
  const std::string* error_value = dict->FindString(kErrorKey);
  if (!error_value)
    return false;
  *error = *error_value;

  // Reset the error subtype and description just to be safe.
  *error_subtype = *error_description = std::string();
  const std::string* error_subtype_value = dict->FindString(kErrorSubTypeKey);
  if (error_subtype_value) {
    *error_subtype = *error_subtype_value;
  }

  const std::string* error_description_value =
      dict->FindString(kErrorDescriptionKey);
  if (error_description_value) {
    *error_description = *error_description_value;
  }

  return true;
}
