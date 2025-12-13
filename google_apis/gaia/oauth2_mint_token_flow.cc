// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_mint_token_flow.h"

#include <stddef.h>

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "google_apis/gaia/oauth2_response.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_constants.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

constexpr char kValueFalse[] = "false";
constexpr char kValueTrue[] = "true";
constexpr char kResponseTypeValueNone[] = "none";
constexpr char kResponseTypeValueToken[] = "token";

constexpr char kOAuth2IssueTokenBodyFormat[] =
    "force=%s"
    "&response_type=%s"
    "&scope=%s"
    "&enable_granular_permissions=%s"
    "&client_id=%s"
    "&lib_ver=%s"
    "&release_channel=%s";
constexpr char kOAuth2IssueTokenBodyFormatExtensionIdAddendum[] = "&origin=%s";
constexpr char kOAuth2IssueTokenBodyFormatSelectedUserIdAddendum[] =
    "&selected_user_id=%s";
constexpr char kOAuth2IssueTokenBodyFormatDeviceIdAddendum[] =
    "&device_id=%s&device_type=chrome";
constexpr char kOAuth2IssueTokenBodyFormatConsentResultAddendum[] =
    "&consent_result=%s";
constexpr char kIssueAdviceKey[] = "issueAdvice";
constexpr char kIssueAdviceValueRemoteConsent[] = "remoteConsent";
constexpr char kAccessTokenKey[] = "token";
constexpr char kExpiresInKey[] = "expiresIn";
constexpr char kGrantedScopesKey[] = "grantedScopes";
constexpr char kError[] = "error";
constexpr char kErrors[] = "errors";
constexpr char kMessage[] = "message";
constexpr char kReason[] = "reason";

constexpr char kTokenBindingChallengeHeader[] =
    "X-Chrome-Auth-Token-Binding-Challenge";
constexpr char kTokenBindingResponseKey[] = "tokenBindingResponse";
constexpr char kDirectedResponseKey[] = "directedResponse";

constexpr auto kOAuth2ResponseByErrorReason =
    base::MakeFixedFlatMap<std::string_view, OAuth2Response>({
        {"authError", OAuth2Response::kInvalidGrant},
        {"badRequest", OAuth2Response::kInvalidRequest},
        {"internalError", OAuth2Response::kInternalFailure},
        {"invalidClientId", OAuth2Response::kInvalidClient},
        {"invalidScope", OAuth2Response::kInvalidScope},
        {"rateLimitExceeded", OAuth2Response::kRateLimitExceeded},
        {"restrictedClient", OAuth2Response::kRestrictedClient},
    });

const std::string* FindMessageInErrorResponse(
    const std::optional<base::Value::Dict>& response) {
  if (!response) {
    return nullptr;
  }

  const base::Value::Dict* error = response->FindDict(kError);
  if (!error) {
    return nullptr;
  }

  return error->FindString(kMessage);
}

const std::string* FindReasonInErrorResponse(
    const std::optional<base::Value::Dict>& response) {
  if (!response) {
    return nullptr;
  }

  const base::Value::Dict* error = response->FindDict(kError);
  if (!error) {
    return nullptr;
  }

  const base::Value::List* errors = error->FindList(kErrors);
  if (!errors || errors->empty()) {
    return nullptr;
  }

  const base::Value::Dict* first_error = errors->front().GetIfDict();
  if (!first_error) {
    return nullptr;
  }

  return first_error->FindString(kReason);
}

OAuth2Response GetOAuth2ResponseFromErrorReason(const std::string* reason) {
  using enum OAuth2Response;
  if (!reason) {
    return kErrorUnexpectedFormat;
  }

  auto it = kOAuth2ResponseByErrorReason.find(*reason);
  if (it != kOAuth2ResponseByErrorReason.end()) {
    return it->second;
  }

  return kUnknownError;
}

