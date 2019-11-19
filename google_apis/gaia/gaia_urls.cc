// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_urls.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/google_api_keys.h"
#include "url/url_canon.h"
#include "url/url_constants.h"

namespace {

// Gaia service constants
const char kDefaultGoogleUrl[] = "http://google.com";
const char kDefaultGaiaUrl[] = "https://accounts.google.com";
const char kDefaultGoogleApisBaseUrl[] = "https://www.googleapis.com";
const char kDefaultOAuthAccountManagerBaseUrl[] =
    "https://oauthaccountmanager.googleapis.com";

// API calls from accounts.google.com
const char kClientLoginUrlSuffix[] = "ClientLogin";
const char kServiceLoginUrlSuffix[] = "ServiceLogin";
const char kEmbeddedSetupChromeOsUrlSuffixV2[] = "embedded/setup/v2/chromeos";
const char kEmbeddedSetupWindowsUrlSuffix[] = "embedded/setup/windows";
// Parameter "ssp=1" is used to skip showing the password bubble when a user
// signs in to Chrome. Note that Gaia will pass this client specified parameter
// to all URLs that are loaded as part of thi sign-in flow.
const char kSigninChromeSyncDice[] = "signin/chrome/sync?ssp=1";
const char kSigninChromeSyncKeysUrl[] = "encryption/unlock/desktop";
const char kServiceLoginAuthUrlSuffix[] = "ServiceLoginAuth";
const char kServiceLogoutUrlSuffix[] = "Logout";
const char kContinueUrlForLogoutSuffix[] = "chrome/blank.html";
const char kGetUserInfoUrlSuffix[] = "GetUserInfo";
const char kTokenAuthUrlSuffix[] = "TokenAuth";
const char kMergeSessionUrlSuffix[] = "MergeSession";
const char kOAuthGetAccessTokenUrlSuffix[] = "OAuthGetAccessToken";
const char kOAuthWrapBridgeUrlSuffix[] = "OAuthWrapBridge";
const char kOAuth1LoginUrlSuffix[] = "OAuthLogin";
const char kOAuthMultiloginSuffix[] = "oauth/multilogin";
const char kOAuthRevokeTokenUrlSuffix[] = "AuthSubRevokeToken";
const char kListAccountsSuffix[] = "ListAccounts?json=standard";
const char kEmbeddedSigninSuffix[] = "embedded/setup/chrome/usermenu";
const char kAddAccountSuffix[] = "AddSession";
const char kGetCheckConnectionInfoSuffix[] = "GetCheckConnectionInfo";

// API calls from accounts.google.com (LSO)
const char kGetOAuthTokenUrlSuffix[] = "o/oauth/GetOAuthToken/";
const char kOAuth2AuthUrlSuffix[] = "o/oauth2/auth";
const char kOAuth2RevokeUrlSuffix[] = "o/oauth2/revoke";

// API calls from www.googleapis.com
const char kOAuth2TokenUrlSuffix[] = "oauth2/v4/token";
const char kOAuth2TokenInfoUrlSuffix[] = "oauth2/v2/tokeninfo";
const char kOAuthUserInfoUrlSuffix[] = "oauth2/v1/userinfo";
const char kReAuthApiUrlSuffix[] = "reauth/v1beta/users/";

// API calls from oauthaccountmanager.googleapis.com
const char kOAuth2IssueTokenUrlSuffix[] = "v1/issuetoken";

void GetSwitchValueWithDefault(const char* switch_value,
                               const char* default_value,
                               std::string* output_value) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switch_value)) {
    *output_value = command_line->GetSwitchValueASCII(switch_value);
  } else {
    *output_value = default_value;
  }
}

GURL GetURLSwitchValueWithDefault(const char* switch_value,
                                  const char* default_value) {
  std::string string_value;
  GetSwitchValueWithDefault(switch_value, default_value, &string_value);
  const GURL result(string_value);
  DCHECK(result.is_valid()) << "Invalid URL \"" << string_value
                            << "\" for switch \"" << switch_value << "\"";
  return result;
}


}  // namespace

GaiaUrls* GaiaUrls::GetInstance() {
  return base::Singleton<GaiaUrls>::get();
}

