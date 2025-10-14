// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_tab_helper.h"

#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "components/enterprise/data_controls/core/browser/test_utils.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_pasteboard_manager.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/data_controls_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/test/fakes/fake_data_controls_commands_handler.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/components/enterprise/data_controls/features.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace data_controls {

namespace {
// Test URLs used to set up rules.
const char kBlockedUrl[] = "https://block.com";
const char kAllowedUrl[] = "https://allow.com";
const char kWarnUrl[] = "https://warn.com";
const char kOtherUrl[] = "https://other.com";
}  // namespace

// Unit tests for DataControlsTabHelper.
class DataControlsTabHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
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

  void SetCopyBlockRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                        "sources": {
                          "urls": ["block.com"]
                        },
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "BLOCK"}
                        ]
                      })"});
  }

  void SetCopyAllowRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                        "sources": {
                          "urls": ["allow.com"]
                        },
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "ALLOW"}
                        ]
                      })"});
  }

  void SetCopyWarnRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                        "sources": {
                          "urls": ["warn.com"]
                        },
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "WARN"}
                        ]
                      })"});
  }

  void SetPasteBlockRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                        "destinations": {
                          "urls": ["block.com"]
                        },
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "BLOCK"}
                        ]
                      })"});
  }

  void SetPasteAllowRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                        "destinations": {
                          "urls": ["allow.com"]
                        },
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "ALLOW"}
                        ]
                      })"});
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
                      })"});
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
                      })"});
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
                      })"});
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
                      })"});
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Tests that copy is allowed by default when no rules are set.
TEST_F(DataControlsTabHelperTest, ShouldAllowCopy_Default) {
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that copy is blocked when a "BLOCK" rule matches the page URL.
TEST_F(DataControlsTabHelperTest, ShouldAllowCopy_Blocked) {
  SetCopyBlockRule();
  web_state_->SetCurrentURL(GURL(kBlockedUrl));
  id snackbar_handler = OCMStrictProtocolMock(@protocol(SnackbarCommands));
  OCMExpect([snackbar_handler
      showSnackbarWithMessage:l10n_util::GetNSString(
                                  IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION)
                   buttonText:nil
                messageAction:nil
             completionAction:OCMOCK_ANY]);
  tab_helper()->SetSnackbarHandler(snackbar_handler);
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_FALSE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
  [(OCMockObject*)snackbar_handler verify];
}

// Tests that copy is allowed when an "ALLOW" rule matches the page URL.
TEST_F(DataControlsTabHelperTest, ShouldAllowCopy_Allowed) {
  SetCopyAllowRule();
  web_state_->SetCurrentURL(GURL(kAllowedUrl));
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
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
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_FALSE(allowed);
    run_loop.Quit();
  }));
  EXPECT_TRUE(
      base::test::RunUntil([&] { return !handler->_callback.is_null(); }));
  EXPECT_EQ(handler.dialogType, DataControlsDialog::Type::kClipboardCopyWarn);
  WarningDialog dialog = GetWarningDialog(handler.dialogType);
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
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    run_loop.Quit();
  }));
  EXPECT_TRUE(
      base::test::RunUntil([&] { return !handler->_callback.is_null(); }));
  EXPECT_EQ(handler.dialogType, DataControlsDialog::Type::kClipboardCopyWarn);
  WarningDialog dialog = GetWarningDialog(handler.dialogType);
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

// Tests that copy is allowed from a URL that doesn't match any rules.
TEST_F(DataControlsTabHelperTest, ShouldAllowCopy_OtherUrl) {
  SetCopyBlockRule();
  // The blocking rule is set for `kBlockedUrl`, so it shouldn't apply to
  // `kOtherUrl`.
  web_state_->SetCurrentURL(GURL(kOtherUrl));
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that copy is allowed when a "BLOCK" rule is set but the feature is
// disabled.
TEST_F(DataControlsTabHelperTest, ShouldAllowCopy_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEnableClipboardDataControlsIOS);
  SetCopyBlockRule();
  web_state_->SetCurrentURL(GURL(kBlockedUrl));
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that paste is allowed by default.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_Default) {
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that paste is blocked when a "BLOCK" rule matches the page URL.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_Blocked) {
  SetPasteBlockRule();
  web_state_->SetCurrentURL(GURL(kBlockedUrl));
  id snackbar_handler = OCMStrictProtocolMock(@protocol(SnackbarCommands));
  OCMExpect([snackbar_handler
      showSnackbarWithMessage:l10n_util::GetNSString(
                                  IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION)
                   buttonText:nil
                messageAction:nil
             completionAction:OCMOCK_ANY]);
  tab_helper()->SetSnackbarHandler(snackbar_handler);
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_FALSE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
  [(OCMockObject*)snackbar_handler verify];
}

