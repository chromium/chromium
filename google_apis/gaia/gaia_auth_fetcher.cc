// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_auth_fetcher.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/gaia/bound_oauth_token.pb.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_id_token_decoder.h"
#include "google_apis/gaia/oauth_multilogin_result.h"
#include "net/base/isolation_info.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

constexpr char kFormEncodedContentType[] = "application/x-www-form-urlencoded";
constexpr char kJsonContentType[] = "application/json;charset=UTF-8";

std::unique_ptr<const GaiaAuthConsumer::ClientOAuthResult>
ExtractOAuth2TokenPairResponse(const std::string& data) {
  std::optional<base::Value::Dict> dict =
      base::JSONReader::ReadDict(data, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!dict) {
    return nullptr;
  }

  std::string* refresh_token = dict->FindString("refresh_token");
  std::string* access_token = dict->FindString("access_token");
  std::optional<int> expires_in_secs = dict->FindInt("expires_in");
  if (!refresh_token || !access_token || !expires_in_secs) {
    return nullptr;
  }

  // Extract ID token when obtaining refresh token. Do not fail if absent,
  // but log to keep track.
  std::string* id_token = dict->FindString("id_token");
  if (!id_token)
    LOG(ERROR) << "Missing ID token on refresh token fetch response.";
  gaia::TokenServiceFlags service_flags =
      gaia::ParseServiceFlags(id_token ? *id_token : std::string());

  bool is_bound_to_key = false;
  // If present, indicates special rules of how the token must be used.
  std::string* refresh_token_type = dict->FindString("refresh_token_type");
  if (refresh_token_type &&
      base::EqualsCaseInsensitiveASCII(*refresh_token_type, "bound_to_key")) {
    is_bound_to_key = true;
  }

  return std::make_unique<const GaiaAuthConsumer::ClientOAuthResult>(
      *refresh_token, *access_token, *expires_in_secs,
      service_flags.is_child_account,
      service_flags.is_under_advanced_protection, is_bound_to_key);
}

// Parses server responses for token revocation.
GaiaAuthConsumer::TokenRevocationStatus
GetTokenRevocationStatusFromResponseData(const std::string& data,
                                         int response_code) {
  if (response_code == net::HTTP_OK)
    return GaiaAuthConsumer::TokenRevocationStatus::kSuccess;

  if (response_code == net::HTTP_INTERNAL_SERVER_ERROR)
    return GaiaAuthConsumer::TokenRevocationStatus::kServerError;

  std::optional<base::Value::Dict> dict =
      base::JSONReader::ReadDict(data, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!dict) {
    return GaiaAuthConsumer::TokenRevocationStatus::kUnknownError;
  }

  std::string* error = dict->FindString("error");
  if (!error)
    return GaiaAuthConsumer::TokenRevocationStatus::kUnknownError;

  if (*error == "invalid_token")
    return GaiaAuthConsumer::TokenRevocationStatus::kInvalidToken;
  if (*error == "invalid_request")
    return GaiaAuthConsumer::TokenRevocationStatus::kInvalidRequest;

  return GaiaAuthConsumer::TokenRevocationStatus::kUnknownError;
}

base::Value::Dict ParseJSONDict(const std::string& data) {
  return base::JSONReader::ReadDict(data, base::JSON_PARSE_CHROMIUM_EXTENSIONS)
      .value_or(base::Value::Dict());
}

GaiaAuthConsumer::ReAuthProofTokenStatus ErrorMessageToReAuthProofTokenStatus(
    const std::string& message) {
  if (message == "INVALID_REQUEST") {
    return GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidRequest;
  }
  if (message == "INVALID_GRANT") {
    return GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant;
  }
  if (message == "UNAUTHORIZED_CLIENT") {
    return GaiaAuthConsumer::ReAuthProofTokenStatus::kUnauthorizedClient;
  }
  if (message == "INSUFFICIENT_SCOPE") {
    return GaiaAuthConsumer::ReAuthProofTokenStatus::kInsufficientScope;
  }
  if (message == "CREDENTIAL_NOT_SET") {
    return GaiaAuthConsumer::ReAuthProofTokenStatus::kCredentialNotSet;
  }
  DLOG(ERROR) << "Unrecognized ReauthAPI error message: " + message;
  return GaiaAuthConsumer::ReAuthProofTokenStatus::kUnknownError;
}

}  // namespace