GoogleServiceAuthError ConvertErrorOAuth2ResponseToAuthError(
    OAuth2Response oauth2_response,
    int http_response_code,
    const std::string& display_message) {
  using enum OAuth2Response;
  switch (oauth2_response) {
    case kOk:
    case kOkUnexpectedFormat:
    case kAccessDenied:
    case kAdminPolicyEnforced:
    case kUnauthorizedClient:
    case kUnsuportedGrantType:
    case kConsentRequired:
    case kTokenBindingChallenge:
      NOTREACHED();

    // Transient errors:
    case kRateLimitExceeded:
    case kInternalFailure:
      return GoogleServiceAuthError::FromServiceUnavailable(display_message);

    // Scope persistent errors that can't be fixed by user action:
    case kInvalidScope:
      return GoogleServiceAuthError::FromScopeLimitedUnrecoverableErrorReason(
          GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
              kInvalidScope);
    case kRestrictedClient:
      return GoogleServiceAuthError::FromScopeLimitedUnrecoverableErrorReason(
          GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
              kRestrictedClient);

    // Persistent errors:
    case kInvalidGrant:
      return GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
    case kInvalidRequest:
    case kInvalidClient:
      return GoogleServiceAuthError::FromServiceError(display_message);

    // Unknown errors fall back to HTTP response codes:
    case kUnknownError:
    case kErrorUnexpectedFormat:
      break;
  }

  if (http_response_code == net::HTTP_PROXY_AUTHENTICATION_REQUIRED ||
      http_response_code >= net::HTTP_INTERNAL_SERVER_ERROR) {
    // HTTP_PROXY_AUTHENTICATION_REQUIRED (407): is treated as a network error.
    // HTTP_INTERNAL_SERVER_ERROR: 5xx is always treated as transient.
    return GoogleServiceAuthError::FromServiceUnavailable(display_message);
  }

  return GoogleServiceAuthError::FromServiceError(display_message);
}

struct OAuth2ErrorDetails {
  GoogleServiceAuthError auth_error;
  std::optional<OAuth2Response> oauth2_response;
};

OAuth2ErrorDetails ParseErrorResponse(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::optional<std::string> body) {
  if (net_error == net::ERR_ABORTED) {
    return {GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
            std::nullopt};
  }

  if (net_error != net::OK || !head || !head->headers) {
    DLOG(WARNING) << "Server returned error: errno " << net_error;
    return {GoogleServiceAuthError::FromConnectionError(net_error),
            std::nullopt};
  }

  std::optional<base::Value::Dict> dict = base::JSONReader::ReadDict(
      body.value_or(""), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  const std::string* message = FindMessageInErrorResponse(dict);
  const std::string* reason = FindReasonInErrorResponse(dict);
  OAuth2Response oauth2_response = GetOAuth2ResponseFromErrorReason(reason);
  int http_response_code = head->headers->response_code();
  auto GetDisplayMessage = [&] {
    if (message) {
      return *message;
    } else if (reason) {
      return *reason;
    } else {
      return base::StringPrintf("Couldn't parse an error. HTTP code %d",
                                http_response_code);
    }
  };
  std::string display_message = GetDisplayMessage();
  GoogleServiceAuthError error = ConvertErrorOAuth2ResponseToAuthError(
      oauth2_response, http_response_code, display_message);
  return {std::move(error), oauth2_response};
}

std::string FindTokenBindingChallenge(
    int net_error,
    const network::mojom::URLResponseHead* head) {
  if (net_error != net::OK || !head || !head->headers) {
    return std::string();
  }

  return head->headers->GetNormalizedHeader(kTokenBindingChallengeHeader)
      .value_or(std::string());
}

void RecordApiCallMetrics(OAuth2MintTokenApiCallResult result,
                          std::optional<OAuth2Response> response) {
  // TODO(crbug.com/401211492): remove the "ApiCallResult" histogram in favor of
  // the "Response" one.
  base::UmaHistogramEnumeration("Signin.OAuth2MintToken.ApiCallResult", result);
  if (response) {
    base::UmaHistogramEnumeration("Signin.OAuth2MintToken.Response", *response);
  }
}

}  // namespace

