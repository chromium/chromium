// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_urls.h"

#include <string_view>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros/concat.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "google_apis/gaia/gaia_config.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/google_api_keys.h"
#include "url/url_canon.h"
#include "url/url_constants.h"

#define URL_KEY_AND_PTR(name) #name, &BASE_CONCAT(name, _)

namespace {

// Gaia service constants
const char kDefaultGoogleUrl[] = "http://google.com";
const char kDefaultGaiaUrl[] = "https://accounts.google.com";
const char kDefaultGoogleApisBaseUrl[] = "https://www.googleapis.com";
const char kDefaultOAuthAccountManagerBaseUrl[] =
    "https://oauthaccountmanager.googleapis.com";
const char kDefaultAccountCapabilitiesBaseUrl[] =
    "https://accountcapabilities-pa.googleapis.com";
constexpr char kDefaultClassroomApiBaseUrl[] =
    "https://classroom.googleapis.com";
constexpr char kDefaultTasksApiBaseUrl[] = "https://tasks.googleapis.com";

// API calls from accounts.google.com
const char kEmbeddedSetupChromeOsUrlSuffix[] = "embedded/setup/v2/chromeos";
const char kEmbeddedReauthChromeOsUrlSuffix[] = "embedded/reauth/chromeos";
const char kEmbeddedSetupChromeOsKidSignupUrlSuffix[] =
    "embedded/setup/kidsignup/chromeos";
const char kEmbeddedSetupChromeOsKidSigninUrlSuffix[] =
    "embedded/setup/kidsignin/chromeos";
const char kEmbeddedSetupWindowsUrlSuffix[] = "embedded/setup/windows";
const char kSamlRedirectChromeOsUrlSuffix[] = "samlredirect";
// Parameter "ssp=1" is used to skip showing the password bubble when a user
// signs in to Chrome. Note that Gaia will pass this client specified parameter
// to all URLs that are loaded as part of thi sign-in flow.
const char kSigninChromeSyncDice[] = "signin/chrome/sync?ssp=1";
// Opens the "Verify it's you" reauth gaia page.
const char kAccountChooser[] = "AccountChooser";

#if BUILDFLAG(IS_ANDROID)
const char kSigninChromeSyncKeysRetrievalUrl[] = "encryption/unlock/android";
#elif BUILDFLAG(IS_IOS)
const char kSigninChromeSyncKeysRetrievalUrl[] = "encryption/unlock/ios";
#elif BUILDFLAG(IS_CHROMEOS)
const char kSigninChromeSyncKeysRetrievalUrl[] = "encryption/unlock/chromeos";
#else
const char kSigninChromeSyncKeysRetrievalUrl[] = "encryption/unlock/desktop";
#endif
// Parameter "kdi" is used to distinguish recoverability management from
// retrieval. The value is a base64-encoded serialized protobuf, referred to
// internally as ClientDecryptableKeyDataInputs.
const char kSigninChromeSyncKeysRecoverabilityUrlSuffix[] =
    "?kdi=CAIaDgoKY2hyb21lc3luYxAB";

const char kServiceLogoutUrlSuffix[] = "Logout";
const char kBlankPageSuffix[] = "chrome/blank.html";
const char kOAuthMultiloginSuffix[] = "oauth/multilogin";
const char kListAccountsSuffix[] = "ListAccounts?json=standard";
const char kEmbeddedSigninSuffix[] = "embedded/setup/chrome/usermenu";
const char kAddAccountSuffix[] = "AddSession";
const char kReauthSuffix[] = "embedded/xreauth/chrome";
const char kGetCheckConnectionInfoSuffix[] = "GetCheckConnectionInfo";

// API calls from accounts.google.com (LSO)
const char kOAuth2RevokeUrlSuffix[] = "o/oauth2/revoke";

// API calls from www.googleapis.com
const char kOAuth2TokenUrlSuffix[] = "oauth2/v4/token";
const char kOAuth2TokenInfoUrlSuffix[] = "oauth2/v2/tokeninfo";
const char kOAuthUserInfoUrlSuffix[] = "oauth2/v1/userinfo";
const char kReAuthApiUrlSuffix[] = "reauth/v1beta/users/";

// API calls from oauthaccountmanager.googleapis.com
const char kOAuth2IssueTokenUrlSuffix[] = "v1/issuetoken";

// API calls from accountcapabilities-pa.googleapis.com
const char kAccountCapabilitiesBatchGetUrlSuffix[] =
    "v1/accountcapabilities:batchGet";

const char kRotateBoundCookiesUrlSuffix[] = "RotateBoundCookies";

GaiaUrls* g_instance_for_testing = nullptr;

void GetSwitchValueWithDefault(std::string_view switch_value,
                               std::string_view default_value,
                               std::string* output_value) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switch_value)) {
    *output_value = command_line->GetSwitchValueASCII(switch_value);
  } else {
    *output_value = std::string(default_value);
  }
}

