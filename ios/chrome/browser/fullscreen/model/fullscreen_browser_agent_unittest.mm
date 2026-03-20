// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/platform_test.h"

// A test FullscreenBrowserAgentObserver.
class TestFullscreenBrowserAgentObserver
    : public FullscreenBrowserAgentObserver {
 public:
  void WillUpdateState(FullscreenBrowserAgent* agent) override {
    will_update_called_ = true;
  }
  void DidUpdateState(FullscreenBrowserAgent* agent) override {
    did_update_called_ = true;
  }
  void WillUpdateObscuredInsetRange(FullscreenBrowserAgent* agent) override {
    will_update_obscured_inset_range_called_ = true;
  }

  bool will_update_called_ = false;
  bool did_update_called_ = false;
  bool will_update_obscured_inset_range_called_ = false;
};

// Test fixture for testing FullscreenBrowserAgent class.
class FullscreenBrowserAgentTest : public PlatformTest {
 protected:
  FullscreenBrowserAgentTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests that the FullscreenBrowserAgent can be created.
TEST_F(FullscreenBrowserAgentTest, CreateForBrowser) {
  EXPECT_EQ(nullptr, FullscreenBrowserAgent::FromBrowser(browser_.get()));
  FullscreenBrowserAgent::CreateForBrowser(browser_.get());
  EXPECT_NE(nullptr, FullscreenBrowserAgent::FromBrowser(browser_.get()));
}

// Tests that the FullscreenBrowserAgent can add and remove observers.
TEST_F(FullscreenBrowserAgentTest, Observers) {
  FullscreenBrowserAgent::CreateForBrowser(browser_.get());
  FullscreenBrowserAgent* agent =
      FullscreenBrowserAgent::FromBrowser(browser_.get());

  TestFullscreenBrowserAgentObserver observer;
  agent->AddObserver(&observer);
  agent->RemoveObserver(&observer);
}