namespace gaia {

GaiaSource::GaiaSource(Type type) : type_(type) {}

GaiaSource::GaiaSource(Type type, const std::string& suffix)
    : type_(type), suffix_(suffix) {}

void GaiaSource::SetGaiaSourceSuffix(const std::string& suffix) {
  suffix_ = suffix;
}

std::string GaiaSource::ToString() {
  std::string source_string;
  switch (type_) {
    case Type::kChrome:
      source_string = GaiaConstants::kChromeSource;
      break;
    case Type::kChromeOS:
      source_string = GaiaConstants::kChromeOSSource;
      break;
    case Type::kAccountReconcilorDice:
      source_string = "ChromiumAccountReconcilorDice";
      break;
    case Type::kAccountReconcilorMirror:
      source_string = "ChromiumAccountReconcilor";
      break;
    case Type::kPrimaryAccountManager:
      // Even though this string refers to an old name from the Chromium POV, it
      // should not be changed as it is passed server-side.
      source_string = "ChromiumSigninManager";
      break;
    case Type::kChromeGlic:
      source_string = "ChromiumGlic";
      break;
  }

  // All sources should start with Chromium or chromeos for better server logs.
  DCHECK(source_string == "chromeos" ||
         base::StartsWith(source_string, "Chromium",
                          base::CompareCase::SENSITIVE));
  return source_string + suffix_;
}

}  // namespace gaia

GaiaAuthFetcher::GaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    gaia::GaiaSource source,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory),
      consumer_(consumer),
      source_(source.ToString()),
      oauth2_token_gurl_(GaiaUrls::GetInstance()->oauth2_token_url()),
      oauth2_revoke_gurl_(GaiaUrls::GetInstance()->oauth2_revoke_url()),
      oauth_multilogin_gurl_(GaiaUrls::GetInstance()->oauth_multilogin_url()),
      list_accounts_gurl_(
          GaiaUrls::GetInstance()->ListAccountsURLWithSource(source_)),
      logout_gurl_(GaiaUrls::GetInstance()->LogOutURLWithSource(source_)),
      get_check_connection_info_url_(
          GaiaUrls::GetInstance()->GetCheckConnectionInfoURLWithSource(
              source_)),
      reauth_api_url_(GaiaUrls::GetInstance()->reauth_api_url()) {}

GaiaAuthFetcher::~GaiaAuthFetcher() = default;

bool GaiaAuthFetcher::HasPendingFetch() {
  return fetch_pending_;
}

void GaiaAuthFetcher::SetPendingFetch(bool pending_fetch) {
  fetch_pending_ = pending_fetch;
}
bool GaiaAuthFetcher::IsMultiloginUrl(const GURL& url) {
  return base::StartsWith(url.spec(), oauth_multilogin_gurl_.spec(),
                          base::CompareCase::SENSITIVE);
}

bool GaiaAuthFetcher::IsReAuthApiUrl(const GURL& url) {
  return base::StartsWith(url.spec(), reauth_api_url_.spec(),
                          base::CompareCase::SENSITIVE);
}

bool GaiaAuthFetcher::IsListAccountsUrl(const GURL& url) {
  return url == list_accounts_gurl_;
}

