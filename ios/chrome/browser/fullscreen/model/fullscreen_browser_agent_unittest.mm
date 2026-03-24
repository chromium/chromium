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
  void DidUpdateObscuredInsetRange(FullscreenBrowserAgent* agent) override {
    did_update_obscured_inset_range_called_ = true;
  }

  bool will_update_called_ = false;
  bool did_update_called_ = false;
  bool will_update_obscured_inset_range_called_ = false;
  bool did_update_obscured_inset_range_called_ = false;
};

// An observer that adds a specific obscured inset range when requested.
class RangeTestFullscreenBrowserAgentObserver
    : public FullscreenBrowserAgentObserver {
 public:
  RangeTestFullscreenBrowserAgentObserver(UIRectEdge edge,
                                          CGFloat min,
                                          CGFloat max)
      : edge_(edge), min_(min), max_(max) {}

  void WillUpdateObscuredInsetRange(FullscreenBrowserAgent* agent) override {
    agent->AddObscuredInsetRange(edge_, min_, max_);
  }

 private:
  UIRectEdge edge_;
  CGFloat min_;
  CGFloat max_;
};

// Test fixture for testing FullscreenBrowserAgent class.
class FullscreenBrowserAgentTest : public PlatformTest {
 protected:
  FullscreenBrowserAgentTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
  }

  void InvalidateInsetRange(FullscreenBrowserAgent* agent) {
    agent->InvalidateInsetRange();
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

// Tests the cycle of invalidating inset ranges and calculating the new min and
// max insets.
TEST_F(FullscreenBrowserAgentTest, InvalidateInsetRange) {
  FullscreenBrowserAgent::CreateForBrowser(browser_.get());
  FullscreenBrowserAgent* agent =
      FullscreenBrowserAgent::FromBrowser(browser_.get());

  TestFullscreenBrowserAgentObserver base_observer;
  RangeTestFullscreenBrowserAgentObserver observer1(UIRectEdgeTop, 10.0, 50.0);
  RangeTestFullscreenBrowserAgentObserver observer2(UIRectEdgeTop, 5.0, 15.0);
  RangeTestFullscreenBrowserAgentObserver observer3(UIRectEdgeBottom, 20.0,
                                                    80.0);

  agent->AddObserver(&base_observer);
  agent->AddObserver(&observer1);
  agent->AddObserver(&observer2);
  agent->AddObserver(&observer3);

  InvalidateInsetRange(agent);

  EXPECT_TRUE(base_observer.will_update_obscured_inset_range_called_);
  EXPECT_TRUE(base_observer.did_update_obscured_inset_range_called_);

  EXPECT_EQ(15.0, agent->min_insets().top);
  EXPECT_EQ(65.0, agent->max_insets().top);
  EXPECT_EQ(20.0, agent->min_insets().bottom);
  EXPECT_EQ(80.0, agent->max_insets().bottom);

  agent->RemoveObserver(&base_observer);
  agent->RemoveObserver(&observer1);
  agent->RemoveObserver(&observer2);
  agent->RemoveObserver(&observer3);
}
