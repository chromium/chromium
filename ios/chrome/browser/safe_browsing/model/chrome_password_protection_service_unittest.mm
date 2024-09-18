// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/chrome_password_protection_service.h"

#import <memory>
#import <string>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/mock_callback.h"
#import "base/values.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_reuse_detector.h"
#import "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/protocol/gaia_password_reuse.pb.h"
#import "components/sync_user_events/fake_user_event_service.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_metrics_collector_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/ios_user_event_service_factory.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_service.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

using password_manager::metrics_util::PasswordType;
using safe_browsing::LoginReputationClientRequest;
using safe_browsing::LoginReputationClientResponse;
using safe_browsing::PasswordProtectionTrigger;
using safe_browsing::RequestOutcome;
using safe_browsing::ReusedPasswordAccountType;
using sync_pb::GaiaPasswordReuse;
using ::testing::_;
using PasswordReuseDialogInteraction =
    sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction;
using PasswordReuseLookup = sync_pb::GaiaPasswordReuse::PasswordReuseLookup;

namespace {

const char kTestEmail[] = "foo@example.com";

const unsigned int kMinute = 60;
const unsigned int kDay = 24 * 60 * kMinute;

constexpr struct {
  // The response from the password protection service.
  RequestOutcome request_outcome;
  // The enum to log in the user event for that response.
  PasswordReuseLookup::LookupResult lookup_result;
} kTestCasesWithoutVerdict[]{
    {RequestOutcome::MATCHED_ALLOWLIST, PasswordReuseLookup::ALLOWLIST_HIT},
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

// A test factory to create a FakeUserEventService.
std::unique_ptr<KeyedService> CreateFakeUserEventService(
    web::BrowserState* browser_state) {
  return std::make_unique<syncer::FakeUserEventService>();
}
}  // namespace

class FakeChromePasswordProtectionService
    : public ChromePasswordProtectionService {
 public:
  explicit FakeChromePasswordProtectionService(
      SafeBrowsingService* sb_service,
      ProfileIOS* profile,
      history::HistoryService* history_service,
      safe_browsing::SafeBrowsingMetricsCollector*
          safe_browsing_metrics_collector,
      ChangePhishedCredentialsCallback add_phished_credentials,
      ChangePhishedCredentialsCallback remove_phished_credentials)
      : ChromePasswordProtectionService(sb_service,
                                        profile,
                                        history_service,
                                        safe_browsing_metrics_collector,
                                        add_phished_credentials,
                                        remove_phished_credentials),
        is_incognito_(false),
        is_account_signed_in_(false),
        is_no_hosted_domain_found_(false) {}

  bool IsIncognito() override { return is_incognito_; }
  bool IsPrimaryAccountSignedIn() const override {
    return is_account_signed_in_;
  }
  bool IsAccountGmail(const std::string& username) const override {
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

class ChromePasswordProtectionServiceTest : public PlatformTest {
 public:
  ChromePasswordProtectionServiceTest() = default;
  ~ChromePasswordProtectionServiceTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                            web::BrowserState,
                            password_manager::MockPasswordStoreInterface>));
    builder.AddTestingFactory(IOSUserEventServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateFakeUserEventService));
    profile_ = std::move(builder).Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);

    safe_browsing_service_ = base::MakeRefCounted<FakeSafeBrowsingService>();

    service_ = std::make_unique<FakeChromePasswordProtectionService>(
        safe_browsing_service_.get(), profile_.get(),
        ios::HistoryServiceFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS),
        SafeBrowsingMetricsCollectorFactory::GetForProfile(profile_.get()),
        mock_add_callback_.Get(), mock_remove_callback_.Get());

    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    fake_navigation_manager_ = navigation_manager.get();
    fake_web_state_.SetNavigationManager(std::move(navigation_manager));
    fake_web_state_.SetBrowserState(profile_.get());
  }

  void NavigateAndCommit(const GURL& url) {
    fake_navigation_manager_->AddItem(
        url, ui::PageTransition::PAGE_TRANSITION_TYPED);
    web::NavigationItem* item = fake_navigation_manager_->GetItemAtIndex(
        fake_navigation_manager_->GetItemCount() - 1);
    item->SetTimestamp(base::Time::Now());
    fake_navigation_manager_->SetLastCommittedItem(item);
  }

  syncer::FakeUserEventService* GetUserEventService() {
    return static_cast<syncer::FakeUserEventService*>(
        IOSUserEventServiceFactory::GetForProfile(profile_.get()));
  }

  CoreAccountInfo SetPrimaryAccount(const std::string& email) {
    identity_test_env_.MakeAccountAvailable(email);
    return identity_test_env_.SetPrimaryAccount(email,
                                                signin::ConsentLevel::kSignin);
  }

  void SetUpSyncAccount(const std::string& hosted_domain,
                        const CoreAccountInfo& account_info) {
    identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
        account_info.account_id, account_info.email, account_info.gaia,
        hosted_domain, "full_name", "given_name", "locale",
        "http://picture.example.com/picture.jpg");
  }

  LoginReputationClientResponse CreateVerdictProto(
      LoginReputationClientResponse::VerdictType verdict,
      int cache_duration_sec,
      const std::string& cache_expression) {
    LoginReputationClientResponse verdict_proto;
    verdict_proto.set_verdict_type(verdict);
    verdict_proto.set_cache_duration_sec(cache_duration_sec);
    verdict_proto.set_cache_expression(cache_expression);
    return verdict_proto;
  }

  void CacheVerdict(const GURL& url,
                    LoginReputationClientRequest::TriggerType trigger,
                    ReusedPasswordAccountType password_type,
                    LoginReputationClientResponse::VerdictType verdict,
                    int cache_duration_sec,
                    const std::string& cache_expression,
                    const base::Time& verdict_received_time) {
    ASSERT_FALSE(cache_expression.empty());
    LoginReputationClientResponse response(
        CreateVerdictProto(verdict, cache_duration_sec, cache_expression));
    service_->CacheVerdict(url, trigger, password_type, response,
                           verdict_received_time);
  }

  size_t GetStoredVerdictCount(LoginReputationClientRequest::TriggerType type) {
    return service_->GetStoredVerdictCount(type);
  }

 protected:
  web::WebState* web_state() { return web_state_.get(); }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;

  scoped_refptr<SafeBrowsingService> safe_browsing_service_;
  std::unique_ptr<FakeChromePasswordProtectionService> service_;
  web::FakeWebState fake_web_state_;
  raw_ptr<web::FakeNavigationManager> fake_navigation_manager_;
  base::MockCallback<
      ChromePasswordProtectionService::ChangePhishedCredentialsCallback>
      mock_add_callback_;
  base::MockCallback<
      ChromePasswordProtectionService::ChangePhishedCredentialsCallback>
      mock_remove_callback_;
  signin::IdentityTestEnvironment identity_test_env_;
};

