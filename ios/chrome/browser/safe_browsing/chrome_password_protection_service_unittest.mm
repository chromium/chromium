// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include <memory>

#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

class ChromePasswordProtectionServiceTest : public PlatformTest {
 public:
  ChromePasswordProtectionServiceTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {
    service_ = std::make_unique<FakeChromePasswordProtectionService>(
        browser_state_.get());
  }

  ~ChromePasswordProtectionServiceTest() override {}

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<FakeChromePasswordProtectionService> service_;
};

// All pinging is disabled when safe browsing is disabled.
TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPingingDisabledWhenSafeBrowsingDisabled) {
  browser_state_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);

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
  browser_state_->GetPrefs()->SetInteger(
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

  browser_state_->GetPrefs()->SetInteger(
      prefs::kPasswordProtectionWarningTrigger, PASSWORD_PROTECTION_OFF);
  service_->SetIsIncognito(false);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));

  browser_state_->GetPrefs()->SetInteger(
      prefs::kPasswordProtectionWarningTrigger, PASSWORD_REUSE);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));
}

}  // namespace safe_browsing
