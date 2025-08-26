// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"

#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

// Test fixture for SnapshotSourceTabHelper.
class SnapshotSourceTabHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    web_state_ = std::make_unique<web::FakeWebState>();
    SnapshotSourceTabHelper::CreateForWebState(web_state_.get());
  }

  SnapshotSourceTabHelper* GetTabHelper() {
    return SnapshotSourceTabHelper::FromWebState(web_state_.get());
  }

  std::unique_ptr<web::FakeWebState> web_state_;
};

// Tests that the helper calls the WebState's methods when no overriding source
// is set.
TEST_F(SnapshotSourceTabHelperTest, NoOverridingSource) {
  web_state_->SetCanTakeSnapshot(true);
  SnapshotSourceTabHelper* tab_helper = GetTabHelper();
  ASSERT_TRUE(tab_helper);

  EXPECT_TRUE(tab_helper->CanTakeSnapshot());
  EXPECT_EQ(web_state_->GetView(), tab_helper->GetView());

  __block bool callback_called = false;
  tab_helper->TakeSnapshot(CGRectZero, base::BindRepeating(^(UIImage* image) {
                             callback_called = true;
                           }));
  EXPECT_TRUE(callback_called);
}

// Tests that the helper calls the overriding WebState's methods when an
// overriding source is set.
TEST_F(SnapshotSourceTabHelperTest, OverridingSource) {
  SnapshotSourceTabHelper* tab_helper = GetTabHelper();
  ASSERT_TRUE(tab_helper);

  auto overriding_web_state = std::make_unique<web::FakeWebState>();
  overriding_web_state->SetCanTakeSnapshot(true);
  tab_helper->SetOverridingSourceWebState(overriding_web_state.get());

  EXPECT_TRUE(tab_helper->CanTakeSnapshot());
  EXPECT_EQ(overriding_web_state->GetView(), tab_helper->GetView());

  __block bool callback_called = false;
  tab_helper->TakeSnapshot(CGRectZero, base::BindRepeating(^(UIImage* image) {
                             callback_called = true;
                           }));
  EXPECT_TRUE(callback_called);
}

// Tests that setting a null overriding source reverts to the original WebState.
TEST_F(SnapshotSourceTabHelperTest, ResetOverridingSource) {
  web_state_->SetCanTakeSnapshot(true);
  SnapshotSourceTabHelper* tab_helper = GetTabHelper();
  ASSERT_TRUE(tab_helper);

  auto overriding_web_state = std::make_unique<web::FakeWebState>();
  tab_helper->SetOverridingSourceWebState(overriding_web_state.get());
  tab_helper->SetOverridingSourceWebState(nullptr);

  EXPECT_TRUE(tab_helper->CanTakeSnapshot());
  EXPECT_EQ(web_state_->GetView(), tab_helper->GetView());
}