GURL GetURLSwitchValueWithDefault(std::string_view switch_value,
                                  std::string_view default_value) {
  std::string string_value;
  GetSwitchValueWithDefault(switch_value, default_value, &string_value);
  const GURL result(string_value);
  if (result.is_valid()) {
    return result;
  }
  LOG(ERROR) << "Ignoring invalid URL \"" << string_value << "\" for switch \""
             << switch_value << "\"";
  return GURL(default_value);
}

url::Origin GetOriginSwitchValueWithDefault(std::string_view switch_value,
                                            std::string_view default_value) {
  std::string string_value;
  GetSwitchValueWithDefault(switch_value, default_value, &string_value);
  const url::Origin result = url::Origin::Create(GURL(string_value));
  if (result.GetURL().SchemeIsHTTPOrHTTPS() &&
      result.GetURL() == GURL(string_value)) {
    return result;
  }
  LOG(ERROR) << "Ignoring invalid origin \"" << string_value
             << "\" for switch \"" << switch_value << "\"";
  return url::Origin::Create(GURL(default_value));
}

void SetDefaultURLIfInvalid(GURL* url_to_set,
                            std::string_view switch_value,
                            std::string_view default_value) {
  if (!url_to_set->is_valid()) {
    *url_to_set = GetURLSwitchValueWithDefault(switch_value, default_value);
  }
}

void SetDefaultOriginIfOpaqueOrInvalidScheme(url::Origin* origin_to_set,
                                             std::string_view switch_value,
                                             std::string_view default_value) {
  if (origin_to_set->opaque() ||
      !origin_to_set->GetURL().SchemeIsHTTPOrHTTPS()) {
    *origin_to_set =
        GetOriginSwitchValueWithDefault(switch_value, default_value);
  }
}

void ResolveURLIfInvalid(GURL* url_to_set,
                         const GURL& base_url,
                         std::string_view suffix) {
  if (!url_to_set->is_valid()) {
    *url_to_set = base_url.Resolve(suffix);
  }
}

}  // namespace

GaiaUrls* GaiaUrls::GetInstance() {
  if (g_instance_for_testing) {
    return g_instance_for_testing;
  }
  return base::Singleton<GaiaUrls>::get();
}

GaiaUrls::GaiaUrls() {
  // Initialize all urls from a config first.
  InitializeFromConfig();

  // Set a default value for all urls not set by the config.
  InitializeDefault();
}

GaiaUrls::~GaiaUrls() = default;

// static
void GaiaUrls::SetInstanceForTesting(GaiaUrls* gaia_urls) {
  g_instance_for_testing = gaia_urls;
}

const GURL& GaiaUrls::google_url() const {
  return google_url_;
}

const GURL& GaiaUrls::secure_google_url() const {
  return secure_google_url_;
}

const url::Origin& GaiaUrls::gaia_origin() const {
  return gaia_origin_;
}

GURL GaiaUrls::gaia_url() const {
  return gaia_origin_.GetURL();
}

