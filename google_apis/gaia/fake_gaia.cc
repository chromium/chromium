// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/fake_gaia.h"

#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/third_party/mozilla/url_parse.h"

#define REGISTER_RESPONSE_HANDLER(url, method) \
  request_handlers_.insert(std::make_pair(     \
      url.path(),                              \
      base::BindRepeating(&FakeGaia::method, base::Unretained(this))))

#define REGISTER_PATH_RESPONSE_HANDLER(path, method) \
  request_handlers_.insert(std::make_pair(           \
      path, base::BindRepeating(&FakeGaia::method, base::Unretained(this))))

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;

namespace {

const char kTestAuthCode[] = "fake-auth-code";
const char kTestGaiaUberToken[] = "fake-uber-token";
const char kTestAuthLoginAccessToken[] = "fake-access-token";
const char kTestRefreshToken[] = "fake-refresh-token";
const char kTestSessionSIDCookie[] = "fake-session-SID-cookie";
const char kTestSessionLSIDCookie[] = "fake-session-LSID-cookie";
const char kTestOAuthLoginSID[] = "fake-oauth-SID-cookie";
const char kTestOAuthLoginLSID[] = "fake-oauth-LSID-cookie";
const char kTestOAuthLoginAuthCode[] = "fake-oauth-auth-code";
const char kTestReauthProofToken[] = "fake-reauth-proof-token";
// Add SameSite=None and Secure because these cookies are needed in a
// cross-site context.
const char kTestCookieAttributes[] =
    "; Path=/; HttpOnly; SameSite=None; Secure";

const char kDefaultGaiaId[] = "12345";
const char kDefaultEmail[] = "email12345@foo.com";

const base::FilePath::CharType kEmbeddedSetupChromeos[] =
    FILE_PATH_LITERAL("google_apis/test/embedded_setup_chromeos.html");

// OAuth2 Authentication header value prefix.
const char kAuthHeaderBearer[] = "Bearer ";
const char kAuthHeaderOAuth[] = "OAuth ";

const char kIndividualListedAccountResponseFormat[] =
    "[\"gaia.l.a\",1,\"\",\"%s\",\"\",1,1,0,0,1,\"%s\",11,12,13,%d]";
const char kListAccountsResponseFormat[] = "[\"gaia.l.a.r\",[%s]]";

const char kFakeRemoveLocalAccountPath[] = "FakeRemoveLocalAccount";
const char kFakeSAMLContinuePath[] = "FakeSAMLContinue";

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

std::string FormatCookieForMultilogin(std::string name, std::string value) {
  const char format[] = R"(
    {
      "name":"%s",
      "value":"%s",
      "domain":".google.fr",
      "path":"/",
      "isSecure":true,
      "isHttpOnly":false,
      "priority":"HIGH",
      "maxAge":63070000
    }
  )";
  return base::StringPrintf(format, name.c_str(), value.c_str());
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
    const std::string& gaia_id,
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
      format, gaia_id.c_str(),
      base::Base64Encode(sync_trusted_vault_keys.encryption_key).c_str(),
      sync_trusted_vault_keys.encryption_key_version,
      FormatSyncTrustedRecoveryMethods(
          sync_trusted_vault_keys.trusted_public_keys)
          .c_str());
}

}  // namespace

FakeGaia::AccessTokenInfo::AccessTokenInfo() = default;

FakeGaia::AccessTokenInfo::AccessTokenInfo(const AccessTokenInfo& other) =
    default;

FakeGaia::AccessTokenInfo::~AccessTokenInfo() = default;

FakeGaia::MergeSessionParams::MergeSessionParams() = default;

FakeGaia::MergeSessionParams::~MergeSessionParams() = default;

