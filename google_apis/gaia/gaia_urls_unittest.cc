// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_urls.h"

#include <memory>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/scoped_command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "google_apis/gaia/gaia_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
#if BUILDFLAG(IS_ANDROID)
const char kSigninChromeSyncKeysPlatformSuffix[] = "android";
#elif BUILDFLAG(IS_IOS)
const char kSigninChromeSyncKeysPlatformSuffix[] = "ios";
#elif BUILDFLAG(IS_CHROMEOS)
const char kSigninChromeSyncKeysPlatformSuffix[] = "chromeos";
#else
const char kSigninChromeSyncKeysPlatformSuffix[] = "desktop";
#endif

base::FilePath GetTestFilePath(const std::string& relative_path) {
  base::FilePath path;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path)) {
    return base::FilePath();
  }
  return path.AppendASCII("google_apis")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("gaia")
      .AppendASCII(relative_path);
}
}  // namespace

class GaiaUrlsTest : public ::testing::Test {
 public:
  GaiaUrlsTest() = default;
  ~GaiaUrlsTest() override = default;

  // Lazily constructs |gaia_urls_|.
  GaiaUrls* gaia_urls() {
    if (!gaia_urls_) {
      GaiaConfig::ResetInstanceForTesting();
      gaia_urls_ = std::make_unique<GaiaUrls>();
    }
    return gaia_urls_.get();
  }

 private:
  std::unique_ptr<GaiaUrls> gaia_urls_;
};

TEST_F(GaiaUrlsTest, InitializeDefault_AllUrls) {
  EXPECT_EQ(gaia_urls()->google_url().spec(), "http://google.com/");
  EXPECT_EQ(gaia_urls()->secure_google_url().spec(), "https://google.com/");
  EXPECT_EQ(gaia_urls()->gaia_url().spec(), "https://accounts.google.com/");
  EXPECT_EQ(gaia_urls()->google_apis_origin_url(),
            "https://www.googleapis.com/");
  EXPECT_EQ(gaia_urls()->classroom_api_origin_url(),
            "https://classroom.googleapis.com/");
  EXPECT_EQ(gaia_urls()->tasks_api_origin_url(),
            "https://tasks.googleapis.com/");
  EXPECT_EQ(gaia_urls()->embedded_setup_chromeos_url().spec(),
            "https://accounts.google.com/embedded/setup/v2/chromeos");
  EXPECT_EQ(gaia_urls()->embedded_setup_chromeos_kid_signup_url().spec(),
            "https://accounts.google.com/embedded/setup/kidsignup/chromeos");
  EXPECT_EQ(gaia_urls()->embedded_setup_chromeos_kid_signin_url().spec(),
            "https://accounts.google.com/embedded/setup/kidsignin/chromeos");
  EXPECT_EQ(gaia_urls()->embedded_setup_windows_url().spec(),
            "https://accounts.google.com/embedded/setup/windows");
  EXPECT_EQ(gaia_urls()->embedded_reauth_chromeos_url().spec(),
            "https://accounts.google.com/embedded/reauth/chromeos");
  EXPECT_EQ(gaia_urls()->saml_redirect_chromeos_url().spec(),
            "https://accounts.google.com/samlredirect");
  EXPECT_EQ(gaia_urls()->signin_chrome_sync_dice().spec(),
            "https://accounts.google.com/signin/chrome/sync?ssp=1");
  EXPECT_EQ(gaia_urls()->signin_chrome_sync_keys_retrieval_url().spec(),
            std::string("https://accounts.google.com/encryption/unlock/") +
                kSigninChromeSyncKeysPlatformSuffix);
  EXPECT_EQ(
      gaia_urls()->signin_chrome_sync_keys_recoverability_degraded_url().spec(),
      std::string("https://accounts.google.com/encryption/unlock/") +
          kSigninChromeSyncKeysPlatformSuffix +
          std::string("?kdi=CAIaDgoKY2hyb21lc3luYxAB"));
  EXPECT_EQ(gaia_urls()->service_logout_url().spec(),
            "https://accounts.google.com/Logout");
  EXPECT_EQ(gaia_urls()->LogOutURLWithSource("").spec(),
            "https://accounts.google.com/Logout?continue=https://"
            "accounts.google.com/chrome/blank.html");
  EXPECT_EQ(gaia_urls()->oauth_multilogin_url().spec(),
            "https://accounts.google.com/oauth/multilogin");
  EXPECT_EQ(gaia_urls()->oauth_user_info_url().spec(),
            "https://www.googleapis.com/oauth2/v1/userinfo");
  EXPECT_EQ(gaia_urls()->ListAccountsURLWithSource("").spec(),
            "https://accounts.google.com/ListAccounts?json=standard");
  EXPECT_EQ(gaia_urls()->embedded_signin_url().spec(),
            "https://accounts.google.com/embedded/setup/chrome/usermenu");
  EXPECT_EQ(gaia_urls()->add_account_url().spec(),
            "https://accounts.google.com/AddSession");
  EXPECT_EQ(gaia_urls()->reauth_url().spec(),
            "https://accounts.google.com/embedded/xreauth/chrome");
  EXPECT_EQ(gaia_urls()->account_capabilities_url().spec(),
            "https://accountcapabilities-pa.googleapis.com/v1/"
            "accountcapabilities:batchGet");
  EXPECT_EQ(gaia_urls()->GetCheckConnectionInfoURLWithSource("").spec(),
            "https://accounts.google.com/GetCheckConnectionInfo");
  EXPECT_EQ(gaia_urls()->oauth2_token_url().spec(),
            "https://www.googleapis.com/oauth2/v4/token");
  EXPECT_EQ(gaia_urls()->oauth2_issue_token_url().spec(),
            "https://oauthaccountmanager.googleapis.com/v1/issuetoken");
  EXPECT_EQ(gaia_urls()->oauth2_token_info_url().spec(),
            "https://www.googleapis.com/oauth2/v2/tokeninfo");
  EXPECT_EQ(gaia_urls()->oauth2_revoke_url().spec(),
            "https://accounts.google.com/o/oauth2/revoke");
  EXPECT_EQ(gaia_urls()->reauth_api_url().spec(),
            "https://www.googleapis.com/reauth/v1beta/users/");
}

