// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/scoped_feature_list.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/common/proto/synced/browser_events.pb.h"
#import "components/enterprise/connectors/core/reporting_constants.h"
#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "components/enterprise/connectors/core/reporting_test_utils.h"
#import "components/policy/core/common/cloud/cloud_external_data_manager.h"
#import "components/policy/core/common/cloud/dm_token.h"
#import "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "url/scheme_host_port.h"

namespace enterprise_connectors {

namespace {

// Alias to reduce verbosity when using TriggeredRuleInfo.
using TriggeredRuleInfo = ::chrome::cros::reporting::proto::TriggeredRuleInfo;
// Alias to reduce verbosity when using the repeated ReferrerChainEntry field.
using ReferrerChain =
    google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>;
// Alias to reduce verbosity when using UrlInfo.
using UrlInfo = ::chrome::cros::reporting::proto::UrlInfo;

inline constexpr char kTestDmToken[] = "dm_token";
inline constexpr char kTestClientId[] = "client_id";

TriggeredRuleInfo MakeTriggeredRuleInfo(TriggeredRuleInfo::Action action,
                                        bool has_watermark) {
  TriggeredRuleInfo info;
  info.set_action(action);
  info.set_rule_id(123);
  info.set_rule_name("test rule name");
  info.set_url_category("test rule category");
  if (has_watermark) {
    info.set_has_watermarking(true);
  }
  return info;
}

}  // namespace

class IOSReportingEventRouterTest : public PlatformTest {
 public:
  IOSReportingEventRouterTest() {}
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IOSRealtimeReportingClientFactory::GetInstance(),
        IOSRealtimeReportingClientFactory::GetDefaultFactory());

    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken(kTestDmToken);
    IOSRealtimeReportingClientFactory::GetForProfile(profile_)
        ->SetBrowserCloudPolicyClientForTesting(client_.get());

    enterprise_connectors::IOSRealtimeReportingClientFactory::GetForProfile(
        profile_)
        ->SetIdentityManagerForTesting(
            identity_test_environment_.identity_manager());
    identity_test_environment_.MakePrimaryAccountAvailable(
        profile_->GetProfileName(), signin::ConsentLevel::kSignin);

    reporting_event_router_ = std::make_unique<ReportingEventRouter>(
        IOSRealtimeReportingClientFactory::GetForProfile(profile_));

    // Setup valid values for DM token.
    fake_browser_dm_token_storage_.SetDMToken(kTestDmToken);
    fake_browser_dm_token_storage_.SetClientId(kTestClientId);
  }

  void TearDown() override {
    enterprise_connectors::IOSRealtimeReportingClientFactory::GetForProfile(
        profile_)
        ->SetBrowserCloudPolicyClientForTesting(nullptr);
  }

  std::string GetProfileIdentifier() const {
    return profile_->GetStatePath().AsUTF8Unsafe();
  }

  void EnableEnhancedFieldsForSecOps() {
    scoped_feature_list_.InitAndEnableFeature(
        safe_browsing::kEnhancedFieldsForSecOps);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  // Add local state to test ApplicationContext. Required by
  // TestProfileManagerIOS.
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  std::unique_ptr<ReportingEventRouter> reporting_event_router_;
  signin::IdentityTestEnvironment identity_test_environment_;
  policy::FakeBrowserDMTokenStorage fake_browser_dm_token_storage_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the event reporting is not enabled for a given event.
TEST_F(IOSReportingEventRouterTest, CheckEventEnabledReturnsFalse) {
  test::SetOnSecurityEventReporting(profile_->GetTestingPrefService(),
                                    /*enabled=*/false,
                                    /*enabled_event_names=*/{},
                                    /*enabled_opt_in_events=*/{});

  EXPECT_FALSE(reporting_event_router_->IsEventEnabled(kKeyPasswordReuseEvent));
}

// Tests that the event reporting is enabled for a given event.
TEST_F(IOSReportingEventRouterTest, CheckEventEnabledReturnsTrue) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyPasswordReuseEvent},
      /*enabled_opt_in_events=*/{});

  EXPECT_TRUE(reporting_event_router_->IsEventEnabled(kKeyPasswordReuseEvent));
}

