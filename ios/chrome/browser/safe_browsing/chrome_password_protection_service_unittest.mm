// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/password_protection/metrics_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/protocol/gaia_password_reuse.pb.h"
#include "components/sync_user_events/fake_user_event_service.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/safe_browsing/fake_safe_browsing_service.h"
#include "ios/chrome/browser/sync/ios_user_event_service_factory.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#include "ios/web/public/navigation/referrer.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ::testing::_;
using password_manager::metrics_util::PasswordType;
using password_manager::MockPasswordStore;
using safe_browsing::LoginReputationClientRequest;
using safe_browsing::PasswordProtectionTrigger;
using safe_browsing::RequestOutcome;
using safe_browsing::ReusedPasswordAccountType;
using sync_pb::GaiaPasswordReuse;
using PasswordReuseDialogInteraction =
    sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction;
using PasswordReuseLookup = sync_pb::GaiaPasswordReuse::PasswordReuseLookup;

namespace {

const char kTestEmail[] = "foo@example.com";

constexpr struct {
  // The response from the password protection service.
  RequestOutcome request_outcome;
  // The enum to log in the user event for that response.
  PasswordReuseLookup::LookupResult lookup_result;
} kTestCasesWithoutVerdict[]{
    {RequestOutcome::MATCHED_ALLOWLIST, PasswordReuseLookup::WHITELIST_HIT},
    {RequestOutcome::URL_NOT_VALID_FOR_REPUTATION_COMPUTING,
     PasswordReuseLookup::URL_UNSUPPORTED},
    {RequestOutcome::CANCELED, PasswordReuseLookup::REQUEST_FAILURE},
    {RequestOutcome::TIMEDOUT, PasswordReuseLookup::REQUEST_FAILURE},
    {RequestOutcome::DISABLED_DUE_TO_INCOGNITO,
     PasswordReuseLookup::REQUEST_FAILURE},
    {RequestOutcome::REQUEST_MALFORMED, PasswordReuseLookup::REQUEST_FAILURE},
    {RequestOutcome::FETCH_FAILED, PasswordReuseLookup::REQUEST_FAILURE},
    {RequestOutcome::RESPONSE_MALFORMED, PasswordReuseLookup::REQUEST_FAILURE},
    {RequestOutcome::SERVICE_DESTROYED, PasswordReuseLookup::REQUEST_FAILURE},
    {RequestOutcome::DISABLED_DUE_TO_FEATURE_DISABLED,
     PasswordReuseLookup::REQUEST_FAILURE},
    {RequestOutcome::DISABLED_DUE_TO_USER_POPULATION,
     PasswordReuseLookup::REQUEST_FAILURE}};

}  // namespace

class FakeChromePasswordProtectionService
    : public ChromePasswordProtectionService {
 public:
  explicit FakeChromePasswordProtectionService(
      SafeBrowsingService* sb_service,
      ChromeBrowserState* browser_state)
      : ChromePasswordProtectionService(sb_service, browser_state),
        is_incognito_(false),
        is_account_signed_in_(false),
        is_no_hosted_domain_found_(false) {}

  bool IsIncognito() override { return is_incognito_; }
  bool IsPrimaryAccountSignedIn() const override {
    return is_account_signed_in_;
  }
  bool IsPrimaryAccountGmail() const override {
    return is_no_hosted_domain_found_;
  }
  void SetIsIncognito(bool is_incognito) { is_incognito_ = is_incognito; }
  void SetIsAccountSignedIn(bool is_account_signed_in) {
    is_account_signed_in_ = is_account_signed_in;
  }
  void SetIsNoHostedDomainFound(bool is_no_hosted_domain_found) {
    is_no_hosted_domain_found_ = is_no_hosted_domain_found;
  }

 protected:
  friend class ChromePasswordProtectionServiceTest;

 private:
  bool is_incognito_;
  bool is_account_signed_in_;
  bool is_no_hosted_domain_found_;
};

