// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/fake_gaia.h"

#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
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
  request_handlers_.insert(std::make_pair( \
        url.path(), base::Bind(&FakeGaia::method, base::Unretained(this))))

#define REGISTER_PATH_RESPONSE_HANDLER(path, method) \
  request_handlers_.insert(std::make_pair( \
        path, base::Bind(&FakeGaia::method, base::Unretained(this))))

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
// Add SameSite=None and Secure because these cookies are needed in a
// cross-site context.
const char kTestCookieAttributes[] =
    "; Path=/; HttpOnly; SameSite=None; Secure";

const char kDefaultGaiaId[] = "12345";

const base::FilePath::CharType kServiceLogin[] =
    FILE_PATH_LITERAL("google_apis/test/service_login.html");

const base::FilePath::CharType kEmbeddedSetupChromeos[] =
    FILE_PATH_LITERAL("google_apis/test/embedded_setup_chromeos.html");

// OAuth2 Authentication header value prefix.
const char kAuthHeaderBearer[] = "Bearer ";
const char kAuthHeaderOAuth[] = "OAuth ";

const char kListAccountsResponseFormat[] =
    "[\"gaia.l.a.r\",[[\"gaia.l.a\",1,\"\",\"%s\",\"\",1,1,0,0,1,\"12345\"]]]";

const char kDummySAMLContinuePath[] = "DummySAMLContinue";

typedef std::map<std::string, std::string> CookieMap;

