// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/scoped_feature_list.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/common/proto/synced/browser_events.pb.h"
#import "components/enterprise/connectors/core/reporting_constants.h"
#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "components/enterprise/connectors/core/reporting_test_utils.h"
#import "components/enterprise/data_controls/core/browser/verdict.h"
#import "components/policy/core/common/cloud/cloud_external_data_manager.h"
#import "components/policy/core/common/cloud/dm_token.h"
#import "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#import "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
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
#import "testing/gtest/include/gtest/gtest.h"
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

class IOSReportingEventRouterTest : public PlatformTest,
                                    public testing::WithParamInterface<bool> {
 public:
  IOSReportingEventRouterTest() {}
  void SetUp() override {
    PlatformTest::SetUp();

    if (use_proto_format()) {
      scoped_feature_list_.InitAndEnableFeature(
          policy::kUploadRealtimeReportingEventsUsingProto);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          policy::kUploadRealtimeReportingEventsUsingProto);
    }

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

  bool use_proto_format() { return GetParam(); }

  void EnableEnhancedFieldsForSecOps() {
    scoped_feature_list_.Reset();
    if (use_proto_format()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{policy::
                                    kUploadRealtimeReportingEventsUsingProto,
                                safe_browsing::kEnhancedFieldsForSecOps},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          safe_browsing::kEnhancedFieldsForSecOps);
    }
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
TEST_P(IOSReportingEventRouterTest, CheckEventEnabledReturnsFalse) {
  test::SetOnSecurityEventReporting(profile_->GetTestingPrefService(),
                                    /*enabled=*/false,
                                    /*enabled_event_names=*/{},
                                    /*enabled_opt_in_events=*/{});

  EXPECT_FALSE(reporting_event_router_->IsEventEnabled(kKeyPasswordReuseEvent));
}

// Tests that the event reporting is enabled for a given event.
TEST_P(IOSReportingEventRouterTest, CheckEventEnabledReturnsTrue) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyPasswordReuseEvent},
      /*enabled_opt_in_events=*/{});

  EXPECT_TRUE(reporting_event_router_->IsEventEnabled(kKeyPasswordReuseEvent));
}

// Tests that the login events are reported as expected.
TEST_P(IOSReportingEventRouterTest, TestOnLoginEvent) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyLoginEvent, {"*"}}});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::LoginEvent expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://www.example.com/");
    expected_event.set_is_federated(false);
    expected_event.set_profile_user_name(profile_->GetProfileName());
    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_login_user_name("*****");

    validator.ExpectLoginEvent(std::move(expected_event));
  } else {
    validator.ExpectLoginEvent("https://www.example.com/", false, "",
                               profile_->GetProfileName(),
                               GetProfileIdentifier(), u"*****");
  }

  reporting_event_router_->OnLoginEvent(GURL("https://www.example.com/"),
                                        url::SchemeHostPort().IsValid(),
                                        url::SchemeHostPort(), u"Fakeuser");
  run_loop.Run();
}

// Tests that no matchting Url pattern for login events reporting.
TEST_P(IOSReportingEventRouterTest, TestOnLoginEventNoMatchingUrlPattern) {
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
TEST_P(IOSReportingEventRouterTest, TestOnLoginEventWithEmailAsLoginUsername) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyLoginEvent, {"*"}}});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::LoginEvent expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://www.example.com/");
    expected_event.set_is_federated(false);
    expected_event.set_profile_user_name(profile_->GetProfileName());
    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_login_user_name("*****@example.com");

    validator.ExpectLoginEvent(std::move(expected_event));
  } else {
    validator.ExpectLoginEvent("https://www.example.com/", false, "",
                               profile_->GetProfileName(),
                               GetProfileIdentifier(), u"*****@example.com");
  }

  reporting_event_router_->OnLoginEvent(
      GURL("https://www.example.com/"), url::SchemeHostPort().IsValid(),
      url::SchemeHostPort(), u"Fakeuser@example.com");
  run_loop.Run();
}

// Tests that the login events on federated login are reported as expected.
TEST_P(IOSReportingEventRouterTest, TestOnLoginEventFederated) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyLoginEvent, {"*"}}});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::LoginEvent expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://www.example.com/");
    expected_event.set_is_federated(true);
    expected_event.set_federated_origin("https://www.google.com");
    expected_event.set_profile_user_name(profile_->GetProfileName());
    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_login_user_name("*****");

    validator.ExpectLoginEvent(std::move(expected_event));
  } else {
    validator.ExpectLoginEvent(
        "https://www.example.com/", true, "https://www.google.com",
        profile_->GetProfileName(), GetProfileIdentifier(), u"*****");
  }

  url::SchemeHostPort federated_origin =
      url::SchemeHostPort(GURL("https://www.google.com"));

  reporting_event_router_->OnLoginEvent(GURL("https://www.example.com/"),
                                        federated_origin.IsValid(),
                                        federated_origin, u"Fakeuser");
  run_loop.Run();
}

