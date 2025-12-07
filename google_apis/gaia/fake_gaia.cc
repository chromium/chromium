// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/fake_gaia.h"

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_test_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_features.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/third_party/mozilla/url_parse.h"

#define REGISTER_RESPONSE_HANDLER(url, method) \
  request_handlers_.insert(std::make_pair(     \
      url.GetPath(),                           \
      base::BindRepeating(&FakeGaia::method, base::Unretained(this))))

#define REGISTER_PATH_RESPONSE_HANDLER(path, method) \
  request_handlers_.insert(std::make_pair(           \
      path, base::BindRepeating(&FakeGaia::method, base::Unretained(this))))

namespace {

using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpRequest;

using MultiloginAction = ::FakeGaia::MultiloginCall::Action;

const char kTestAuthCode[] = "fake-auth-code";
const char kTestAuthLoginAccessToken[] = "fake-access-token";
const char kTestRefreshToken[] = "fake-refresh-token";
const char kTestSessionSIDCookie[] = "fake-session-SID-cookie";
const char kTestSessionLSIDCookie[] = "fake-session-LSID-cookie";
const char kTestSession1PSIDTSCookie[] = "fake-session-1p-SIDTS-cookie";
const char kTestSession3PSIDTSCookie[] = "fake-session-3p-SIDTS-cookie";
const char kTestReauthProofToken[] = "fake-reauth-proof-token";
// Add SameSite=None and Secure because these cookies are needed in a
// cross-site context.
const char kTestCookieAttributes[] =
    "; Path=/; HttpOnly; SameSite=None; Secure";

const char kDefaultEmail[] = "email12345@foo.com";

const base::FilePath::CharType kEmbeddedSetupChromeos[] =
    FILE_PATH_LITERAL("google_apis/test/embedded_setup_chromeos.html");

// OAuth2 Authentication header value prefix.
const char kAuthHeaderBearer[] = "Bearer ";
const char kAuthHeaderBoundOAuth[] = "BoundOAuth ";
const char kAuthHeaderMultiOAuth[] = "MultiOAuth ";
const char kAuthHeaderOAuth[] = "OAuth ";

const char kFakeRemoveLocalAccountPath[] = "FakeRemoveLocalAccount";
const char kFakeSAMLContinuePath[] = "FakeSAMLContinue";

const char kFakeTokenBindingAssertionChallenge[] =
    "fake-token-binding-assertion-challenge";

const char kXSSIPrefix[] = ")]}'\n";

typedef std::map<std::string, std::string> CookieMap;

// Extracts the |access_token| from authorization header of |request|.
bool GetAccessToken(const HttpRequest& request,
                    const char* auth_token_prefix,
                    std::string* access_token) {
  auto auth_header_entry = request.headers.find("Authorization");
  if (auth_header_entry != request.headers.end()) {
    if (base::StartsWith(auth_header_entry->second, auth_token_prefix,
                         base::CompareCase::SENSITIVE)) {
      *access_token =
          auth_header_entry->second.substr(strlen(auth_token_prefix));
      return true;
    }
  }

  return false;
}

void SetCookies(BasicHttpResponse* http_response,
                const std::string& sid_cookie,
                const std::string& lsid_cookie) {
  http_response->AddCustomHeader(
      "Set-Cookie", base::StringPrintf("SID=%s%s", sid_cookie.c_str(),
                                       kTestCookieAttributes));
  http_response->AddCustomHeader(
      "Set-Cookie", base::StringPrintf("LSID=%s%s", lsid_cookie.c_str(),
                                       kTestCookieAttributes));
}

base::Value::Dict GetCookieForMultilogin(const std::string& name,
                                         const std::string& value) {
  return base::Value::Dict()
      .Set("name", name)
      .Set("value", value)
      .Set("domain", ".google.fr")
      .Set("path", "/")
      .Set("isSecure", true)
      .Set("isHttpOnly", false)
      .Set("priority", "HIGH")
      .Set("maxAge", 63070000);
}

base::Value::List GetCookiesForMultilogin(
    const FakeGaia::Configuration& configuration) {
  CHECK(!configuration.session_sid_cookie.empty());
  CHECK(!configuration.session_lsid_cookie.empty());

  base::Value::List cookies;

  cookies.Append(
      GetCookieForMultilogin("SID", configuration.session_sid_cookie));
  cookies.Append(
      GetCookieForMultilogin("LSID", configuration.session_lsid_cookie));

  if (!configuration.session_1p_sidts_cookie.empty()) {
    cookies.Append(GetCookieForMultilogin(
        "__Secure-1PSIDTS", configuration.session_1p_sidts_cookie));
  }
  if (!configuration.session_3p_sidts_cookie.empty()) {
    cookies.Append(GetCookieForMultilogin(
        "__Secure-3PSIDTS", configuration.session_3p_sidts_cookie));
  }

  return cookies;
}

base::Value::Dict GetFailedAccountForMultilogin(const std::string& gaia_id,
                                                const std::string& status,
                                                const std::string& challenge) {
  return base::Value::Dict()
      .Set("obfuscated_id", gaia_id)
      .Set("status", status)
      .Set("token_binding_retry_response",
           base::Value::Dict().Set("challenge", challenge));
}

base::Value::List GetFailedAccountsForMultilogin(
    const gaia::MultiOAuthHeader& multi_oauth_header) {
  CHECK_GT(multi_oauth_header.account_requests().size(), 0);

  base::Value::List failed_accounts;
  failed_accounts.reserve(multi_oauth_header.account_requests().size());

  for (const gaia::MultiOAuthHeader_AccountRequest& account_request :
       multi_oauth_header.account_requests()) {
    failed_accounts.Append(GetFailedAccountForMultilogin(
        account_request.gaia_id(), /*status=*/"RECOVERABLE",
        kFakeTokenBindingAssertionChallenge));
  }

  return failed_accounts;
}

base::Value::List GetDeviceBoundSessionInfoForMultilogin(
    const FakeGaia::Configuration& configuration) {
  auto device_bound_session_info = base::Value::Dict()
                                       .Set("domain", "GOOGLE_COM")
                                       .Set("is_device_bound", true);
  if (!configuration.reuse_bound_session) {
    base::Value::List credentials;
    if (!configuration.session_1p_sidts_cookie.empty()) {
      credentials.Append(base::Value::Dict()
                             .Set("type", "cookie")
                             .Set("name", "__Secure-1PSIDTS")
                             .Set("scope", base::Value::Dict()
                                               .Set("domain", ".google.com")
                                               .Set("path", "/")));
    }
    if (!configuration.session_3p_sidts_cookie.empty()) {
      credentials.Append(base::Value::Dict()
                             .Set("type", "cookie")
                             .Set("name", "__Secure-3PSIDTS")
                             .Set("scope", base::Value::Dict()
                                               .Set("domain", ".google.com")
                                               .Set("path", "/")));
    }
    auto register_session_payload =
        base::Value::Dict()
            .Set("session_identifier", "sidts_session")
            .Set("refresh_url", "/RotateBoundCookies");
    if (configuration.spec_compliant_device_bound_session) {
      register_session_payload.Set("scope",
                                   base::Value::Dict()
                                       .Set("origin", "https://google.com")
                                       .Set("include_site", true));
      register_session_payload.Set("allowed_refresh_initiators",
                                   base::Value::List().Append("*"));
      for (auto& credential : credentials) {
        credential.GetDict().Set("attributes",
                                 "Secure; HttpOnly; Domain=.google.com; "
                                 "Path=/; SameSite=None");
      }
    }
    register_session_payload.Set("credentials", std::move(credentials));
    device_bound_session_info.Set("register_session_payload",
                                  std::move(register_session_payload));
  }
  return base::Value::List().Append(std::move(device_bound_session_info));
}

MultiloginAction GetMultiloginAction(
    const std::optional<gaia::MultiOAuthHeader>& header) {
  if (!header.has_value()) {
    return MultiloginAction::kReturnUnboundCookies;
  }
  CHECK_GT(header->account_requests().size(), 0);
  // To simplify, look at the first account request only.
  const gaia::MultiOAuthHeader::AccountRequest& account_request =
      header->account_requests(0);
  if (account_request.token_binding_assertion().empty()) {
    return MultiloginAction::kReturnUnboundCookies;
  }
  if (account_request.token_binding_assertion() ==
      GaiaConstants::kTokenBindingAssertionSentinel) {
    return MultiloginAction::kReturnBindingChallenge;
  }
  // Assume that the client properly signed the challenge and is eligible to
  // receive bound cookies.
  return MultiloginAction::kReturnBoundCookies;
}

std::string FormatSyncTrustedRecoveryMethods(
    const std::vector<std::vector<uint8_t>>& public_keys) {
  std::string result;
  for (const std::vector<uint8_t>& public_key : public_keys) {
    if (!result.empty()) {
      base::StrAppend(&result, {","});
    }
    base::StrAppend(&result,
                    {"{\"publicKey\":\"", base::Base64Encode(public_key),
                     "\",\"type\":3}"});
  }
  return result;
}

std::string FormatSyncTrustedVaultKeysHeader(
    const GaiaId& gaia_id,
    const FakeGaia::SyncTrustedVaultKeys& sync_trusted_vault_keys) {
  // Single line used because this string populates HTTP headers. Similarly,
  // base64 encoding is used to avoid line breaks and meanwhile adopt JSON
  // format (which doesn't support binary blobs). This base64 encoding is undone
  // embedded_setup_chromeos.html.
  const char format[] =
      "{"
      "\"obfuscatedGaiaId\":\"%s\","
      "\"fakeEncryptionKeyMaterial\":\"%s\","
      "\"fakeEncryptionKeyVersion\":%d,"
      "\"fakeTrustedRecoveryMethods\":[%s]"
      "}";
  return base::StringPrintf(
      format, gaia_id.ToString().c_str(),
      base::Base64Encode(sync_trusted_vault_keys.encryption_key).c_str(),
      sync_trusted_vault_keys.encryption_key_version,
      FormatSyncTrustedRecoveryMethods(
          sync_trusted_vault_keys.trusted_public_keys)
          .c_str());
}

// It gets the bound access token from the authorization header of `request` and
// returns `true`. If it fails at any step, it returns `false`.
bool GetBoundAccessToken(const HttpRequest& request,
                         std::string* access_token) {
  std::string encoded_token;
  if (!GetAccessToken(request, kAuthHeaderBoundOAuth, &encoded_token)) {
    return false;
  }
  std::string decoded_token;
  if (!base::Base64UrlDecode(encoded_token,
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &decoded_token)) {
    return false;
  }
  gaia::BoundOAuthToken bound_oauth_token;
  if (!bound_oauth_token.ParseFromString(decoded_token) ||
      !bound_oauth_token.has_token()) {
    return false;
  }
  *access_token = bound_oauth_token.token();
  return true;
}

// It gets `gaia::MultiOAuthHeader` encoded in the authorization header of
// `request`. If it fails at any step, it returns `std::nullopt`.
std::optional<gaia::MultiOAuthHeader> GetMultiOAuthHeader(
    const HttpRequest& request) {
  auto it = request.headers.find("Authorization");
  if (it == request.headers.end()) {
    return std::nullopt;
  }
  std::optional<std::string_view> encoded_header =
      base::RemovePrefix(it->second, kAuthHeaderMultiOAuth);
  if (!encoded_header.has_value()) {
    return std::nullopt;
  }
  std::string decoded_header;
  if (!base::Base64UrlDecode(*encoded_header,
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &decoded_header)) {
    return std::nullopt;
  }
  gaia::MultiOAuthHeader multi_oauth_header;
  if (!multi_oauth_header.ParseFromString(decoded_header)) {
    return std::nullopt;
  }
  return multi_oauth_header;
}

// Formats a JSON response with the data in |value|, setting the http status
// to |status|.
void FormatJSONResponse(const base::ValueView& value,
                        net::HttpStatusCode status,
                        BasicHttpResponse* http_response,
                        const std::string& prefix = "") {
  std::string response_json = base::WriteJson(value).value_or("");
  http_response->set_content(base::StrCat({prefix, response_json}));
  http_response->set_code(status);
}

// Formats a JSON response with the data in |value|, setting the http status
// to net::HTTP_OK.
void FormatOkJSONResponse(const base::ValueView& value,
                          BasicHttpResponse* http_response,
                          const std::string& prefix = "") {
  FormatJSONResponse(value, net::HTTP_OK, http_response, prefix);
}

}  // namespace

