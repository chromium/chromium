// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_tab_helper.h"

#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "components/enterprise/data_controls/core/browser/prefs.h"
#import "components/enterprise/data_controls/core/browser/test_utils.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/enterprise/common/test/mock_reporting_event_router.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client_factory.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_reporting_event_router_factory.h"
#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_metrics.h"
#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_pasteboard_manager.h"
#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/data_controls_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/test/fakes/fake_data_controls_commands_handler.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/components/enterprise/data_controls/features.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

using ::testing::_;

namespace data_controls {

namespace {
// Test URLs used to set up rules.
const char kDataControlsBlockedUrl[] = "https://block.com";
const char kAllowedUrl[] = "https://allow.com";
const char kWarnUrl[] = "https://warn.com";
const char kOtherUrl[] = "https://other.com";
inline constexpr std::u16string_view kOrganizationDomain = u"google.com";

}  // namespace

// Unit tests for DataControlsTabHelper.
class DataControlsTabHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(
        enterprise_connectors::IOSReportingEventRouterFactory::GetInstance(),
        base::BindOnce(
            &MockReportingEventRouter::BuildMockReportingEventRouter));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    reporting_router_ = static_cast<MockReportingEventRouter*>(
        enterprise_connectors::IOSReportingEventRouterFactory::GetForProfile(
            profile_.get()));
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_);
    feature_list_.InitAndEnableFeature(kEnableClipboardDataControlsIOS);
  }

  void TearDown() override {
    DataControlsPasteboardManager::GetInstance()->ResetForTesting();
    PlatformTest::TearDown();
  }

  DataControlsTabHelper* tab_helper() {
    return DataControlsTabHelper::GetOrCreateForWebState(web_state_.get());
  }

  bool ShouldAllowCopy(DataControlsTabHelper* tab_helper) {
    base::RunLoop run_loop;
    bool result;
    tab_helper->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
      result = allowed;
      run_loop.Quit();
    }));
    run_loop.Run();
    return result;
  }

  bool ShouldAllowPaste(DataControlsTabHelper* tab_helper) {
    base::RunLoop run_loop;
    bool result;
    tab_helper->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
      result = allowed;
      run_loop.Quit();
    }));
    run_loop.Run();
    return result;
  }

  void SetCopyFromOsClipboardBlockRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                          "sources": {
                            "os_clipboard": true
                          },
                          "restrictions": [
                            {"class": "CLIPBOARD", "level": "BLOCK"}
                          ]
                        })"},
                    /*machine_scope=*/false);
  }

  void SetCopyAllowRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                        "sources": {
                          "urls": ["allow.com"]
                        },
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "ALLOW"}
                        ]
                      })"},
                    /*machine_scope=*/false);
  }

  void SetCopyWarnRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                        "sources": {
                          "urls": ["warn.com"]
                        },
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "WARN"}
                        ]
                      })"},
                    /*machine_scope=*/false);
  }

  void SetPasteBlockRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                        "destinations": {
                          "urls": ["block.com"]
                        },
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "BLOCK"}
                        ]
                      })"},
                    /*machine_scope=*/false);
  }

  void SetPasteAllowRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                        "destinations": {
                          "urls": ["allow.com"]
                        },
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "ALLOW"}
                        ]
                      })"},
                    /*machine_scope=*/false);
  }

  void SetPasteBlockForSourceRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                        "and": [
                              {
                                "destinations": {
                                  "urls": [
                                    "allow.com"
                                  ]
                                }
                              },
                              {
                                "sources": {
                                  "urls": [
                                    "other.com"
                                  ]
                                }
                              }
                            ],
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "BLOCK"}
                        ]
                      })"},
                    /*machine_scope=*/false);
  }

  void SetPasteAllowForSourceRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                          "and": [
                                {
                                  "destinations": {
                                    "urls": [
                                      "allow.com"
                                    ]
                                  }
                                },
                                {
                                  "sources": {
                                    "urls": [
                                      "other.com"
                                    ]
                                  }
                                }
                              ],
                          "restrictions": [
                            {"class": "CLIPBOARD", "level": "ALLOW"}
                          ]
                        })"});
  }

  void SetPasteWarnRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                        "destinations": {
                          "urls": ["warn.com"]
                        },
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "WARN"}
                        ]
                      })"},
                    /*machine_scope=*/false);
  }

  void SetPasteBlockFromIncognitoRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                        "and": [
                          {
                            "destinations": {
                              "urls": [
                                "block.com"
                              ]
                            }
                          },
                          {
                            "sources": {
                              "incognito": true
                            }
                          }
                        ],
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "BLOCK"}
                        ]
                      })"},
                    /*machine_scope=*/false);
  }

  void SetPasteBlockFromOtherProfileRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                        "and": [
                          {
                            "destinations": {
                              "urls": [
                                "block.com"
                              ]
                            }
                          },
                          {
                            "sources": {
                              "other_profile": true
                            }
                          }
                        ],
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "BLOCK"}
                        ]
                      })"},
                    /*machine_scope=*/false);
  }

  void SetPasteBlockFromOSClipboardRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                        "and": [
                          {
                            "destinations": {
                              "urls": [
                                "block.com"
                              ]
                            }
                          },
                          {
                            "sources": {
                              "os_clipboard": true
                            }
                          }
                        ],
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "BLOCK"}
                        ]
                      })"},
                    /*machine_scope=*/false);
  }

  void SetPasteBlockToOSClipboardRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                          "and": [
                            {
                              "sources": {
                                "urls": [
                                  "block.com"
                                ]
                              }
                            },
                            {
                              "destinations": {
                                "os_clipboard": true
                              }
                            }
                          ],
                          "restrictions": [
                            {"class": "CLIPBOARD", "level": "BLOCK"}
                          ]
                        })"});
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  raw_ptr<MockReportingEventRouter> reporting_router_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Tests that copy is allowed by default when no rules are set.
TEST_F(DataControlsTabHelperTest, ShouldAllowCopy_Default) {
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardCopyVerdictHistogram,
        Rule::Level::kNotSet, 1);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that copy is blocked when a "BLOCK" rule matches the page URL.