TEST_F(GaiaUrlsTest, InitializeDefault_URLSwitches) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "google-url", "http://test-google.com");
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "gaia-url", "https://test-gaia.com");
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "google-apis-url", "https://test-googleapis.com");
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "lso-url", "https://test-lso.com");
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "oauth-account-manager-url", "https://test-oauthaccountmanager.com");

  EXPECT_EQ(gaia_urls()->google_url().spec(), "http://test-google.com/");
  EXPECT_EQ(gaia_urls()->secure_google_url().spec(),
            "https://test-google.com/");
  EXPECT_EQ(gaia_urls()->gaia_url().spec(), "https://test-gaia.com/");
  EXPECT_EQ(gaia_urls()->embedded_setup_chromeos_url().spec(),
            "https://test-gaia.com/embedded/setup/v2/chromeos");
  EXPECT_EQ(gaia_urls()->embedded_setup_chromeos_kid_signup_url().spec(),
            "https://test-gaia.com/embedded/setup/kidsignup/chromeos");
  EXPECT_EQ(gaia_urls()->embedded_setup_chromeos_kid_signin_url().spec(),
            "https://test-gaia.com/embedded/setup/kidsignin/chromeos");
  EXPECT_EQ(gaia_urls()->embedded_setup_windows_url().spec(),
            "https://test-gaia.com/embedded/setup/windows");
  EXPECT_EQ(gaia_urls()->embedded_reauth_chromeos_url().spec(),
            "https://test-gaia.com/embedded/reauth/chromeos");
  EXPECT_EQ(gaia_urls()->saml_redirect_chromeos_url().spec(),
            "https://test-gaia.com/samlredirect");
  EXPECT_EQ(gaia_urls()->signin_chrome_sync_dice().spec(),
            "https://test-gaia.com/signin/chrome/sync?ssp=1");
  EXPECT_EQ(gaia_urls()->signin_chrome_sync_keys_retrieval_url().spec(),
            std::string("https://test-gaia.com/encryption/unlock/") +
                kSigninChromeSyncKeysPlatformSuffix);
  EXPECT_EQ(
      gaia_urls()->signin_chrome_sync_keys_recoverability_degraded_url().spec(),
      std::string("https://test-gaia.com/encryption/unlock/") +
          kSigninChromeSyncKeysPlatformSuffix +
          std::string("?kdi=CAIaDgoKY2hyb21lc3luYxAB"));
  EXPECT_EQ(gaia_urls()->service_logout_url().spec(),
            "https://test-gaia.com/Logout");
  EXPECT_EQ(gaia_urls()->LogOutURLWithSource("").spec(),
            "https://test-gaia.com/Logout?continue=https://"
            "test-gaia.com/chrome/blank.html");
  EXPECT_EQ(gaia_urls()->oauth_multilogin_url().spec(),
            "https://test-gaia.com/oauth/multilogin");
  EXPECT_EQ(gaia_urls()->oauth_user_info_url().spec(),
            "https://test-googleapis.com/oauth2/v1/userinfo");
  EXPECT_EQ(gaia_urls()->ListAccountsURLWithSource("").spec(),
            "https://test-gaia.com/ListAccounts?json=standard");
  EXPECT_EQ(gaia_urls()->embedded_signin_url().spec(),
            "https://test-gaia.com/embedded/setup/chrome/usermenu");
  EXPECT_EQ(gaia_urls()->add_account_url().spec(),
            "https://test-gaia.com/AddSession");
  EXPECT_EQ(gaia_urls()->reauth_url().spec(),
            "https://test-gaia.com/embedded/xreauth/chrome");
  EXPECT_EQ(gaia_urls()->GetCheckConnectionInfoURLWithSource("").spec(),
            "https://test-gaia.com/GetCheckConnectionInfo");
  EXPECT_EQ(gaia_urls()->oauth2_token_url().spec(),
            "https://test-googleapis.com/oauth2/v4/token");
  EXPECT_EQ(gaia_urls()->oauth2_issue_token_url().spec(),
            "https://test-oauthaccountmanager.com/v1/issuetoken");
  EXPECT_EQ(gaia_urls()->oauth2_token_info_url().spec(),
            "https://test-googleapis.com/oauth2/v2/tokeninfo");
  EXPECT_EQ(gaia_urls()->oauth2_revoke_url().spec(),
            "https://test-lso.com/o/oauth2/revoke");
  EXPECT_EQ(gaia_urls()->reauth_api_url().spec(),
            "https://test-googleapis.com/reauth/v1beta/users/");
}

