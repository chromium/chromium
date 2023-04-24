// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_mint_token_flow.h"

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_constants.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

const char kValueFalse[] = "false";
const char kValueTrue[] = "true";
const char kResponseTypeValueNone[] = "none";
const char kResponseTypeValueToken[] = "token";

const char kOAuth2IssueTokenBodyFormat[] =
    "force=%s"
    "&response_type=%s"
    "&scope=%s"
    "&enable_granular_permissions=%s"
    "&client_id=%s"
    "&origin=%s"
    "&lib_ver=%s"
    "&release_channel=%s";
const char kOAuth2IssueTokenBodyFormatSelectedUserIdAddendum[] =
    "&selected_user_id=%s";
const char kOAuth2IssueTokenBodyFormatDeviceIdAddendum[] =
    "&device_id=%s&device_type=chrome";
const char kOAuth2IssueTokenBodyFormatConsentResultAddendum[] =
    "&consent_result=%s";
const char kIssueAdviceKey[] = "issueAdvice";
const char kIssueAdviceValueRemoteConsent[] = "remoteConsent";
const char kAccessTokenKey[] = "token";
const char kExpiresInKey[] = "expiresIn";
const char kGrantedScopesKey[] = "grantedScopes";
const char kError[] = "error";
const char kMessage[] = "message";

static GoogleServiceAuthError CreateAuthError(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  if (net_error == net::ERR_ABORTED)
    return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);

  if (net_error != net::OK) {
    DLOG(WARNING) << "Server returned error: errno " << net_error;
    return GoogleServiceAuthError::FromConnectionError(net_error);
  }

  std::string response_body;
  if (body)
    response_body = std::move(*body);

  absl::optional<base::Value> value = base::JSONReader::Read(response_body);
  if (!value || !value->is_dict()) {
    int http_response_code = -1;
    if (head && head->headers)
      http_response_code = head->headers->response_code();
    return GoogleServiceAuthError::FromUnexpectedServiceResponse(
        base::StringPrintf("Not able to parse a JSON object from "
                           "a service response. "
                           "HTTP Status of the response is: %d",
                           http_response_code));
  }
  const base::Value::Dict* error = value->GetDict().FindDict(kError);
  if (!error) {
    return GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to find a detailed error in a service response.");
  }
  const std::string* message = error->FindString(kMessage);
  if (!message) {
    return GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to find an error message within a service error.");
  }
  return GoogleServiceAuthError::FromServiceError(*message);
}

bool AreCookiesEqual(const net::CanonicalCookie& lhs,
                     const net::CanonicalCookie& rhs) {
  return lhs.IsEquivalent(rhs);
}

void RecordApiCallResult(OAuth2MintTokenApiCallResult result) {
  base::UmaHistogramEnumeration(kOAuth2MintTokenApiCallResultHistogram, result);
}

}  // namespace

const char kOAuth2MintTokenApiCallResultHistogram[] =
    "Signin.OAuth2MintToken.ApiCallResult";

RemoteConsentResolutionData::RemoteConsentResolutionData() = default;
RemoteConsentResolutionData::~RemoteConsentResolutionData() = default;
RemoteConsentResolutionData::RemoteConsentResolutionData(
    const RemoteConsentResolutionData& other) = default;
RemoteConsentResolutionData& RemoteConsentResolutionData::operator=(
    const RemoteConsentResolutionData& other) = default;

bool RemoteConsentResolutionData::operator==(
    const RemoteConsentResolutionData& rhs) const {
  return url == rhs.url &&
         base::ranges::equal(cookies, rhs.cookies, &AreCookiesEqual);
}

OAuth2MintTokenFlow::Parameters::Parameters() : mode(MODE_ISSUE_ADVICE) {}

OAuth2MintTokenFlow::Parameters::Parameters(
    const std::string& eid,
    const std::string& cid,
    const std::vector<std::string>& scopes_arg,
    bool enable_granular_permissions,
    const std::string& device_id,
    const std::string& selected_user_id,
    const std::string& consent_result,
    const std::string& version,
    const std::string& channel,
    Mode mode_arg)
    : extension_id(eid),
      client_id(cid),
      scopes(scopes_arg),
      enable_granular_permissions(enable_granular_permissions),
      device_id(device_id),
      selected_user_id(selected_user_id),
      consent_result(consent_result),
      version(version),
      channel(channel),
      mode(mode_arg) {}

OAuth2MintTokenFlow::Parameters::Parameters(const Parameters& other) = default;

OAuth2MintTokenFlow::Parameters::~Parameters() {}

OAuth2MintTokenFlow::OAuth2MintTokenFlow(Delegate* delegate,
                                         const Parameters& parameters)
    : delegate_(delegate), parameters_(parameters) {}

OAuth2MintTokenFlow::~OAuth2MintTokenFlow() { }