TEST_F(DataControlsTabHelperTest, ShouldAllowCopy_Blocked) {
  SetCopyBlockRule(profile_->GetPrefs());
  web_state_->SetCurrentURL(GURL(kDataControlsBlockedUrl));
  id snackbar_handler = OCMStrictProtocolMock(@protocol(SnackbarCommands));
  OCMExpect([snackbar_handler
      showSnackbarMessageAfterDismissingKeyboard:[OCMArg checkWithBlock:^BOOL(
                                                             SnackbarMessage*
                                                                 obj) {
        return [obj.title
            isEqualToString:l10n_util::GetNSString(
                                IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION)];
      }]]);
  tab_helper()->SetSnackbarHandler(snackbar_handler);
  EXPECT_CALL(*reporting_router_, ReportCopy(_, _)).Times(1);
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_FALSE(allowed);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardCopyVerdictHistogram,
        Rule::Level::kBlock, 1);
    run_loop.Quit();
  }));
  run_loop.Run();
  [(OCMockObject*)snackbar_handler verify];
}

// Tests that copy is blocked when a "BLOCK" rule matches the page URL and
// the snackbar message is correct when the organization domain is not empty.
TEST_F(DataControlsTabHelperTest, ShouldAllowCopy_Blocked_WithDomain) {
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile_),
      "user@" + base::UTF16ToUTF8(kOrganizationDomain),
      signin::ConsentLevel::kSignin);
  SetCopyBlockRule(profile_->GetPrefs());
  web_state_->SetCurrentURL(GURL(kDataControlsBlockedUrl));
  id snackbar_handler = OCMStrictProtocolMock(@protocol(SnackbarCommands));
  OCMExpect([snackbar_handler
      showSnackbarMessageAfterDismissingKeyboard:[OCMArg checkWithBlock:^BOOL(
                                                             SnackbarMessage*
                                                                 obj) {
        return [obj.title
            isEqualToString:l10n_util::GetNSStringF(
                                IDS_DATA_CONTROLS_BLOCKED_LABEL_WITH_DOMAIN,
                                std::u16string(kOrganizationDomain))];
      }]]);
  tab_helper()->SetSnackbarHandler(snackbar_handler);
  EXPECT_CALL(*reporting_router_, ReportCopy(_, _)).Times(1);
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_FALSE(allowed);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardCopyVerdictHistogram,
        Rule::Level::kBlock, 1);
    run_loop.Quit();
  }));
  run_loop.Run();
  [(OCMockObject*)snackbar_handler verify];
}

