// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_edit_menu_builder.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_tab_helper.h"
#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/components/enterprise/data_controls/features.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class DataControlsEditMenuBuilderTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
    feature_list_.InitAndEnableFeature(
        data_controls::kEnableClipboardDataControlsIOS);
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Tests that the share menu is removed when sharing is blocked.
TEST_F(DataControlsEditMenuBuilderTest, ShareBlocked) {
  SetCopyBlockRule(profile_->GetPrefs());
  web_state_->SetCurrentURL(GURL(kDataControlsBlockedUrl));

  DataControlsEditMenuBuilder* builder =
      [[DataControlsEditMenuBuilder alloc] init];
  id menu_builder = OCMProtocolMock(@protocol(UIMenuBuilder));
  OCMExpect([menu_builder removeMenuForIdentifier:UIMenuShare]);

  [builder buildEditMenuWithBuilder:menu_builder inWebState:web_state_.get()];

  EXPECT_OCMOCK_VERIFY(menu_builder);
}

// Tests that the share menu is not removed when sharing is allowed.
TEST_F(DataControlsEditMenuBuilderTest, ShareAllowed) {
  DataControlsEditMenuBuilder* builder =
      [[DataControlsEditMenuBuilder alloc] init];
  id menu_builder = OCMProtocolMock(@protocol(UIMenuBuilder));

  [builder buildEditMenuWithBuilder:menu_builder inWebState:web_state_.get()];

  EXPECT_OCMOCK_VERIFY(menu_builder);
}
