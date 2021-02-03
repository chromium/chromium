// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#include "testing/gmock/include/gmock/gmock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ::testing::_;
using password_manager::MockPasswordStore;

namespace safe_browsing {

class FakeChromePasswordProtectionService
    : public ChromePasswordProtectionService {
 public:
  explicit FakeChromePasswordProtectionService(
      TestChromeBrowserState* browser_state)
      : ChromePasswordProtectionService(browser_state),
        is_incognito_(false),
        is_no_hosted_domain_found_(false) {}

  bool IsIncognito() override { return is_incognito_; }
  bool IsPrimaryAccountGmail() const override {
    return is_no_hosted_domain_found_;
  }
  void SetIsIncognito(bool is_incognito) { is_incognito_ = is_incognito; }
  void SetIsNoHostedDomainFound(bool is_no_hosted_domain_found) {
    is_no_hosted_domain_found_ = is_no_hosted_domain_found;
  }

 protected:
  friend class ChromePasswordProtectionServiceTest;

 private:
  bool is_incognito_;
  bool is_no_hosted_domain_found_;
};

class ChromePasswordProtectionServiceTest : public ChromeWebTest {
 public:
  ChromePasswordProtectionServiceTest() : ChromeWebTest() {
    service_ = std::make_unique<FakeChromePasswordProtectionService>(
        chrome_browser_state_.get());
  }

  ~ChromePasswordProtectionServiceTest() override = default;

  MockPasswordStore* GetProfilePasswordStore() const {
    return static_cast<MockPasswordStore*>(
        IOSChromePasswordStoreFactory::GetForBrowserState(
            chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

 protected:
  std::unique_ptr<FakeChromePasswordProtectionService> service_;
};

// All pinging is disabled when safe browsing is disabled.
TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPingingDisabledWhenSafeBrowsingDisabled) {
  chrome_browser_state_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                                false);

  LoginReputationClientRequest::TriggerType trigger_type;
  ReusedPasswordAccountType reused_password_type;

  trigger_type = LoginReputationClientRequest::PASSWORD_REUSE_EVENT;
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::SAVED_PASSWORD);
  service_->SetIsIncognito(false);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));
  service_->SetIsIncognito(true);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));

  trigger_type = LoginReputationClientRequest::PASSWORD_REUSE_EVENT;
  reused_password_type.set_account_type(ReusedPasswordAccountType::UNKNOWN);
  service_->SetIsIncognito(false);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));

  trigger_type = LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE;
  reused_password_type.set_account_type(ReusedPasswordAccountType::UNKNOWN);
  service_->SetIsIncognito(false);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));
  service_->SetIsIncognito(true);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));

  reused_password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
  reused_password_type.set_is_account_syncing(true);
  trigger_type = LoginReputationClientRequest::PASSWORD_REUSE_EVENT;
  service_->SetIsIncognito(false);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));
  service_->SetIsIncognito(true);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));
  service_->SetIsIncognito(true);
  service_->SetIsNoHostedDomainFound(true);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));
  chrome_browser_state_->GetPrefs()->SetInteger(
      prefs::kPasswordProtectionWarningTrigger, PASSWORD_REUSE);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));
}

// Saved password pinging is enabled.
TEST_F(ChromePasswordProtectionServiceTest,
       VerifyUserPopulationForSavedPasswordEntryPing) {
  LoginReputationClientRequest::TriggerType trigger_type =
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT;
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::SAVED_PASSWORD);

  service_->SetIsIncognito(false);
  EXPECT_TRUE(service_->IsPingingEnabled(trigger_type, reused_password_type));

  service_->SetIsIncognito(true);
  EXPECT_TRUE(service_->IsPingingEnabled(trigger_type, reused_password_type));

  service_->SetIsIncognito(false);
  reused_password_type.set_account_type(ReusedPasswordAccountType::UNKNOWN);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));
}

// Password field on focus pinging is disabled on iOS since SBER and enhanced
// protection are both disabled.
TEST_F(ChromePasswordProtectionServiceTest,
       VerifyUserPopulationForPasswordOnFocusPing) {
  LoginReputationClientRequest::TriggerType trigger_type =
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE;
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(ReusedPasswordAccountType::UNKNOWN);

  service_->SetIsIncognito(false);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));

  service_->SetIsIncognito(true);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));
}