// Tests that copy is allowed when an "ALLOW" rule matches the page URL.
TEST_F(DataControlsTabHelperTest, ShouldAllowCopy_Allowed) {
  SetCopyAllowRule();
  web_state_->SetCurrentURL(GURL(kAllowedUrl));
  EXPECT_CALL(*reporting_router_, ReportCopy(_, _)).Times(1);
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardCopyVerdictHistogram,
        Rule::Level::kAllow, 1);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that copy is blocked when a "WARN" rule matches the page URL and the
// user does not bypass the warning.
TEST_F(DataControlsTabHelperTest, ShouldAllowCopy_Warn_NotBypassed) {
  SetCopyWarnRule();
  web_state_->SetCurrentURL(GURL(kWarnUrl));
  auto* handler = [[FakeDataControlsCommandsHandler alloc] init];
  tab_helper()->SetDataControlsCommandsHandler(handler);
  EXPECT_CALL(*reporting_router_, ReportCopy(_, _)).Times(1);
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_FALSE(allowed);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardCopyVerdictHistogram,
        Rule::Level::kWarn, 1);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardCopyClipboardWarningBypassedHistogram,
        FALSE, 1);
    run_loop.Quit();
  }));
  EXPECT_TRUE(
      base::test::RunUntil([&] { return !handler->_callback.is_null(); }));
  EXPECT_EQ(handler.dialogType, DataControlsDialog::Type::kClipboardCopyWarn);
  WarningDialog dialog =
      GetWarningDialog(handler.dialogType, handler.organizationDomain);
  EXPECT_TRUE([dialog.title
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_CLIPBOARD_COPY_WARN_TITLE)]);
  EXPECT_TRUE([dialog.label
      isEqualToString:l10n_util::GetNSString(IDS_DATA_CONTROLS_WARNED_LABEL)]);
  EXPECT_TRUE([dialog.ok_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_COPY_WARN_CONTINUE_BUTTON)]);
  EXPECT_TRUE([dialog.cancel_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_COPY_WARN_CANCEL_BUTTON)]);
  std::move(handler->_callback).Run(false);
  run_loop.Run();
}

// Tests that copy is allowed when a "WARN" rule matches the page URL and the
// user bypasses the warning.
TEST_F(DataControlsTabHelperTest, ShouldAllowCopy_Warn_Bypassed) {
  SetCopyWarnRule();
  web_state_->SetCurrentURL(GURL(kWarnUrl));
  auto* handler = [[FakeDataControlsCommandsHandler alloc] init];
  tab_helper()->SetDataControlsCommandsHandler(handler);
  EXPECT_CALL(*reporting_router_, ReportCopyWarningBypassed(_, _)).Times(1);
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardCopyVerdictHistogram,
        Rule::Level::kWarn, 1);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardCopyClipboardWarningBypassedHistogram,
        TRUE, 1);
    run_loop.Quit();
  }));
  EXPECT_TRUE(
      base::test::RunUntil([&] { return !handler->_callback.is_null(); }));
  EXPECT_EQ(handler.dialogType, DataControlsDialog::Type::kClipboardCopyWarn);
  WarningDialog dialog =
      GetWarningDialog(handler.dialogType, handler.organizationDomain);
  EXPECT_TRUE([dialog.title
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_CLIPBOARD_COPY_WARN_TITLE)]);
  EXPECT_TRUE([dialog.label
      isEqualToString:l10n_util::GetNSString(IDS_DATA_CONTROLS_WARNED_LABEL)]);
  EXPECT_TRUE([dialog.ok_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_COPY_WARN_CONTINUE_BUTTON)]);
  EXPECT_TRUE([dialog.cancel_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_COPY_WARN_CANCEL_BUTTON)]);
  std::move(handler->_callback).Run(true);
  run_loop.Run();
}

