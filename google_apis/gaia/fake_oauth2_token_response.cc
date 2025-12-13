// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/fake_oauth2_token_response.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_response.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace gaia {

namespace {

std::string GetGetTokenErrorString(OAuth2Response error) {
  switch (error) {
    case OAuth2Response::kInvalidRequest:
      return "invalid_request";
    case OAuth2Response::kInvalidClient:
      return "invalid_client";
    case OAuth2Response::kInvalidGrant:
      return "invalid_grant";
    case OAuth2Response::kUnauthorizedClient:
      return "unauthorized_client";
    case OAuth2Response::kUnsuportedGrantType:
      return "unsupported_grant_type";
    case OAuth2Response::kInvalidScope:
      return "invalid_scope";
    case OAuth2Response::kRestrictedClient:
      return "restricted_client";
    case OAuth2Response::kRateLimitExceeded:
      return "rate_limit_exceeded";
    case OAuth2Response::kInternalFailure:
      return "internal_failure";
    case OAuth2Response::kAdminPolicyEnforced:
      return "admin_policy_enforced";
    case OAuth2Response::kAccessDenied:
      return "access_denied";
    case OAuth2Response::kConsentRequired:
      return "consent_required";
    case OAuth2Response::kTokenBindingChallenge:
      return "token_binding_challenge";
    case OAuth2Response::kUnknownError:
      return "unknown_test_error";
    case OAuth2Response::kOk:
    case OAuth2Response::kOkUnexpectedFormat:
    case OAuth2Response::kErrorUnexpectedFormat:
      NOTREACHED();
  }
}

std::string GetIssueTokenErrorReason(OAuth2Response error) {
  switch (error) {
    case OAuth2Response::kUnknownError:
      return "unknownTestError";
    case OAuth2Response::kInvalidRequest:
      return "badRequest";
    case OAuth2Response::kInvalidClient:
      return "invalidClientId";
    case OAuth2Response::kInvalidGrant:
      return "authError";
    case OAuth2Response::kInvalidScope:
      return "invalidScope";
    case OAuth2Response::kRestrictedClient:
      return "restrictedClient";
    case OAuth2Response::kRateLimitExceeded:
      return "rateLimitExceeded";
    case OAuth2Response::kInternalFailure:
      return "internalError";
    case OAuth2Response::kTokenBindingChallenge:
    case OAuth2Response::kOk:
    case OAuth2Response::kOkUnexpectedFormat:
    case OAuth2Response::kAccessDenied:
    case OAuth2Response::kAdminPolicyEnforced:
    case OAuth2Response::kUnauthorizedClient:
    case OAuth2Response::kErrorUnexpectedFormat:
    case OAuth2Response::kUnsuportedGrantType:
    case OAuth2Response::kConsentRequired:
      NOTREACHED();
  }
}

net::HttpStatusCode GetDefaultHttpStatusCodeForError(OAuth2Response error) {
  switch (error) {
    case OAuth2Response::kInvalidRequest:
    case OAuth2Response::kInvalidGrant:
    case OAuth2Response::kUnsuportedGrantType:
    case OAuth2Response::kInvalidScope:
    case OAuth2Response::kAdminPolicyEnforced:
      return net::HTTP_BAD_REQUEST;
    case OAuth2Response::kInvalidClient:
    case OAuth2Response::kUnauthorizedClient:
      return net::HTTP_UNAUTHORIZED;
    case OAuth2Response::kRestrictedClient:
    case OAuth2Response::kRateLimitExceeded:
      return net::HTTP_FORBIDDEN;
    case OAuth2Response::kAccessDenied:
      return net::HTTP_FORBIDDEN;
    case OAuth2Response::kInternalFailure:
      return net::HTTP_INTERNAL_SERVER_ERROR;
    case OAuth2Response::kOkUnexpectedFormat:
      return net::HTTP_OK;
    case OAuth2Response::kErrorUnexpectedFormat:
    case OAuth2Response::kUnknownError:
      return net::HTTP_INTERNAL_SERVER_ERROR;
    case OAuth2Response::kConsentRequired:
    case OAuth2Response::kTokenBindingChallenge:
    case OAuth2Response::kOk:
      NOTREACHED();
  }
}

std::string BuildIssueTokenSuccess(std::string_view access_token,
                                   base::TimeDelta expires_in) {
  auto response =
      base::Value::Dict()
          .Set("token", std::string(access_token))
          .Set("issueAdvice", "auto")
          .Set("expiresIn", base::NumberToString((expires_in.InSeconds())))
          .Set("grantedScopes", "http://ignored");
  return *base::WriteJson(response);
}

std::string BuildGetAccessTokenSuccess(std::string_view access_token,
                                       base::TimeDelta expires_in) {
  auto response =
      base::Value::Dict()
          .Set("access_token", access_token)
          .Set("expires_in", static_cast<int>(expires_in.InSeconds()));
  return *base::WriteJson(response);
}

std::string BuildIssueTokenError(OAuth2Response error) {
  using Dict = base::Value::Dict;
  using List = base::Value::List;
  auto response = Dict().Set(
      "error",
      Dict()
          .Set("code", GetDefaultHttpStatusCodeForError(error))
          .Set("errors", List().Append(Dict().Set(
                             "reason", GetIssueTokenErrorReason(error)))));
  return *base::WriteJson(response);
}

std::string BuildGetAccessTokenError(OAuth2Response error) {
  auto response =
      base::Value::Dict().Set("error", GetGetTokenErrorString(error));
  return *base::WriteJson(response);
}

}  // namespace