void GaiaAuthFetcher::CreateAndStartGaiaFetcher(
    const std::string& body,
    const std::string& body_content_type,
    const net::HttpRequestHeaders& request_headers,
    const GURL& gaia_gurl,
    network::mojom::CredentialsMode credentials_mode,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = gaia_gurl;
  original_url_ = gaia_gurl;

  if (credentials_mode != network::mojom::CredentialsMode::kOmit &&
      credentials_mode !=
          network::mojom::CredentialsMode::kOmitBug_775438_Workaround) {
    CHECK(gaia::HasGaiaSchemeHostPort(gaia_gurl)) << gaia_gurl;

    url::Origin origin = GaiaUrls::GetInstance()->gaia_origin();
    resource_request->site_for_cookies =
        net::SiteForCookies::FromOrigin(origin);
    resource_request->trusted_params =
        network::ResourceRequest::TrustedParams();
    resource_request->trusted_params->isolation_info =
        net::IsolationInfo::CreateForInternalRequest(origin);
  }

  if (!body.empty())
    resource_request->method = "POST";

  resource_request->headers = request_headers;

  resource_request->credentials_mode = credentials_mode;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  if (!body.empty()) {
    DCHECK(!body_content_type.empty());
    url_loader_->AttachStringForUpload(body, body_content_type);
  }

  url_loader_->SetAllowHttpErrorResults(true);

  VLOG(2) << "Gaia fetcher URL: " << gaia_gurl.spec();
  VLOG(2) << "Gaia fetcher headers: " << request_headers.ToString();
  VLOG(2) << "Gaia fetcher body: " << body;

  // Fetchers are sometimes cancelled because a network change was detected,
  // especially at startup and after sign-in on ChromeOS. Retrying once should
  // be enough in those cases; let the fetcher retry up to 3 times just in case.
  // http://crbug.com/163710
  url_loader_->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  fetch_pending_ = true;

  // Unretained is OK below as |url_loader_| is owned by this.
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&GaiaAuthFetcher::OnURLLoadComplete,
                     base::Unretained(this)),
      // Limit to 1 MiB.
      1024 * 1024);
}

// static
std::string GaiaAuthFetcher::MakeGetTokenPairBody(
    const std::string& auth_code,
    const std::string& device_id,
    const std::string& binding_registration_token) {
  std::string encoded_scope =
      base::EscapeUrlEncodedData(GaiaConstants::kOAuth1LoginScope, true);
  std::string encoded_client_id = base::EscapeUrlEncodedData(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(), true);
  std::string encoded_client_secret = base::EscapeUrlEncodedData(
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(), true);
  std::string encoded_auth_code = base::EscapeUrlEncodedData(auth_code, true);
  std::string body = base::StrCat(
      {"scope=", encoded_scope,
       "&grant_type=authorization_code&client_id=", encoded_client_id,
       "&client_secret=", encoded_client_secret, "&code=", encoded_auth_code});
  if (!device_id.empty()) {
    body =
        base::StrCat({body, "&device_id=", device_id, "&device_type=chrome"});
  }
  if (!binding_registration_token.empty()) {
    body = base::StrCat(
        {body, "&bound_token_registration_jwt=", binding_registration_token});
  }
  return body;
}

// static
std::string GaiaAuthFetcher::MakeRevokeTokenBody(
    const std::string& auth_token) {
  return "token=" + auth_token;
}

// static
void GaiaAuthFetcher::ParseFailureResponse(const std::string& data,
                                           std::string* error,
                                           std::string* error_url) {
  base::StringPairs tokens;
  base::SplitStringIntoKeyValuePairs(data, '=', '\n', &tokens);

  for (const auto& token : tokens) {
    if (token.first == "Error") {
      error->assign(token.second);
    } else if (token.first == "Url") {
      error_url->assign(token.second);
    }
  }
}

void GaiaAuthFetcher::StartRevokeOAuth2Token(const std::string& auth_token) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  VLOG(1) << "Starting OAuth2 token revocation";
  request_body_ = MakeRevokeTokenBody(auth_token);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gaia_auth_revoke_token", R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description: "This request revokes an OAuth 2.0 refresh token."
          trigger:
            "This request is part of Gaia Auth API, and is triggered whenever "
            "an OAuth 2.0 refresh token needs to be revoked."
          data: "The OAuth 2.0 refresh token that should be revoked."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  CreateAndStartGaiaFetcher(
      request_body_, kFormEncodedContentType, net::HttpRequestHeaders(),
      oauth2_revoke_gurl_, google_apis::GetOmitCredentialsModeForGaiaRequests(),
      traffic_annotation);
}