void OAuth2MintTokenFlow::ReportSuccess(
    const std::string& access_token,
    const std::set<std::string>& granted_scopes,
    int time_to_live) {
  if (delegate_)
    delegate_->OnMintTokenSuccess(access_token, granted_scopes, time_to_live);

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
      base::EscapeUrlEncodedData(parameters_.extension_id, true).c_str(),
      base::EscapeUrlEncodedData(parameters_.version, true).c_str(),
      base::EscapeUrlEncodedData(parameters_.channel, true).c_str());
  if (!parameters_.device_id.empty()) {
    body.append(base::StringPrintf(
        kOAuth2IssueTokenBodyFormatDeviceIdAddendum,
        base::EscapeUrlEncodedData(parameters_.device_id, true).c_str()));
  }
  if (!parameters_.selected_user_id.empty()) {
    body.append(base::StringPrintf(
        kOAuth2IssueTokenBodyFormatSelectedUserIdAddendum,
        base::EscapeUrlEncodedData(parameters_.selected_user_id, true)
            .c_str()));
  }
  if (!parameters_.consent_result.empty()) {
    body.append(base::StringPrintf(
        kOAuth2IssueTokenBodyFormatConsentResultAddendum,
        base::EscapeUrlEncodedData(parameters_.consent_result, true).c_str()));
  }
  return body;
}

void OAuth2MintTokenFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  std::string response_body;
  if (body)
    response_body = std::move(*body);

  absl::optional<base::Value> value = base::JSONReader::Read(response_body);
  if (!value || !value->is_dict()) {
    RecordApiCallResult(OAuth2MintTokenApiCallResult::kParseJsonFailure);
    ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to parse a JSON object from a service response."));
    return;
  }

  base::Value::Dict& dict = value->GetDict();
  std::string* issue_advice_value = dict.FindString(kIssueAdviceKey);
  if (!issue_advice_value) {
    RecordApiCallResult(
        OAuth2MintTokenApiCallResult::kIssueAdviceKeyNotFoundFailure);
    ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to find an issueAdvice in a service response."));
    return;
  }

  if (*issue_advice_value == kIssueAdviceValueRemoteConsent) {
    RemoteConsentResolutionData resolution_data;
    if (ParseRemoteConsentResponse(dict, &resolution_data)) {
      RecordApiCallResult(OAuth2MintTokenApiCallResult::kRemoteConsentSuccess);
      ReportRemoteConsentSuccess(resolution_data);
    } else {
      RecordApiCallResult(
          OAuth2MintTokenApiCallResult::kParseRemoteConsentFailure);
      ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
          "Not able to parse the contents of remote consent from a service "
          "response."));
    }
    return;
  }

  std::string access_token;
  std::set<std::string> granted_scopes;
  int time_to_live;
  if (ParseMintTokenResponse(dict, &access_token, &granted_scopes,
                             &time_to_live)) {
    RecordApiCallResult(OAuth2MintTokenApiCallResult::kMintTokenSuccess);
    ReportSuccess(access_token, granted_scopes, time_to_live);
  } else {
    RecordApiCallResult(OAuth2MintTokenApiCallResult::kParseMintTokenFailure);
    ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to parse the contents of access token "
        "from a service response."));
  }

  // |this| may be deleted!
}

void OAuth2MintTokenFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  RecordApiCallResult(OAuth2MintTokenApiCallResult::kApiCallFailure);
  ReportFailure(CreateAuthError(net_error, head, std::move(body)));
}

// static
bool OAuth2MintTokenFlow::ParseMintTokenResponse(
    const base::Value::Dict& dict,
    std::string* access_token,
    std::set<std::string>* granted_scopes,
    int* time_to_live) {
  CHECK(access_token);
  CHECK(granted_scopes);
  CHECK(time_to_live);

  const std::string* ttl_string = dict.FindString(kExpiresInKey);
  if (!ttl_string || !base::StringToInt(*ttl_string, time_to_live))
    return false;

  const std::string* access_token_ptr = dict.FindString(kAccessTokenKey);
  if (!access_token_ptr)
    return false;

  *access_token = *access_token_ptr;

  const std::string* granted_scopes_string = dict.FindString(kGrantedScopesKey);

  if (!granted_scopes_string)
    return false;

  const std::vector<std::string> granted_scopes_vector =
      base::SplitString(*granted_scopes_string, " ", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  if (granted_scopes_vector.empty())
    return false;

  const std::set<std::string> granted_scopes_set(granted_scopes_vector.begin(),
                                                 granted_scopes_vector.end());
  *granted_scopes = std::move(granted_scopes_set);
  return true;
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
      absl::optional<bool> is_secure = cookie_dict->FindBool("isSecure");
      absl::optional<bool> is_http_only = cookie_dict->FindBool("isHttpOnly");
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
              time_now, expiration_time, time_now,
              is_secure ? *is_secure : false,
              is_http_only ? *is_http_only : false,
              net::StringToCookieSameSite(same_site ? *same_site : ""),
              net::COOKIE_PRIORITY_DEFAULT, /* same_party */ false,
              /* partition_key */ absl::nullopt);
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