// static
FakeOAuth2TokenResponse FakeOAuth2TokenResponse::Success(
    std::string_view access_token,
    base::TimeDelta expires_in) {
  return FakeOAuth2TokenResponse(
      SuccessData{std::string(access_token), expires_in});
}

// static
FakeOAuth2TokenResponse FakeOAuth2TokenResponse::OAuth2Error(
    OAuth2Response error,
    std::optional<net::HttpStatusCode> http_status_code_override) {
  return FakeOAuth2TokenResponse(ErrorData{error, http_status_code_override});
}

// static
FakeOAuth2TokenResponse FakeOAuth2TokenResponse::NetError(
    net::Error net_error) {
  return FakeOAuth2TokenResponse(net_error);
}

FakeOAuth2TokenResponse::FakeOAuth2TokenResponse(
    const FakeOAuth2TokenResponse&) = default;
FakeOAuth2TokenResponse& FakeOAuth2TokenResponse::operator=(
    const FakeOAuth2TokenResponse&) = default;
FakeOAuth2TokenResponse::~FakeOAuth2TokenResponse() = default;

void FakeOAuth2TokenResponse::AddToTestURLLoaderFactory(
    network::TestURLLoaderFactory& test_url_loader_factory,
    std::optional<ApiEndpoint> exclusive_endpoint) const {
  std::vector<ApiEndpoint> endpoints;
  if (exclusive_endpoint.has_value()) {
    endpoints.push_back(*exclusive_endpoint);
  } else {
    endpoints = {ApiEndpoint::kGetToken, ApiEndpoint::kIssueToken};
  }

  for (const auto& endpoint : endpoints) {
    test_url_loader_factory.AddResponse(
        GetUrl(endpoint),
        network::CreateURLResponseHead(GetHttpStatusCode(endpoint)),
        GetBody(endpoint),
        network::URLLoaderCompletionStatus(GetNetError(endpoint)));
  }
}

std::string FakeOAuth2TokenResponse::GetBody(ApiEndpoint endpoint) const {
  return std::visit(
      absl::Overload(
          [endpoint](const SuccessData& success_data) {
            switch (endpoint) {
              case ApiEndpoint::kGetToken:
                return BuildGetAccessTokenSuccess(success_data.access_token,
                                                  success_data.expires_in);
              case ApiEndpoint::kIssueToken:
                return BuildIssueTokenSuccess(success_data.access_token,
                                              success_data.expires_in);
            }
            NOTREACHED();
          },
          [endpoint](const ErrorData& error_data) {
            if (error_data.error == OAuth2Response::kOkUnexpectedFormat ||
                error_data.error == OAuth2Response::kErrorUnexpectedFormat) {
              return std::string(R"({ "foo": })");
            }

            switch (endpoint) {
              case ApiEndpoint::kGetToken:
                return BuildGetAccessTokenError(error_data.error);
              case ApiEndpoint::kIssueToken:
                return BuildIssueTokenError(error_data.error);
            }
            NOTREACHED();
          },
          [](net::Error) { return std::string(); }),
      data_);
}

net::HttpStatusCode FakeOAuth2TokenResponse::GetHttpStatusCode(
    ApiEndpoint endpoint) const {
  return std::visit(
      absl::Overload(
          [](const ErrorData& error_data) {
            return error_data.http_status_code_override.value_or(
                GetDefaultHttpStatusCodeForError(error_data.error));
          },
          [](const auto&) { return net::HTTP_OK; }),
      data_);
}

net::Error FakeOAuth2TokenResponse::GetNetError(ApiEndpoint endpoint) const {
  return std::visit(
      absl::Overload([](net::Error net_error) { return net_error; },
                     [](const auto&) { return net::OK; }),
      data_);
}

GURL FakeOAuth2TokenResponse::GetUrl(ApiEndpoint endpoint) const {
  switch (endpoint) {
    case ApiEndpoint::kGetToken:
      return GaiaUrls::GetInstance()->oauth2_token_url();
    case ApiEndpoint::kIssueToken:
      return GaiaUrls::GetInstance()->oauth2_issue_token_url();
  }
  NOTREACHED();
}

FakeOAuth2TokenResponse::FakeOAuth2TokenResponse(ResponseData data)
    : data_(std::move(data)) {}

}  // namespace gaia