void GaiaAuthFetcher::StartAuthCodeForOAuth2TokenExchange(
    const std::string& auth_code,
    const std::string& user_agent_full_version_list,
    const std::string& binding_registration_token) {
  StartAuthCodeForOAuth2TokenExchangeWithDeviceId(
      auth_code, /*device_id=*/std::string(), user_agent_full_version_list,
      binding_registration_token);
}

void GaiaAuthFetcher::StartAuthCodeForOAuth2TokenExchangeWithDeviceId(
    const std::string& auth_code,
    const std::string& device_id,
    const std::string& user_agent_full_version_list,
    const std::string& binding_registration_token) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  VLOG(1) << "Starting OAuth token pair fetch";

  net::HttpRequestHeaders headers;
  if (!user_agent_full_version_list.empty()) {
    headers.SetHeader("Sec-CH-UA-Full-Version-List",
                      user_agent_full_version_list);
  }

  request_body_ =
      MakeGetTokenPairBody(auth_code, device_id, binding_registration_token);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gaia_auth_exchange_device_id",
                                          R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request exchanges an authorization code for an OAuth 2.0 "
            "refresh token."
          trigger:
            "This request is part of Gaia Auth API, and may be triggered at "
            "the end of the Chrome sign-in flow."
          data:
            "The Google console client ID and client secret of the Chrome "
            "application, the OAuth 2.0 authorization code, the ID of the "
            "device, and the public binding key."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  CreateAndStartGaiaFetcher(
      request_body_, kFormEncodedContentType,
      headers, oauth2_token_gurl_,
      google_apis::GetOmitCredentialsModeForGaiaRequests(), traffic_annotation);
}

void GaiaAuthFetcher::StartListAccounts() {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gaia_auth_list_accounts", R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request is used to list the accounts in the Google "
            "authentication cookies."
          trigger:
            "This request is part of Gaia Auth API, and is triggered whenever "
            "the list of all available accounts in the Google authentication "
            "cookies is required."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  net::HttpRequestHeaders headers;
  headers.SetHeader("Origin", "https://www.google.com");
  CreateAndStartGaiaFetcher(
      " ",  // To force an HTTP POST.
      kFormEncodedContentType, headers,
      list_accounts_gurl_, network::mojom::CredentialsMode::kInclude,
      traffic_annotation);
}

void GaiaAuthFetcher::StartOAuthMultilogin(
    gaia::MultiloginMode mode,
    const std::vector<gaia::MultiloginAccountAuthCredentials>& accounts,
    const std::string& external_cc_result,
    OAuthMultiloginResult::CookieDecryptor cookie_decryptor,
    gaia::MultiloginCookieBindingParams cookie_binding_params) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  UMA_HISTOGRAM_COUNTS_100("Signin.Multilogin.NumberOfAccounts",
                           accounts.size());

  bool has_binding_assertion = std::ranges::any_of(
      accounts, [](const gaia::MultiloginAccountAuthCredentials& account) {
        return !account.token_binding_assertion.empty();
      });
  net::HttpRequestHeaders headers;
  if (has_binding_assertion) {
    headers.SetHeader("Authorization",
                      "MultiOAuth " + gaia::CreateMultiOAuthHeader(accounts));
  } else {
    std::vector<std::string> authorization_header_parts;
    std::ranges::transform(
        accounts, std::back_inserter(authorization_header_parts),
        [](const auto& account) {
          return base::StrCat({account.token, ":", account.gaia_id.ToString()});
        });
    headers.SetHeader(
        "Authorization",
        "MultiBearer " + base::JoinString(authorization_header_parts, ","));
  }

  GURL url = oauth_multilogin_gurl_;
  url = net::AppendQueryParameter(url, "source", source_);
  url = net::AppendQueryParameter(
      url, "reuseCookies",
      mode == gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER
          ? "1"
          : "0");
  if (!external_cc_result.empty()) {
    url =
        net::AppendQueryParameter(url, "externalCcResult", external_cc_result);
  }

  constexpr std::string_view kCookieBindingModeParameter = "cookie_binding";
  switch (cookie_binding_params.mode) {
    case gaia::MultiloginCookieBindingParams::Mode::kDisabled:
      break;
    case gaia::MultiloginCookieBindingParams::Mode::kEnabledUnenforced:
      url = net::AppendQueryParameter(url, kCookieBindingModeParameter, "1");
      break;
    case gaia::MultiloginCookieBindingParams::Mode::kEnabledEnforced:
      url = net::AppendQueryParameter(url, kCookieBindingModeParameter, "2");
      break;
  }

  oauth_multilogin_cookie_decryptor_ = std::move(cookie_decryptor);
  standard_device_bound_session_credentials_ =
      cookie_binding_params.standard_device_bound_session_credentials;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gaia_auth_multilogin", R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request is used to set chrome accounts in browser in the "
            "Google authentication cookies for several google websites "
            "(e.g. youtube)."
          trigger:
            "This request is part of Gaia Auth API, and is triggered whenever "
            "accounts in cookies are not consistent with accounts in browser."
          data:
            "This request includes the vector of account ids and auth-login "
            "tokens."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              SigninAllowed: false
            }
          }
        })");

  CreateAndStartGaiaFetcher(" ",  // Non-empty to force a POST
                            kFormEncodedContentType, headers, url,
                            network::mojom::CredentialsMode::kInclude,
                            traffic_annotation);
}