class ChromePasswordProtectionServiceTest : public ChromeWebTest {
 public:
  ChromePasswordProtectionServiceTest() : ChromeWebTest() {
    safe_browsing_service_ = base::MakeRefCounted<FakeSafeBrowsingService>();

    service_ = std::make_unique<FakeChromePasswordProtectionService>(
        safe_browsing_service_.get(), chrome_browser_state_.get());

    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    fake_navigation_manager_ = navigation_manager.get();
    fake_web_state_.SetNavigationManager(std::move(navigation_manager));
    fake_web_state_.SetBrowserState(chrome_browser_state_.get());

    IOSUserEventServiceFactory::GetInstance()->SetTestingFactory(
        chrome_browser_state_.get(),
        base::BindRepeating(
            &ChromePasswordProtectionServiceTest::CreateFakeUserEventService,
            base::Unretained(this)));
  }

  ~ChromePasswordProtectionServiceTest() override = default;

  void NavigateAndCommit(const GURL& url) {
    fake_navigation_manager_->AddItem(
        url, ui::PageTransition::PAGE_TRANSITION_TYPED);
    web::NavigationItem* item = fake_navigation_manager_->GetItemAtIndex(
        fake_navigation_manager_->GetItemCount() - 1);
    item->SetTimestamp(base::Time::Now());
    fake_navigation_manager_->SetLastCommittedItem(item);
  }