// Tests that the password breaching events are reported as expected.
TEST_P(IOSReportingEventRouterTest, TestOnPasswordBreach) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{kKeyPasswordBreachEvent, {"*"}}});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::PasswordBreachEvent expected_event;

  if (use_proto_format()) {
    chrome::cros::reporting::proto::PasswordBreachEvent::Identity identity_1;
    identity_1.set_url("https://first.example.com/");
    identity_1.set_username("*****");
    chrome::cros::reporting::proto::PasswordBreachEvent::Identity identity_2;
    identity_2.set_url("https://second.example.com/");
    identity_2.set_username("*****@gmail.com");
    *expected_event.add_identities() = identity_1;
    *expected_event.add_identities() = identity_2;
    expected_event.set_trigger(
        chrome::cros::reporting::proto::PasswordBreachEvent::SAFETY_CHECK);
    expected_event.set_profile_user_name(profile_->GetProfileName());
    expected_event.set_profile_identifier(GetProfileIdentifier());

    validator.ExpectPasswordBreachEvent(std::move(expected_event));
  } else {
    validator.ExpectPasswordBreachEvent(
        "SAFETY_CHECK",
        {
            {"https://first.example.com/", u"*****"},
            {"https://second.example.com/", u"*****@gmail.com"},
        },
        profile_->GetProfileName(), GetProfileIdentifier());
  }

  reporting_event_router_->OnPasswordBreach(
      "SAFETY_CHECK",
      {
          {GURL("https://first.example.com"), u"first_user_name"},
          {GURL("https://second.example.com"), u"second_user_name@gmail.com"},
      });
  run_loop.Run();
}

// Tests that the password breaching events with no matching url pattern.
TEST_P(IOSReportingEventRouterTest, TestOnPasswordBreachNoMatchingUrlPattern) {
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
TEST_P(IOSReportingEventRouterTest,
       TestOnPasswordBreachPartiallyMatchingUrlPatterns) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/
      {{kKeyPasswordBreachEvent, {"secondexample.com"}}});

  // The event is only enabled on secondexample.com, so expect only the
  // information related to that origin to be reported.
  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::PasswordBreachEvent expected_event;

  if (use_proto_format()) {
    chrome::cros::reporting::proto::PasswordBreachEvent::Identity identity;
    identity.set_url("https://secondexample.com/");
    identity.set_username("*****");
    *expected_event.add_identities() = identity;
    expected_event.set_trigger(
        chrome::cros::reporting::proto::PasswordBreachEvent::SAFETY_CHECK);
    expected_event.set_profile_user_name(profile_->GetProfileName());
    expected_event.set_profile_identifier(GetProfileIdentifier());

    validator.ExpectPasswordBreachEvent(std::move(expected_event));
  } else {
    validator.ExpectPasswordBreachEvent(
        "SAFETY_CHECK",
        {
            {"https://secondexample.com/", u"*****"},
        },
        profile_->GetProfileName(), GetProfileIdentifier());
  }

  reporting_event_router_->OnPasswordBreach(
      "SAFETY_CHECK",
      {
          {GURL("https://firstexample.com"), u"first_user_name"},
          {GURL("https://secondexample.com"), u"second_user_name"},
      });
  run_loop.Run();
}

// Test that the url filtering reporting events are blocked as expected.
TEST_P(IOSReportingEventRouterTest, TestOnUrlFilteringInterstitial_Blocked) {
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyUrlFilteringInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent expected_event;
  expected_event.set_url("https://filteredurl.com/");
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EVENT_RESULT_BLOCKED);
  expected_event.set_threat_type(
      chrome::cros::reporting::proto::UrlFilteringInterstitialEvent::
          ENTERPRISE_BLOCKED_SEEN);
  expected_event.set_profile_user_name(profile_->GetProfileName());
  expected_event.set_profile_identifier(GetProfileIdentifier());
  *expected_event.add_triggered_rule_info() = MakeTriggeredRuleInfo(
      /*action=*/TriggeredRuleInfo::BLOCK, /*has_watermark=*/false);
  *expected_event.add_referrers() = test::MakeUrlInfoReferrer();

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());

  if (use_proto_format()) {
    validator.ExpectProtoBasedUrlFilteringInterstitialEvent(expected_event);
  } else {
    validator.ExpectURLFilteringInterstitialEventWithReferrers(expected_event);
  }

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
  run_loop.Run();
}