const GURL& GaiaUrls::embedded_setup_chromeos_url() const {
  return embedded_setup_chromeos_url_;
}

const GURL& GaiaUrls::embedded_setup_chromeos_kid_signup_url() const {
  return embedded_setup_chromeos_kid_signup_url_;
}

const GURL& GaiaUrls::embedded_setup_chromeos_kid_signin_url() const {
  return embedded_setup_chromeos_kid_signin_url_;
}

const GURL& GaiaUrls::embedded_setup_windows_url() const {
  return embedded_setup_windows_url_;
}

const GURL& GaiaUrls::embedded_reauth_chromeos_url() const {
  return embedded_reauth_chromeos_url_;
}

const GURL& GaiaUrls::saml_redirect_chromeos_url() const {
  return saml_redirect_chromeos_url_;
}

const GURL& GaiaUrls::signin_chrome_sync_dice() const {
  return signin_chrome_sync_dice_;
}

const GURL& GaiaUrls::reauth_chrome_dice() const {
  return reauth_chrome_dice_;
}

const GURL& GaiaUrls::signin_chrome_sync_keys_retrieval_url() const {
  return signin_chrome_sync_keys_retrieval_url_;
}

const GURL& GaiaUrls::signin_chrome_sync_keys_recoverability_degraded_url()
    const {
  return signin_chrome_sync_keys_recoverability_degraded_url_;
}

const GURL& GaiaUrls::service_logout_url() const {
  return service_logout_url_;
}

const GURL& GaiaUrls::oauth_multilogin_url() const {
  return oauth_multilogin_url_;
}

const GURL& GaiaUrls::oauth_user_info_url() const {
  return oauth_user_info_url_;
}

const GURL& GaiaUrls::embedded_signin_url() const {
  return embedded_signin_url_;
}

const GURL& GaiaUrls::add_account_url() const {
  return add_account_url_;
}

const GURL& GaiaUrls::reauth_url() const {
  return reauth_url_;
}

const GURL& GaiaUrls::account_capabilities_url() const {
  return account_capabilities_url_;
}

const std::string& GaiaUrls::oauth2_chrome_client_id() const {
  return google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
}

const std::string& GaiaUrls::oauth2_chrome_client_secret() const {
  return google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);
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

const GURL& GaiaUrls::rotate_bound_cookies_url() const {
  return rotate_bound_cookies_url_;
}

const GURL& GaiaUrls::classroom_api_origin_url() const {
  return classroom_api_origin_url_;
}

const GURL& GaiaUrls::tasks_api_origin_url() const {
  return tasks_api_origin_url_;
}

const GURL& GaiaUrls::blank_page_url() const {
  return blank_page_url_;
}

const GURL& GaiaUrls::google_apis_origin_url() const {
  return google_apis_origin_url_;
}

GURL GaiaUrls::ListAccountsURLWithSource(const std::string& source) {
  if (source.empty()) {
    return list_accounts_url_;
  } else {
    std::string query = list_accounts_url_.query();
    return list_accounts_url_.Resolve(base::StringPrintf(
        "?gpsia=1&source=%s&%s", source.c_str(), query.c_str()));
  }
}

GURL GaiaUrls::LogOutURLWithSource(const std::string& source) {
  std::string params =
      source.empty()
          ? base::StringPrintf("?continue=%s", blank_page_url_.spec().c_str())
          : base::StringPrintf("?source=%s&continue=%s", source.c_str(),
                               blank_page_url_.spec().c_str());
  return service_logout_url_.Resolve(params);
}

GURL GaiaUrls::GetCheckConnectionInfoURLWithSource(const std::string& source) {
  return source.empty() ? get_check_connection_info_url_
                        : get_check_connection_info_url_.Resolve(
                              base::StringPrintf("?source=%s", source.c_str()));
}