FakeGaia::AccessTokenInfo::AccessTokenInfo() = default;

FakeGaia::AccessTokenInfo::AccessTokenInfo(const AccessTokenInfo& other) =
    default;

FakeGaia::AccessTokenInfo::~AccessTokenInfo() = default;

FakeGaia::Configuration::Configuration() = default;

FakeGaia::Configuration::~Configuration() = default;

FakeGaia::MultiloginCall::MultiloginCall() = default;

FakeGaia::MultiloginCall::~MultiloginCall() = default;

FakeGaia::MultiloginCall::MultiloginCall(const MultiloginCall& other) = default;

void FakeGaia::Configuration::Update(const Configuration& update) {
  // This lambda uses a pointer to data member to merge attributes.
  auto maybe_update_field = [this,
                             &update](std::string Configuration::* field_ptr) {
    if (!(update.*field_ptr).empty()) {
      this->*field_ptr = update.*field_ptr;
    }
  };

  maybe_update_field(&Configuration::auth_sid_cookie);
  maybe_update_field(&Configuration::auth_lsid_cookie);
  maybe_update_field(&Configuration::auth_code);
  maybe_update_field(&Configuration::refresh_token);
  maybe_update_field(&Configuration::access_token);
  maybe_update_field(&Configuration::id_token);
  maybe_update_field(&Configuration::session_sid_cookie);
  maybe_update_field(&Configuration::session_lsid_cookie);

  if (!update.emails.empty()) {
    emails = update.emails;
  }

  if (!update.signed_out_gaia_ids.empty()) {
    signed_out_gaia_ids = update.signed_out_gaia_ids;
  }
}