GaiaUrls::GaiaUrls() {
  google_url_ = GetURLSwitchValueWithDefault(switches::kGoogleUrl,
                                             kDefaultGoogleUrl);
  url::Replacements<char> scheme_replacement;
  scheme_replacement.SetScheme(url::kHttpsScheme,
                               url::Component(0, strlen(url::kHttpsScheme)));
  secure_google_url_ = google_url_.ReplaceComponents(scheme_replacement);
  gaia_url_ = GetURLSwitchValueWithDefault(switches::kGaiaUrl, kDefaultGaiaUrl);
  GURL lso_origin_url =
      GetURLSwitchValueWithDefault(switches::kLsoUrl, kDefaultGaiaUrl);
  GURL google_apis_origin_url = GetURLSwitchValueWithDefault(
      switches::kGoogleApisUrl, kDefaultGoogleApisBaseUrl);
  GURL oauth_account_manager_origin_url = GetURLSwitchValueWithDefault(
      switches::kOAuthAccountManagerUrl, kDefaultOAuthAccountManagerBaseUrl);

  captcha_base_url_ =
      GURL("http://" + gaia_url_.host() +
           (gaia_url_.has_port() ? ":" + gaia_url_.port() : ""));

  oauth2_chrome_client_id_ =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
  oauth2_chrome_client_secret_ =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);

  // URLs from accounts.google.com.
  client_login_url_ = gaia_url_.Resolve(kClientLoginUrlSuffix);
  service_login_url_ = gaia_url_.Resolve(kServiceLoginUrlSuffix);
  embedded_setup_chromeos_url_v2_ =
      gaia_url_.Resolve(kEmbeddedSetupChromeOsUrlSuffixV2);
  embedded_setup_windows_url_ =
      gaia_url_.Resolve(kEmbeddedSetupWindowsUrlSuffix);
  signin_chrome_sync_dice_ = gaia_url_.Resolve(kSigninChromeSyncDice);
  signin_chrome_sync_keys_url_ = gaia_url_.Resolve(kSigninChromeSyncKeysUrl);
  service_login_auth_url_ = gaia_url_.Resolve(kServiceLoginAuthUrlSuffix);
  service_logout_url_ = gaia_url_.Resolve(kServiceLogoutUrlSuffix);
  continue_url_for_logout_ = gaia_url_.Resolve(kContinueUrlForLogoutSuffix);
  get_user_info_url_ = gaia_url_.Resolve(kGetUserInfoUrlSuffix);
  token_auth_url_ = gaia_url_.Resolve(kTokenAuthUrlSuffix);
  merge_session_url_ = gaia_url_.Resolve(kMergeSessionUrlSuffix);
  oauth_multilogin_url_ = gaia_url_.Resolve(kOAuthMultiloginSuffix);
  oauth_get_access_token_url_ =
      gaia_url_.Resolve(kOAuthGetAccessTokenUrlSuffix);
  oauth_wrap_bridge_url_ = gaia_url_.Resolve(kOAuthWrapBridgeUrlSuffix);
  oauth_revoke_token_url_ = gaia_url_.Resolve(kOAuthRevokeTokenUrlSuffix);
  oauth1_login_url_ = gaia_url_.Resolve(kOAuth1LoginUrlSuffix);
  list_accounts_url_ = gaia_url_.Resolve(kListAccountsSuffix);
  embedded_signin_url_ = gaia_url_.Resolve(kEmbeddedSigninSuffix);
  add_account_url_ = gaia_url_.Resolve(kAddAccountSuffix);
  get_check_connection_info_url_ =
      gaia_url_.Resolve(kGetCheckConnectionInfoSuffix);

  // URLs from accounts.google.com (LSO).
  get_oauth_token_url_ = lso_origin_url.Resolve(kGetOAuthTokenUrlSuffix);
  oauth2_auth_url_ = lso_origin_url.Resolve(kOAuth2AuthUrlSuffix);
  oauth2_revoke_url_ = lso_origin_url.Resolve(kOAuth2RevokeUrlSuffix);

  // URLs from www.googleapis.com.
  oauth2_token_url_ = google_apis_origin_url.Resolve(kOAuth2TokenUrlSuffix);
  oauth2_token_info_url_ =
      google_apis_origin_url.Resolve(kOAuth2TokenInfoUrlSuffix);
  oauth_user_info_url_ =
      google_apis_origin_url.Resolve(kOAuthUserInfoUrlSuffix);
  reauth_api_url_ = google_apis_origin_url.Resolve(kReAuthApiUrlSuffix);

  // URLs from oauthaccountmanager.googleapis.com/v1/issuetoken
  oauth2_issue_token_url_ =
      oauth_account_manager_origin_url.Resolve(kOAuth2IssueTokenUrlSuffix);

  gaia_login_form_realm_ = gaia_url_;
}

GaiaUrls::~GaiaUrls() {
}