void FakeGaia::MergeSessionParams::Update(const MergeSessionParams& update) {
  // This lambda uses a pointer to data member to merge attributes.
  auto maybe_update_field =
      [this, &update](std::string MergeSessionParams::*field_ptr) {
        if (!(update.*field_ptr).empty())
          this->*field_ptr = update.*field_ptr;
      };

  maybe_update_field(&MergeSessionParams::auth_sid_cookie);
  maybe_update_field(&MergeSessionParams::auth_lsid_cookie);
  maybe_update_field(&MergeSessionParams::auth_code);
  maybe_update_field(&MergeSessionParams::refresh_token);
  maybe_update_field(&MergeSessionParams::access_token);
  maybe_update_field(&MergeSessionParams::id_token);
  maybe_update_field(&MergeSessionParams::gaia_uber_token);
  maybe_update_field(&MergeSessionParams::session_sid_cookie);
  maybe_update_field(&MergeSessionParams::session_lsid_cookie);
  maybe_update_field(&MergeSessionParams::email);

  if (!update.signed_out_gaia_ids.empty())
    signed_out_gaia_ids = update.signed_out_gaia_ids;
}

FakeGaia::SyncTrustedVaultKeys::SyncTrustedVaultKeys() = default;

FakeGaia::SyncTrustedVaultKeys::~SyncTrustedVaultKeys() = default;

FakeGaia::FakeGaia() : issue_oauth_code_cookie_(false) {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
  CHECK(base::ReadFileToString(
      source_root_dir.Append(base::FilePath(kEmbeddedSetupChromeos)),
      &embedded_setup_chromeos_response_));
}

FakeGaia::~FakeGaia() = default;

void FakeGaia::SetFakeMergeSessionParams(const std::string& email,
                                         const std::string& auth_sid_cookie,
                                         const std::string& auth_lsid_cookie) {
  FakeGaia::MergeSessionParams params;
  params.auth_sid_cookie = auth_sid_cookie;
  params.auth_lsid_cookie = auth_lsid_cookie;
  params.auth_code = kTestAuthCode;
  params.refresh_token = kTestRefreshToken;
  params.access_token = kTestAuthLoginAccessToken;
  params.gaia_uber_token = kTestGaiaUberToken;
  params.session_sid_cookie = kTestSessionSIDCookie;
  params.session_lsid_cookie = kTestSessionLSIDCookie;
  params.email = email;
  SetMergeSessionParams(params);
}

void FakeGaia::SetMergeSessionParams(const MergeSessionParams& params) {
  merge_session_params_ = params;
}

void FakeGaia::UpdateMergeSessionParams(const MergeSessionParams& params) {
  merge_session_params_.Update(params);
}