// Extracts the |access_token| from authorization header of |request|.
bool GetAccessToken(const HttpRequest& request,
                    const char* auth_token_prefix,
                    std::string* access_token) {
  auto auth_header_entry = request.headers.find("Authorization");
  if (auth_header_entry != request.headers.end()) {
    if (base::StartsWith(auth_header_entry->second, auth_token_prefix,
                         base::CompareCase::SENSITIVE)) {
      *access_token = auth_header_entry->second.substr(
          strlen(auth_token_prefix));
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
      [this, &update](std::string MergeSessionParams::* field_ptr) {
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
}

FakeGaia::FakeGaia() : issue_oauth_code_cookie_(false) {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
  CHECK(base::ReadFileToString(
      source_root_dir.Append(base::FilePath(kServiceLogin)),
      &service_login_response_));
  CHECK(base::ReadFileToString(
      source_root_dir.Append(base::FilePath(kEmbeddedSetupChromeos)),
      &embedded_setup_chromeos_response_));
}

FakeGaia::~FakeGaia() {}

void FakeGaia::SetFakeMergeSessionParams(
    const std::string& email,
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

void FakeGaia::SetMergeSessionParams(
    const MergeSessionParams& params) {
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

std::string FakeGaia::GetGaiaIdOfEmail(const std::string& email) const {
  const auto it = email_to_gaia_id_map_.find(email);
  return it == email_to_gaia_id_map_.end() ? std::string(kDefaultGaiaId) :
      it->second;
}

void FakeGaia::AddGoogleAccountsSigninHeader(BasicHttpResponse* http_response,
                                             const std::string& email) const {
  http_response->AddCustomHeader("google-accounts-signin",
      base::StringPrintf(
          "email=\"%s\", obfuscatedid=\"%s\", sessionindex=0",
          email.c_str(), GetGaiaIdOfEmail(email).c_str()));
}

void FakeGaia::SetOAuthCodeCookie(BasicHttpResponse* http_response) const {
  http_response->AddCustomHeader(
      "Set-Cookie", base::StringPrintf("oauth_code=%s%s",
                                       merge_session_params_.auth_code.c_str(),
                                       kTestCookieAttributes));
}

void FakeGaia::Initialize() {
  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  // Handles /MergeSession GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->merge_session_url(), HandleMergeSession);

  // Handles /ServiceLogin GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->service_login_url(), HandleServiceLogin);

  // Handles /embedded/setup/v2/chromeos GAIA call.
  REGISTER_RESPONSE_HANDLER(gaia_urls->embedded_setup_chromeos_url(2),
                            HandleEmbeddedSetupChromeos);

  // Handles /OAuthLogin GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->oauth1_login_url(), HandleOAuthLogin);

  // Handles /ServiceLoginAuth GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->service_login_auth_url(), HandleServiceLoginAuth);

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
      gaia_urls->gaia_url().Resolve(kDummySAMLContinuePath),
      HandleDummySAMLContinue);

  // Handles /oauth2/v4/token GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->oauth2_token_url(), HandleAuthToken);

  // Handles /oauth2/v2/tokeninfo GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->oauth2_token_info_url(), HandleTokenInfo);

  // Handles /oauth2/v2/IssueToken GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->oauth2_issue_token_url(), HandleIssueToken);

  // Handles /ListAccounts GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->ListAccountsURLWithSource(std::string()), HandleListAccounts);

  // Handles /GetUserInfo GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->get_user_info_url(), HandleGetUserInfo);

  // Handles /oauth2/v1/userinfo call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->oauth_user_info_url(), HandleOAuthUserInfo);

  // Handles /GetCheckConnectionInfo GAIA call.
  REGISTER_RESPONSE_HANDLER(
      gaia_urls->GetCheckConnectionInfoURLWithSource(std::string()),
      HandleGetCheckConnectionInfo);

  // Handles ReAuth API token fetch call.
  REGISTER_RESPONSE_HANDLER(gaia_urls->reauth_api_url(),
                            HandleGetReAuthProofToken);
}

FakeGaia::RequestHandlerMap::iterator FakeGaia::FindHandlerByPathPrefix(
    const std::string& request_path) {
  return std::find_if(
      request_handlers_.begin(), request_handlers_.end(),
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
  RequestHandlerMap::iterator iter = request_handlers_.find(request_path);
  if (iter == request_handlers_.end()) {
    // If exact match yielded no handler, try to find one by prefix,
    // which is required for gaia endpoints that use variable path
    // components, like the ReAuth API.
    iter = FindHandlerByPathPrefix(request_path);
  }
  if (iter != request_handlers_.end()) {
    LOG(WARNING) << "Serving request " << request_path;
    iter->second.Run(request, http_response.get());
  } else {
    LOG(ERROR) << "Unhandled request " << request_path;
    return std::unique_ptr<net::test_server::HttpResponse>();
  }

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

void FakeGaia::RegisterSamlDomainRedirectUrl(const std::string& domain,
                                             const GURL& saml_redirect_url) {
  saml_domain_url_map_[domain] = saml_redirect_url;
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

  SetCookies(http_response,
             merge_session_params_.session_sid_cookie,
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
       entry != access_token_info_map_.upper_bound(auth_token);
       ++entry) {
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
  for (AccessTokenInfoMap::const_iterator entry(
           access_token_info_map_.begin());
       entry != access_token_info_map_.end();
       ++entry) {
    if (entry->second.token == access_token)
      return &(entry->second);
  }

  return nullptr;
}

void FakeGaia::HandleServiceLogin(const HttpRequest& request,
                                  BasicHttpResponse* http_response) {
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(service_login_response_);
  http_response->set_content_type("text/html");
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

  http_response->set_code(net::HTTP_OK);
  http_response->set_content(embedded_setup_chromeos_response_);
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
    http_response->set_content(base::StringPrintf(
        "SID=%s\nLSID=%s\nAuth=%s",
        kTestOAuthLoginSID, kTestOAuthLoginLSID, kTestOAuthLoginAuthCode));
    http_response->set_code(net::HTTP_OK);
  }
}

void FakeGaia::HandleServiceLoginAuth(const HttpRequest& request,
                                      BasicHttpResponse* http_response) {
  std::string continue_url =
      GaiaUrls::GetInstance()->service_login_url().spec();
  GetQueryParameter(request.content, "continue", &continue_url);

  std::string redirect_url = continue_url;

  std::string email;
  const bool is_saml =
      GetQueryParameter(request.content, "Email", &email) &&
      saml_account_idp_map_.find(email) != saml_account_idp_map_.end();

  if (is_saml) {
    GURL url(saml_account_idp_map_[email]);
    url = net::AppendQueryParameter(url, "SAMLRequest", "fake_request");
    url = net::AppendQueryParameter(url, "RelayState", continue_url);
    redirect_url = url.spec();
    http_response->AddCustomHeader("Google-Accounts-SAML", "Start");
  } else if (!merge_session_params_.auth_sid_cookie.empty() &&
             !merge_session_params_.auth_lsid_cookie.empty()) {
    SetCookies(http_response,
               merge_session_params_.auth_sid_cookie,
               merge_session_params_.auth_lsid_cookie);
  }

  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url);

  // SAML sign-ins complete in HandleSSO().
  if (is_saml)
    return;

  AddGoogleAccountsSigninHeader(http_response, email);
  if (issue_oauth_code_cookie_)
    SetOAuthCodeCookie(http_response);
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
                                      .Resolve(kDummySAMLContinuePath)
                                      .spec());
  std::string redirect_url = url.spec();
  http_response->AddCustomHeader("Google-Accounts-SAML", "Start");

  http_response->AddCustomHeader("continue", redirect_url);
}

void FakeGaia::HandleEmbeddedSigninChallenge(const HttpRequest& request,
                                             BasicHttpResponse* http_response) {
  std::string email;
  GetQueryParameter(request.content, "identifier", &email);

  if (!merge_session_params_.auth_sid_cookie.empty() &&
      !merge_session_params_.auth_lsid_cookie.empty()) {
    SetCookies(http_response, merge_session_params_.auth_sid_cookie,
               merge_session_params_.auth_lsid_cookie);
  }

  AddGoogleAccountsSigninHeader(http_response, email);

  if (issue_oauth_code_cookie_)
    SetOAuthCodeCookie(http_response);
}

void FakeGaia::HandleSSO(const HttpRequest& request,
                         BasicHttpResponse* http_response) {
  if (!merge_session_params_.auth_sid_cookie.empty() &&
      !merge_session_params_.auth_lsid_cookie.empty()) {
    SetCookies(http_response,
               merge_session_params_.auth_sid_cookie,
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

void FakeGaia::HandleDummySAMLContinue(const HttpRequest& request,
                                       BasicHttpResponse* http_response) {
  http_response->set_content("");
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

    base::DictionaryValue response_dict;
    response_dict.SetString("refresh_token",
                            merge_session_params_.refresh_token);
    if (!device_id.empty())
      refresh_token_to_device_id_map_[merge_session_params_.refresh_token] =
          device_id;
    response_dict.SetString("access_token",
                            merge_session_params_.access_token);
    if (!merge_session_params_.id_token.empty())
      response_dict.SetString("id_token", merge_session_params_.id_token);
    response_dict.SetInteger("expires_in", 3600);
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
      base::DictionaryValue response_dict;
      response_dict.SetString("access_token", token_info->token);
      response_dict.SetInteger("expires_in", 3600);
      response_dict.SetString("id_token", token_info->id_token);
      FormatOkJSONResponse(response_dict, http_response);
      return;
    }
  }

  LOG(ERROR) << "Bad request for /oauth2/v4/token - "
              << "refresh_token = " << refresh_token
              << ", scope = " << scope
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
    base::DictionaryValue response_dict;
    response_dict.SetString("issued_to", token_info->issued_to);
    response_dict.SetString("audience", token_info->audience);
    response_dict.SetString("user_id", token_info->user_id);
    std::vector<base::StringPiece> scope_vector(token_info->scopes.begin(),
                                                token_info->scopes.end());
    response_dict.SetString("scope", base::JoinString(scope_vector, " "));
    response_dict.SetInteger("expires_in", token_info->expires_in);
    response_dict.SetString("email", token_info->email);
    response_dict.SetString("id_token", token_info->id_token);
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
  if (GetAccessToken(request, kAuthHeaderBearer, &access_token) &&
      GetQueryParameter(request.content, "scope", &scope) &&
      GetQueryParameter(request.content, "client_id", &client_id)) {
    const AccessTokenInfo* token_info =
        FindAccessTokenInfo(access_token, client_id, scope);
    if (token_info) {
      base::DictionaryValue response_dict;
      response_dict.SetString("issueAdvice", "auto");
      response_dict.SetString("expiresIn",
                              base::NumberToString(token_info->expires_in));
      response_dict.SetString("token", token_info->token);
      response_dict.SetString("id_token", token_info->id_token);
      FormatOkJSONResponse(response_dict, http_response);
      return;
    }
  }
  http_response->set_code(net::HTTP_BAD_REQUEST);
}

void FakeGaia::HandleListAccounts(const HttpRequest& request,
                                  BasicHttpResponse* http_response) {
  http_response->set_content(base::StringPrintf(
      kListAccountsResponseFormat, merge_session_params_.email.c_str()));
  http_response->set_code(net::HTTP_OK);
}

void FakeGaia::HandleGetUserInfo(const HttpRequest& request,
                                 BasicHttpResponse* http_response) {
  http_response->set_content(base::StringPrintf(
      "email=%s\ndisplayEmail=%s",
      merge_session_params_.email.c_str(),
      merge_session_params_.email.c_str()));
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
    base::DictionaryValue response_dict;
    response_dict.SetString("id", GetGaiaIdOfEmail(token_info->email));
    response_dict.SetString("email", token_info->email);
    response_dict.SetString("verified_email", token_info->email);
    response_dict.SetString("id_token", token_info->id_token);
    FormatOkJSONResponse(response_dict, http_response);
  } else {
    http_response->set_code(net::HTTP_BAD_REQUEST);
  }
}

void FakeGaia::HandleSAMLRedirect(const HttpRequest& request,
                                  BasicHttpResponse* http_response) {
  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
  std::string domain;
  GetQueryParameter(request_url.query(), "domain", &domain);

  // Get the redirect url.
  auto itr = saml_domain_url_map_.find(domain);
  if (itr == saml_domain_url_map_.end()) {
    http_response->set_code(net::HTTP_BAD_REQUEST);
    return;
  }

  GURL url = itr->second;
  url = net::AppendQueryParameter(url, "SAMLRequest", "fake_request");
  url = net::AppendQueryParameter(url, "RelayState",
                                  GaiaUrls::GetInstance()
                                      ->gaia_url()
                                      .Resolve(kDummySAMLContinuePath)
                                      .spec());
  std::string redirect_url = url.spec();
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Google-Accounts-SAML", "Start");
  http_response->AddCustomHeader("Location", redirect_url);
}

void FakeGaia::HandleGetCheckConnectionInfo(const HttpRequest& request,
                                            BasicHttpResponse* http_response) {
  base::ListValue connection_list;
  FormatOkJSONResponse(connection_list, http_response);
}

void FakeGaia::HandleGetReAuthProofToken(const HttpRequest& request,
                                         BasicHttpResponse* http_response) {
  base::DictionaryValue response_dict;
  std::unique_ptr<base::DictionaryValue> error =
      std::make_unique<base::DictionaryValue>();

  switch (next_reauth_status_) {
    case GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess:
      response_dict.SetString("encodedRapt", "abc123");
      FormatOkJSONResponse(response_dict, http_response);
      break;

    case GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant:
      error->SetString("message", "INVALID_GRANT");
      response_dict.SetDictionary("error", std::move(error));
      FormatJSONResponse(response_dict, net::HTTP_BAD_REQUEST, http_response);
      break;

    case GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidRequest:
      error->SetString("message", "INVALID_REQUEST");
      response_dict.SetDictionary("error", std::move(error));
      FormatJSONResponse(response_dict, net::HTTP_BAD_REQUEST, http_response);
      break;

    case GaiaAuthConsumer::ReAuthProofTokenStatus::kUnauthorizedClient:
      error->SetString("message", "UNAUTHORIZED_CLIENT");
      response_dict.SetDictionary("error", std::move(error));
      FormatJSONResponse(response_dict, net::HTTP_FORBIDDEN, http_response);
      break;

    case GaiaAuthConsumer::ReAuthProofTokenStatus::kInsufficientScope:
      error->SetString("message", "INSUFFICIENT_SCOPE");
      response_dict.SetDictionary("error", std::move(error));
      FormatJSONResponse(response_dict, net::HTTP_FORBIDDEN, http_response);
      break;

    case GaiaAuthConsumer::ReAuthProofTokenStatus::kCredentialNotSet:
      response_dict.SetDictionary("error", std::move(error));
      FormatJSONResponse(response_dict, net::HTTP_FORBIDDEN, http_response);
      break;

    default:
      LOG(FATAL) << "Unsupported ReAuthProofTokenStatus: "
                 << static_cast<int>(next_reauth_status_);
      break;
  }
}