  MockPasswordStore* GetProfilePasswordStore() const {
    return static_cast<MockPasswordStore*>(
        IOSChromePasswordStoreFactory::GetForBrowserState(
            chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  syncer::FakeUserEventService* GetUserEventService() const {
    return static_cast<syncer::FakeUserEventService*>(
        IOSUserEventServiceFactory::GetForBrowserState(
            chrome_browser_state_.get()));
  }

  std::unique_ptr<KeyedService> CreateFakeUserEventService(
      web::BrowserState* browser_state) {
    return std::make_unique<syncer::FakeUserEventService>();
  }

  CoreAccountInfo SetPrimaryAccount(const std::string& email) {
    identity_test_env_.MakeAccountAvailable(email);
    return identity_test_env_.SetPrimaryAccount(email);
  }

  void SetUpSyncAccount(const std::string& hosted_domain,
                        const CoreAccountInfo& account_info) {
    identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
        account_info.account_id, account_info.email, account_info.gaia,
        hosted_domain, "full_name", "given_name", "locale",
        "http://picture.example.com/picture.jpg");
  }

 protected:
  scoped_refptr<SafeBrowsingService> safe_browsing_service_;
  std::unique_ptr<FakeChromePasswordProtectionService> service_;
  web::FakeWebState fake_web_state_;
  web::FakeNavigationManager* fake_navigation_manager_;
  signin::IdentityTestEnvironment identity_test_env_;
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
      prefs::kPasswordProtectionWarningTrigger, safe_browsing::PASSWORD_REUSE);
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
      prefs::kPasswordProtectionWarningTrigger,
      safe_browsing::PASSWORD_PROTECTION_OFF);
  service_->SetIsIncognito(false);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));

  chrome_browser_state_->GetPrefs()->SetInteger(
      prefs::kPasswordProtectionWarningTrigger, safe_browsing::PASSWORD_REUSE);
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

  EXPECT_CALL(*GetProfilePasswordStore(), AddInsecureCredentialImpl(_))
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
              RemoveInsecureCredentialsImpl(
                  _, _,
                  password_manager::RemoveInsecureCredentialsReason::
                      kMarkSiteAsLegitimate))
      .Times(2);
  service_->RemovePhishedSavedPasswordCredential(credentials);
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordReuseUserEventNotRecordedDueToIncognito) {
  // Configure sync account type to GMAIL.
  CoreAccountInfo account_info = SetPrimaryAccount(kTestEmail);
  SetUpSyncAccount(kNoHostedDomainFound, account_info);
  service_->SetIsIncognito(true);
  ASSERT_TRUE(service_->IsIncognito());

  // Nothing should be logged because of incognito.
  NavigateAndCommit(GURL("https:www.example.com/"));

  // PasswordReuseDetected
  service_->MaybeLogPasswordReuseDetectedEvent(web_state());
  EXPECT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty());
  service_->MaybeLogPasswordReuseLookupEvent(
      web_state(), RequestOutcome::MATCHED_ALLOWLIST,
      PasswordType::PRIMARY_ACCOUNT_PASSWORD, nullptr);
  EXPECT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty());

  // PasswordReuseLookup
  unsigned long t = 0;
  for (const auto& it : kTestCasesWithoutVerdict) {
    service_->MaybeLogPasswordReuseLookupEvent(
        web_state(), it.request_outcome, PasswordType::PRIMARY_ACCOUNT_PASSWORD,
        nullptr);
    ASSERT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty()) << t;
    t++;
  }

  // PasswordReuseDialogInteraction
  service_->MaybeLogPasswordReuseDialogInteraction(
      1000 /* navigation_id */,
      PasswordReuseDialogInteraction::WARNING_ACTION_TAKEN);
  ASSERT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty());
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordReuseDetectedUserEventRecorded) {
  // Configure sync account type to GMAIL.
  CoreAccountInfo account_info = SetPrimaryAccount(kTestEmail);
  SetUpSyncAccount(kNoHostedDomainFound, account_info);
  service_->SetIsAccountSignedIn(true);
  NavigateAndCommit(GURL("https://www.example.com/"));

  // Case 1: safe_browsing_enabled = true
  chrome_browser_state_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                                true);
  service_->MaybeLogPasswordReuseDetectedEvent(&fake_web_state_);
  ASSERT_EQ(1ul, GetUserEventService()->GetRecordedUserEvents().size());
  GaiaPasswordReuse event = GetUserEventService()
                                ->GetRecordedUserEvents()[0]
                                .gaia_password_reuse_event();
  EXPECT_TRUE(event.reuse_detected().status().enabled());

  // Case 2: safe_browsing_enabled = false
  chrome_browser_state_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                                false);
  service_->MaybeLogPasswordReuseDetectedEvent(&fake_web_state_);
  ASSERT_EQ(2ul, GetUserEventService()->GetRecordedUserEvents().size());
  event = GetUserEventService()
              ->GetRecordedUserEvents()[1]
              .gaia_password_reuse_event();
  EXPECT_FALSE(event.reuse_detected().status().enabled());
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetWarningDetailTextSaved) {
  std::u16string warning_text =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SAVED);
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::SAVED_PASSWORD);
  std::vector<size_t> placeholder_offsets;
  EXPECT_EQ(warning_text, service_->GetWarningDetailText(reused_password_type,
                                                         &placeholder_offsets));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyGetWarningDetailTextCheckSavedDomains) {
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::SAVED_PASSWORD);
  std::vector<std::string> domains{"www.example.com"};
  service_->set_saved_passwords_matching_domains(domains);
  std::u16string warning_text = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_CHECK_PASSWORD_DETAILS_SAVED_1_DOMAIN,
      base::UTF8ToUTF16(domains[0]));
  std::vector<size_t> placeholder_offsets;
  EXPECT_EQ(warning_text, service_->GetWarningDetailText(reused_password_type,
                                                         &placeholder_offsets));

  placeholder_offsets.clear();
  domains.push_back("www.2.example.com");
  service_->set_saved_passwords_matching_domains(domains);
  warning_text = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_CHECK_PASSWORD_DETAILS_SAVED_2_DOMAIN,
      base::UTF8ToUTF16(domains[0]), base::UTF8ToUTF16(domains[1]));
  EXPECT_EQ(warning_text, service_->GetWarningDetailText(reused_password_type,
                                                         &placeholder_offsets));

  placeholder_offsets.clear();
  domains.push_back("www.3.example.com");
  service_->set_saved_passwords_matching_domains(domains);
  warning_text = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_CHECK_PASSWORD_DETAILS_SAVED_3_DOMAIN,
      base::UTF8ToUTF16(domains[0]), base::UTF8ToUTF16(domains[1]),
      base::UTF8ToUTF16(domains[2]));
  EXPECT_EQ(warning_text, service_->GetWarningDetailText(reused_password_type,
                                                         &placeholder_offsets));
  // Default domains should be prioritzed over other domains.
  placeholder_offsets.clear();
  domains.push_back("amazon.com");
  service_->set_saved_passwords_matching_domains(domains);
  warning_text = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_CHECK_PASSWORD_DETAILS_SAVED_3_DOMAIN,
      base::UTF8ToUTF16("amazon.com"), base::UTF8ToUTF16(domains[0]),
      base::UTF8ToUTF16(domains[1]));
  EXPECT_EQ(warning_text, service_->GetWarningDetailText(reused_password_type,
                                                         &placeholder_offsets));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyGetPlaceholdersForSavedPasswordWarningText) {
  std::vector<std::string> domains{"www.example.com"};
  domains.push_back("www.2.example.com");
  domains.push_back("www.3.example.com");
  domains.push_back("amazon.com");
  service_->set_saved_passwords_matching_domains(domains);
  // Default domains should be prioritzed over other domains.
  std::vector<std::u16string> expected_placeholders{
      base::UTF8ToUTF16("amazon.com"), base::UTF8ToUTF16(domains[0]),
      base::UTF8ToUTF16(domains[1])};
  EXPECT_EQ(expected_placeholders,
            service_->GetPlaceholdersForSavedPasswordWarningText());
}