TEST_F(GaiaUrlsTest, InitializeFromConfig_OneUrl) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      "gaia-config", GetTestFilePath("one_url.json"));

  // A URL present in config should be set.
  EXPECT_EQ(gaia_urls()->add_account_url().spec(),
            "https://accounts.example.com/ExampleAddSession");
  // All other URLs should have default values.
  EXPECT_EQ(gaia_urls()->oauth_multilogin_url().spec(),
            "https://accounts.google.com/oauth/multilogin");
}

TEST_F(GaiaUrlsTest, InitializeFromConfig_OneBaseUrl) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      "gaia-config", GetTestFilePath("one_base_url.json"));

  // A base URL present in config should be set and should be use to compute all
  // derived URLs with default suffixes.
  EXPECT_EQ(gaia_urls()->gaia_url().spec(), "https://accounts.example.com/");
  EXPECT_EQ(gaia_urls()->add_account_url().spec(),
            "https://accounts.example.com/AddSession");
  EXPECT_EQ(gaia_urls()->oauth_multilogin_url().spec(),
            "https://accounts.example.com/oauth/multilogin");
  // All other URLs should have default values.
  EXPECT_EQ(gaia_urls()->oauth2_token_url().spec(),
            "https://www.googleapis.com/oauth2/v4/token");
}