GURL GaiaUrls::LogOutURLWithContinueURL(const GURL& continue_url) {
  std::string params = base::StringPrintf(
      "?continue=%s", (continue_url.is_valid() ? continue_url.spec().c_str()
                                               : kDefaultGaiaUrl));
  return service_logout_url_.Resolve(params);
}

void GaiaUrls::InitializeDefault() {
  SetDefaultURLIfInvalid(&google_url_, switches::kGoogleUrl, kDefaultGoogleUrl);
  SetDefaultOriginIfOpaqueOrInvalidScheme(&gaia_origin_, switches::kGaiaUrl,
                                          kDefaultGaiaUrl);
  SetDefaultURLIfInvalid(&lso_origin_url_, switches::kLsoUrl, kDefaultGaiaUrl);
  SetDefaultURLIfInvalid(&google_apis_origin_url_, switches::kGoogleApisUrl,
                         kDefaultGoogleApisBaseUrl);
  SetDefaultURLIfInvalid(&oauth_account_manager_origin_url_,
                         switches::kOAuthAccountManagerUrl,
                         kDefaultOAuthAccountManagerBaseUrl);
  if (!account_capabilities_origin_url_.is_valid()) {
    account_capabilities_origin_url_ = GURL(kDefaultAccountCapabilitiesBaseUrl);
  }
  if (!secure_google_url_.is_valid()) {
    GURL::Replacements scheme_replacement;
    scheme_replacement.SetSchemeStr(url::kHttpsScheme);
    secure_google_url_ = google_url_.ReplaceComponents(scheme_replacement);
  }
  if (!classroom_api_origin_url_.is_valid()) {
    classroom_api_origin_url_ = GURL(kDefaultClassroomApiBaseUrl);
  }
  if (!tasks_api_origin_url_.is_valid()) {
    tasks_api_origin_url_ = GURL(kDefaultTasksApiBaseUrl);
  }

  CHECK(!gaia_origin_.opaque());
  const GURL gaia_url = gaia_origin_.GetURL();
  CHECK(gaia_url.SchemeIsHTTPOrHTTPS());

  // URLs from |gaia_origin_|.
  ResolveURLIfInvalid(&embedded_setup_chromeos_url_, gaia_url,
                      kEmbeddedSetupChromeOsUrlSuffix);
  ResolveURLIfInvalid(&embedded_setup_chromeos_kid_signup_url_, gaia_url,
                      kEmbeddedSetupChromeOsKidSignupUrlSuffix);
  ResolveURLIfInvalid(&embedded_setup_chromeos_kid_signin_url_, gaia_url,
                      kEmbeddedSetupChromeOsKidSigninUrlSuffix);
  ResolveURLIfInvalid(&embedded_setup_windows_url_, gaia_url,
                      kEmbeddedSetupWindowsUrlSuffix);
  ResolveURLIfInvalid(&embedded_reauth_chromeos_url_, gaia_url,
                      kEmbeddedReauthChromeOsUrlSuffix);
  ResolveURLIfInvalid(&saml_redirect_chromeos_url_, gaia_url,
                      kSamlRedirectChromeOsUrlSuffix);
  ResolveURLIfInvalid(&signin_chrome_sync_dice_, gaia_url,
                      kSigninChromeSyncDice);
  ResolveURLIfInvalid(&reauth_chrome_dice_, gaia_url, kAccountChooser);
  ResolveURLIfInvalid(&signin_chrome_sync_keys_retrieval_url_, gaia_url,
                      kSigninChromeSyncKeysRetrievalUrl);
  ResolveURLIfInvalid(
      &signin_chrome_sync_keys_recoverability_degraded_url_, gaia_url,
      base::StrCat({kSigninChromeSyncKeysRetrievalUrl,
                    kSigninChromeSyncKeysRecoverabilityUrlSuffix}));
  ResolveURLIfInvalid(&service_logout_url_, gaia_url, kServiceLogoutUrlSuffix);
  ResolveURLIfInvalid(&blank_page_url_, gaia_url, kBlankPageSuffix);
  ResolveURLIfInvalid(&oauth_multilogin_url_, gaia_url, kOAuthMultiloginSuffix);
  ResolveURLIfInvalid(&list_accounts_url_, gaia_url, kListAccountsSuffix);
  ResolveURLIfInvalid(&embedded_signin_url_, gaia_url, kEmbeddedSigninSuffix);
  ResolveURLIfInvalid(&add_account_url_, gaia_url, kAddAccountSuffix);
  ResolveURLIfInvalid(&reauth_url_, gaia_url, kReauthSuffix);
  ResolveURLIfInvalid(&get_check_connection_info_url_, gaia_url,
                      kGetCheckConnectionInfoSuffix);
  ResolveURLIfInvalid(&rotate_bound_cookies_url_, gaia_url,
                      kRotateBoundCookiesUrlSuffix);

  // URLs from |lso_origin_url_|.
  ResolveURLIfInvalid(&oauth2_revoke_url_, lso_origin_url_,
                      kOAuth2RevokeUrlSuffix);

  // URLs from |google_apis_origin_url_|.
  ResolveURLIfInvalid(&oauth2_token_url_, google_apis_origin_url_,
                      kOAuth2TokenUrlSuffix);
  ResolveURLIfInvalid(&oauth2_token_info_url_, google_apis_origin_url_,
                      kOAuth2TokenInfoUrlSuffix);
  ResolveURLIfInvalid(&oauth_user_info_url_, google_apis_origin_url_,
                      kOAuthUserInfoUrlSuffix);
  ResolveURLIfInvalid(&reauth_api_url_, google_apis_origin_url_,
                      kReAuthApiUrlSuffix);

  // URLs from |oauth_account_manager_origin_url_|.
  ResolveURLIfInvalid(&oauth2_issue_token_url_,
                      oauth_account_manager_origin_url_,
                      kOAuth2IssueTokenUrlSuffix);

  // URLs from |account_capabilities_origin_url_|.
  ResolveURLIfInvalid(&account_capabilities_url_,
                      account_capabilities_origin_url_,
                      kAccountCapabilitiesBatchGetUrlSuffix);
}