// Tests that the login events are reported as expected.
TEST_F(IOSReportingEventRouterTest, TestOnLoginEvent) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyLoginEvent, {"*"}}});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectLoginEvent("https://www.example.com/", false, "",
                             profile_->GetProfileName(), GetProfileIdentifier(),
                             u"*****");

  reporting_event_router_->OnLoginEvent(GURL("https://www.example.com/"),
                                        url::SchemeHostPort().IsValid(),
                                        url::SchemeHostPort(), u"Fakeuser");
}

// Tests that no matchting Url pattern for login events reporting.
TEST_F(IOSReportingEventRouterTest, TestOnLoginEventNoMatchingUrlPattern) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyLoginEvent, {"notexample.com"}}});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectNoReport();

  reporting_event_router_->OnLoginEvent(
      GURL("https://www.example.com/"), url::SchemeHostPort().IsValid(),
      url::SchemeHostPort(), u"login-username");
}

// Tests that the login event reports the user name as expected.
TEST_F(IOSReportingEventRouterTest, TestOnLoginEventWithEmailAsLoginUsername) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyLoginEvent, {"*"}}});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectLoginEvent("https://www.example.com/", false, "",
                             profile_->GetProfileName(), GetProfileIdentifier(),
                             u"*****@example.com");

  reporting_event_router_->OnLoginEvent(
      GURL("https://www.example.com/"), url::SchemeHostPort().IsValid(),
      url::SchemeHostPort(), u"Fakeuser@example.com");
}

// Tests that the login events on federated login are reported as expected.
TEST_F(IOSReportingEventRouterTest, TestOnLoginEventFederated) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyLoginEvent, {"*"}}});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectLoginEvent(
      "https://www.example.com/", true, "https://www.google.com",
      profile_->GetProfileName(), GetProfileIdentifier(), u"*****");

  url::SchemeHostPort federated_origin =
      url::SchemeHostPort(GURL("https://www.google.com"));

  reporting_event_router_->OnLoginEvent(GURL("https://www.example.com/"),
                                        federated_origin.IsValid(),
                                        federated_origin, u"Fakeuser");
}

// Tests that the password breaching events are reported as expected.
TEST_F(IOSReportingEventRouterTest, TestOnPasswordBreach) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyPasswordBreachEvent, {"*"}}});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectPasswordBreachEvent(
      "SAFETY_CHECK",
      {
          {"https://first.example.com/", u"*****"},
          {"https://second.example.com/", u"*****@gmail.com"},
      },
      profile_->GetProfileName(), GetProfileIdentifier());

  reporting_event_router_->OnPasswordBreach(
      "SAFETY_CHECK",
      {
          {GURL("https://first.example.com"), u"first_user_name"},
          {GURL("https://second.example.com"), u"second_user_name@gmail.com"},
      });
}

// Tests that the password breaching events with no matching url pattern.
TEST_F(IOSReportingEventRouterTest, TestOnPasswordBreachNoMatchingUrlPattern) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/
      {{kKeyPasswordBreachEvent, {"notexample.com"}}});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectNoReport();

  reporting_event_router_->OnPasswordBreach(
      "SAFETY_CHECK",
      {
          {GURL("https://first.example.com"), u"first_user_name"},
          {GURL("https://second.example.com"), u"second_user_name"},
      });
}

// Test that the password breaching events with partial mathcing url pattern.
TEST_F(IOSReportingEventRouterTest,
       TestOnPasswordBreachPartiallyMatchingUrlPatterns) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/
      {{kKeyPasswordBreachEvent, {"secondexample.com"}}});

  // The event is only enabled on secondexample.com, so expect only the
  // information related to that origin to be reported.
  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectPasswordBreachEvent(
      "SAFETY_CHECK",
      {
          {"https://secondexample.com/", u"*****"},
      },
      profile_->GetProfileName(), GetProfileIdentifier());

  reporting_event_router_->OnPasswordBreach(
      "SAFETY_CHECK",
      {
          {GURL("https://firstexample.com"), u"first_user_name"},
          {GURL("https://secondexample.com"), u"second_user_name"},
      });
}