TEST_F(GaiaUrlsTest, InitializeFromConfig_PrecedenceOverSwitches) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      "gaia-config", GetTestFilePath("one_url.json"));
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "gaia-url", "https://myaccounts.com");

  // A URL present in config should be overridden.
  EXPECT_EQ(gaia_urls()->add_account_url().spec(),
            "https://accounts.example.com/ExampleAddSession");
  // All other URLs should be computed according command line flags.
  EXPECT_EQ(gaia_urls()->gaia_url().spec(), "https://myaccounts.com/");
  EXPECT_EQ(gaia_urls()->oauth_multilogin_url().spec(),
            "https://myaccounts.com/oauth/multilogin");
}

TEST_F(GaiaUrlsTest, InitializeFromConfig_AllUrls) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      "gaia-config", GetTestFilePath("all_urls.json"));

  EXPECT_EQ(gaia_urls()->google_url().spec(), "http://example.com/");
  EXPECT_EQ(gaia_urls()->secure_google_url().spec(), "https://example.com/");
  EXPECT_EQ(gaia_urls()->gaia_url().spec(), "https://accounts.example.com/");
  EXPECT_EQ(gaia_urls()->google_apis_origin_url(),
            "https://googleapis.will-be-overridden.com/");
  EXPECT_EQ(gaia_urls()->classroom_api_origin_url(),
            "https://classroom.will-be-overridden.com/");
  EXPECT_EQ(gaia_urls()->tasks_api_origin_url(),
            "https://tasks.will-be-overridden.com/");
  EXPECT_EQ(gaia_urls()->embedded_setup_chromeos_url().spec(),
            "https://accounts.example.com/embedded/setup/v2/chromeos");
  EXPECT_EQ(gaia_urls()->embedded_setup_chromeos_kid_signup_url().spec(),
            "https://accounts.example.com/embedded/setup/kidsignup/chromeos");
  EXPECT_EQ(gaia_urls()->embedded_setup_chromeos_kid_signin_url().spec(),
            "https://accounts.example.com/embedded/setup/kidsignin/chromeos");
  EXPECT_EQ(gaia_urls()->embedded_setup_windows_url().spec(),
            "https://accounts.example.com/embedded/setup/windows");
  EXPECT_EQ(gaia_urls()->embedded_reauth_chromeos_url().spec(),
            "https://accounts.example.com/embedded/reauth/chromeos");
  EXPECT_EQ(gaia_urls()->saml_redirect_chromeos_url().spec(),
            "https://accounts.example.com/samlredirect");
  EXPECT_EQ(gaia_urls()->signin_chrome_sync_dice().spec(),
            "https://accounts.example.com/signin/chrome/sync?ssp=1");
  EXPECT_EQ(gaia_urls()->signin_chrome_sync_keys_retrieval_url().spec(),
            "https://accounts.example.com/encryption/unlock/example-platform");
  EXPECT_EQ(
      gaia_urls()->signin_chrome_sync_keys_recoverability_degraded_url().spec(),
      "https://accounts.example.com/encryption/unlock/example-platform?"
      "kdi=CAIaDgoKY2hyb21lc3luYxAB");
  EXPECT_EQ(gaia_urls()->service_logout_url().spec(),
            "https://accounts.example.com/Logout");
  EXPECT_EQ(gaia_urls()->LogOutURLWithSource("").spec(),
            "https://accounts.example.com/Logout?continue=https://"
            "accounts.example.com/chrome/blank.html");
  EXPECT_EQ(gaia_urls()->oauth_multilogin_url().spec(),
            "https://accounts.example.com/oauth/multilogin");
  EXPECT_EQ(gaia_urls()->oauth_user_info_url().spec(),
            "https://www.exampleapis.com/oauth2/v1/userinfo");
  EXPECT_EQ(gaia_urls()->ListAccountsURLWithSource("").spec(),
            "https://accounts.example.com/ListAccounts?json=standard");
  EXPECT_EQ(gaia_urls()->embedded_signin_url().spec(),
            "https://accounts.example.com/embedded/setup/chrome/usermenu");
  EXPECT_EQ(gaia_urls()->add_account_url().spec(),
            "https://accounts.example.com/AddSession");
  EXPECT_EQ(gaia_urls()->reauth_url().spec(),
            "https://accounts.example.com/embedded/xreauth/chrome");
  EXPECT_EQ(gaia_urls()->account_capabilities_url().spec(),
            "https://accountcapabilities.exampleapis.com/v1/capabilities");
  EXPECT_EQ(gaia_urls()->GetCheckConnectionInfoURLWithSource("").spec(),
            "https://accounts.example.com/GetCheckConnectionInfo");
  EXPECT_EQ(gaia_urls()->oauth2_token_url().spec(),
            "https://www.exampleapis.com/oauth2/v4/token");
  EXPECT_EQ(gaia_urls()->oauth2_issue_token_url().spec(),
            "https://oauthaccountmanager.exampleapis.com/v1/issuetoken");
  EXPECT_EQ(gaia_urls()->oauth2_token_info_url().spec(),
            "https://www.exampleapis.com/oauth2/v2/tokeninfo");
  EXPECT_EQ(gaia_urls()->oauth2_revoke_url().spec(),
            "https://accounts.example.com/o/oauth2/revoke");
  EXPECT_EQ(gaia_urls()->reauth_api_url().spec(),
            "https://www.exampleapis.com/reauth/v1beta/users/");
}