// Test that the url filtering reporting events are warned as expected.
TEST_P(IOSReportingEventRouterTest, TestOnUrlFilteringInterstitial_Warned) {
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyUrlFilteringInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent expected_event;
  expected_event.set_url("https://filteredurl.com/");
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EVENT_RESULT_WARNED);
  expected_event.set_threat_type(
      chrome::cros::reporting::proto::UrlFilteringInterstitialEvent::
          ENTERPRISE_WARNED_SEEN);
  expected_event.set_profile_user_name(profile_->GetProfileName());
  expected_event.set_profile_identifier(GetProfileIdentifier());
  *expected_event.add_triggered_rule_info() = MakeTriggeredRuleInfo(
      /*action=*/TriggeredRuleInfo::WARN, /*has_watermark=*/false);
  *expected_event.add_referrers() = test::MakeUrlInfoReferrer();

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());

  if (use_proto_format()) {
    validator.ExpectProtoBasedUrlFilteringInterstitialEvent(expected_event);
  } else {
    validator.ExpectURLFilteringInterstitialEventWithReferrers(expected_event);
  }

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
  run_loop.Run();
}

// Test that the url filtering reporting events are bypassed as expected.
TEST_P(IOSReportingEventRouterTest, TestOnUrlFilteringInterstitial_Bypassed) {
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyUrlFilteringInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent expected_event;
  expected_event.set_url("https://filteredurl.com/");
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EVENT_RESULT_BYPASSED);
  expected_event.set_clicked_through(true);
  expected_event.set_threat_type(
      chrome::cros::reporting::proto::UrlFilteringInterstitialEvent::
          ENTERPRISE_WARNED_BYPASS);
  expected_event.set_profile_user_name(profile_->GetProfileName());
  expected_event.set_profile_identifier(GetProfileIdentifier());
  *expected_event.add_triggered_rule_info() = MakeTriggeredRuleInfo(
      /*action=*/TriggeredRuleInfo::WARN, /*has_watermark=*/false);
  *expected_event.add_referrers() = test::MakeUrlInfoReferrer();

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());

  if (use_proto_format()) {
    validator.ExpectProtoBasedUrlFilteringInterstitialEvent(expected_event);
  } else {
    validator.ExpectURLFilteringInterstitialEventWithReferrers(expected_event);
  }

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
  run_loop.Run();
}

// Test that the url filtering reporting events with unknown action taken by
// chrome as expected.
TEST_P(IOSReportingEventRouterTest,
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
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());

  if (use_proto_format()) {
    validator.ExpectProtoBasedUrlFilteringInterstitialEvent(expected_event);
  } else {
    validator.ExpectURLFilteringInterstitialEventWithReferrers(expected_event);
  }

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
  run_loop.Run();
}

// Tests that interstitial reporting events are warned as expected.
TEST_P(IOSReportingEventRouterTest, TestInterstitialShownWarned) {
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::SafeBrowsingInterstitialEvent expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://phishing.com/");
    expected_event.set_reason(chrome::cros::reporting::proto::
                                  SafeBrowsingInterstitialEvent::PHISHING);
    expected_event.set_profile_user_name(profile_->GetProfileName());
    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EVENT_RESULT_WARNED);
    expected_event.set_clicked_through(false);
    expected_event.set_net_error_code(0);
    expected_event.mutable_referrers()->Add(test::MakeUrlInfoReferrer());

    validator.ExpectSecurityInterstitialEvent(std::move(expected_event));
  } else {
    validator.ExpectSecurityInterstitialEventWithReferrers(
        "https://phishing.com/", "PHISHING", profile_->GetProfileName(),
        GetProfileIdentifier(), "EVENT_RESULT_WARNED", false, 0,
        test::MakeUrlInfoReferrer());
  }

  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnSecurityInterstitialShown(
      GURL("https://phishing.com/"), "PHISHING", 0, false, referrer_chain);
  run_loop.Run();
}

// Tests that interstitial reporting events blocked as expected.
TEST_P(IOSReportingEventRouterTest, TestInterstitialShownBlocked) {
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::SafeBrowsingInterstitialEvent expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://phishing.com/");
    expected_event.set_reason(chrome::cros::reporting::proto::
                                  SafeBrowsingInterstitialEvent::PHISHING);
    expected_event.set_profile_user_name(profile_->GetProfileName());
    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EVENT_RESULT_BLOCKED);
    expected_event.set_clicked_through(false);
    expected_event.set_net_error_code(0);
    expected_event.mutable_referrers()->Add(test::MakeUrlInfoReferrer());

    validator.ExpectSecurityInterstitialEvent(std::move(expected_event));
  } else {
    validator.ExpectSecurityInterstitialEventWithReferrers(
        "https://phishing.com/", "PHISHING", profile_->GetProfileName(),
        GetProfileIdentifier(), "EVENT_RESULT_BLOCKED", false, 0,
        test::MakeUrlInfoReferrer());
  }

  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnSecurityInterstitialShown(
      GURL("https://phishing.com/"), "PHISHING", 0, true, referrer_chain);
  run_loop.Run();
}