// Tests that paste is allowed when an "ALLOW" rule matches the page URL.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_Allowed) {
  SetPasteAllowRule();
  web_state_->SetCurrentURL(GURL(kAllowedUrl));
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that paste is blocked when a "WARN" rule matches the page URL and the
// user does not bypass the warning.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_Warn_NotBypassed) {
  SetPasteWarnRule();
  web_state_->SetCurrentURL(GURL(kWarnUrl));
  auto* handler = [[FakeDataControlsCommandsHandler alloc] init];
  tab_helper()->SetDataControlsCommandsHandler(handler);
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_FALSE(allowed);
    run_loop.Quit();
  }));
  EXPECT_TRUE(
      base::test::RunUntil([&] { return !handler->_callback.is_null(); }));
  EXPECT_EQ(handler.dialogType, DataControlsDialog::Type::kClipboardPasteWarn);
  WarningDialog dialog = GetWarningDialog(handler.dialogType);
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
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    run_loop.Quit();
  }));
  EXPECT_TRUE(
      base::test::RunUntil([&] { return !handler->_callback.is_null(); }));
  EXPECT_EQ(handler.dialogType, DataControlsDialog::Type::kClipboardPasteWarn);
  WarningDialog dialog = GetWarningDialog(handler.dialogType);
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

// Tests that paste is allowed to a URL that doesn't match any rules.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_OtherUrl) {
  SetPasteBlockRule();
  web_state_->SetCurrentURL(GURL(kOtherUrl));
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
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

  UIPasteboard.generalPasteboard.string = @"copied content";

  EXPECT_TRUE(WaitUntilConditionOrTimeout(
      kWaitForUIElementTimeout, /* run_message_loop= */ true, ^bool {
        return DataControlsPasteboardManager::GetInstance()
            ->GetCurrentPasteboardItemsSource()
            .source_profile;
      }));

  // Simulate pasting to allow.com
  web_state_->SetCurrentURL(GURL(kAllowedUrl));
  EXPECT_FALSE(ShouldAllowPaste(tab_helper()));
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

  UIPasteboard.generalPasteboard.string = @"copied content";

  EXPECT_TRUE(WaitUntilConditionOrTimeout(
      kWaitForUIElementTimeout, /* run_message_loop= */ true, ^bool {
        return DataControlsPasteboardManager::GetInstance()
            ->GetCurrentPasteboardItemsSource()
            .source_profile;
      }));

  // Simulate pasting to kBlockedUrl in the non-incognito profile.
  web_state_->SetCurrentURL(GURL(kBlockedUrl));
  EXPECT_FALSE(ShouldAllowPaste(tab_helper()));
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

  UIPasteboard.generalPasteboard.string = @"copied content";

  EXPECT_TRUE(WaitUntilConditionOrTimeout(
      kWaitForUIElementTimeout, /* run_message_loop= */ true, ^bool {
        return DataControlsPasteboardManager::GetInstance()
            ->GetCurrentPasteboardItemsSource()
            .source_profile;
      }));

  // Simulate pasting to kBlockedUrl in the primary profile.
  web_state_->SetCurrentURL(GURL(kBlockedUrl));
  EXPECT_FALSE(ShouldAllowPaste(tab_helper()));
}

// Tests that paste is blocked when a rule is set to block pasting from the OS
// clipboard to a specific destination.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_BlockedFromOSClipboard) {
  SetPasteBlockFromOSClipboardRule();

  // Simulate pasting to kBlockedUrl.
  web_state_->SetCurrentURL(GURL(kBlockedUrl));
  EXPECT_FALSE(ShouldAllowPaste(tab_helper()));
}

// Tests that paste is allowed when a "BLOCK" rule is set but the feature is
// disabled.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEnableClipboardDataControlsIOS);
  SetPasteBlockRule();
  web_state_->SetCurrentURL(GURL(kBlockedUrl));
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that cut is blocked when a "BLOCK" rule matches the page URL.
TEST_F(DataControlsTabHelperTest, ShouldAllowCut) {
  SetCopyBlockRule();
  web_state_->SetCurrentURL(GURL(kBlockedUrl));
  id snackbar_handler = OCMStrictProtocolMock(@protocol(SnackbarCommands));
  OCMExpect([snackbar_handler
      showSnackbarWithMessage:l10n_util::GetNSString(
                                  IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION)
                   buttonText:nil
                messageAction:nil
             completionAction:OCMOCK_ANY]);
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
TEST_F(DataControlsTabHelperTest, ShouldAllowShare) {
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowShare(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
}

}  // namespace data_controls