void FakeGaia::MapEmailToGaiaId(const std::string& email,
                                const std::string& gaia_id) {
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

std::string FakeGaia::GetGaiaIdOfEmail(const std::string& email) const {
  const auto it = email_to_gaia_id_map_.find(email);
  return it == email_to_gaia_id_map_.end() ? std::string(kDefaultGaiaId)
                                           : it->second;
}

std::string FakeGaia::GetEmailOfGaiaId(const std::string& gaia_id) const {
  for (const auto& email_and_gaia_id : email_to_gaia_id_map_) {
    if (email_and_gaia_id.second == gaia_id)
      return email_and_gaia_id.first;
  }
  return kDefaultEmail;
}

void FakeGaia::AddGoogleAccountsSigninHeader(BasicHttpResponse* http_response,
                                             const std::string& email) const {
  DCHECK(http_response);
  http_response->AddCustomHeader(
      "google-accounts-signin",
      base::StringPrintf("email=\"%s\", obfuscatedid=\"%s\", sessionindex=0",
                         email.c_str(), GetGaiaIdOfEmail(email).c_str()));
}

void FakeGaia::SetOAuthCodeCookie(BasicHttpResponse* http_response) const {
  DCHECK(http_response);
  http_response->AddCustomHeader(
      "Set-Cookie", base::StringPrintf("oauth_code=%s%s",
                                       merge_session_params_.auth_code.c_str(),
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
  // Handles /MergeSession GAIA call.
  REGISTER_RESPONSE_HANDLER(gaia_urls->merge_session_url(), HandleMergeSession);

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

  // Handles /OAuthLogin GAIA call.
  REGISTER_RESPONSE_HANDLER(gaia_urls->oauth1_login_url(), HandleOAuthLogin);

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
  REGISTER_PATH_RESPONSE_HANDLER("/samlredirect", HandleSAMLRedirect);

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
}

FakeGaia::RequestHandlerMap::iterator FakeGaia::FindHandlerByPathPrefix(
    const std::string& request_path) {
  return base::ranges::find_if(
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
  std::string request_path = request_url.path();
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
    fixed_responses_.erase(gaia_url.path());
  } else {
    fixed_responses_[gaia_url.path()] =
        std::make_pair(http_status_code, http_response_body);
  }
}

GURL FakeGaia::GetFakeRemoveLocalAccountURL(const std::string& gaia_id) const {
  GURL url =
      GaiaUrls::GetInstance()->gaia_url().Resolve(kFakeRemoveLocalAccountPath);
  return net::AppendQueryParameter(url, "gaia_id", gaia_id);
}

void FakeGaia::SetRefreshTokenToDeviceIdMap(
    const RefreshTokenToDeviceIdMap& refresh_token_to_device_id_map) {
  refresh_token_to_device_id_map_ = refresh_token_to_device_id_map;
}

void FakeGaia::HandleMergeSession(const HttpRequest& request,
                                  BasicHttpResponse* http_response) {
  http_response->set_code(net::HTTP_UNAUTHORIZED);
  if (merge_session_params_.session_sid_cookie.empty() ||
      merge_session_params_.session_lsid_cookie.empty()) {
    http_response->set_code(net::HTTP_BAD_REQUEST);
    return;
  }

  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
  std::string request_query = request_url.query();

  std::string uber_token;
  if (!GetQueryParameter(request_query, "uberauth", &uber_token) ||
      uber_token != merge_session_params_.gaia_uber_token) {
    LOG(ERROR) << "Missing or invalid 'uberauth' param in /MergeSession call";
    return;
  }

  std::string continue_url;
  if (!GetQueryParameter(request_query, "continue", &continue_url)) {
    LOG(ERROR) << "Missing or invalid 'continue' param in /MergeSession call";
    return;
  }

  std::string source;
  if (!GetQueryParameter(request_query, "source", &source)) {
    LOG(ERROR) << "Missing or invalid 'source' param in /MergeSession call";
    return;
  }

  SetCookies(http_response, merge_session_params_.session_sid_cookie,
             merge_session_params_.session_lsid_cookie);
  // TODO(zelidrag): Not used now.
  http_response->set_content("OK");
  http_response->set_code(net::HTTP_OK);
}

void FakeGaia::FormatOkJSONResponse(const base::Value& value,
                                    BasicHttpResponse* http_response) {
  FormatJSONResponse(value, net::HTTP_OK, http_response);
}

void FakeGaia::FormatJSONResponse(const base::Value& value,
                                  net::HttpStatusCode status,
                                  BasicHttpResponse* http_response) {
  std::string response_json;
  base::JSONWriter::Write(value, &response_json);
  http_response->set_content(response_json);
  http_response->set_code(status);
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
  if (!GetQueryParameter(request_url.query(), "client_id", &client_id) ||
      GaiaUrls::GetInstance()->oauth2_chrome_client_id() != client_id) {
    LOG(ERROR) << "Missing or invalid param 'client_id' in "
                  "/embedded/setup/chromeos call";
    return;
  }

  GetQueryParameter(request_url.query(), "Email", &prefilled_email_);
  GetQueryParameter(request_url.query(), "rart", &reauth_request_token_);

  http_response->set_code(net::HTTP_OK);
  http_response->set_content(GetEmbeddedSetupChromeosResponseContent());
  http_response->set_content_type("text/html");
}

void FakeGaia::HandleEmbeddedReauthChromeos(const HttpRequest& request,
                                            BasicHttpResponse* http_response) {
  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);

  std::string client_id;
  if (!GetQueryParameter(request_url.query(), "client_id", &client_id) ||
      GaiaUrls::GetInstance()->oauth2_chrome_client_id() != client_id) {
    LOG(ERROR) << "Missing or invalid param 'client_id' in "
                  "/embedded/reauth/chromeos call";
    return;
  }

  if (!GetQueryParameter(request_url.query(), "is_supervised",
                         &is_supervised_)) {
    LOG(ERROR) << "Missing param 'is_supervised' in "
                  "/embedded/reauth/chromeos call";
    return;
  }

  GetQueryParameter(request_url.query(), "is_device_owner", &is_device_owner_);
  GetQueryParameter(request_url.query(), "Email", &prefilled_email_);

  http_response->set_code(net::HTTP_OK);
  http_response->set_content(GetEmbeddedSetupChromeosResponseContent());
  http_response->set_content_type("text/html");
}

void FakeGaia::HandleOAuthLogin(const HttpRequest& request,
                                BasicHttpResponse* http_response) {
  http_response->set_code(net::HTTP_UNAUTHORIZED);
  if (merge_session_params_.gaia_uber_token.empty()) {
    http_response->set_code(net::HTTP_FORBIDDEN);
    http_response->set_content("Error=BadAuthentication");
    return;
  }

  std::string access_token;
  if (!GetAccessToken(request, kAuthHeaderBearer, &access_token) &&
      !GetAccessToken(request, kAuthHeaderOAuth, &access_token)) {
    LOG(ERROR) << "/OAuthLogin missing access token in the header";
    return;
  }

  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
  std::string request_query = request_url.query();

  std::string source;
  if (!GetQueryParameter(request_query, "source", &source) &&
      !GetQueryParameter(request.content, "source", &source)) {
    LOG(ERROR) << "Missing 'source' param in /OAuthLogin call";
    return;
  }

  std::string issue_uberauth;
  if (GetQueryParameter(request_query, "issueuberauth", &issue_uberauth) &&
      issue_uberauth == "1") {
    http_response->set_content(merge_session_params_.gaia_uber_token);
    http_response->set_code(net::HTTP_OK);
    // Issue GAIA uber token.
  } else {
    http_response->set_content(
        base::StringPrintf("SID=%s\nLSID=%s\nAuth=%s", kTestOAuthLoginSID,
                           kTestOAuthLoginLSID, kTestOAuthLoginAuthCode));
    http_response->set_code(net::HTTP_OK);
  }
}

void FakeGaia::HandleEmbeddedLookupAccountLookup(
    const HttpRequest& request,
    BasicHttpResponse* http_response) {
  std::string email;
  const bool is_saml =
      GetQueryParameter(request.content, "identifier", &email) &&
      saml_account_idp_map_.find(email) != saml_account_idp_map_.end();

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

  if (!merge_session_params_.auth_sid_cookie.empty() &&
      !merge_session_params_.auth_lsid_cookie.empty()) {
    SetCookies(http_response, merge_session_params_.auth_sid_cookie,
               merge_session_params_.auth_lsid_cookie);
  }

  AddGoogleAccountsSigninHeader(http_response, email);

  if (issue_oauth_code_cookie_)
    SetOAuthCodeCookie(http_response);

  if (base::Contains(email_to_sync_trusted_vault_keys_map_, email)) {
    AddSyncTrustedKeysHeader(http_response, email);
  }
}

void FakeGaia::HandleSSO(const HttpRequest& request,
                         BasicHttpResponse* http_response) {
  if (!merge_session_params_.auth_sid_cookie.empty() &&
      !merge_session_params_.auth_lsid_cookie.empty()) {
    SetCookies(http_response, merge_session_params_.auth_sid_cookie,
               merge_session_params_.auth_lsid_cookie);
  }
  std::string relay_state;
  GetQueryParameter(request.content, "RelayState", &relay_state);
  std::string redirect_url = relay_state;
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url);
  http_response->AddCustomHeader("Google-Accounts-SAML", "End");

  AddGoogleAccountsSigninHeader(http_response, merge_session_params_.email);

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
        auth_code != merge_session_params_.auth_code) {
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

    base::Value::Dict response_dict;
    response_dict.Set("refresh_token", merge_session_params_.refresh_token);
    if (!device_id.empty())
      refresh_token_to_device_id_map_[merge_session_params_.refresh_token] =
          device_id;
    response_dict.Set("access_token", merge_session_params_.access_token);
    if (!merge_session_params_.id_token.empty())
      response_dict.Set("id_token", merge_session_params_.id_token);
    response_dict.Set("expires_in", 3600);
    FormatOkJSONResponse(base::Value(std::move(response_dict)), http_response);
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
      base::Value::Dict response_dict;
      response_dict.Set("access_token", token_info->token);
      response_dict.Set("expires_in", 3600);
      response_dict.Set("id_token", token_info->id_token);
      FormatOkJSONResponse(base::Value(std::move(response_dict)),
                           http_response);
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
    base::Value::Dict response_dict;
    response_dict.Set("issued_to", token_info->issued_to);
    response_dict.Set("audience", token_info->audience);
    response_dict.Set("user_id", token_info->user_id);
    std::vector<base::StringPiece> scope_vector(token_info->scopes.begin(),
                                                token_info->scopes.end());
    response_dict.Set("scope", base::JoinString(scope_vector, " "));
    response_dict.Set("expires_in", token_info->expires_in);
    response_dict.Set("email", token_info->email);
    response_dict.Set("id_token", token_info->id_token);
    FormatOkJSONResponse(base::Value(std::move(response_dict)), http_response);
  } else {
    http_response->set_code(net::HTTP_BAD_REQUEST);
  }
}

