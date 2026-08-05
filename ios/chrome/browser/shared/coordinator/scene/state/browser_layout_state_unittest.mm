// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/state/browser_layout_state.h"

#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state_test_passkey_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/scene_layout_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

using layout_state::LayoutStateTestPassKeyFactory;

class BrowserLayoutStateTest : public PlatformTest {
 public:
  BrowserLayoutStateTest() {
    profile_ = TestProfileIOS::Builder().Build();
    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    layout_state_ = browser_->GetBrowserLayoutState();
  }

  void TearDown() override {
    layout_state_ = nil;
    [scene_state_ shutdown];
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeSceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  __weak BrowserLayoutState* layout_state_ = nil;
};

// Test that toolbarPosition defaults to top and updates observers on change.
TEST_F(BrowserLayoutStateTest, UpdatesToolbarPositionAndNotifiesObservers) {
  EXPECT_EQ(layout_state_.toolbarPosition, ToolbarPosition::kTop);

  id mock_observer = OCMProtocolMock(@protocol(BrowserLayoutStateObserver));
  [layout_state_ addObserver:mock_observer];

  OCMExpect([mock_observer browserLayoutState:layout_state_
                     didChangeToolbarPosition:ToolbarPosition::kBottom]);

  [layout_state_
      setToolbarPosition:ToolbarPosition::kBottom
                 passKey:LayoutStateTestPassKeyFactory::CreateToolbarKey()];

  EXPECT_EQ(layout_state_.toolbarPosition, ToolbarPosition::kBottom);
  [mock_observer verify];

  [layout_state_ removeObserver:mock_observer];
}

}  // namespace