// Tests that copy is allowed when a "WARN" rule matches the page URL, the
// user bypasses the warning, and the warning dialog is correct when the
// organization domain is not empty.
TEST_F(DataControlsTabHelperTest, ShouldAllowCopy_Warn_Bypassed_WithDomain) {
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile_),
      "user@" + base::UTF16ToUTF8(kOrganizationDomain),
      signin::ConsentLevel::kSignin);
  SetCopyWarnRule();
  web_state_->SetCurrentURL(GURL(kWarnUrl));
  auto* handler = [[FakeDataControlsCommandsHandler alloc] init];
  tab_helper()->SetDataControlsCommandsHandler(handler);
  EXPECT_CALL(*reporting_router_, ReportCopyWarningBypassed(_, _)).Times(1);
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardCopyVerdictHistogram,
        Rule::Level::kWarn, 1);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardCopyClipboardWarningBypassedHistogram,
        TRUE, 1);
    run_loop.Quit();
  }));
  EXPECT_TRUE(
      base::test::RunUntil([&] { return !handler->_callback.is_null(); }));
  EXPECT_EQ(handler.dialogType, DataControlsDialog::Type::kClipboardCopyWarn);
  WarningDialog dialog =
      GetWarningDialog(handler.dialogType, handler.organizationDomain);
  EXPECT_TRUE([dialog.title
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_CLIPBOARD_COPY_WARN_TITLE)]);
  EXPECT_TRUE([dialog.label
      isEqualToString:l10n_util::GetNSStringF(
                          IDS_DATA_CONTROLS_WARNED_LABEL_WITH_DOMAIN,
                          std::u16string(kOrganizationDomain))]);
  EXPECT_TRUE([dialog.ok_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_COPY_WARN_CONTINUE_BUTTON)]);
  EXPECT_TRUE([dialog.cancel_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_COPY_WARN_CANCEL_BUTTON)]);
  std::move(handler->_callback).Run(true);
  run_loop.Run();
}

// Tests that copy is allowed from a URL that doesn't match any rules.
TEST_F(DataControlsTabHelperTest, ShouldAllowCopy_OtherUrl) {
  SetCopyBlockRule(profile_->GetPrefs());
  // The blocking rule is set for `kDataControlsBlockedUrl`, so it shouldn't
  // apply to `kOtherUrl`.
  web_state_->SetCurrentURL(GURL(kOtherUrl));
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardCopyVerdictHistogram,
        Rule::Level::kNotSet, 1);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that copy is allowed when a "BLOCK" rule is set but the feature is
// disabled.
TEST_F(DataControlsTabHelperTest, ShouldAllowCopy_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEnableClipboardDataControlsIOS);
  SetCopyBlockRule(profile_->GetPrefs());
  web_state_->SetCurrentURL(GURL(kDataControlsBlockedUrl));
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    histogram_tester_.ExpectTotalCount(
        kIOSWebStateDataControlsClipboardCopyVerdictHistogram, 0);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that paste is allowed by default.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_Default) {
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardPasteVerdictHistogram,
        Rule::Level::kNotSet, 1);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that paste is blocked when a "BLOCK" rule matches the page URL.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_Blocked) {
  SetPasteBlockRule();
  web_state_->SetCurrentURL(GURL(kDataControlsBlockedUrl));
  id snackbar_handler = OCMStrictProtocolMock(@protocol(SnackbarCommands));
  OCMExpect([snackbar_handler
      showSnackbarMessageAfterDismissingKeyboard:[OCMArg checkWithBlock:^BOOL(
                                                             SnackbarMessage*
                                                                 obj) {
        return [obj.title
            isEqualToString:l10n_util::GetNSString(
                                IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION)];
      }]]);
  tab_helper()->SetSnackbarHandler(snackbar_handler);
  EXPECT_CALL(*reporting_router_, ReportPaste(_, _)).Times(1);
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_FALSE(allowed);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardPasteVerdictHistogram,
        Rule::Level::kBlock, 1);
    run_loop.Quit();
  }));
  run_loop.Run();
  [(OCMockObject*)snackbar_handler verify];
}