// Test that the url filtering reporting events are blocked as expected.
TEST_F(IOSReportingEventRouterTest, TestOnUrlFilteringInterstitial_Blocked) {
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyUrlFilteringInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent expected_event;
  expected_event.set_url("https://filteredurl.com/");
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EVENT_RESULT_BLOCKED);
  expected_event.set_profile_user_name(profile_->GetProfileName());
  expected_event.set_profile_identifier(GetProfileIdentifier());
  *expected_event.add_triggered_rule_info() = MakeTriggeredRuleInfo(
      /*action=*/TriggeredRuleInfo::BLOCK, /*has_watermark=*/false);
  *expected_event.add_referrers() = test::MakeUrlInfoReferrer();

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectURLFilteringInterstitialEvent(expected_event);

  safe_browsing::RTLookupResponse response;
  auto* threat_info = response.add_threat_info();
  threat_info->set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS);
  auto* matched_url_navigation_rule =
      threat_info->mutable_matched_url_navigation_rule();
  matched_url_navigation_rule->set_rule_id("123");
  matched_url_navigation_rule->set_rule_name("test rule name");
  matched_url_navigation_rule->set_matched_url_category("test rule category");
  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());

  reporting_event_router_->OnUrlFilteringInterstitial(
      GURL("https://filteredurl.com"), "ENTERPRISE_BLOCKED_SEEN", response,
      referrer_chain);
}

// Test that the url filtering reporting events are warned as expected.
TEST_F(IOSReportingEventRouterTest, TestOnUrlFilteringInterstitial_Warned) {
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyUrlFilteringInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent expected_event;
  expected_event.set_url("https://filteredurl.com/");
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EVENT_RESULT_WARNED);
  expected_event.set_profile_user_name(profile_->GetProfileName());
  expected_event.set_profile_identifier(GetProfileIdentifier());
  *expected_event.add_triggered_rule_info() = MakeTriggeredRuleInfo(
      /*action=*/TriggeredRuleInfo::WARN, /*has_watermark=*/false);
  *expected_event.add_referrers() = test::MakeUrlInfoReferrer();

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectURLFilteringInterstitialEvent(expected_event);

  safe_browsing::RTLookupResponse response;
  auto* threat_info = response.add_threat_info();
  threat_info->set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::WARN);
  auto* matched_url_navigation_rule =
      threat_info->mutable_matched_url_navigation_rule();
  matched_url_navigation_rule->set_rule_id("123");
  matched_url_navigation_rule->set_rule_name("test rule name");
  matched_url_navigation_rule->set_matched_url_category("test rule category");
  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());

  reporting_event_router_->OnUrlFilteringInterstitial(
      GURL("https://filteredurl.com"), "ENTERPRISE_WARNED_SEEN", response,
      referrer_chain);
}

// Test that the url filtering reporting events are bypassed as expected.
TEST_F(IOSReportingEventRouterTest, TestOnUrlFilteringInterstitial_Bypassed) {
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyUrlFilteringInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent expected_event;
  expected_event.set_url("https://filteredurl.com/");
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EVENT_RESULT_BYPASSED);
  expected_event.set_profile_user_name(profile_->GetProfileName());
  expected_event.set_profile_identifier(GetProfileIdentifier());
  *expected_event.add_triggered_rule_info() = MakeTriggeredRuleInfo(
      /*action=*/TriggeredRuleInfo::WARN, /*has_watermark=*/false);
  *expected_event.add_referrers() = test::MakeUrlInfoReferrer();

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectURLFilteringInterstitialEvent(expected_event);

  safe_browsing::RTLookupResponse response;
  auto* threat_info = response.add_threat_info();
  threat_info->set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::WARN);
  auto* matched_url_navigation_rule =
      threat_info->mutable_matched_url_navigation_rule();
  matched_url_navigation_rule->set_rule_id("123");
  matched_url_navigation_rule->set_rule_name("test rule name");
  matched_url_navigation_rule->set_matched_url_category("test rule category");
  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());

  reporting_event_router_->OnUrlFilteringInterstitial(
      GURL("https://filteredurl.com"), "ENTERPRISE_WARNED_BYPASS", response,
      referrer_chain);
}