// Sync password entry pinging is not yet enabled for iOS.
TEST_F(ChromePasswordProtectionServiceTest,
       VerifyUserPopulationForSyncPasswordEntryPing) {
  LoginReputationClientRequest::TriggerType trigger_type =
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT;
  // Sets up the account as a gmail account as there is no hosted domain.
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
  reused_password_type.set_is_account_syncing(true);

  service_->SetIsIncognito(false);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));

  service_->SetIsIncognito(true);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));

  service_->SetIsIncognito(true);
  service_->SetIsNoHostedDomainFound(true);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));

  chrome_browser_state_->GetPrefs()->SetInteger(
      prefs::kPasswordProtectionWarningTrigger, PASSWORD_PROTECTION_OFF);
  service_->SetIsIncognito(false);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));

  chrome_browser_state_->GetPrefs()->SetInteger(
      prefs::kPasswordProtectionWarningTrigger, PASSWORD_REUSE);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPingingIsSkippedIfMatchEnterpriseAllowlist) {
  ASSERT_FALSE(chrome_browser_state_->GetPrefs()->HasPrefPath(
      prefs::kSafeBrowsingAllowlistDomains));

  // If there's no allowlist, IsURLAllowlistedForPasswordEntry(_) should
  // return false.
  EXPECT_FALSE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://www.mydomain.com")));

  // Verify URL is allowed after setting allowlist in prefs.
  base::ListValue allowlist;
  allowlist.AppendString("mydomain.com");
  allowlist.AppendString("mydomain.net");
  chrome_browser_state_->GetPrefs()->Set(prefs::kSafeBrowsingAllowlistDomains,
                                         allowlist);
  EXPECT_TRUE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://www.mydomain.com")));

  // Verify change password URL (used for enterprise) is allowed (when set in
  // prefs), even when the domain is not allowed.
  chrome_browser_state_->GetPrefs()->ClearPref(
      prefs::kSafeBrowsingAllowlistDomains);
  EXPECT_FALSE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://www.mydomain.com")));
  chrome_browser_state_->GetPrefs()->SetString(
      prefs::kPasswordProtectionChangePasswordURL,
      "https://mydomain.com/change_password.html");
  EXPECT_TRUE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://mydomain.com/change_password.html#ref?user_name=alice")));

  // Verify login URL (used for enterprise) is allowed (when set in prefs), even
  // when the domain is not allowed.
  chrome_browser_state_->GetPrefs()->ClearPref(
      prefs::kSafeBrowsingAllowlistDomains);
  chrome_browser_state_->GetPrefs()->ClearPref(
      prefs::kPasswordProtectionChangePasswordURL);
  EXPECT_FALSE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://www.mydomain.com")));
  base::ListValue login_urls;
  login_urls.AppendString("https://mydomain.com/login.html");
  chrome_browser_state_->GetPrefs()->Set(prefs::kPasswordProtectionLoginURLs,
                                         login_urls);
  EXPECT_TRUE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://mydomain.com/login.html#ref?user_name=alice")));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPersistPhishedSavedPasswordCredential) {
  service_->SetIsIncognito(false);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {"http://example.test"}, {"http://2.example.com"}};

  EXPECT_CALL(*GetProfilePasswordStore(), AddCompromisedCredentialsImpl(_))
      .Times(2);
  service_->PersistPhishedSavedPasswordCredential(credentials);
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyRemovePhishedSavedPasswordCredential) {
  service_->SetIsIncognito(false);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {"http://example.test", base::ASCIIToUTF16("username1")},
      {"http://2.example.test", base::ASCIIToUTF16("username2")}};

  EXPECT_CALL(*GetProfilePasswordStore(),
              RemoveCompromisedCredentialsImpl(
                  _, _,
                  password_manager::RemoveInsecureCredentialsReason::
                      kMarkSiteAsLegitimate))
      .Times(2);
  service_->RemovePhishedSavedPasswordCredential(credentials);
}

}  // namespace safe_browsing