FakeGaia::SyncTrustedVaultKeys::SyncTrustedVaultKeys() = default;

FakeGaia::SyncTrustedVaultKeys::~SyncTrustedVaultKeys() = default;

FakeGaia::FakeGaia() : issue_oauth_code_cookie_(false) {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
  CHECK(base::ReadFileToString(
      source_root_dir.Append(base::FilePath(kEmbeddedSetupChromeos)),
      &embedded_setup_chromeos_response_));
}

FakeGaia::~FakeGaia() = default;

void FakeGaia::SetConfigurationHelper(const std::string& email,
                                      const std::string& auth_sid_cookie,
                                      const std::string& auth_lsid_cookie) {
  FakeGaia::Configuration params;
  params.auth_sid_cookie = auth_sid_cookie;
  params.auth_lsid_cookie = auth_lsid_cookie;
  params.auth_code = kTestAuthCode;
  params.refresh_token = kTestRefreshToken;
  params.access_token = kTestAuthLoginAccessToken;
  params.session_sid_cookie = kTestSessionSIDCookie;
  params.session_lsid_cookie = kTestSessionLSIDCookie;
  params.session_1p_sidts_cookie = kTestSession1PSIDTSCookie;
  params.session_3p_sidts_cookie = kTestSession3PSIDTSCookie;
  params.emails = {email};
  SetConfiguration(params);
}

void FakeGaia::SetConfiguration(const Configuration& params) {
  configuration_ = params;
}

void FakeGaia::UpdateConfiguration(const Configuration& params) {
  configuration_.Update(params);
}

void FakeGaia::MapEmailToGaiaId(const std::string& email,
                                const GaiaId& gaia_id) {
  DCHECK(!email.empty());
  DCHECK(!gaia_id.empty());
  email_to_gaia_id_map_[email] = gaia_id;
}

void FakeGaia::SetSyncTrustedVaultKeys(
    const std::string& email,
    const SyncTrustedVaultKeys& sync_trusted_vault_keys) {
  DCHECK(!email.empty());
  email_to_sync_trusted_vault_keys_map_[email] = sync_trusted_vault_keys;
}

GaiaId FakeGaia::GetGaiaIdOfEmail(const std::string& email) const {
  const auto it = email_to_gaia_id_map_.find(email);
  return it == email_to_gaia_id_map_.end() ? GetDefaultGaiaId() : it->second;
}

std::string FakeGaia::GetEmailOfGaiaId(const GaiaId& gaia_id) const {
  for (const auto& email_and_gaia_id : email_to_gaia_id_map_) {
    if (email_and_gaia_id.second == gaia_id)
      return email_and_gaia_id.first;
  }
  return kDefaultEmail;
}

void FakeGaia::AddGoogleAccountsSigninHeader(
    BasicHttpResponse* http_response,
    const std::vector<std::string>& emails) const {
  DCHECK(http_response);
  std::vector<std::string> accounts;
  for (size_t i = 0; i < emails.size(); ++i) {
    accounts.push_back(base::StringPrintf(
        "email=\"%s\", obfuscatedid=\"%s\", sessionindex=%d", emails[i],
        GetGaiaIdOfEmail(emails[i]).ToString().c_str(), i));
  }

  http_response->AddCustomHeader("google-accounts-signin",
                                 base::JoinString(accounts, ", "));
}

void FakeGaia::SetOAuthCodeCookie(BasicHttpResponse* http_response) const {
  DCHECK(http_response);
  http_response->AddCustomHeader(
      "Set-Cookie",
      base::StringPrintf("oauth_code=%s%s", configuration_.auth_code.c_str(),
                         kTestCookieAttributes));
}

void FakeGaia::AddSyncTrustedKeysHeader(BasicHttpResponse* http_response,
                                        const std::string& email) const {
  DCHECK(http_response);
  DCHECK(base::Contains(email_to_sync_trusted_vault_keys_map_, email));
  http_response->AddCustomHeader(
      "fake-sync-trusted-vault-keys",
      FormatSyncTrustedVaultKeysHeader(
          GetGaiaIdOfEmail(email),
          email_to_sync_trusted_vault_keys_map_.at(email)));
}