// Tests that paste is allowed when an "ALLOW" rule matches the page URL.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_Allowed) {
  SetPasteAllowRule();
  web_state_->SetCurrentURL(GURL(kAllowedUrl));
  EXPECT_CALL(*reporting_router_, ReportPaste(_, _)).Times(1);
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardPasteVerdictHistogram,
        Rule::Level::kAllow, 1);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that paste is blocked when a "BLOCK" rule matches the page URL and
// the snackbar message is correct when the organization domain is not empty.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_Blocked_WithDomain) {
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile_),
      "user@" + base::UTF16ToUTF8(kOrganizationDomain),
      signin::ConsentLevel::kSignin);
  SetPasteBlockRule();
  web_state_->SetCurrentURL(GURL(kDataControlsBlockedUrl));
  id snackbar_handler = OCMStrictProtocolMock(@protocol(SnackbarCommands));
  OCMExpect([snackbar_handler
      showSnackbarMessageAfterDismissingKeyboard:[OCMArg checkWithBlock:^BOOL(
                                                             SnackbarMessage*
                                                                 obj) {
        return [obj.title
            isEqualToString:l10n_util::GetNSStringF(
                                IDS_DATA_CONTROLS_BLOCKED_LABEL_WITH_DOMAIN,
                                std::u16string(kOrganizationDomain))];
      }]]);
  EXPECT_CALL(*reporting_router_, ReportPaste(_, _)).Times(1);
  tab_helper()->SetSnackbarHandler(snackbar_handler);
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_FALSE(allowed);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardPasteVerdictHistogram,
        Rule::Level::kBlock, 1);
    run_loop.Quit();
  }));
  run_loop.Run();
  [(OCMockObject*)snackbar_handler verify];
}

// Tests that paste is blocked when a "WARN" rule matches the page URL and the
// user does not bypass the warning.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_Warn_NotBypassed) {
  SetPasteWarnRule();
  web_state_->SetCurrentURL(GURL(kWarnUrl));
  auto* handler = [[FakeDataControlsCommandsHandler alloc] init];
  tab_helper()->SetDataControlsCommandsHandler(handler);
  EXPECT_CALL(*reporting_router_, ReportPaste(_, _)).Times(1);
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_FALSE(allowed);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardPasteVerdictHistogram,
        Rule::Level::kWarn, 1);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardPasteClipboardWarningBypassedHistogram,
        FALSE, 1);
    run_loop.Quit();
  }));
  EXPECT_TRUE(
      base::test::RunUntil([&] { return !handler->_callback.is_null(); }));
  EXPECT_EQ(handler.dialogType, DataControlsDialog::Type::kClipboardPasteWarn);
  WarningDialog dialog =
      GetWarningDialog(handler.dialogType, handler.organizationDomain);
  EXPECT_TRUE([dialog.title
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_CLIPBOARD_PASTE_WARN_TITLE)]);
  EXPECT_TRUE([dialog.label
      isEqualToString:l10n_util::GetNSString(IDS_DATA_CONTROLS_WARNED_LABEL)]);
  EXPECT_TRUE([dialog.ok_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_PASTE_WARN_CONTINUE_BUTTON)]);
  EXPECT_TRUE([dialog.cancel_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_PASTE_WARN_CANCEL_BUTTON)]);
  std::move(handler->_callback).Run(false);
  run_loop.Run();
}