TEST_F(GaiaUrlsTest, InitializeFromConfig_AllBaseUrls) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      "gaia-config", GetTestFilePath("all_base_urls.json"));

  EXPECT_EQ(gaia_urls()->google_url().spec(), "http://example.com/");
  EXPECT_EQ(gaia_urls()->secure_google_url().spec(), "https://example.com/");
  EXPECT_EQ(gaia_urls()->gaia_url().spec(), "https://accounts.example.com/");
  EXPECT_EQ(gaia_urls()->google_apis_origin_url(),
            "https://www.exampleapis.com/");
  EXPECT_EQ(gaia_urls()->classroom_api_origin_url(),
            "https://classroom.exampleapis.com/");
  EXPECT_EQ(gaia_urls()->tasks_api_origin_url(),
            "https://tasks.exampleapis.com/");
  EXPECT_EQ(gaia_urls()->embedded_setup_chromeos_url().spec(),
            "https://accounts.example.com/embedded/setup/v2/chromeos");
  EXPECT_EQ(gaia_urls()->embedded_setup_windows_url().spec(),
            "https://accounts.example.com/embedded/setup/windows");
  EXPECT_EQ(gaia_urls()->embedded_reauth_chromeos_url().spec(),
            "https://accounts.example.com/embedded/reauth/chromeos");
  EXPECT_EQ(gaia_urls()->saml_redirect_chromeos_url().spec(),
            "https://accounts.example.com/samlredirect");
  EXPECT_EQ(gaia_urls()->signin_chrome_sync_dice().spec(),
            "https://accounts.example.com/signin/chrome/sync?ssp=1");
  EXPECT_EQ(gaia_urls()->signin_chrome_sync_keys_retrieval_url().spec(),
            std::string("https://accounts.example.com/encryption/unlock/") +
                kSigninChromeSyncKeysPlatformSuffix);
  EXPECT_EQ(
      gaia_urls()->signin_chrome_sync_keys_recoverability_degraded_url().spec(),
      std::string("https://accounts.example.com/encryption/unlock/") +
          kSigninChromeSyncKeysPlatformSuffix +
          std::string("?kdi=CAIaDgoKY2hyb21lc3luYxAB"));
  EXPECT_EQ(gaia_urls()->service_logout_url().spec(),
            "https://accounts.example.com/Logout");
  EXPECT_EQ(gaia_urls()->LogOutURLWithSource("").spec(),
            "https://accounts.example.com/Logout?continue=https://"
            "accounts.example.com/chrome/blank.html");
  EXPECT_EQ(gaia_urls()->oauth_multilogin_url().spec(),
            "https://accounts.example.com/oauth/multilogin");
  EXPECT_EQ(gaia_urls()->oauth_user_info_url().spec(),
            "https://www.exampleapis.com/oauth2/v1/userinfo");
  EXPECT_EQ(gaia_urls()->ListAccountsURLWithSource("").spec(),
            "https://accounts.example.com/ListAccounts?json=standard");
  EXPECT_EQ(gaia_urls()->embedded_signin_url().spec(),
            "https://accounts.example.com/embedded/setup/chrome/usermenu");
  EXPECT_EQ(gaia_urls()->add_account_url().spec(),
            "https://accounts.example.com/AddSession");
  EXPECT_EQ(gaia_urls()->reauth_url().spec(),
            "https://accounts.example.com/embedded/xreauth/chrome");
  EXPECT_EQ(gaia_urls()->account_capabilities_url().spec(),
            "https://accountcapabilities.exampleapis.com/v1/"
            "accountcapabilities:batchGet");
  EXPECT_EQ(gaia_urls()->GetCheckConnectionInfoURLWithSource("").spec(),
            "https://accounts.example.com/GetCheckConnectionInfo");
  EXPECT_EQ(gaia_urls()->oauth2_token_url().spec(),
            "https://www.exampleapis.com/oauth2/v4/token");
  EXPECT_EQ(gaia_urls()->oauth2_issue_token_url().spec(),
            "https://oauthaccountmanager.exampleapis.com/v1/issuetoken");
  EXPECT_EQ(gaia_urls()->oauth2_token_info_url().spec(),
            "https://www.exampleapis.com/oauth2/v2/tokeninfo");
  EXPECT_EQ(gaia_urls()->oauth2_revoke_url().spec(),
            "https://lso.example.com/o/oauth2/revoke");
  EXPECT_EQ(gaia_urls()->reauth_api_url().spec(),
            "https://www.exampleapis.com/reauth/v1beta/users/");
}