void FakeGaia::Initialize() {
  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  // Handles /oauth/multilogin GAIA call.
  REGISTER_RESPONSE_HANDLER(gaia_urls->oauth_multilogin_url(),
                            HandleMultilogin);

  // Handles /embedded/setup/v2/chromeos GAIA call.
  REGISTER_RESPONSE_HANDLER(gaia_urls->embedded_setup_chromeos_url(),
                            HandleEmbeddedSetupChromeos);

  // Handles /embedded/setup/kidsignup/chromeos GAIA call.
  REGISTER_RESPONSE_HANDLER(gaia_urls->embedded_setup_chromeos_kid_signup_url(),
                            HandleEmbeddedSetupChromeos);

  // Handles /embedded/setup/kidsignin/chromeos GAIA call.
  REGISTER_RESPONSE_HANDLER(gaia_urls->embedded_setup_chromeos_kid_signin_url(),
                            HandleEmbeddedSetupChromeos);

  // Handles /embedded/reauth/chromeos GAIA call.
  REGISTER_RESPONSE_HANDLER(gaia_urls->embedded_reauth_chromeos_url(),
                            HandleEmbeddedReauthChromeos);

  // Handles /_/embedded/lookup/accountlookup for /embedded/setup/chromeos
  // authentication request.
  REGISTER_PATH_RESPONSE_HANDLER("/_/embedded/lookup/accountlookup",
                                 HandleEmbeddedLookupAccountLookup);

  // Handles /_/embedded/signin/challenge for /embedded/setup/chromeos
  // authentication request.
  REGISTER_PATH_RESPONSE_HANDLER("/_/embedded/signin/challenge",
                                 HandleEmbeddedSigninChallenge);

  // Handles /SSO GAIA call (not GAIA, made up for SAML tests).
  REGISTER_PATH_RESPONSE_HANDLER("/SSO", HandleSSO);

  // Handles the /samlredirect requests for tests.
  REGISTER_RESPONSE_HANDLER(gaia_urls->saml_redirect_chromeos_url(),
                            HandleSAMLRedirect);

  REGISTER_RESPONSE_HANDLER(
      gaia_urls->gaia_url().Resolve(kFakeSAMLContinuePath),
      HandleFakeSAMLContinue);

  // Handles /oauth2/v4/token GAIA call.
  REGISTER_RESPONSE_HANDLER(gaia_urls->oauth2_token_url(), HandleAuthToken);

  // Handles /oauth2/v2/tokeninfo GAIA call.
  REGISTER_RESPONSE_HANDLER(gaia_urls->oauth2_token_info_url(),
                            HandleTokenInfo);

  // Handles /oauth2/v2/IssueToken GAIA call.
  REGISTER_RESPONSE_HANDLER(gaia_urls->oauth2_issue_token_url(),
                            HandleIssueToken);

  // Handles /ListAccounts GAIA call.
  REGISTER_RESPONSE_HANDLER(gaia_urls->ListAccountsURLWithSource(std::string()),
                            HandleListAccounts);

  // Handles /oauth2/v1/userinfo call.
  REGISTER_RESPONSE_HANDLER(gaia_urls->oauth_user_info_url(),
                            HandleOAuthUserInfo);

  // Handles /GetCheckConnectionInfo GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->GetCheckConnectionInfoURLWithSource(std::string()),
      HandleGetCheckConnectionInfo);

  // Handles ReAuth API token fetch call.
  REGISTER_RESPONSE_HANDLER(gaia_urls->reauth_api_url(),
                            HandleGetReAuthProofToken);

  // Handles API for browser tests to manually remove local accounts.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->gaia_url().Resolve(kFakeRemoveLocalAccountPath),
      HandleFakeRemoveLocalAccount);

  REGISTER_RESPONSE_HANDLER(gaia_urls->oauth2_revoke_url(),
                            HandleOAuth2TokenRevoke);

  REGISTER_RESPONSE_HANDLER(gaia_urls->rotate_bound_cookies_url(),
                            HandleRotateBoundCookies);
}

FakeGaia::RequestHandlerMap::iterator FakeGaia::FindHandlerByPathPrefix(
    const std::string& request_path) {
  return std::ranges::find_if(
      request_handlers_,
      [request_path](std::pair<std::string, HttpRequestHandlerCallback> entry) {
        return base::StartsWith(request_path, entry.first,
                                base::CompareCase::SENSITIVE);
      });
}

std::unique_ptr<net::test_server::HttpResponse> FakeGaia::HandleRequest(
    const HttpRequest& request) {
  // The scheme and host of the URL is actually not important but required to
  // get a valid GURL in order to parse |request.relative_url|.
  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
  std::string request_path = request_url.GetPath();
  auto http_response = std::make_unique<BasicHttpResponse>();

  auto fixed_response = fixed_responses_.find(request_path);
  if (fixed_response != fixed_responses_.end()) {
    const auto& [code, content] = fixed_response->second;
    http_response->set_code(code);
    http_response->set_content(content);
    http_response->set_content_type("text/html");
    return std::move(http_response);
  }

  RequestHandlerMap::iterator iter = request_handlers_.find(request_path);
  if (iter == request_handlers_.end()) {
    // If exact match yielded no handler, try to find one by prefix,
    // which is required for gaia endpoints that use variable path
    // components, like the ReAuth API.
    iter = FindHandlerByPathPrefix(request_path);
  }
  if (iter == request_handlers_.end()) {
    LOG(ERROR) << "Unhandled request " << request_path;
    return nullptr;
  }

  LOG(WARNING) << "Serving request " << request_path;
  iter->second.Run(request, http_response.get());
  return std::move(http_response);
}

void FakeGaia::IssueOAuthToken(const std::string& auth_token,
                               const AccessTokenInfo& token_info) {
  access_token_info_map_.insert(std::make_pair(auth_token, token_info));
}

bool FakeGaia::HasAccessTokenForAuthToken(const std::string& auth_token) const {
  return access_token_info_map_.contains(auth_token);
}

void FakeGaia::RegisterSamlUser(const std::string& account_id,
                                const GURL& saml_idp) {
  saml_account_idp_map_[account_id] = saml_idp;
}

void FakeGaia::RemoveSamlIdpForUser(const std::string& account_id) {
  saml_account_idp_map_.erase(account_id);
}

void FakeGaia::RegisterSamlDomainRedirectUrl(const std::string& domain,
                                             const GURL& saml_redirect_url) {
  saml_domain_url_map_[domain] = saml_redirect_url;
}

