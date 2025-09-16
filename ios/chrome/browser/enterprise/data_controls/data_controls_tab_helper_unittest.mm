// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/data_controls_tab_helper.h"

#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "components/enterprise/data_controls/core/browser/test_utils.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/components/enterprise/data_controls/features.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

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

  void SetBlockRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                        "sources": {
                          "urls": ["block.com"]
                        },
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "BLOCK"}
                        ]
                      })"});
  }

  void SetAllowRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                        "sources": {
                          "urls": ["allow.com"]
                        },
                        "restrictions": [
                          {"class": "CLIPBOARD", "level": "ALLOW"}
                        ]
                      })"});
  }

  void SetWarnRule() {
    SetDataControls(profile_->GetTestingPrefService(), {R"({
                        "sources": {
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
  SetBlockRule();
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
  SetAllowRule();
  web_state_->SetCurrentURL(GURL(kAllowedUrl));
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that copy is allowed when a "WARN" rule matches the page URL.
TEST_F(DataControlsTabHelperTest, ShouldAllowCopy_Warn) {
  SetWarnRule();
  web_state_->SetCurrentURL(GURL(kWarnUrl));
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that copy is allowed from a URL that doesn't match any rules.
TEST_F(DataControlsTabHelperTest, ShouldAllowCopy_OtherUrl) {
  SetBlockRule();
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
  SetBlockRule();
  web_state_->SetCurrentURL(GURL(kBlockedUrl));
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that paste is allowed by default.
TEST_F(DataControlsTabHelperTest, ShouldAllowPaste) {
  base::RunLoop run_loop;
  tab_helper()->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// Tests that cut is blocked when a "BLOCK" rule matches the page URL.
TEST_F(DataControlsTabHelperTest, ShouldAllowCut) {
  SetBlockRule();
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