void GaiaUrls::InitializeFromConfig() {
  GaiaConfig* config = GaiaConfig::GetInstance();
  if (!config)
    return;

  config->GetURLIfExists(URL_KEY_AND_PTR(google_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(secure_google_url));

  GURL gaia_origin_url;
  config->GetURLIfExists("gaia_url", &gaia_origin_url);
  gaia_origin_ = url::Origin::Create(gaia_origin_url);

  config->GetURLIfExists(URL_KEY_AND_PTR(lso_origin_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(google_apis_origin_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(oauth_account_manager_origin_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(account_capabilities_origin_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(classroom_api_origin_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(tasks_api_origin_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(embedded_setup_chromeos_url));
  config->GetURLIfExists(
      URL_KEY_AND_PTR(embedded_setup_chromeos_kid_signup_url));
  config->GetURLIfExists(
      URL_KEY_AND_PTR(embedded_setup_chromeos_kid_signin_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(embedded_setup_windows_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(embedded_reauth_chromeos_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(saml_redirect_chromeos_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(signin_chrome_sync_dice));
  config->GetURLIfExists(URL_KEY_AND_PTR(reauth_chrome_dice));
  config->GetURLIfExists(
      URL_KEY_AND_PTR(signin_chrome_sync_keys_retrieval_url));
  config->GetURLIfExists(
      URL_KEY_AND_PTR(signin_chrome_sync_keys_recoverability_degraded_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(service_logout_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(blank_page_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(oauth_multilogin_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(oauth_user_info_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(list_accounts_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(embedded_signin_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(add_account_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(reauth_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(account_capabilities_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(get_check_connection_info_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(oauth2_token_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(oauth2_issue_token_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(oauth2_token_info_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(oauth2_revoke_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(reauth_api_url));
  config->GetURLIfExists(URL_KEY_AND_PTR(rotate_bound_cookies_url));
}
