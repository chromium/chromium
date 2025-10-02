// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_tab_helper.h"

#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/test/bind.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "components/enterprise/data_controls/core/browser/test_utils.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/data_controls_commands.h"
#import "ios/chrome/test/fakes/fake_data_controls_commands_handler.h"
#import "ios/components/enterprise/data_controls/features.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

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
    profile_ = TestProfileIOS::Builder().Build();
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
    feature_list_.InitAndEnableFeature(kEnableClipboardDataControlsIOS);
  }

  DataControlsTabHelper* tab_helper() {
    return DataControlsTabHelper::GetOrCreateForWebState(web_state_.get());
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

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
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
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_FALSE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
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
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_FALSE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
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
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCut(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_FALSE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
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