TEST_F(GaiaUrlsTest, InitializeFromConfigContents) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "gaia-config-contents", R"(
{
  "urls": {
    "gaia_url": {
      "url": "https://accounts.example.com"
    }
  }
})");

  EXPECT_EQ(gaia_urls()->gaia_url().spec(), "https://accounts.example.com/");
}

TEST_F(GaiaUrlsTest, InitializeFromConfig_BadUrl) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      "gaia-config", GetTestFilePath("bad_url.json"));

  // A bad URL should be ignored and fallback to the default URL.
  EXPECT_EQ(gaia_urls()->google_url().spec(), "http://google.com/");
}

TEST_F(GaiaUrlsTest, InitializeFromConfig_BadUrlKey) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      "gaia-config", GetTestFilePath("bad_url_key.json"));

  // Fallback to the default URL.
  EXPECT_EQ(gaia_urls()->google_url().spec(), "http://google.com/");
}

TEST_F(GaiaUrlsTest, InitializeFromConfig_BadUrlsKey) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      "gaia-config", GetTestFilePath("bad_urls_key.json"));

  // Fallback to the default URL.
  EXPECT_EQ(gaia_urls()->google_url().spec(), "http://google.com/");
}

TEST_F(GaiaUrlsTest, InitializeFromConfig_FileNotFound) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      "gaia-config", GetTestFilePath("no_such_file.json"));

  EXPECT_DEATH_IF_SUPPORTED(gaia_urls(), "Couldn't read Gaia config file");
}

TEST_F(GaiaUrlsTest, InitializeFromConfig_NotAJson) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      "gaia-config", GetTestFilePath("not_a_json.txt"));

  EXPECT_DEATH_IF_SUPPORTED(gaia_urls(), "Couldn't parse Gaia config file");
}