TEST_F(ChromePasswordProtectionServiceTest, VerifySendsPingForAboutBlank) {
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::SAVED_PASSWORD);
  service_->SetIsIncognito(false);
  EXPECT_TRUE(
      service_->CanSendPing(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                            GURL("about:blank"), reused_password_type));
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetPingNotSentReason) {
  {
    // SBER disabled.
    ReusedPasswordAccountType reused_password_type;
    service_->SetIsIncognito(false);
    EXPECT_EQ(RequestOutcome::DISABLED_DUE_TO_USER_POPULATION,
              service_->GetPingNotSentReason(
                  LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                  GURL("about:blank"), reused_password_type));
    reused_password_type.set_account_type(ReusedPasswordAccountType::UNKNOWN);
    EXPECT_EQ(RequestOutcome::DISABLED_DUE_TO_USER_POPULATION,
              service_->GetPingNotSentReason(
                  LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                  GURL("about:blank"), reused_password_type));
  }
  {
    // In Incognito.
    ReusedPasswordAccountType reused_password_type;
    service_->SetIsIncognito(true);
    EXPECT_EQ(RequestOutcome::DISABLED_DUE_TO_INCOGNITO,
              service_->GetPingNotSentReason(
                  LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                  GURL("about:blank"), reused_password_type));
  }
  {
    // Turned off by admin.
    ReusedPasswordAccountType reused_password_type;
    service_->SetIsIncognito(false);
    reused_password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
    chrome_browser_state_->GetPrefs()->SetInteger(
        prefs::kPasswordProtectionWarningTrigger,
        safe_browsing::PASSWORD_PROTECTION_OFF);
    EXPECT_EQ(RequestOutcome::TURNED_OFF_BY_ADMIN,
              service_->GetPingNotSentReason(
                  LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                  GURL("about:blank"), reused_password_type));
  }
  {
    // Allowlisted by policy.
    ReusedPasswordAccountType reused_password_type;
    service_->SetIsIncognito(false);
    reused_password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
    chrome_browser_state_->GetPrefs()->SetInteger(
        prefs::kPasswordProtectionWarningTrigger,
        safe_browsing::PHISHING_REUSE);
    base::ListValue allowlist;
    allowlist.AppendString("mydomain.com");
    allowlist.AppendString("mydomain.net");
    chrome_browser_state_->GetPrefs()->Set(prefs::kSafeBrowsingAllowlistDomains,
                                           allowlist);
    EXPECT_EQ(RequestOutcome::MATCHED_ENTERPRISE_ALLOWLIST,
              service_->GetPingNotSentReason(
                  LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                  GURL("https://www.mydomain.com"), reused_password_type));
  }
  {
    // Password alert mode.
    ReusedPasswordAccountType reused_password_type;
    service_->SetIsIncognito(false);
    reused_password_type.set_account_type(ReusedPasswordAccountType::UNKNOWN);
    chrome_browser_state_->GetPrefs()->SetInteger(
        prefs::kPasswordProtectionWarningTrigger,
        safe_browsing::PASSWORD_REUSE);
    EXPECT_EQ(RequestOutcome::PASSWORD_ALERT_MODE,
              service_->GetPingNotSentReason(
                  LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                  GURL("about:blank"), reused_password_type));
  }
}