void GaiaAuthFetcher::StartLogOut() {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gaia_auth_log_out", R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request is part of the Chrome - Google authentication API "
            "and allows its callers to sign out all Google accounts from the "
            "content area."
          trigger:
            "This request is part of Gaia Auth API, and is triggered whenever "
            "signing out of all Google accounts is required."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  CreateAndStartGaiaFetcher(
      std::string(), std::string(), net::HttpRequestHeaders(), logout_gurl_,
      network::mojom::CredentialsMode::kInclude, traffic_annotation);
}

void GaiaAuthFetcher::StartCreateReAuthProofTokenForParent(
    const std::string& child_oauth_access_token,
    const GaiaId& parent_obfuscated_gaia_id,
    const std::string& parent_credential) {
  // Create the post body.
  base::Value::Dict post_body_value;
  post_body_value.Set("credentialType", "password");
  post_body_value.Set("credential", parent_credential);
  std::string post_body;
  bool write_success = base::JSONWriter::Write(post_body_value, &post_body);
  DCHECK(write_success);

  // Create the Authorization header.
  net::HttpRequestHeaders headers;
  headers.SetHeader("Authorization", "Bearer " + child_oauth_access_token);
  headers.SetHeader("Content-Type", kJsonContentType);

  // Create the traffic annotation.
  net::NetworkTrafficAnnotationTag traffic_annotation(
      net::DefineNetworkTrafficAnnotation(
          "gaia_create_reauth_proof_token_for_parent", R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request exchanges a set of credentials for a parent-type "
            "Google Family Link user for a ReAuth Proof Token (RAPT), "
            "the successful receipt of which re-authorizes (but explicitly "
            "does not authenticate) the parent user."
          trigger:
            "This request is triggered when a Chrome service needs to "
            "re-authorize a parent-type Google Family Link user given the "
            "parent's login credential."
          data:
            "The obfuscated GAIA id of the parent, the Google OAuth access "
            "token of the child account, and the credential to be used to "
            "reauthorize the user."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })"));

  // Create the ReAuth URL.
  GURL reauth_url = GaiaUrls::GetInstance()->reauth_api_url().Resolve(
      parent_obfuscated_gaia_id.ToString() +
      "/reauthProofTokens?delegationType=unicorn");
  DCHECK(reauth_url.is_valid());

  // Start the request.
  CreateAndStartGaiaFetcher(
      post_body, kJsonContentType, headers, reauth_url,
      google_apis::GetOmitCredentialsModeForGaiaRequests(), traffic_annotation);
}