void FakeGaia::RegisterSamlSsoProfileRedirectUrl(
    const std::string& sso_profile,
    const GURL& saml_redirect_url) {
  saml_sso_profile_url_map_[sso_profile] = saml_redirect_url;
}

// static
bool FakeGaia::GetQueryParameter(const std::string& query,
                                 const std::string& key,
                                 std::string* value) {
  // Name and scheme actually don't matter, but are required to get a valid URL
  // for parsing.
  GURL query_url("http://localhost?" + query);
  return net::GetValueForKeyInQuery(query_url, key, value);
}

std::string FakeGaia::GetDeviceIdByRefreshToken(
    const std::string& refresh_token) const {
  auto it = refresh_token_to_device_id_map_.find(refresh_token);
  return it != refresh_token_to_device_id_map_.end() ? it->second
                                                     : std::string();
}

void FakeGaia::SetFixedResponse(const GURL& gaia_url,
                                net::HttpStatusCode http_status_code,
                                const std::string& http_response_body) {
  if (http_status_code == net::HTTP_OK && http_response_body.empty()) {
    fixed_responses_.erase(gaia_url.GetPath());
  } else {
    fixed_responses_[gaia_url.GetPath()] =
        std::make_pair(http_status_code, http_response_body);
  }
}

base::queue<FakeGaia::MultiloginCall> FakeGaia::GetAndResetMultiloginCalls() {
  base::queue<MultiloginCall> result;
  result.swap(multilogin_calls_);
  return result;
}

GURL FakeGaia::GetFakeRemoveLocalAccountURL(const GaiaId& gaia_id) const {
  GURL url =
      GaiaUrls::GetInstance()->gaia_url().Resolve(kFakeRemoveLocalAccountPath);
  return net::AppendQueryParameter(url, "gaia_id", gaia_id.ToString());
}

void FakeGaia::SetRefreshTokenToDeviceIdMap(
    const RefreshTokenToDeviceIdMap& refresh_token_to_device_id_map) {
  refresh_token_to_device_id_map_ = refresh_token_to_device_id_map;
}

const FakeGaia::AccessTokenInfo* FakeGaia::FindAccessTokenInfo(
    const std::string& auth_token,
    const std::string& client_id,
    const std::string& scope_string) const {
  if (auth_token.empty() || client_id.empty())
    return nullptr;

  std::vector<std::string> scope_list = base::SplitString(
      scope_string, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ScopeSet scopes(scope_list.begin(), scope_list.end());

  for (AccessTokenInfoMap::const_iterator entry(
           access_token_info_map_.lower_bound(auth_token));
       entry != access_token_info_map_.upper_bound(auth_token); ++entry) {
    if (entry->second.audience == client_id &&
        (scope_string.empty() || entry->second.any_scope ||
         entry->second.scopes == scopes)) {
      return &(entry->second);
    }
  }

  return nullptr;
}

const FakeGaia::AccessTokenInfo* FakeGaia::GetAccessTokenInfo(
    const std::string& access_token) const {
  for (auto& [key, value] : access_token_info_map_) {
    if (value.token == access_token)
      return &value;
  }

  return nullptr;
}

void FakeGaia::HandleEmbeddedSetupChromeos(const HttpRequest& request,
                                           BasicHttpResponse* http_response) {
  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);

  std::string client_id;
  if (!GetQueryParameter(request_url.GetQuery(), "client_id", &client_id) ||
      GaiaUrls::GetInstance()->oauth2_chrome_client_id() != client_id) {
    LOG(ERROR) << "Missing or invalid param 'client_id' in "
                  "/embedded/setup/chromeos call";
    return;
  }

  GetQueryParameter(request_url.GetQuery(), "Email", &prefilled_email_);
  GetQueryParameter(request_url.GetQuery(), "rart", &reauth_request_token_);
  GetQueryParameter(request_url.GetQuery(), "pwl",
                    &passwordless_support_level_);

  http_response->set_code(net::HTTP_OK);
  http_response->set_content(GetEmbeddedSetupChromeosResponseContent());
  http_response->set_content_type("text/html");
}

void FakeGaia::HandleEmbeddedReauthChromeos(const HttpRequest& request,
                                            BasicHttpResponse* http_response) {
  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);

  std::string client_id;
  if (!GetQueryParameter(request_url.GetQuery(), "client_id", &client_id) ||
      GaiaUrls::GetInstance()->oauth2_chrome_client_id() != client_id) {
    LOG(ERROR) << "Missing or invalid param 'client_id' in "
                  "/embedded/reauth/chromeos call";
    return;
  }

  GetQueryParameter(request_url.GetQuery(), "is_supervised", &is_supervised_);
  GetQueryParameter(request_url.GetQuery(), "is_device_owner",
                    &is_device_owner_);
  GetQueryParameter(request_url.GetQuery(), "Email", &prefilled_email_);
  GetQueryParameter(request_url.GetQuery(), "rart", &reauth_request_token_);
  GetQueryParameter(request_url.GetQuery(), "pwl",
                    &passwordless_support_level_);

  http_response->set_code(net::HTTP_OK);
  http_response->set_content(GetEmbeddedSetupChromeosResponseContent());
  http_response->set_content_type("text/html");
}

void FakeGaia::HandleEmbeddedLookupAccountLookup(
    const HttpRequest& request,
    BasicHttpResponse* http_response) {
  std::string email;
  const bool is_saml =
      GetQueryParameter(request.content, "identifier", &email) &&
      base::Contains(saml_account_idp_map_, email);

  if (!is_saml)
    return;

  GURL url(saml_account_idp_map_[email]);
  url = net::AppendQueryParameter(url, "SAMLRequest", "fake_request");
  url = net::AppendQueryParameter(url, "RelayState",
                                  GaiaUrls::GetInstance()
                                      ->gaia_url()
                                      .Resolve(kFakeSAMLContinuePath)
                                      .spec());
  std::string redirect_url = url.spec();
  http_response->AddCustomHeader("Google-Accounts-SAML", "Start");

  http_response->AddCustomHeader("continue", redirect_url);
}