// All pinging is disabled when safe browsing is disabled.
TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPingingDisabledWhenSafeBrowsingDisabled) {
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);

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
  profile_->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                   safe_browsing::PASSWORD_REUSE);
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

  profile_->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                   safe_browsing::PASSWORD_PROTECTION_OFF);
  service_->SetIsIncognito(false);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));

  profile_->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                   safe_browsing::PASSWORD_REUSE);
  EXPECT_FALSE(service_->IsPingingEnabled(trigger_type, reused_password_type));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPingingIsSkippedIfMatchEnterpriseAllowlist) {
  ASSERT_FALSE(
      profile_->GetPrefs()->HasPrefPath(prefs::kSafeBrowsingAllowlistDomains));

  // If there's no allowlist, IsURLAllowlistedForPasswordEntry(_) should
  // return false.
  EXPECT_FALSE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://www.mydomain.com")));

  // Verify URL is allowed after setting allowlist in prefs.
  base::Value::List allowlist;
  allowlist.Append("mydomain.com");
  allowlist.Append("mydomain.net");
  profile_->GetPrefs()->SetList(prefs::kSafeBrowsingAllowlistDomains,
                                std::move(allowlist));
  EXPECT_TRUE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://www.mydomain.com")));

  // Verify change password URL (used for enterprise) is allowed (when set in
  // prefs), even when the domain is not allowed.
  profile_->GetPrefs()->ClearPref(prefs::kSafeBrowsingAllowlistDomains);
  EXPECT_FALSE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://www.mydomain.com")));
  profile_->GetPrefs()->SetString(prefs::kPasswordProtectionChangePasswordURL,
                                  "https://mydomain.com/change_password.html");
  EXPECT_TRUE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://mydomain.com/change_password.html#ref?user_name=alice")));

  // Verify login URL (used for enterprise) is allowed (when set in prefs), even
  // when the domain is not allowed.
  profile_->GetPrefs()->ClearPref(prefs::kSafeBrowsingAllowlistDomains);
  profile_->GetPrefs()->ClearPref(prefs::kPasswordProtectionChangePasswordURL);
  EXPECT_FALSE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://www.mydomain.com")));
  base::Value::List login_urls;
  login_urls.Append("https://mydomain.com/login.html");
  profile_->GetPrefs()->SetList(prefs::kPasswordProtectionLoginURLs,
                                std::move(login_urls));
  EXPECT_TRUE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://mydomain.com/login.html#ref?user_name=alice")));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPersistPhishedSavedPasswordCredential) {
  service_->SetIsIncognito(false);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {"http://example.test"}, {"http://2.example.com"}};

  EXPECT_CALL(mock_add_callback_, Run(_, credentials[0]));
  EXPECT_CALL(mock_add_callback_, Run(_, credentials[1]));
  service_->PersistPhishedSavedPasswordCredential(credentials);
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyRemovePhishedSavedPasswordCredential) {
  service_->SetIsIncognito(false);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {"http://example.test", u"username1"},
      {"http://2.example.test", u"username2"}};

  EXPECT_CALL(mock_remove_callback_, Run(_, credentials[0]));
  EXPECT_CALL(mock_remove_callback_, Run(_, credentials[1]));
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
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  service_->MaybeLogPasswordReuseDetectedEvent(&fake_web_state_);
  ASSERT_EQ(1ul, GetUserEventService()->GetRecordedUserEvents().size());
  GaiaPasswordReuse event = GetUserEventService()
                                ->GetRecordedUserEvents()[0]
                                .gaia_password_reuse_event();
  EXPECT_TRUE(event.reuse_detected().status().enabled());

  // Case 2: safe_browsing_enabled = false
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
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
  EXPECT_EQ(warning_text, service_->GetWarningDetailText(reused_password_type));
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
    profile_->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
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
    profile_->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                     safe_browsing::PHISHING_REUSE);
    base::Value::List allowlist;
    allowlist.Append("mydomain.com");
    allowlist.Append("mydomain.net");
    profile_->GetPrefs()->SetList(prefs::kSafeBrowsingAllowlistDomains,
                                  std::move(allowlist));
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
    profile_->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                     safe_browsing::PASSWORD_REUSE);
    EXPECT_EQ(RequestOutcome::PASSWORD_ALERT_MODE,
              service_->GetPingNotSentReason(
                  LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                  GURL("about:blank"), reused_password_type));
  }
}