void GaiaAuthFetcher::StartGetCheckConnectionInfo() {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gaia_auth_check_connection_info", R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request is used to fetch from the Google authentication "
            "server the the list of URLs to check its connection info."
          trigger:
            "This request is part of Gaia Auth API, and is triggered once "
            "after a Google account is added to the browser."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  CreateAndStartGaiaFetcher(
      std::string(), std::string(), net::HttpRequestHeaders(),
      get_check_connection_info_url_,
      google_apis::GetOmitCredentialsModeForGaiaRequests(), traffic_annotation);
}

// static
GoogleServiceAuthError GaiaAuthFetcher::GenerateAuthError(
    const std::string& data,
    net::Error net_error) {
  VLOG(1) << "Got authentication error";
  VLOG(1) << "net_error: " << net::ErrorToString(net_error);
  VLOG(1) << "response body: " << data;

  if (net_error != net::OK) {
    if (net_error == net::ERR_ABORTED) {
      return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
    }
    DVLOG(1) << "Could not reach Google Accounts servers: errno " << net_error;
    return GoogleServiceAuthError::FromConnectionError(net_error);
  }

  std::string error;
  std::string url;
  ParseFailureResponse(data, &error, &url);
  DLOG(WARNING) << "Authentication request failed"
                << (error.empty() ? std::string() : (" with error: " + error));

  if (error == "badauth" || error == "BadAuthentication") {
    return GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
        GoogleServiceAuthError::InvalidGaiaCredentialsReason::
            CREDENTIALS_REJECTED_BY_SERVER);
  }
  return GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE);
}

void GaiaAuthFetcher::OnOAuth2TokenPairFetched(const std::string& data,
                                               net::Error net_error,
                                               int response_code) {
  std::unique_ptr<const GaiaAuthConsumer::ClientOAuthResult> result;
  if (net_error == net::OK && response_code == net::HTTP_OK) {
    result = ExtractOAuth2TokenPairResponse(data);
  }

  if (result) {
    consumer_->OnClientOAuthSuccess(*result);
  } else {
    consumer_->OnClientOAuthFailure(GenerateAuthError(data, net_error));
  }
}

void GaiaAuthFetcher::OnOAuth2RevokeTokenFetched(const std::string& data,
                                                 net::Error net_error,
                                                 int response_code) {
  GaiaAuthConsumer::TokenRevocationStatus revocation_status =
      GaiaAuthConsumer::TokenRevocationStatus::kUnknownError;

  switch (net_error) {
    case net::OK:
      revocation_status =
          GetTokenRevocationStatusFromResponseData(data, response_code);
      break;
    case net::ERR_IO_PENDING:
      NOTREACHED();
    case net::ERR_ABORTED:
      revocation_status =
          GaiaAuthConsumer::TokenRevocationStatus::kConnectionCanceled;
      break;
    default:
      revocation_status =
          (net_error == net::ERR_TIMED_OUT)
              ? GaiaAuthConsumer::TokenRevocationStatus::kConnectionTimeout
              : GaiaAuthConsumer::TokenRevocationStatus::kConnectionFailed;
      break;
  }

  consumer_->OnOAuth2RevokeTokenCompleted(revocation_status);
}

void GaiaAuthFetcher::OnListAccountsFetched(const std::string& data,
                                            net::Error net_error,
                                            int response_code) {
  base::UmaHistogramSparse("Gaia.AuthFetcher.ListAccounts.NetErrorCodes",
                           -net_error);

  if (net_error == net::OK && response_code == net::HTTP_OK)
    consumer_->OnListAccountsSuccess(data);
  else
    consumer_->OnListAccountsFailure(GenerateAuthError(data, net_error));
}

void GaiaAuthFetcher::OnLogOutFetched(const std::string& data,
                                      net::Error net_error,
                                      int response_code) {
  if (net_error == net::OK && response_code == net::HTTP_OK) {
    consumer_->OnLogOutSuccess();
  } else {
    consumer_->OnLogOutFailure(GenerateAuthError(data, net_error));
  }
}