void FakeGaia::HandleIssueToken(const HttpRequest& request,
                                BasicHttpResponse* http_response) {
  std::string access_token;
  std::string scope;
  std::string client_id;
  if (GetAccessToken(request, kAuthHeaderBearer, &access_token) &&
      GetQueryParameter(request.content, "scope", &scope) &&
      GetQueryParameter(request.content, "client_id", &client_id)) {
    const AccessTokenInfo* token_info =
        FindAccessTokenInfo(access_token, client_id, scope);
    if (token_info) {
      base::Value::Dict response_dict;
      response_dict.Set("issueAdvice", "auto");
      response_dict.Set("expiresIn",
                        base::NumberToString(token_info->expires_in));
      response_dict.Set("token", token_info->token);
      response_dict.Set("grantedScopes", scope);
      response_dict.Set("id_token", token_info->id_token);
      FormatOkJSONResponse(base::Value(std::move(response_dict)),
                           http_response);
      return;
    }
  }
  http_response->set_code(net::HTTP_BAD_REQUEST);
}

void FakeGaia::HandleListAccounts(const HttpRequest& request,
                                  BasicHttpResponse* http_response) {
  const int kAccountIsSignedIn = 0;
  const int kAccountIsSignedOut = 1;

  std::vector<std::string> listed_accounts;
  listed_accounts.push_back(base::StringPrintf(
      kIndividualListedAccountResponseFormat,
      merge_session_params_.email.c_str(), kDefaultGaiaId, kAccountIsSignedIn));

  for (const std::string& gaia_id : merge_session_params_.signed_out_gaia_ids) {
    DCHECK_NE(kDefaultGaiaId, gaia_id);

    const std::string email = GetEmailOfGaiaId(gaia_id);
    listed_accounts.push_back(base::StringPrintf(
        kIndividualListedAccountResponseFormat, email.c_str(), gaia_id.c_str(),
        kAccountIsSignedOut));
  }

  http_response->set_content(
      base::StringPrintf(kListAccountsResponseFormat,
                         base::JoinString(listed_accounts, ",").c_str()));
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
    base::Value::Dict response_dict;
    response_dict.Set("id", GetGaiaIdOfEmail(token_info->email));
    response_dict.Set("email", token_info->email);
    response_dict.Set("verified_email", token_info->email);
    response_dict.Set("id_token", token_info->id_token);
    FormatOkJSONResponse(base::Value(std::move(response_dict)), http_response);
  } else {
    http_response->set_code(net::HTTP_BAD_REQUEST);
  }
}