RemoteConsentResolutionData::RemoteConsentResolutionData() = default;
RemoteConsentResolutionData::~RemoteConsentResolutionData() = default;
RemoteConsentResolutionData::RemoteConsentResolutionData(
    const RemoteConsentResolutionData& other) = default;
RemoteConsentResolutionData& RemoteConsentResolutionData::operator=(
    const RemoteConsentResolutionData& other) = default;

OAuth2MintTokenFlow::Parameters::Parameters() = default;

// static
OAuth2MintTokenFlow::Parameters
OAuth2MintTokenFlow::Parameters::CreateForExtensionFlow(
    std::string_view extension_id,
    std::string_view client_id,
    base::span<const std::string_view> scopes,
    Mode mode,
    bool enable_granular_permissions,
    std::string_view version,
    std::string_view channel,
    std::string_view device_id,
    const GaiaId& selected_user_id,
    std::string_view consent_result) {
  Parameters parameters;
  parameters.extension_id = extension_id;
  parameters.client_id = client_id;
  parameters.scopes = std::vector<std::string>(scopes.begin(), scopes.end());
  parameters.mode = mode;
  parameters.enable_granular_permissions = enable_granular_permissions;
  parameters.version = version;
  parameters.channel = channel;
  parameters.device_id = device_id;
  parameters.selected_user_id = selected_user_id;
  parameters.consent_result = consent_result;
  return parameters;
}

// static
OAuth2MintTokenFlow::Parameters
OAuth2MintTokenFlow::Parameters::CreateForClientFlow(
    std::string_view client_id,
    base::span<const std::string_view> scopes,
    std::string_view version,
    std::string_view channel,
    std::string_view device_id,
    std::string_view bound_oauth_token) {
  Parameters parameters;
  parameters.client_id = client_id;
  parameters.scopes = std::vector<std::string>(scopes.begin(), scopes.end());
  parameters.mode = MODE_MINT_TOKEN_NO_FORCE;
  parameters.version = version;
  parameters.channel = channel;
  parameters.device_id = device_id;
  parameters.bound_oauth_token = bound_oauth_token;
  return parameters;
}

OAuth2MintTokenFlow::Parameters::Parameters(Parameters&& other) noexcept =
    default;
OAuth2MintTokenFlow::Parameters& OAuth2MintTokenFlow::Parameters::operator=(
    Parameters&& other) noexcept = default;

OAuth2MintTokenFlow::Parameters::Parameters(const Parameters& other) = default;
OAuth2MintTokenFlow::Parameters::~Parameters() = default;

OAuth2MintTokenFlow::Parameters OAuth2MintTokenFlow::Parameters::Clone() {
  return Parameters(*this);
}

OAuth2MintTokenFlow::MintTokenResult::MintTokenResult() = default;
OAuth2MintTokenFlow::MintTokenResult::~MintTokenResult() = default;
OAuth2MintTokenFlow::MintTokenResult::MintTokenResult(
    MintTokenResult&& other) noexcept = default;
OAuth2MintTokenFlow::MintTokenResult&
OAuth2MintTokenFlow::MintTokenResult::operator=(
    MintTokenResult&& other) noexcept = default;

OAuth2MintTokenFlow::OAuth2MintTokenFlow(Delegate* delegate,
                                         Parameters parameters)
    : delegate_(delegate), parameters_(std::move(parameters)) {}

OAuth2MintTokenFlow::~OAuth2MintTokenFlow() = default;

void OAuth2MintTokenFlow::ReportSuccess(const MintTokenResult& result) {
  if (delegate_) {
    delegate_->OnMintTokenSuccess(result);
  }

  // |this| may already be deleted.
}