// Tests that interstitial reporting events bypassed as expected.
TEST_P(IOSReportingEventRouterTest, TestInterstitialProceeded) {
  EnableEnhancedFieldsForSecOps();
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeyInterstitialEvent},
      /*enabled_opt_in_events=*/{});

  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::SafeBrowsingInterstitialEvent expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://phishing.com/");
    expected_event.set_reason(chrome::cros::reporting::proto::
                                  SafeBrowsingInterstitialEvent::PHISHING);
    expected_event.set_profile_user_name(profile_->GetProfileName());
    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EVENT_RESULT_BYPASSED);
    expected_event.set_clicked_through(true);
    expected_event.set_net_error_code(0);
    expected_event.mutable_referrers()->Add(test::MakeUrlInfoReferrer());

    validator.ExpectSecurityInterstitialEvent(std::move(expected_event));
  } else {
    validator.ExpectSecurityInterstitialEventWithReferrers(
        "https://phishing.com/", "PHISHING", profile_->GetProfileName(),
        GetProfileIdentifier(), "EVENT_RESULT_BYPASSED", true, 0,
        test::MakeUrlInfoReferrer());
  }

  ReferrerChain referrer_chain;
  referrer_chain.Add(test::MakeReferrerChainEntry());
  reporting_event_router_->OnSecurityInterstitialProceeded(
      GURL("https://phishing.com/"), "PHISHING", 0, referrer_chain);
  run_loop.Run();
}

TEST_P(IOSReportingEventRouterTest, TestOnDataControlsSensitiveDataEvent) {
  test::SetOnSecurityEventReporting(
      profile_->GetTestingPrefService(), /*enabled=*/true,
      /*enabled_event_names=*/{kKeySensitiveDataEvent},
      /*enabled_opt_in_events=*/{});

  data_controls::Verdict::TriggeredRules triggered_rules = {
      {{0, true}, {"1", "rule_1_name"}}};
  test::EventReportValidatorBase validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;

  if (use_proto_format()) {
    expected_event.set_url("https://example.com/");
    expected_event.set_tab_url("https://example.com/");
    expected_event.set_source("exampleSource");
    expected_event.set_destination("exampleDestination");
    expected_event.set_content_type("text/html");
    expected_event.set_content_size(1234);
    expected_event.set_trigger(
        chrome::cros::reporting::proto::DataTransferEventTrigger::
            WEB_CONTENT_UPLOAD);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_ALLOWED);
    expected_event.set_web_app_signed_in_account("content_area_user@gmail.com");
    expected_event.set_source_web_app_signed_in_account(
        "active_user@gmail.com");

    TriggeredRuleInfo triggered_rule;
    triggered_rule.set_rule_id(1);
    triggered_rule.set_rule_name("rule_1_name");

    *expected_event.add_triggered_rule_info() = triggered_rule;
    expected_event.set_profile_identifier(GetProfileIdentifier());
    expected_event.set_profile_user_name(profile_->GetProfileName());

    validator.ExpectSensitiveDataEvent(std::move(expected_event));
  } else {
    validator.ExpectDataControlsSensitiveDataEvent(
        /*expected_url*/
        "https://example.com/",
        /*expected_tab_url*/ "https://example.com/",
        /*expected_source*/ "exampleSource",
        /*expected_destination*/ "exampleDestination",
        /*expected_mimetypes=*/
        []() -> const std::set<std::string>* {
          static base::NoDestructor<std::set<std::string>> set({"text/html"});
          return set.get();
        }(),
        /*expected_trigger=*/"WEB_CONTENT_UPLOAD",
        /*triggered_rules=*/triggered_rules,
        /*expected_result*/ "EVENT_RESULT_ALLOWED",
        /*expected_profile_username*/ profile_->GetProfileName(),
        /*expected_profile_identifier*/ GetProfileIdentifier(),
        /*expected_content_size=*/1234);
  }

  reporting_event_router_->OnDataControlsSensitiveDataEvent(
      GURL("https://example.com/"), GURL("https://example.com/"),
      "exampleSource", "exampleDestination", "text/html",
      enterprise_connectors::kWebContentUploadDataTransferEventTrigger,
      "active_user@gmail.com", "content_area_user@gmail.com", triggered_rules,
      enterprise_connectors::EventResult::ALLOWED, 1234);
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(/* No InstantiationName */,
                         IOSReportingEventRouterTest,
                         /* is_profile_reporting */ testing::Bool());

}  // namespace enterprise_connectors