void FakeGaia::HandleEmbeddedSigninChallenge(const HttpRequest& request,
                                             BasicHttpResponse* http_response) {
  std::string email;
  GetQueryParameter(request.content, "identifier", &email);

  std::string reauth_request_token;
  if (GetQueryParameter(request.content, "rart", &reauth_request_token)) {
    http_response->AddCustomHeader(
        "Set-Cookie", base::StringPrintf("RAPT=%s%s", kTestReauthProofToken,
                                         kTestCookieAttributes));
  }

  if (!configuration_.auth_sid_cookie.empty() &&
      !configuration_.auth_lsid_cookie.empty()) {
    SetCookies(http_response, configuration_.auth_sid_cookie,
               configuration_.auth_lsid_cookie);
  }

  AddGoogleAccountsSigninHeader(http_response, {email});

  if (issue_oauth_code_cookie_)
    SetOAuthCodeCookie(http_response);

  if (base::Contains(email_to_sync_trusted_vault_keys_map_, email)) {
    AddSyncTrustedKeysHeader(http_response, email);
  }
}

void FakeGaia::HandleSSO(const HttpRequest& request,
                         BasicHttpResponse* http_response) {
  if (!configuration_.auth_sid_cookie.empty() &&
      !configuration_.auth_lsid_cookie.empty()) {
    SetCookies(http_response, configuration_.auth_sid_cookie,
               configuration_.auth_lsid_cookie);
  }
  std::string relay_state;
  GetQueryParameter(request.content, "RelayState", &relay_state);
  std::string redirect_url = relay_state;
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url);
  http_response->AddCustomHeader("Google-Accounts-SAML", "End");

  AddGoogleAccountsSigninHeader(http_response, configuration_.emails);

  if (issue_oauth_code_cookie_)
    SetOAuthCodeCookie(http_response);
}

void FakeGaia::HandleFakeSAMLContinue(const HttpRequest& request,
                                      BasicHttpResponse* http_response) {
  http_response->set_content(fake_saml_continue_response_);
  http_response->set_code(net::HTTP_OK);
}

void FakeGaia::HandleAuthToken(const HttpRequest& request,
                               BasicHttpResponse* http_response) {
  std::string grant_type;
  if (!GetQueryParameter(request.content, "grant_type", &grant_type)) {
    http_response->set_code(net::HTTP_BAD_REQUEST);
    LOG(ERROR) << "No 'grant_type' param in /oauth2/v4/token";
    return;
  }

  if (grant_type == "authorization_code") {
    std::string auth_code;
    if (!GetQueryParameter(request.content, "code", &auth_code) ||
        auth_code != configuration_.auth_code) {
      http_response->set_code(net::HTTP_BAD_REQUEST);
      LOG(ERROR) << "No 'code' param in /oauth2/v4/token";
      return;
    }

    std::string device_id;
    if (GetQueryParameter(request.content, "device_id", &device_id)) {
      std::string device_type;
      if (!GetQueryParameter(request.content, "device_type", &device_type)) {
        http_response->set_code(net::HTTP_BAD_REQUEST);
        LOG(ERROR) << "'device_type' should be set if 'device_id' is set.";
        return;
      }
      if (device_type != "chrome") {
        http_response->set_code(net::HTTP_BAD_REQUEST);
        LOG(ERROR) << "'device_type' is not 'chrome'.";
        return;
      }
    }

    if (!device_id.empty()) {
      refresh_token_to_device_id_map_[configuration_.refresh_token] = device_id;
    }

    auto response_dict = base::Value::Dict()
                             .Set("refresh_token", configuration_.refresh_token)
                             .Set("access_token", configuration_.access_token)
                             .Set("expires_in", 3600);
    if (!configuration_.id_token.empty()) {
      response_dict.Set("id_token", configuration_.id_token);
    }
    FormatOkJSONResponse(response_dict, http_response);
    return;
  }

  std::string scope;
  GetQueryParameter(request.content, "scope", &scope);

  std::string refresh_token;
  std::string client_id;
  if (GetQueryParameter(request.content, "refresh_token", &refresh_token) &&
      GetQueryParameter(request.content, "client_id", &client_id)) {
    const AccessTokenInfo* token_info =
        FindAccessTokenInfo(refresh_token, client_id, scope);
    if (token_info) {
      auto response_dict = base::Value::Dict()
                               .Set("access_token", token_info->token)
                               .Set("expires_in", 3600)
                               .Set("id_token", token_info->id_token);
      FormatOkJSONResponse(response_dict, http_response);
      return;
    }
  }

  LOG(ERROR) << "Bad request for /oauth2/v4/token - "
             << "refresh_token = " << refresh_token << ", scope = " << scope
             << ", client_id = " << client_id;
  http_response->set_code(net::HTTP_BAD_REQUEST);
}

void FakeGaia::HandleTokenInfo(const HttpRequest& request,
                               BasicHttpResponse* http_response) {
  const AccessTokenInfo* token_info = nullptr;
  std::string access_token;
  if (GetQueryParameter(request.content, "access_token", &access_token))
    token_info = GetAccessTokenInfo(access_token);

  if (token_info) {
    auto response_dict =
        base::Value::Dict()
            .Set("issued_to", token_info->issued_to)
            .Set("audience", token_info->audience)
            .Set("user_id", token_info->user_id.ToString())
            .Set("scope", base::JoinString(std::vector<std::string_view>(
                                               token_info->scopes.begin(),
                                               token_info->scopes.end()),
                                           " "))
            .Set("expires_in", token_info->expires_in)
            .Set("email", token_info->email)
            .Set("id_token", token_info->id_token);
    FormatOkJSONResponse(response_dict, http_response);
  } else {
    http_response->set_code(net::HTTP_BAD_REQUEST);
  }
}

void FakeGaia::HandleIssueToken(const HttpRequest& request,
                                BasicHttpResponse* http_response) {
  std::string access_token;
  std::string scope;
  std::string client_id;
  if ((GetAccessToken(request, kAuthHeaderBearer, &access_token) ||
       GetBoundAccessToken(request, &access_token)) &&
      GetQueryParameter(request.content, "scope", &scope) &&
      GetQueryParameter(request.content, "client_id", &client_id)) {
    const AccessTokenInfo* token_info =
        FindAccessTokenInfo(access_token, client_id, scope);
    if (token_info) {
      auto response_dict =
          base::Value::Dict()
              .Set("issueAdvice", "auto")
              .Set("expiresIn", base::NumberToString(token_info->expires_in))
              .Set("token", token_info->token)
              .Set("grantedScopes", scope)
              .Set("id_token", token_info->id_token);
      FormatOkJSONResponse(response_dict, http_response);
      return;
    }
  }
  http_response->set_code(net::HTTP_BAD_REQUEST);
}