void GaiaAuthFetcher::OnReAuthApiInfoFetched(const std::string& data,
                                             net::Error net_error,
                                             int response_code) {
  if (net_error == net::OK) {
    base::Value::Dict response_dict = ParseJSONDict(data);

    if (response_code == net::HTTP_OK) {
      std::string* rapt_token = response_dict.FindString("encodedRapt");
      if (!rapt_token) {
        // This should not happen unless there is a bug on the server,
        // since if we get HTTP_OK response, we should get a RAPT token.
        DLOG(ERROR) << "Got HTTP-OK ReauthAPI response with empty RAPT token";
        consumer_->OnReAuthProofTokenFailure(
            GaiaAuthConsumer::ReAuthProofTokenStatus::kUnknownError);
        return;
      }
      consumer_->OnReAuthProofTokenSuccess(*rapt_token);
    } else {
      const std::string* error_message =
          response_dict.FindStringByDottedPath("error.message");
      CHECK(error_message);

      consumer_->OnReAuthProofTokenFailure(
          ErrorMessageToReAuthProofTokenStatus(*error_message));
    }
  } else {
    consumer_->OnReAuthProofTokenFailure(
        GaiaAuthConsumer::ReAuthProofTokenStatus::kNetworkError);
  }
}

void GaiaAuthFetcher::OnGetCheckConnectionInfoFetched(const std::string& data,
                                                      net::Error net_error,
                                                      int response_code) {
  if (net_error == net::Error::OK && response_code == net::HTTP_OK) {
    consumer_->OnGetCheckConnectionInfoSuccess(data);
  } else {
    consumer_->OnGetCheckConnectionInfoError(
        GenerateAuthError(data, net_error));
  }
}

void GaiaAuthFetcher::OnOAuthMultiloginFetched(const std::string& data,
                                               net::Error net_error,
                                               int response_code) {
  OAuthMultiloginResult result =
      (net_error == net::Error::OK)
          ? OAuthMultiloginResult(data, response_code,
                                  oauth_multilogin_cookie_decryptor_,
                                  standard_device_bound_session_credentials_)
          : OAuthMultiloginResult(OAuthMultiloginResponseStatus::kRetry);
  consumer_->OnOAuthMultiloginFinished(result);
}

void GaiaAuthFetcher::OnURLLoadComplete(
    std::optional<std::string> response_body) {
  net::Error net_error = static_cast<net::Error>(url_loader_->NetError());

  int response_code = 0;
  if (url_loader_->ResponseInfo()) {
    if (url_loader_->ResponseInfo()->headers)
      response_code = url_loader_->ResponseInfo()->headers->response_code();
  }
  OnURLLoadCompleteInternal(net_error, response_code,
                            std::move(response_body).value_or(""));
}

void GaiaAuthFetcher::OnURLLoadCompleteInternal(net::Error net_error,
                                                int response_code,
                                                std::string data) {
  fetch_pending_ = false;

  // Some of the GAIA requests perform redirects, which results in the final URL
  // of the fetcher not being the original URL requested.  Therefore use the
  // original URL when determining which OnXXX function to call.
  GURL url = std::move(original_url_);
  original_url_ = GURL();
  DispatchFetchedRequest(url, data, net_error, response_code);
}

void GaiaAuthFetcher::DispatchFetchedRequest(const GURL& url,
                                             const std::string& data,
                                             net::Error net_error,
                                             int response_code) {
  if (url == oauth2_token_gurl_) {
    OnOAuth2TokenPairFetched(data, net_error, response_code);
  } else if (IsMultiloginUrl(url)) {
    OnOAuthMultiloginFetched(data, net_error, response_code);
  } else if (url == oauth2_revoke_gurl_) {
    OnOAuth2RevokeTokenFetched(data, net_error, response_code);
  } else if (IsListAccountsUrl(url)) {
    OnListAccountsFetched(data, net_error, response_code);
  } else if (url == logout_gurl_) {
    OnLogOutFetched(data, net_error, response_code);
  } else if (url == get_check_connection_info_url_) {
    OnGetCheckConnectionInfoFetched(data, net_error, response_code);
  } else if (IsReAuthApiUrl(url)) {
    OnReAuthApiInfoFetched(data, net_error, response_code);
  } else {
    NOTREACHED() << "Unknown url: '" << url << "'";
  }
}
