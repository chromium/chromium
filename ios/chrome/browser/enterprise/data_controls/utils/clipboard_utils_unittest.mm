// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/utils/clipboard_utils.h"

#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "components/enterprise/data_controls/core/browser/verdict.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/enterprise/common/test/mock_reporting_event_router.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client_factory.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_reporting_event_router_factory.h"
#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service.h"
#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/clipboard/clipboard_metadata.h"
#import "url/gurl.h"

using ::testing::_;

namespace data_controls {

namespace {

const char kSourceUrl[] = "https://google.com";
const char kDestinationUrl[] = "https://test.com";

class MockIOSRulesService : public IOSRulesService {
 public:
  explicit MockIOSRulesService(ProfileIOS* profile)
      : IOSRulesService(profile) {}

  MOCK_METHOD(Verdict,
              GetPasteVerdict,
              (const GURL&, const GURL&, ProfileIOS*, ProfileIOS*),
              (override));
  MOCK_METHOD(Verdict,
              GetCopyRestrictedBySourceVerdict,
              (const GURL&),
              (const, override));
  MOCK_METHOD(Verdict,
              GetCopyToOSClipboardVerdict,
              (const GURL&),
              (const, override));
};

std::unique_ptr<KeyedService> BuildMockIOSRulesService(ProfileIOS* profile) {
  return std::make_unique<MockIOSRulesService>(profile);
}

}  // namespace

class ClipboardUtilsTest : public PlatformTest {
 public:
  ClipboardUtilsTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(IOSRulesServiceFactory::GetInstance(),
                              base::BindOnce(&BuildMockIOSRulesService));
    builder.AddTestingFactory(
        enterprise_connectors::IOSReportingEventRouterFactory::GetInstance(),
        base::BindOnce(
            &MockReportingEventRouter::BuildMockReportingEventRouter));
    profile_ = std::move(builder).Build();
    rules_service_ = static_cast<MockIOSRulesService*>(
        IOSRulesServiceFactory::GetForProfile(profile_.get()));
    reporting_router_ = static_cast<MockReportingEventRouter*>(
        enterprise_connectors::IOSReportingEventRouterFactory::GetForProfile(
            profile_.get()));