void FakeGaia::HandleSAMLRedirect(const HttpRequest& request,
                                  BasicHttpResponse* http_response) {
  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);

  absl::optional<GURL> redirect_url = GetSamlRedirectUrl(request_url);
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
  FormatOkJSONResponse(base::Value(base::Value::Type::LIST), http_response);
}

void FakeGaia::HandleGetReAuthProofToken(const HttpRequest& request,
                                         BasicHttpResponse* http_response) {
  base::Value response_dict(base::Value::Type::DICT);
  base::Value error(base::Value::Type::DICT);

  switch (next_reauth_status_) {
    case GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess:
      response_dict.GetDict().Set("encodedRapt", "abc123");
      FormatOkJSONResponse(response_dict, http_response);
      break;

    case GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant:
      error.GetDict().Set("message", "INVALID_GRANT");
      response_dict.GetDict().Set("error", std::move(error));
      FormatJSONResponse(response_dict, net::HTTP_BAD_REQUEST, http_response);
      break;

    case GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidRequest:
      error.GetDict().Set("message", "INVALID_REQUEST");
      response_dict.GetDict().Set("error", std::move(error));
      FormatJSONResponse(response_dict, net::HTTP_BAD_REQUEST, http_response);
      break;

    case GaiaAuthConsumer::ReAuthProofTokenStatus::kUnauthorizedClient:
      error.GetDict().Set("message", "UNAUTHORIZED_CLIENT");
      response_dict.GetDict().Set("error", std::move(error));
      FormatJSONResponse(response_dict, net::HTTP_FORBIDDEN, http_response);
      break;

    case GaiaAuthConsumer::ReAuthProofTokenStatus::kInsufficientScope:
      error.GetDict().Set("message", "INSUFFICIENT_SCOPE");
      response_dict.GetDict().Set("error", std::move(error));
      FormatJSONResponse(response_dict, net::HTTP_FORBIDDEN, http_response);
      break;

    case GaiaAuthConsumer::ReAuthProofTokenStatus::kCredentialNotSet:
      response_dict.GetDict().Set("error", std::move(error));
      FormatJSONResponse(response_dict, net::HTTP_FORBIDDEN, http_response);
      break;

    default:
      LOG(FATAL) << "Unsupported ReAuthProofTokenStatus: "
                 << static_cast<int>(next_reauth_status_);
      break;
  }
}