// Test that the url filtering reporting events with unknown action taken by
// chrome as expected.
TEST_F(IOSReportingEventRouterTest,
       TestOnUrlFilteringInterstitial_WatermarkAudit) {
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyUrlFilteringInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent expected_event;
  expected_event.set_url("https://filteredurl.com/");
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EVENT_RESULT_ALLOWED);
  expected_event.set_profile_user_name(profile_->GetProfileName());
  expected_event.set_profile_identifier(GetProfileIdentifier());
  *expected_event.add_triggered_rule_info() = MakeTriggeredRuleInfo(
      /*action=*/TriggeredRuleInfo::ACTION_UNKNOWN, /*has_watermark=*/false);
  *expected_event.add_referrers() = test::MakeUrlInfoReferrer();

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectURLFilteringInterstitialEvent(expected_event);

  safe_browsing::RTLookupResponse response;
  auto* threat_info = response.add_threat_info();
  auto* matched_url_navigation_rule =
      threat_info->mutable_matched_url_navigation_rule();
  matched_url_navigation_rule->set_rule_id("123");
  matched_url_navigation_rule->set_rule_name("test rule name");
  matched_url_navigation_rule->set_matched_url_category("test rule category");
  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());

  reporting_event_router_->OnUrlFilteringInterstitial(
      GURL("https://filteredurl.com"), "", response, referrer_chain);
}

// Tests that interstitial reporting events are warned as expected.
TEST_F(IOSReportingEventRouterTest, TestInterstitialShownWarned) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectSecurityInterstitialEvent(
      "https://phishing.com/", "PHISHING", profile_->GetProfileName(),
      GetProfileIdentifier(), "EVENT_RESULT_WARNED", false, 0);
  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnSecurityInterstitialShown(
      GURL("https://phishing.com/"), "PHISHING", 0, false, referrer_chain);
}

// Tests that interstitial reporting events blocked as expected.
TEST_F(IOSReportingEventRouterTest, TestInterstitialShownBlocked) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectSecurityInterstitialEvent(
      "https://phishing.com/", "PHISHING", profile_->GetProfileName(),
      GetProfileIdentifier(), "EVENT_RESULT_BLOCKED", false, 0);
  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnSecurityInterstitialShown(
      GURL("https://phishing.com/"), "PHISHING", 0, true, referrer_chain);
}

// Tests that interstitial reporting events bypassed as expected.
TEST_F(IOSReportingEventRouterTest, TestInterstitialProceeded) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectSecurityInterstitialEvent(
      "https://phishing.com/", "PHISHING", profile_->GetProfileName(),
      GetProfileIdentifier(), "EVENT_RESULT_BYPASSED", true, 0);
  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnSecurityInterstitialProceeded(
      GURL("https://phishing.com/"), "PHISHING", 0, referrer_chain);
}

// Tests that password reuse reporting events warned as expected.
TEST_F(IOSReportingEventRouterTest, TestPasswordReuseWarned) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyPasswordReuseEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectPasswordReuseEvent(
      "https://phishing.com/", "user_name_1", true, "EVENT_RESULT_WARNED",
      profile_->GetProfileName(), GetProfileIdentifier());
  reporting_event_router_->OnPasswordReuse(
      GURL("https://phishing.com/"), "user_name_1", /*is_phishing_url*/ true,
      /*warning_shown*/ true);
}

// Tests that password reuse reporting events allowed as expected.
TEST_F(IOSReportingEventRouterTest, TestPasswordReuseAllowed) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyPasswordReuseEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  validator.ExpectPasswordReuseEvent(
      "https://phishing.com/", "user_name_1", true, "EVENT_RESULT_ALLOWED",
      profile_->GetProfileName(), GetProfileIdentifier());
  reporting_event_router_->OnPasswordReuse(
      GURL("https://phishing.com/"), "user_name_1", /*is_phishing_url*/ true,
      /*warning_shown*/ false);
}

}  // namespace enterprise_connectors