    TestProfileIOS::Builder builder2;
    builder2.AddTestingFactory(IOSRulesServiceFactory::GetInstance(),
                               base::BindOnce(&BuildMockIOSRulesService));
    builder2.AddTestingFactory(
        enterprise_connectors::IOSReportingEventRouterFactory::GetInstance(),
        base::BindOnce(
            &MockReportingEventRouter::BuildMockReportingEventRouter));
    profile2_ = std::move(builder2).Build();
    rules_service2_ = static_cast<MockIOSRulesService*>(
        IOSRulesServiceFactory::GetForProfile(profile2_.get()));
    reporting_router2_ = static_cast<MockReportingEventRouter*>(
        enterprise_connectors::IOSReportingEventRouterFactory::GetForProfile(
            profile2_.get()));
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<MockIOSRulesService> rules_service_;
  raw_ptr<MockReportingEventRouter> reporting_router_;
  std::unique_ptr<TestProfileIOS> profile2_;
  raw_ptr<MockIOSRulesService> rules_service2_;
  raw_ptr<MockReportingEventRouter> reporting_router2_;
};

TEST_F(ClipboardUtilsTest, IsPasteAllowedByPolicy_Allow) {
  GURL source_url(kSourceUrl);
  GURL destination_url(kDestinationUrl);
  EXPECT_CALL(*rules_service_, GetPasteVerdict(source_url, destination_url,
                                               nullptr, profile_.get()))
      .WillOnce(::testing::Return(Verdict::Allow()));

  PastePolicyVerdict verdict =
      IsPasteAllowedByPolicy(source_url, destination_url,
                             ui::ClipboardMetadata(), nullptr, profile_.get());
  EXPECT_EQ(verdict.verdict.level(), Rule::Level::kAllow);
  EXPECT_FALSE(verdict.dialog_triggered_by_source);
}

TEST_F(ClipboardUtilsTest, IsPasteAllowedByPolicy_Warn) {
  GURL source_url(kSourceUrl);
  GURL destination_url(kDestinationUrl);
  EXPECT_CALL(*rules_service_, GetPasteVerdict(source_url, destination_url,
                                               profile_.get(), profile_.get()))
      .WillOnce(::testing::Return(Verdict::Warn({})));

  PastePolicyVerdict verdict = IsPasteAllowedByPolicy(
      source_url, destination_url, ui::ClipboardMetadata(), profile_.get(),
      profile_.get());
  EXPECT_EQ(verdict.verdict.level(), Rule::Level::kWarn);
  EXPECT_FALSE(verdict.dialog_triggered_by_source);
}

TEST_F(ClipboardUtilsTest, IsPasteAllowedByPolicy_Block) {
  GURL source_url(kSourceUrl);
  GURL destination_url(kDestinationUrl);
  EXPECT_CALL(*rules_service_, GetPasteVerdict(source_url, destination_url,
                                               profile_.get(), profile_.get()))
      .WillOnce(::testing::Return(Verdict::Block({})));

  PastePolicyVerdict verdict = IsPasteAllowedByPolicy(
      source_url, destination_url, ui::ClipboardMetadata(), profile_.get(),
      profile_.get());
  EXPECT_EQ(verdict.verdict.level(), Rule::Level::kBlock);
  EXPECT_FALSE(verdict.dialog_triggered_by_source);
}

TEST_F(ClipboardUtilsTest,
       IsPasteAllowedByPolicy_DifferentProfiles_SourceWarns) {
  GURL source_url(kSourceUrl);
  GURL destination_url(kDestinationUrl);
  EXPECT_CALL(*rules_service_,
              GetPasteVerdict(source_url, destination_url, _, _))
      .WillOnce(::testing::Return(Verdict::Warn({})));
  EXPECT_CALL(*rules_service2_,
              GetPasteVerdict(source_url, destination_url, _, _))
      .WillOnce(::testing::Return(Verdict::NotSet()));

  PastePolicyVerdict verdict = IsPasteAllowedByPolicy(
      source_url, destination_url, ui::ClipboardMetadata(), profile_.get(),
      profile2_.get());
  EXPECT_EQ(verdict.verdict.level(), Rule::Level::kWarn);
  EXPECT_TRUE(verdict.dialog_triggered_by_source);
}

TEST_F(ClipboardUtilsTest,
       IsPasteAllowedByPolicy_DifferentProfiles_DestinationWarns) {
  GURL source_url(kSourceUrl);
  GURL destination_url(kDestinationUrl);
  EXPECT_CALL(*rules_service_,
              GetPasteVerdict(source_url, destination_url, _, _))
      .WillOnce(::testing::Return(Verdict::NotSet()));
  EXPECT_CALL(*rules_service2_,
              GetPasteVerdict(source_url, destination_url, _, _))
      .WillOnce(::testing::Return(Verdict::Warn({})));

  PastePolicyVerdict verdict = IsPasteAllowedByPolicy(
      source_url, destination_url, ui::ClipboardMetadata(), profile_.get(),
      profile2_.get());
  EXPECT_EQ(verdict.verdict.level(), Rule::Level::kWarn);
  EXPECT_FALSE(verdict.dialog_triggered_by_source);
}

TEST_F(ClipboardUtilsTest,
       IsPasteAllowedByPolicy_DifferentProfiles_SourceBlocks) {
  GURL source_url(kSourceUrl);
  GURL destination_url(kDestinationUrl);
  EXPECT_CALL(*rules_service_,
              GetPasteVerdict(source_url, destination_url, _, _))
      .WillOnce(::testing::Return(Verdict::Block({})));
  EXPECT_CALL(*rules_service2_,
              GetPasteVerdict(source_url, destination_url, _, _))
      .WillOnce(::testing::Return(Verdict::NotSet()));

  PastePolicyVerdict verdict = IsPasteAllowedByPolicy(
      source_url, destination_url, ui::ClipboardMetadata(), profile_.get(),
      profile2_.get());
  EXPECT_EQ(verdict.verdict.level(), Rule::Level::kBlock);
  EXPECT_TRUE(verdict.dialog_triggered_by_source);
}

TEST_F(ClipboardUtilsTest,
       IsPasteAllowedByPolicy_DifferentProfiles_DestinationBlocks) {
  GURL source_url(kSourceUrl);
  GURL destination_url(kDestinationUrl);
  EXPECT_CALL(*rules_service_,
              GetPasteVerdict(source_url, destination_url, _, _))
      .WillOnce(::testing::Return(Verdict::NotSet()));
  EXPECT_CALL(*rules_service2_,
              GetPasteVerdict(source_url, destination_url, _, _))
      .WillOnce(::testing::Return(Verdict::Block({})));

  PastePolicyVerdict verdict = IsPasteAllowedByPolicy(
      source_url, destination_url, ui::ClipboardMetadata(), profile_.get(),
      profile2_.get());
  EXPECT_EQ(verdict.verdict.level(), Rule::Level::kBlock);
  EXPECT_FALSE(verdict.dialog_triggered_by_source);
}

TEST_F(ClipboardUtilsTest, IsCopyAllowedByPolicy_Allow) {
  GURL source_url(kSourceUrl);
  EXPECT_CALL(*rules_service_, GetCopyRestrictedBySourceVerdict(source_url))
      .WillOnce(::testing::Return(Verdict::Allow()));
  EXPECT_CALL(*rules_service_, GetCopyToOSClipboardVerdict(source_url))
      .WillOnce(::testing::Return(Verdict::Allow()));

  CopyPolicyVerdicts verdicts = IsCopyAllowedByPolicy(
      source_url, ui::ClipboardMetadata(), profile_.get());
  EXPECT_EQ(verdicts.copy_action_verdict.level(), Rule::Level::kAllow);
  EXPECT_TRUE(verdicts.copy_to_os_clipbord);
}

TEST_F(ClipboardUtilsTest, IsCopyAllowedByPolicy_SourceBlocked) {
  GURL source_url(kSourceUrl);
  EXPECT_CALL(*rules_service_, GetCopyRestrictedBySourceVerdict(source_url))
      .WillOnce(::testing::Return(Verdict::Block({})));

  CopyPolicyVerdicts verdicts = IsCopyAllowedByPolicy(
      source_url, ui::ClipboardMetadata(), profile_.get());
  EXPECT_EQ(verdicts.copy_action_verdict.level(), Rule::Level::kBlock);
  EXPECT_FALSE(verdicts.copy_to_os_clipbord);
}

TEST_F(ClipboardUtilsTest, IsCopyAllowedByPolicy_OSBlocked) {
  GURL source_url(kSourceUrl);
  EXPECT_CALL(*rules_service_, GetCopyRestrictedBySourceVerdict(source_url))
      .WillOnce(::testing::Return(Verdict::Allow()));
  EXPECT_CALL(*rules_service_, GetCopyToOSClipboardVerdict(source_url))
      .WillOnce(::testing::Return(Verdict::Block({})));

  CopyPolicyVerdicts verdicts = IsCopyAllowedByPolicy(
      source_url, ui::ClipboardMetadata(), profile_.get());
  EXPECT_EQ(verdicts.copy_action_verdict.level(), Rule::Level::kAllow);
  EXPECT_FALSE(verdicts.copy_to_os_clipbord);
}

TEST_F(ClipboardUtilsTest, IsCopyAllowedByPolicy_Warn) {
  GURL source_url(kSourceUrl);
  EXPECT_CALL(*rules_service_, GetCopyRestrictedBySourceVerdict(source_url))
      .WillOnce(::testing::Return(Verdict::Warn({})));
  EXPECT_CALL(*rules_service_, GetCopyToOSClipboardVerdict(source_url))
      .WillOnce(::testing::Return(Verdict::Allow()));

  CopyPolicyVerdicts verdicts = IsCopyAllowedByPolicy(
      source_url, ui::ClipboardMetadata(), profile_.get());
  EXPECT_EQ(verdicts.copy_action_verdict.level(), Rule::Level::kWarn);
  EXPECT_TRUE(verdicts.copy_to_os_clipbord);
}

TEST_F(ClipboardUtilsTest, IsCopyAllowedByPolicy_WarnAndOSWarn) {
  GURL source_url(kSourceUrl);
  EXPECT_CALL(*rules_service_, GetCopyRestrictedBySourceVerdict(source_url))
      .WillOnce(::testing::Return(Verdict::Warn({})));
  EXPECT_CALL(*rules_service_, GetCopyToOSClipboardVerdict(source_url))
      .WillOnce(::testing::Return(Verdict::Warn({})));

  CopyPolicyVerdicts verdicts = IsCopyAllowedByPolicy(
      source_url, ui::ClipboardMetadata(), profile_.get());
  EXPECT_EQ(verdicts.copy_action_verdict.level(), Rule::Level::kWarn);
  EXPECT_TRUE(verdicts.copy_to_os_clipbord);
}

TEST_F(ClipboardUtilsTest, MaybeReportDataControlsPaste) {
  GURL source_url(kSourceUrl);
  GURL destination_url(kDestinationUrl);
  Verdict verdict =
      Verdict::Warn({{Verdict::TriggeredRuleKey{0, false},
                      Verdict::TriggeredRule{"rule_1", "rule_1"}}});
  EXPECT_CALL(*reporting_router2_, ReportPaste(_, _)).Times(1);
  MaybeReportDataControlsPaste(source_url, destination_url, profile_.get(),
                               profile2_.get(), ui::ClipboardMetadata(),
                               verdict);
}

TEST_F(ClipboardUtilsTest, MaybeReportDataControlsPaste_Bypassed) {
  GURL source_url(kSourceUrl);
  GURL destination_url(kDestinationUrl);
  Verdict verdict =
      Verdict::Warn({{Verdict::TriggeredRuleKey{0, false},
                      Verdict::TriggeredRule{"rule_1", "rule_1"}}});
  EXPECT_CALL(*reporting_router2_, ReportPasteWarningBypassed(_, _)).Times(1);
  MaybeReportDataControlsPaste(source_url, destination_url, profile_.get(),
                               profile2_.get(), ui::ClipboardMetadata(),
                               verdict,
                               /*bypassed=*/true);
}

TEST_F(ClipboardUtilsTest, MaybeReportDataControlsCopy) {
  GURL source_url(kSourceUrl);
  Verdict verdict =
      Verdict::Warn({{Verdict::TriggeredRuleKey{0, false},
                      Verdict::TriggeredRule{"rule_1", "rule_1"}}});
  EXPECT_CALL(*reporting_router_, ReportCopy(_, _)).Times(1);
  MaybeReportDataControlsCopy(source_url, profile_.get(),
                              ui::ClipboardMetadata(), verdict);
}

TEST_F(ClipboardUtilsTest, MaybeReportDataControlsCopy_Bypassed) {
  GURL source_url(kSourceUrl);
  Verdict verdict =
      Verdict::Warn({{Verdict::TriggeredRuleKey{0, false},
                      Verdict::TriggeredRule{"rule_1", "rule_1"}}});
  EXPECT_CALL(*reporting_router_, ReportCopyWarningBypassed(_, _)).Times(1);
  MaybeReportDataControlsCopy(source_url, profile_.get(),
                              ui::ClipboardMetadata(), verdict,
                              /*bypassed=*/true);
}

}  // namespace data_controls