// Tests that paste is allowed when a "WARN" rule matches the page URL and the
// user bypasses the warning.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_Warn_Bypassed) {
  SetPasteWarnRule();
  web_state_->SetCurrentURL(GURL(kWarnUrl));
  auto* handler = [[FakeDataControlsCommandsHandler alloc] init];
  tab_helper()->SetDataControlsCommandsHandler(handler);
  EXPECT_CALL(*reporting_router_, ReportPasteWarningBypassed(_, _)).Times(1);
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardPasteVerdictHistogram,
        Rule::Level::kWarn, 1);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardPasteClipboardWarningBypassedHistogram,
        TRUE, 1);
    run_loop.Quit();
  }));
  EXPECT_TRUE(
      base::test::RunUntil([&] { return !handler->_callback.is_null(); }));
  EXPECT_EQ(handler.dialogType, DataControlsDialog::Type::kClipboardPasteWarn);
  WarningDialog dialog =
      GetWarningDialog(handler.dialogType, handler.organizationDomain);
  EXPECT_TRUE([dialog.title
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_CLIPBOARD_PASTE_WARN_TITLE)]);
  EXPECT_TRUE([dialog.label
      isEqualToString:l10n_util::GetNSString(IDS_DATA_CONTROLS_WARNED_LABEL)]);
  EXPECT_TRUE([dialog.ok_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_PASTE_WARN_CONTINUE_BUTTON)]);
  EXPECT_TRUE([dialog.cancel_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_PASTE_WARN_CANCEL_BUTTON)]);
  std::move(handler->_callback).Run(true);
  run_loop.Run();
}

// Tests that paste is allowed when a "WARN" rule matches the page URL, the
// user bypasses the warning, and the warning dialog is correct when the
// organization domain is not empty.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_Warn_Bypassed_WithDomain) {
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile_),
      "user@" + base::UTF16ToUTF8(kOrganizationDomain),
      signin::ConsentLevel::kSignin);
  SetPasteWarnRule();
  web_state_->SetCurrentURL(GURL(kWarnUrl));
  auto* handler = [[FakeDataControlsCommandsHandler alloc] init];
  tab_helper()->SetDataControlsCommandsHandler(handler);
  EXPECT_CALL(*reporting_router_, ReportPasteWarningBypassed(_, _)).Times(1);
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardPasteVerdictHistogram,
        Rule::Level::kWarn, 1);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardPasteClipboardWarningBypassedHistogram,
        TRUE, 1);
    run_loop.Quit();
  }));
  EXPECT_TRUE(
      base::test::RunUntil([&] { return !handler->_callback.is_null(); }));
  EXPECT_EQ(handler.dialogType, DataControlsDialog::Type::kClipboardPasteWarn);
  WarningDialog dialog =
      GetWarningDialog(handler.dialogType, handler.organizationDomain);
  EXPECT_TRUE([dialog.title
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_CLIPBOARD_PASTE_WARN_TITLE)]);
  EXPECT_TRUE([dialog.label
      isEqualToString:l10n_util::GetNSStringF(
                          IDS_DATA_CONTROLS_WARNED_LABEL_WITH_DOMAIN,
                          std::u16string(kOrganizationDomain))]);
  EXPECT_TRUE([dialog.ok_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_PASTE_WARN_CONTINUE_BUTTON)]);
  EXPECT_TRUE([dialog.cancel_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_PASTE_WARN_CANCEL_BUTTON)]);
  std::move(handler->_callback).Run(true);
  run_loop.Run();
}

// Tests that paste is allowed to a URL that doesn't match any rules.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_OtherUrl) {
  SetPasteBlockRule();
  web_state_->SetCurrentURL(GURL(kOtherUrl));
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    histogram_tester_.ExpectUniqueSample(
        kIOSWebStateDataControlsClipboardPasteVerdictHistogram,
        Rule::Level::kNotSet, 1);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that paste is blocked when a rule blocks pasting from a specific
// source URL.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_BlockedForSource) {
  // Set a rule to block copying from other.com and pasting into allow.com.
  SetPasteBlockForSourceRule();

  // Simulate copying from other.com.
  web_state_->SetCurrentURL(GURL(kOtherUrl));
  EXPECT_TRUE(ShouldAllowCopy(tab_helper()));
  histogram_tester_.ExpectUniqueSample(
      kIOSWebStateDataControlsClipboardCopyVerdictHistogram,
      Rule::Level::kNotSet, 1);

  UIPasteboard.generalPasteboard.string = @"copied content";

  EXPECT_TRUE(WaitForKnownPasteboardSource());

  // Simulate pasting to allow.com
  web_state_->SetCurrentURL(GURL(kAllowedUrl));
  EXPECT_CALL(*reporting_router_, ReportPaste(_, _)).Times(1);
  EXPECT_FALSE(ShouldAllowPaste(tab_helper()));
  histogram_tester_.ExpectUniqueSample(
      kIOSWebStateDataControlsClipboardPasteVerdictHistogram,
      Rule::Level::kBlock, 1);
}

