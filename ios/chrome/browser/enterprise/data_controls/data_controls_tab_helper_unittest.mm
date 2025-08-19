// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/data_controls_tab_helper.h"

#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/test/bind.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace data_controls {

// Unit tests for DataControlsTabHelper.
class DataControlsTabHelperTest : public PlatformTest {
 protected:
  void SetUp() final {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
    tab_helper_ =
        DataControlsTabHelper::GetOrCreateForWebState(web_state_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<DataControlsTabHelper> tab_helper_ = nullptr;
};

TEST_F(DataControlsTabHelperTest, ShouldAllowCopy) {
  base::RunLoop run_loop;
  tab_helper_->ShouldAllowCopy(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
}

TEST_F(DataControlsTabHelperTest, ShouldAllowPaste) {
  base::RunLoop run_loop;
  tab_helper_->ShouldAllowPaste(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
}

TEST_F(DataControlsTabHelperTest, ShouldAllowCut) {
  base::RunLoop run_loop;
  tab_helper_->ShouldAllowCut(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
}

TEST_F(DataControlsTabHelperTest, ShouldAllowShare) {
  base::RunLoop run_loop;
  tab_helper_->ShouldAllowShare(base::BindLambdaForTesting([&](bool allowed) {
    EXPECT_TRUE(allowed);
    run_loop.Quit();
  }));
  run_loop.Run();
}

}  // namespace data_controls