void FakeGaia::HandleMultilogin(const HttpRequest& request,
                                BasicHttpResponse* http_response) {
  http_response->set_code(net::HTTP_UNAUTHORIZED);

  if (merge_session_params_.session_sid_cookie.empty() ||
      merge_session_params_.session_lsid_cookie.empty()) {
    http_response->set_code(net::HTTP_BAD_REQUEST);
    return;
  }

  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
  std::string request_query = request_url.query();

  std::string source;
  if (!GetQueryParameter(request_query, "source", &source)) {
    LOG(ERROR) << "Missing or invalid 'source' param in /Multilogin call";
    return;
  }

  http_response->set_content(
      ")]}'\n{\"status\":\"OK\",\"cookies\":[" +
      FormatCookieForMultilogin("SID",
                                merge_session_params_.session_sid_cookie) +
      "," +
      FormatCookieForMultilogin("LSID",
                                merge_session_params_.session_lsid_cookie) +
      "]}");
  http_response->set_code(net::HTTP_OK);
}

void FakeGaia::HandleFakeRemoveLocalAccount(
    const net::test_server::HttpRequest& request,
    net::test_server::BasicHttpResponse* http_response) {
  DCHECK(http_response);

  std::string gaia_id;
  GetQueryParameter(request.GetURL().query(), "gaia_id", &gaia_id);

  if (!base::Erase(merge_session_params_.signed_out_gaia_ids, gaia_id)) {
    http_response->set_code(net::HTTP_BAD_REQUEST);
    return;
  }

  http_response->AddCustomHeader(
      "Google-Accounts-RemoveLocalAccount",
      base::StringPrintf("obfuscatedid=\"%s\"", gaia_id.c_str()));
  http_response->set_content("");
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

absl::optional<GURL> FakeGaia::GetSamlRedirectUrl(
    const GURL& request_url) const {
  // When deciding on saml redirection, gaia is expected to prioritize sso
  // profile over the domain.

  // First check sso profile.
  std::string sso_profile;
  GetQueryParameter(request_url.query(), "sso_profile", &sso_profile);
  auto itr_sso = saml_sso_profile_url_map_.find(sso_profile);
  if (itr_sso != saml_sso_profile_url_map_.end()) {
    return itr_sso->second;
  }

  // If we failed to find redirect url based on sso profile, try with domain.
  std::string domain;
  GetQueryParameter(request_url.query(), "domain", &domain);
  auto itr_domain = saml_domain_url_map_.find(domain);
  if (itr_domain != saml_domain_url_map_.end()) {
    return itr_domain->second;
  }

  return absl::nullopt;
}