void OAuth2MintTokenFlow::ReportRemoteConsentSuccess(
    const RemoteConsentResolutionData& resolution_data) {
  if (delegate_)
    delegate_->OnRemoteConsentSuccess(resolution_data);

  // |this| may already be deleted;
}

void OAuth2MintTokenFlow::ReportFailure(
    const GoogleServiceAuthError& error) {
  if (delegate_)
    delegate_->OnMintTokenFailure(error);

  // |this| may already be deleted.
}

GURL OAuth2MintTokenFlow::CreateApiCallUrl() {
  return GaiaUrls::GetInstance()->oauth2_issue_token_url();
}

net::HttpRequestHeaders OAuth2MintTokenFlow::CreateApiCallHeaders() {
  net::HttpRequestHeaders headers;
  headers.SetHeader("X-OAuth-Client-ID", parameters_.client_id);
  return headers;
}

std::string OAuth2MintTokenFlow::CreateApiCallBody() {
  const char* force_value = (parameters_.mode == MODE_MINT_TOKEN_FORCE ||
                             parameters_.mode == MODE_RECORD_GRANT)
                                ? kValueTrue
                                : kValueFalse;
  const char* response_type_value =
      (parameters_.mode == MODE_MINT_TOKEN_NO_FORCE ||
       parameters_.mode == MODE_MINT_TOKEN_FORCE)
          ? kResponseTypeValueToken : kResponseTypeValueNone;
  const char* enable_granular_permissions_value =
      parameters_.enable_granular_permissions ? kValueTrue : kValueFalse;
  std::string body = base::StringPrintf(
      kOAuth2IssueTokenBodyFormat,
      base::EscapeUrlEncodedData(force_value, true).c_str(),
      base::EscapeUrlEncodedData(response_type_value, true).c_str(),
      base::EscapeUrlEncodedData(base::JoinString(parameters_.scopes, " "),
                                 true)
          .c_str(),
      base::EscapeUrlEncodedData(enable_granular_permissions_value, true)
          .c_str(),
      base::EscapeUrlEncodedData(parameters_.client_id, true).c_str(),
      base::EscapeUrlEncodedData(parameters_.version, true).c_str(),
      base::EscapeUrlEncodedData(parameters_.channel, true).c_str());
  if (!parameters_.extension_id.empty()) {
    body.append(base::StringPrintf(
        kOAuth2IssueTokenBodyFormatExtensionIdAddendum,
        base::EscapeUrlEncodedData(parameters_.extension_id, true).c_str()));
  }
  if (!parameters_.device_id.empty()) {
    body.append(base::StringPrintf(
        kOAuth2IssueTokenBodyFormatDeviceIdAddendum,
        base::EscapeUrlEncodedData(parameters_.device_id, true).c_str()));
  }
  if (!parameters_.selected_user_id.empty()) {
    body.append(
        base::StringPrintf(kOAuth2IssueTokenBodyFormatSelectedUserIdAddendum,
                           base::EscapeUrlEncodedData(
                               parameters_.selected_user_id.ToString(), true)
                               .c_str()));
  }
  if (!parameters_.consent_result.empty()) {
    body.append(base::StringPrintf(
        kOAuth2IssueTokenBodyFormatConsentResultAddendum,
        base::EscapeUrlEncodedData(parameters_.consent_result, true).c_str()));
  }
  return body;
}

std::string OAuth2MintTokenFlow::CreateAuthorizationHeaderValue(
    const std::string& access_token) {
  if (!parameters_.bound_oauth_token.empty()) {
    // Replace a regular token with the one containing binding assertion.
    return base::StrCat({"BoundOAuth ", parameters_.bound_oauth_token});
  }

  // Call the base class method to get a regular authorization value.
  return OAuth2ApiCallFlow::CreateAuthorizationHeaderValue(access_token);
}