const GURL& GaiaUrls::google_url() const {
  return google_url_;
}

const GURL& GaiaUrls::secure_google_url() const {
  return secure_google_url_;
}

const GURL& GaiaUrls::gaia_url() const {
  return gaia_url_;
}

const GURL& GaiaUrls::captcha_base_url() const {
  return captcha_base_url_;
}

const GURL& GaiaUrls::client_login_url() const {
  return client_login_url_;
}

const GURL& GaiaUrls::service_login_url() const {
  return service_login_url_;
}

const GURL& GaiaUrls::embedded_setup_chromeos_url(unsigned version) const {
  DCHECK_EQ(version, 2U);
  return embedded_setup_chromeos_url_v2_;
}

const GURL& GaiaUrls::embedded_setup_windows_url() const {
  return embedded_setup_windows_url_;
}

const GURL& GaiaUrls::signin_chrome_sync_dice() const {
  return signin_chrome_sync_dice_;
}

const GURL& GaiaUrls::signin_chrome_sync_keys_url() const {
  return signin_chrome_sync_keys_url_;
}

const GURL& GaiaUrls::service_login_auth_url() const {
  return service_login_auth_url_;
}

const GURL& GaiaUrls::service_logout_url() const {
  return service_logout_url_;
}

const GURL& GaiaUrls::get_user_info_url() const {
  return get_user_info_url_;
}

const GURL& GaiaUrls::token_auth_url() const {
  return token_auth_url_;
}

const GURL& GaiaUrls::merge_session_url() const {
  return merge_session_url_;
}

const GURL& GaiaUrls::get_oauth_token_url() const {
  return get_oauth_token_url_;
}

const GURL& GaiaUrls::oauth_multilogin_url() const {
  return oauth_multilogin_url_;
}

const GURL& GaiaUrls::oauth_get_access_token_url() const {
  return oauth_get_access_token_url_;
}

const GURL& GaiaUrls::oauth_wrap_bridge_url() const {
  return oauth_wrap_bridge_url_;
}

const GURL& GaiaUrls::oauth_user_info_url() const {
  return oauth_user_info_url_;
}

const GURL& GaiaUrls::oauth_revoke_token_url() const {
  return oauth_revoke_token_url_;
}

const GURL& GaiaUrls::oauth1_login_url() const {
  return oauth1_login_url_;
}

const GURL& GaiaUrls::embedded_signin_url() const {
  return embedded_signin_url_;
}

const GURL& GaiaUrls::add_account_url() const {
  return add_account_url_;
}

const std::string& GaiaUrls::oauth2_chrome_client_id() const {
  return oauth2_chrome_client_id_;
}

const std::string& GaiaUrls::oauth2_chrome_client_secret() const {
  return oauth2_chrome_client_secret_;
}

const GURL& GaiaUrls::oauth2_auth_url() const {
  return oauth2_auth_url_;
}

const GURL& GaiaUrls::oauth2_token_url() const {
  return oauth2_token_url_;
}

const GURL& GaiaUrls::oauth2_issue_token_url() const {
  return oauth2_issue_token_url_;
}

const GURL& GaiaUrls::oauth2_token_info_url() const {
  return oauth2_token_info_url_;
}

const GURL& GaiaUrls::oauth2_revoke_url() const {
  return oauth2_revoke_url_;
}

const GURL& GaiaUrls::reauth_api_url() const {
  return reauth_api_url_;
}

const GURL& GaiaUrls::gaia_login_form_realm() const {
  return gaia_url_;
}

GURL GaiaUrls::ListAccountsURLWithSource(const std::string& source) {
  if (source.empty()) {
    return list_accounts_url_;
  } else {
    std::string query = list_accounts_url_.query();
    return list_accounts_url_.Resolve(
        base::StringPrintf("?gpsia=1&source=%s&%s",
                           source.c_str(),
                           query.c_str()));
  }
}

GURL GaiaUrls::LogOutURLWithSource(const std::string& source) {
  std::string params =
      source.empty()
          ? base::StringPrintf("?continue=%s",
                               continue_url_for_logout_.spec().c_str())
          : base::StringPrintf("?source=%s&continue=%s", source.c_str(),
                               continue_url_for_logout_.spec().c_str());
  return service_logout_url_.Resolve(params);
}

GURL GaiaUrls::GetCheckConnectionInfoURLWithSource(const std::string& source) {
  return source.empty()
      ? get_check_connection_info_url_
      : get_check_connection_info_url_.Resolve(
            base::StringPrintf("?source=%s", source.c_str()));
}