// Tests that paste is blocked when a rule is set to block pasting from an
// incognito profile to a specific destination.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_BlockedFromIncognito) {
  SetPasteBlockFromIncognitoRule();

  // Get the incognito profile.
  ProfileIOS* incognito_profile = profile_->GetOffTheRecordProfile();
  auto incognito_web_state = std::make_unique<web::FakeWebState>();
  incognito_web_state->SetBrowserState(incognito_profile);
  DataControlsTabHelper* incognito_tab_helper =
      DataControlsTabHelper::GetOrCreateForWebState(incognito_web_state.get());

  // Simulate copying from the incognito profile.
  incognito_web_state->SetCurrentURL(GURL(kOtherUrl));
  EXPECT_TRUE(ShouldAllowCopy(incognito_tab_helper));
  histogram_tester_.ExpectUniqueSample(
      kIOSWebStateDataControlsClipboardCopyVerdictHistogram,
      Rule::Level::kNotSet, 1);

  UIPasteboard.generalPasteboard.string = @"copied content";

  EXPECT_TRUE(WaitForKnownPasteboardSource());

  // Simulate pasting to kDataControlsBlockedUrl in the non-incognito profile.
  web_state_->SetCurrentURL(GURL(kDataControlsBlockedUrl));
  EXPECT_CALL(*reporting_router_, ReportPaste(_, _)).Times(1);
  EXPECT_FALSE(ShouldAllowPaste(tab_helper()));
  histogram_tester_.ExpectUniqueSample(
      kIOSWebStateDataControlsClipboardPasteVerdictHistogram,
      Rule::Level::kBlock, 1);
}

// Tests that paste is blocked when a rule is set to block pasting from another
// profile to a specific destination.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_BlockedFromOtherProfile) {
  SetPasteBlockFromOtherProfileRule();

  // Create a second profile.
  ProfileIOS* source_profile = profile_manager_.AddProfileWithBuilder(
      std::move(TestProfileIOS::Builder().SetName("SourceProfile")));
  auto source_web_state = std::make_unique<web::FakeWebState>();
  source_web_state->SetBrowserState(source_profile);
  DataControlsTabHelper* source_tab_helper =
      DataControlsTabHelper::GetOrCreateForWebState(source_web_state.get());

  // Simulate copying from the second profile.
  source_web_state->SetCurrentURL(GURL(kOtherUrl));
  EXPECT_TRUE(ShouldAllowCopy(source_tab_helper));
  histogram_tester_.ExpectUniqueSample(
      kIOSWebStateDataControlsClipboardCopyVerdictHistogram,
      Rule::Level::kNotSet, 1);

  UIPasteboard.generalPasteboard.string = @"copied content";

  EXPECT_TRUE(WaitForKnownPasteboardSource());

  // Simulate pasting to kDataControlsBlockedUrl in the primary profile.
  web_state_->SetCurrentURL(GURL(kDataControlsBlockedUrl));
  EXPECT_CALL(*reporting_router_, ReportPaste(_, _)).Times(1);
  EXPECT_FALSE(ShouldAllowPaste(tab_helper()));
  histogram_tester_.ExpectUniqueSample(
      kIOSWebStateDataControlsClipboardPasteVerdictHistogram,
      Rule::Level::kBlock, 1);
}

// Tests that paste is blocked when a rule is set to block pasting from the OS
// clipboard to a specific destination.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_BlockedFromOSClipboard) {
  SetPasteBlockFromOSClipboardRule();

  // Simulate pasting to kDataControlsBlockedUrl.
  web_state_->SetCurrentURL(GURL(kDataControlsBlockedUrl));
  EXPECT_CALL(*reporting_router_, ReportPaste(_, _)).Times(1);
  EXPECT_FALSE(ShouldAllowPaste(tab_helper()));
  histogram_tester_.ExpectUniqueSample(
      kIOSWebStateDataControlsClipboardPasteVerdictHistogram,
      Rule::Level::kBlock, 1);
}