TEST_F(ChromePasswordProtectionServiceTest, TestCachePasswordReuseVerdicts) {
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  service_->SetIsAccountSignedIn(true);

  // Assume each verdict has a TTL of 10 minutes.
  // Cache a verdict for http://www.test.com/foo/index.html
  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  reused_password_account_type.set_is_account_syncing(true);
  CacheVerdict(GURL("http://www.test.com/foo/index.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/foo/", base::Time::Now());

  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Cache another verdict with the some origin and cache_expression should
  // override the cache.
  CacheVerdict(GURL("http://www.test.com/foo/index2.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::PHISHING, 10 * kMinute,
               "test.com/foo/", base::Time::Now());
  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  LoginReputationClientResponse out_verdict;
  EXPECT_EQ(LoginReputationClientResponse::PHISHING,
            service_->GetCachedVerdict(
                GURL("http://www.test.com/foo/index2.html"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &out_verdict));

  // Cache a password reuse verdict with a different password type but same
  // origin and cache expression should add a new entry.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  CacheVerdict(GURL("http://www.test.com/foo/index2.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::PHISHING, 10 * kMinute,
               "test.com/foo/", base::Time::Now());
  EXPECT_EQ(2U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  EXPECT_EQ(LoginReputationClientResponse::PHISHING,
            service_->GetCachedVerdict(
                GURL("http://www.test.com/foo/index2.html"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &out_verdict));

  // Cache another verdict with the same origin but different cache_expression
  // will increase the number of verdicts in the given origin.
  CacheVerdict(GURL("http://www.test.com/bar/index2.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/bar/", base::Time::Now());
  EXPECT_EQ(3U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Now cache a UNFAMILIAR_LOGIN_PAGE verdict, stored verdict count for
  // PASSWORD_REUSE_EVENT should be the same.
  CacheVerdict(GURL("http://www.test.com/foobar/index3.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/foobar/", base::Time::Now());
  EXPECT_EQ(3U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
}

TEST_F(ChromePasswordProtectionServiceTest,
       TestCachePasswordReuseVerdictsIncognito) {
  service_->SetIsIncognito(true);
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  reused_password_account_type.set_is_account_syncing(true);
  // No verdict will be cached for incognito profile.
  CacheVerdict(GURL("http://www.test.com/foo/index.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/foo/", base::Time::Now());

  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Try cache another verdict with the some origin and cache_expression.
  // Verdict count should not increase.
  CacheVerdict(GURL("http://www.test.com/foo/index2.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::PHISHING, 10 * kMinute,
               "test.com/foo/", base::Time::Now());
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Now cache a UNFAMILIAR_LOGIN_PAGE verdict, verdict count should not
  // increase.
  CacheVerdict(GURL("http://www.test.com/foobar/index3.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/foobar/", base::Time::Now());
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
}

TEST_F(ChromePasswordProtectionServiceTest, TestCacheUnfamiliarLoginVerdicts) {
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  reused_password_account_type.set_is_account_syncing(true);
  // Assume each verdict has a TTL of 10 minutes.
  // Cache a verdict for http://www.test.com/foo/index.html
  CacheVerdict(GURL("http://www.test.com/foo/index.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/foo/", base::Time::Now());

  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Cache another verdict with the same origin but different cache_expression
  // will increase the number of verdicts in the given origin.
  CacheVerdict(GURL("http://www.test.com/bar/index2.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/bar/", base::Time::Now());
  EXPECT_EQ(2U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Now cache a PASSWORD_REUSE_EVENT verdict, stored verdict count for
  // UNFAMILIAR_LOGIN_PAGE should be the same.
  CacheVerdict(GURL("http://www.test.com/foobar/index3.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/foobar/", base::Time::Now());
  EXPECT_EQ(2U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
}

TEST_F(ChromePasswordProtectionServiceTest,
       TestCacheUnfamiliarLoginVerdictsIncognito) {
  service_->SetIsIncognito(true);

  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  reused_password_account_type.set_is_account_syncing(true);
  // No verdict will be cached for incognito profile.
  CacheVerdict(GURL("http://www.test.com/foo/index.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/foo/", base::Time::Now());

  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  CacheVerdict(GURL("http://www.test.com/bar/index2.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/bar/", base::Time::Now());
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Now cache a PASSWORD_REUSE_EVENT verdict. Verdict count should not
  // increase.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  reused_password_account_type.set_is_account_syncing(true);
  CacheVerdict(GURL("http://www.test.com/foobar/index3.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/foobar/", base::Time::Now());
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
}

TEST_F(ChromePasswordProtectionServiceTest, TestGetCachedVerdicts) {
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  reused_password_account_type.set_is_account_syncing(true);
  // Prepare 4 verdicts of the same origin with different cache expressions,
  // or password type, one is expired, one is not, one is of a different
  // trigger type, and the other is with a different password type.
  base::Time now = base::Time::Now();
  CacheVerdict(GURL("http://test.com/login.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute, "test.com/",
               now);
  CacheVerdict(
      GURL("http://test.com/def/index.jsp"),
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_account_type, LoginReputationClientResponse::PHISHING,
      10 * kMinute, "test.com/def/",
      base::Time::FromSecondsSinceUnixEpoch(now.InSecondsFSinceUnixEpoch() -
                                            kDay));  // Yesterday, expired.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  CacheVerdict(GURL("http://test.com/bar/login.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::PHISHING, 10 * kMinute,
               "test.com/bar/", now);
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  CacheVerdict(GURL("http://test.com/login.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute, "test.com/",
               now);

  ASSERT_EQ(3U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ASSERT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Return VERDICT_TYPE_UNSPECIFIED if look up for a URL with unknown origin.
  LoginReputationClientResponse actual_verdict;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            service_->GetCachedVerdict(
                GURL("http://www.unknown.com/"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &actual_verdict));
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            service_->GetCachedVerdict(
                GURL("http://www.unknown.com/"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &actual_verdict));

  // Return SAFE if look up for a URL that matches "test.com" cache expression.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            service_->GetCachedVerdict(
                GURL("http://test.com/xyz/foo.jsp"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &actual_verdict));
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            service_->GetCachedVerdict(
                GURL("http://test.com/xyz/foo.jsp"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &actual_verdict));

  // Return VERDICT_TYPE_UNSPECIFIED if look up for a URL whose variants match
  // test.com/def, but the corresponding verdict is expired, so the most
  // matching unexpired verdict will return SAFE
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            service_->GetCachedVerdict(
                GURL("http://test.com/def/ghi/index.html"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &actual_verdict));

  // Return PHISHING. Matches "test.com/bar/" cache expression.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  EXPECT_EQ(LoginReputationClientResponse::PHISHING,
            service_->GetCachedVerdict(
                GURL("http://test.com/bar/foo.jsp"),
                LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                reused_password_account_type, &actual_verdict));

  // Now cache SAFE verdict for the full path.
  CacheVerdict(GURL("http://test.com/bar/foo.jsp"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, 10 * kMinute,
               "test.com/bar/foo.jsp", now);

  // Return SAFE now. Matches the full cache expression.
  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            service_->GetCachedVerdict(
                GURL("http://test.com/bar/foo.jsp"),
                LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                reused_password_account_type, &actual_verdict));
}

TEST_F(ChromePasswordProtectionServiceTest, TestDoesNotCacheAboutBlank) {
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);

  // Should not actually cache, since about:blank is not valid for reputation
  // computing.
  CacheVerdict(
      GURL("about:blank"), LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
      reused_password_account_type, LoginReputationClientResponse::SAFE,
      10 * kMinute, "about:blank", base::Time::Now());

  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
}