void OAuth2MintTokenFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::optional<std::string> body) {
  std::optional<base::Value::Dict> dict = base::JSONReader::ReadDict(
      body.value_or(""), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!dict) {
    RecordApiCallMetrics(OAuth2MintTokenApiCallResult::kParseJsonFailure,
                         OAuth2Response::kOkUnexpectedFormat);
    ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to parse a JSON object from a service response."));
    return;
  }

  std::string* issue_advice_value = dict->FindString(kIssueAdviceKey);
  if (!issue_advice_value) {
    RecordApiCallMetrics(
        OAuth2MintTokenApiCallResult::kIssueAdviceKeyNotFoundFailure,
        OAuth2Response::kOkUnexpectedFormat);
    ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to find an issueAdvice in a service response."));
    return;
  }

  if (*issue_advice_value == kIssueAdviceValueRemoteConsent) {
    RemoteConsentResolutionData resolution_data;
    if (ParseRemoteConsentResponse(*dict, &resolution_data)) {
      RecordApiCallMetrics(OAuth2MintTokenApiCallResult::kRemoteConsentSuccess,
                           OAuth2Response::kConsentRequired);
      ReportRemoteConsentSuccess(resolution_data);
    } else {
      RecordApiCallMetrics(
          OAuth2MintTokenApiCallResult::kParseRemoteConsentFailure,
          OAuth2Response::kOkUnexpectedFormat);
      ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
          "Not able to parse the contents of remote consent from a service "
          "response."));
    }
    return;
  }

  if (std::optional<MintTokenResult> result = ParseMintTokenResponse(*dict);
      result.has_value()) {
    RecordApiCallMetrics(OAuth2MintTokenApiCallResult::kMintTokenSuccess,
                         OAuth2Response::kOk);
    ReportSuccess(result.value());
  } else {
    RecordApiCallMetrics(OAuth2MintTokenApiCallResult::kParseMintTokenFailure,
                         OAuth2Response::kOkUnexpectedFormat);
    ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to parse the contents of access token "
        "from a service response."));
  }

  // |this| may be deleted!
}

void OAuth2MintTokenFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::optional<std::string> body) {
  std::string challenge = FindTokenBindingChallenge(net_error, head);
  if (!challenge.empty()) {
    RecordApiCallMetrics(
        OAuth2MintTokenApiCallResult::kChallengeResponseRequiredFailure,
        OAuth2Response::kTokenBindingChallenge);
    ReportFailure(GoogleServiceAuthError::FromTokenBindingChallenge(challenge));
    return;
  }

  OAuth2ErrorDetails error_details =
      ParseErrorResponse(net_error, head, std::move(body));
  RecordApiCallMetrics(OAuth2MintTokenApiCallResult::kApiCallFailure,
                       error_details.oauth2_response);
  ReportFailure(std::move(error_details.auth_error));
}