void FakeGaia::HandleListAccounts(const HttpRequest& request,
                                  BasicHttpResponse* http_response) {
  // Add the signed in accounts.
  std::vector<gaia::CookieParams> params;
  for (const std::string& email : configuration_.emails) {
    params.push_back({
        .email = email,
        .gaia_id = GetGaiaIdOfEmail(email),
        .valid = true,
        .signed_out = false,
        .verified = true,
    });
  }

  // Add the other signed out accounts.
  for (const GaiaId& gaia_id : configuration_.signed_out_gaia_ids) {
    DCHECK_NE(GetDefaultGaiaId(), gaia_id);

    params.push_back({.email = GetEmailOfGaiaId(gaia_id),
                      .gaia_id = gaia_id,
                      .valid = true,
                      .signed_out = true,
                      .verified = true});
  }

  http_response->set_content(
      gaia::CreateListAccountsResponseInBinaryFormat(params));
  http_response->set_code(net::HTTP_OK);
}

void FakeGaia::HandleOAuthUserInfo(const HttpRequest& request,
                                   BasicHttpResponse* http_response) {
  const AccessTokenInfo* token_info = nullptr;
  std::string access_token;
  if (GetAccessToken(request, kAuthHeaderBearer, &access_token) ||
      GetAccessToken(request, kAuthHeaderOAuth, &access_token)) {
    token_info = GetAccessTokenInfo(access_token);
  }

  if (token_info) {
    auto response_dict =
        base::Value::Dict()
            .Set("id", GetGaiaIdOfEmail(token_info->email).ToString())
            .Set("email", token_info->email)
            .Set("verified_email", token_info->email)
            .Set("id_token", token_info->id_token);
    FormatOkJSONResponse(response_dict, http_response);
  } else {
    http_response->set_code(net::HTTP_BAD_REQUEST);
  }
}

void FakeGaia::HandleSAMLRedirect(const HttpRequest& request,
                                  BasicHttpResponse* http_response) {
  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);

  std::optional<GURL> redirect_url = GetSamlRedirectUrl(request_url);
  if (!redirect_url) {
    http_response->set_code(net::HTTP_BAD_REQUEST);
    return;
  }

  redirect_url =
      net::AppendQueryParameter(*redirect_url, "SAMLRequest", "fake_request");
  redirect_url = net::AppendQueryParameter(*redirect_url, "RelayState",
                                           GaiaUrls::GetInstance()
                                               ->gaia_url()
                                               .Resolve(kFakeSAMLContinuePath)
                                               .spec());
  const std::string& final_url = redirect_url->spec();
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Google-Accounts-SAML", "Start");
  http_response->AddCustomHeader("Location", final_url);
}

void FakeGaia::HandleGetCheckConnectionInfo(const HttpRequest& request,
                                            BasicHttpResponse* http_response) {
  FormatOkJSONResponse(base::Value::List(), http_response);
}

void FakeGaia::HandleGetReAuthProofToken(const HttpRequest& request,
                                         BasicHttpResponse* http_response) {
  base::Value::Dict response_dict;
  base::Value::Dict error;

  switch (next_reauth_status_) {
    case GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess:
      response_dict.Set("encodedRapt", "abc123");
      FormatOkJSONResponse(response_dict, http_response);
      break;

    case GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant:
      error.Set("message", "INVALID_GRANT");
      response_dict.Set("error", std::move(error));
      FormatJSONResponse(response_dict, net::HTTP_BAD_REQUEST, http_response);
      break;

    case GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidRequest:
      error.Set("message", "INVALID_REQUEST");
      response_dict.Set("error", std::move(error));
      FormatJSONResponse(response_dict, net::HTTP_BAD_REQUEST, http_response);
      break;

    case GaiaAuthConsumer::ReAuthProofTokenStatus::kUnauthorizedClient:
      error.Set("message", "UNAUTHORIZED_CLIENT");
      response_dict.Set("error", std::move(error));
      FormatJSONResponse(response_dict, net::HTTP_FORBIDDEN, http_response);
      break;

    case GaiaAuthConsumer::ReAuthProofTokenStatus::kInsufficientScope:
      error.Set("message", "INSUFFICIENT_SCOPE");
      response_dict.Set("error", std::move(error));
      FormatJSONResponse(response_dict, net::HTTP_FORBIDDEN, http_response);
      break;

    case GaiaAuthConsumer::ReAuthProofTokenStatus::kCredentialNotSet:
      response_dict.Set("error", std::move(error));
      FormatJSONResponse(response_dict, net::HTTP_FORBIDDEN, http_response);
      break;

    default:
      LOG(FATAL) << "Unsupported ReAuthProofTokenStatus: "
                 << static_cast<int>(next_reauth_status_);
  }
}