// Tests that, for content that is not allowed on the OS clipboard, the content
// is replaced with a placeholder, then temporarily restored for a paste to an
// allowed destination, and then replaced with the placeholder again once the
// paste is complete.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_BlockedToOSClipboard) {
  SetPasteBlockToOSClipboardRule();

  // Simulate copying from block.com.
  web_state_->SetCurrentURL(GURL(kDataControlsBlockedUrl));
  ASSERT_TRUE(ShouldAllowCopy(tab_helper()));
  histogram_tester_.ExpectUniqueSample(
      kIOSWebStateDataControlsClipboardCopyVerdictHistogram,
      Rule::Level::kNotSet, 1);

  NSString* copied_content = @"copied content";
  UIPasteboard.generalPasteboard.string = copied_content;

  // Wait for the source to be available, as it is only available once the
  // content is replaced with the placeholder.
  ASSERT_TRUE(WaitForKnownPasteboardSource());

  NSString* expected_placeholder = l10n_util::GetNSString(
      IDS_ENTERPRISE_DATA_CONTROLS_COPY_PREVENTION_WARNING_MESSAGE);

  ASSERT_NSEQ(expected_placeholder, UIPasteboard.generalPasteboard.string);

  // Simulate pasting to allow.com
  web_state_->SetCurrentURL(GURL(kAllowedUrl));
  ASSERT_TRUE(ShouldAllowPaste(tab_helper()));

  histogram_tester_.ExpectUniqueSample(
      kIOSWebStateDataControlsClipboardPasteVerdictHistogram,
      Rule::Level::kNotSet, 1);

  ASSERT_TRUE(WaitForKnownPasteboardSource());

  // Original content should be restored to the pasteboard.
  ASSERT_NSEQ(UIPasteboard.generalPasteboard.string, copied_content);

  // Once pasting is done the placeholder should be restored.
  tab_helper()->DidFinishClipboardRead();

  ASSERT_TRUE(WaitForStringInPasteboard(expected_placeholder));
}

// Tests that paste is allowed when a "BLOCK" rule is set but the feature is
// disabled.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEnableClipboardDataControlsIOS);
  SetPasteBlockRule();
  web_state_->SetCurrentURL(GURL(kDataControlsBlockedUrl));
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    histogram_tester_.ExpectTotalCount(
        kIOSWebStateDataControlsClipboardPasteVerdictHistogram, 0);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that cut is blocked when a "BLOCK" rule matches the page URL.
TEST_F(DataControlsTabHelperTest, ShouldAllowCut) {
  SetCopyBlockRule(profile_->GetPrefs());
  web_state_->SetCurrentURL(GURL(kDataControlsBlockedUrl));
  id snackbar_handler = OCMStrictProtocolMock(@protocol(SnackbarCommands));
  OCMExpect([snackbar_handler
      showSnackbarMessageAfterDismissingKeyboard:[OCMArg checkWithBlock:^BOOL(
                                                             SnackbarMessage*
                                                                 obj) {
        return [obj.title
            isEqualToString:l10n_util::GetNSString(
                                IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION)];
      }]]);
  tab_helper()->SetSnackbarHandler(snackbar_handler);
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCut(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_FALSE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
  [(OCMockObject*)snackbar_handler verify];
}

// Tests that share is allowed by default.
TEST_F(DataControlsTabHelperTest, ShouldAllowShareSync_Default) {
  EXPECT_TRUE(tab_helper()->ShouldAllowShare());
}

// Tests that share is blocked when a "BLOCK" rule matches the page URL.
TEST_F(DataControlsTabHelperTest, ShouldAllowShareSync_Blocked) {
  SetCopyBlockRule(profile_->GetPrefs());
  web_state_->SetCurrentURL(GURL(kDataControlsBlockedUrl));
  EXPECT_FALSE(tab_helper()->ShouldAllowShare());
}

// Tests that share is allowed when an "ALLOW" rule matches the page URL.
TEST_F(DataControlsTabHelperTest, ShouldAllowShareSync_Allowed) {
  SetCopyAllowRule();
  web_state_->SetCurrentURL(GURL(kAllowedUrl));
  EXPECT_TRUE(tab_helper()->ShouldAllowShare());
}

}  // namespace data_controls