// static
std::optional<OAuth2MintTokenFlow::MintTokenResult>
OAuth2MintTokenFlow::ParseMintTokenResponse(const base::Value::Dict& dict) {
  MintTokenResult result;

  const std::string* ttl_string = dict.FindString(kExpiresInKey);
  int ttl_seconds = 0;
  if (!ttl_string || !base::StringToInt(*ttl_string, &ttl_seconds)) {
    return std::nullopt;
  }
  result.time_to_live = base::Seconds(ttl_seconds);

  const std::string* access_token_ptr = dict.FindString(kAccessTokenKey);
  if (!access_token_ptr) {
    return std::nullopt;
  }
  result.access_token = *access_token_ptr;

  const std::string* granted_scopes_string = dict.FindString(kGrantedScopesKey);
  if (!granted_scopes_string) {
    return std::nullopt;
  }
  const std::vector<std::string> granted_scopes_vector =
      base::SplitString(*granted_scopes_string, " ", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  if (granted_scopes_vector.empty()) {
    return std::nullopt;
  }
  result.granted_scopes.insert(granted_scopes_vector.begin(),
                               granted_scopes_vector.end());

  const base::Value::Dict* token_binding_response =
      dict.FindDict(kTokenBindingResponseKey);
  // The presence of `kDirectedResponseKey` indicates that the returned token is
  // encrypted to the public key provided by the client earlier.
  result.is_token_encrypted =
      token_binding_response &&
      token_binding_response->FindDict(kDirectedResponseKey);

  return result;
}

// static
bool OAuth2MintTokenFlow::ParseRemoteConsentResponse(
    const base::Value::Dict& dict,
    RemoteConsentResolutionData* resolution_data) {
  CHECK(resolution_data);

  const base::Value::Dict* resolution_dict = dict.FindDict("resolutionData");
  if (!resolution_dict)
    return false;

  const std::string* resolution_approach =
      resolution_dict->FindString("resolutionApproach");
  if (!resolution_approach || *resolution_approach != "resolveInBrowser")
    return false;

  const std::string* resolution_url_string =
      resolution_dict->FindString("resolutionUrl");
  if (!resolution_url_string)
    return false;
  GURL resolution_url(*resolution_url_string);
  if (!resolution_url.is_valid())
    return false;

  const base::Value::List* browser_cookies =
      resolution_dict->FindList("browserCookies");

  base::Time time_now = base::Time::Now();
  bool success = true;
  std::vector<net::CanonicalCookie> cookies;
  if (browser_cookies) {
    for (const auto& cookie_value : *browser_cookies) {
      const base::Value::Dict* cookie_dict = cookie_value.GetIfDict();
      if (!cookie_dict) {
        success = false;
        break;
      }

      // Required parameters:
      const std::string* name = cookie_dict->FindString("name");
      const std::string* value = cookie_dict->FindString("value");
      const std::string* domain = cookie_dict->FindString("domain");

      if (!name || !value || !domain) {
        success = false;
        break;
      }

      // Optional parameters:
      const std::string* path = cookie_dict->FindString("path");
      const std::string* max_age_seconds =
          cookie_dict->FindString("maxAgeSeconds");
      std::optional<bool> is_secure = cookie_dict->FindBool("isSecure");
      std::optional<bool> is_http_only = cookie_dict->FindBool("isHttpOnly");
      const std::string* same_site = cookie_dict->FindString("sameSite");

      int64_t max_age = -1;
      if (max_age_seconds && !base::StringToInt64(*max_age_seconds, &max_age)) {
        success = false;
        break;
      }

      base::Time expiration_time = base::Time();
      if (max_age > 0)
        expiration_time = time_now + base::Seconds(max_age);

      std::unique_ptr<net::CanonicalCookie> cookie =
          net::CanonicalCookie::CreateSanitizedCookie(
              resolution_url, *name, *value, *domain, path ? *path : "/",
              time_now, expiration_time, time_now, (is_secure && *is_secure),
              (is_http_only && *is_http_only),
              net::StringToCookieSameSite(same_site ? *same_site : "").first,
              net::COOKIE_PRIORITY_DEFAULT,
              /* partition_key */ std::nullopt, /*status=*/nullptr);
      cookies.push_back(*cookie);
    }
  }

  if (success) {
    resolution_data->url = std::move(resolution_url);
    resolution_data->cookies = std::move(cookies);
  }

  return success;
}

net::PartialNetworkTrafficAnnotationTag
OAuth2MintTokenFlow::GetNetworkTrafficAnnotationTag() {
  return net::DefinePartialNetworkTrafficAnnotation(
      "oauth2_mint_token_flow", "oauth2_api_call_flow", R"(
      semantics {
        sender: "Chrome Identity API"
        description:
          "Requests a token from gaia allowing an app or extension to act as "
          "the user when calling other google APIs."
        trigger: "API call from the app/extension."
        data:
          "User's login token, the identity of a chrome app/extension, and a "
          "list of oauth scopes requested by the app/extension."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        setting:
          "This feature cannot be disabled by settings, however the request is "
          "made only for signed-in users."
        chrome_policy {
          SigninAllowed {
            policy_options {mode: MANDATORY}
            SigninAllowed: false
          }
        }
      })");
}