void FakeGaia::HandleMultilogin(const HttpRequest& request,
                                BasicHttpResponse* http_response) {
  CHECK(http_response);

  if (configuration_.oauth_multilogin_response_status.has_value()) {
    switch (*configuration_.oauth_multilogin_response_status) {
      case OAuthMultiloginResponseStatus::kInvalidInput:
        FormatJSONResponse(base::Value::Dict().Set("status", "INVALID_INPUT"),
                           net::HTTP_BAD_REQUEST, http_response);
        return;
      case OAuthMultiloginResponseStatus::kError:
        FormatJSONResponse(base::Value::Dict().Set("status", "ERROR"),
                           net::HTTP_INTERNAL_SERVER_ERROR, http_response);
        return;
      default:
        // Overriding the status is currently supported for the above two only.
        NOTREACHED() << "Unsupported OAutMultilogin status override: "
                     << static_cast<int>(
                            *configuration_.oauth_multilogin_response_status);
    }
  }

  if (configuration_.session_sid_cookie.empty() ||
      configuration_.session_lsid_cookie.empty()) {
    http_response->set_code(net::HTTP_BAD_REQUEST);
    return;
  }

  http_response->set_code(net::HTTP_UNAUTHORIZED);

  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
  std::string request_query = request_url.GetQuery();

  std::string source;
  if (!GetQueryParameter(request_query, "source", &source)) {
    LOG(ERROR) << "Missing or invalid 'source' param in /Multilogin call";
    return;
  }

  const std::optional<gaia::MultiOAuthHeader> multi_oauth_header =
      GetMultiOAuthHeader(request);
  const MultiloginAction action = GetMultiloginAction(multi_oauth_header);
  switch (action) {
    case MultiloginAction::kReturnUnboundCookies: {
      const base::Value::Dict response =
          base::Value::Dict()
              .Set("status", "OK")
              .Set("cookies", GetCookiesForMultilogin(configuration_));
      FormatOkJSONResponse(response, http_response, kXSSIPrefix);
      break;
    }
    case MultiloginAction::kReturnBindingChallenge: {
      CHECK(multi_oauth_header.has_value());
      const base::Value::Dict response =
          base::Value::Dict()
              .Set("status", "RETRY")
              .Set("failed_accounts",
                   GetFailedAccountsForMultilogin(*multi_oauth_header));
      FormatJSONResponse(response, net::HTTP_BAD_REQUEST, http_response,
                         kXSSIPrefix);
      break;
    }
    case MultiloginAction::kReturnBoundCookies: {
      const base::Value::Dict response =
          base::Value::Dict()
              .Set("status", "OK")
              .Set("cookies", GetCookiesForMultilogin(configuration_))
              .Set("device_bound_session_info",
                   GetDeviceBoundSessionInfoForMultilogin(configuration_));
      FormatOkJSONResponse(response, http_response, kXSSIPrefix);
      break;
    }
  }

  MultiloginCall call;
  call.header = multi_oauth_header;
  call.action = action;
  multilogin_calls_.push(std::move(call));
}

void FakeGaia::HandleFakeRemoveLocalAccount(
    const net::test_server::HttpRequest& request,
    net::test_server::BasicHttpResponse* http_response) {
  DCHECK(http_response);

  std::string gaia_id_str;
  GetQueryParameter(request.GetURL().GetQuery(), "gaia_id", &gaia_id_str);
  GaiaId gaia_id(gaia_id_str);

  if (!std::erase(configuration_.signed_out_gaia_ids, gaia_id)) {
    http_response->set_code(net::HTTP_BAD_REQUEST);
    return;
  }

  http_response->AddCustomHeader(
      "Google-Accounts-RemoveLocalAccount",
      base::StringPrintf("obfuscatedid=\"%s\"", gaia_id.ToString().c_str()));
  http_response->set_content("");
  http_response->set_code(net::HTTP_OK);
}

void FakeGaia::HandleOAuth2TokenRevoke(
    const net::test_server::HttpRequest& request,
    net::test_server::BasicHttpResponse* http_response) {
  CHECK(http_response);

  static constexpr std::string_view kTokenPrefix = "token=";

  if (!request.content.starts_with(kTokenPrefix)) {
    http_response->set_code(net::HTTP_BAD_REQUEST);
    return;
  }

  const std::string token = request.content.substr(kTokenPrefix.size());
  if (access_token_info_map_.erase(token) == 0) {
    FormatJSONResponse(base::Value::Dict().Set("error", "invalid_token"),
                       net::HTTP_NOT_FOUND, http_response);
    return;
  }

  http_response->set_code(net::HTTP_OK);
}

void FakeGaia::HandleRotateBoundCookies(
    const net::test_server::HttpRequest& request,
    net::test_server::BasicHttpResponse* http_response) {
  CHECK(http_response);
  if (configuration_.rotated_cookies.empty()) {
    http_response->set_code(net::HTTP_BAD_REQUEST);
    return;
  }
  const GURL url("https://google.com");
  for (const std::string& cookie_name : configuration_.rotated_cookies) {
    const std::unique_ptr<net::CanonicalCookie> cookie =
        net::CanonicalCookie::CreateSanitizedCookie(
            url, cookie_name, "dummy_value", url.GetHost(), url.GetPath(),
            /*creation_time=*/base::Time::Now(),
            /*expiration_time=*/base::Time::Now() + base::Hours(2),
            /*last_access_time=*/base::Time::Now(),
            /*secure=*/true, /*http_only=*/true,
            net::CookieSameSite::UNSPECIFIED, net::COOKIE_PRIORITY_HIGH,
            /*partition_key=*/std::nullopt, /*status=*/nullptr);
    http_response->AddCustomHeader(
        "Set-Cookie", net::CanonicalCookie::BuildCookieAttributesLine(*cookie));
  }
  http_response->set_code(net::HTTP_OK);
}

std::string FakeGaia::GetEmbeddedSetupChromeosResponseContent() const {
  if (embedded_setup_chromeos_iframe_url_.is_empty())
    return embedded_setup_chromeos_response_;
  const std::string iframe =
      base::StringPrintf("<iframe src=\"%s\" style=\"%s\"></iframe>",
                         embedded_setup_chromeos_iframe_url_.spec().c_str(),
                         "width:0; height:0; border:none;");
  // Insert the iframe right before </body>
  std::string response_with_iframe = embedded_setup_chromeos_response_;
  size_t pos_of_body_closing_tag = response_with_iframe.find("</body>");
  CHECK(pos_of_body_closing_tag != std::string::npos);
  response_with_iframe.insert(pos_of_body_closing_tag, iframe);
  return response_with_iframe;
}

std::optional<GURL> FakeGaia::GetSamlRedirectUrl(
    const GURL& request_url) const {
  // When deciding on saml redirection, gaia is expected to prioritize sso
  // profile over the domain.

  // First check sso profile.
  std::string sso_profile;
  GetQueryParameter(request_url.GetQuery(), "sso_profile", &sso_profile);
  auto itr_sso = saml_sso_profile_url_map_.find(sso_profile);
  if (itr_sso != saml_sso_profile_url_map_.end()) {
    return itr_sso->second;
  }

  // If we failed to find redirect url based on sso profile, try with domain.
  std::string domain;
  GetQueryParameter(request_url.GetQuery(), "domain", &domain);
  auto itr_domain = saml_domain_url_map_.find(domain);
  if (itr_domain != saml_domain_url_map_.end()) {
    return itr_domain->second;
  }

  return std::nullopt;
}
