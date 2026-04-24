// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/fullscreen/public/fullscreen_metrics.h"
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
  void FullscreenDidTransition(FullscreenBrowserAgent* agent,
                               FullscreenTransition transition) override {
    did_transition_called_ = true;
    transition_ = transition;
  }
  void WillShutDown(FullscreenBrowserAgent* agent) override {
    will_shut_down_called_ = true;
    agent->RemoveObserver(this);
  }

  bool will_update_called_ = false;
  bool did_update_called_ = false;
  bool will_update_obscured_inset_range_called_ = false;
  bool did_update_obscured_inset_range_called_ = false;
  bool did_transition_called_ = false;
  bool will_shut_down_called_ = false;
  FullscreenTransition transition_ = FullscreenTransition::kEnterFullscreen;
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

  void WillUpdateState(FullscreenBrowserAgent* agent) override {
    CGFloat progress = edge_ == UIRectEdgeBottom ? agent->bottom_progress()
                                                 : agent->top_progress();
    CGFloat current = min_ + (max_ - min_) * progress;
    agent->AddObscuredInset(edge_, current);
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

  base::PassKey<FullscreenBrowserAgentTest> PassKey() {
    return base::PassKey<FullscreenBrowserAgentTest>();
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

  agent->InvalidateInsetRange();

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

// Tests that IncrementalScroll calculates progress correctly and clamps values.
TEST_F(FullscreenBrowserAgentTest, IncrementalScroll) {
  FullscreenBrowserAgent::CreateForBrowser(browser_.get());
  FullscreenBrowserAgent* agent =
      FullscreenBrowserAgent::FromBrowser(browser_.get());

  TestFullscreenBrowserAgentObserver base_observer;
  RangeTestFullscreenBrowserAgentObserver observer1(UIRectEdgeTop, 10.0, 50.0);
  RangeTestFullscreenBrowserAgentObserver observer2(UIRectEdgeBottom, 20.0,
                                                    80.0);

  agent->AddObserver(&base_observer);
  agent->AddObserver(&observer1);
  agent->AddObserver(&observer2);

  // Initialize ranges. Top delta = 40, Bottom delta = 60.
  agent->InvalidateInsetRange();

  EXPECT_EQ(1.0, agent->top_progress());
  EXPECT_EQ(1.0, agent->bottom_progress());
  EXPECT_TRUE(base_observer.will_update_called_);
  EXPECT_TRUE(base_observer.did_update_called_);

  // Reset observer flags.
  base_observer.will_update_called_ = false;
  base_observer.did_update_called_ = false;

  // Scroll down partially.
  agent->IncrementalScroll(20.0, PassKey());

  EXPECT_EQ(0.5, agent->top_progress());
  EXPECT_NEAR(0.6666, agent->bottom_progress(), 0.001);
  EXPECT_EQ(30.0, agent->insets().top);
  EXPECT_NEAR(60.0, agent->insets().bottom, 0.001);

  EXPECT_TRUE(base_observer.will_update_called_);
  EXPECT_TRUE(base_observer.did_update_called_);

  // Fast scroll down to check 0.0 bounds clamping.
  agent->IncrementalScroll(200.0, PassKey());

  EXPECT_EQ(0.0, agent->top_progress());
  EXPECT_EQ(0.0, agent->bottom_progress());
  EXPECT_EQ(10.0, agent->insets().top);
  EXPECT_EQ(20.0, agent->insets().bottom);

  // Fast scroll up to check 1.0 bounds clamping.
  agent->IncrementalScroll(-500.0, PassKey());

  EXPECT_EQ(1.0, agent->top_progress());
  EXPECT_EQ(1.0, agent->bottom_progress());
  EXPECT_EQ(50.0, agent->insets().top);
  EXPECT_EQ(80.0, agent->insets().bottom);

  agent->RemoveObserver(&base_observer);
  agent->RemoveObserver(&observer1);
  agent->RemoveObserver(&observer2);
}

// Tests that EnterFullscreen and ExitFullscreen correctly update progress.
TEST_F(FullscreenBrowserAgentTest, EnterExitFullscreen) {
  base::HistogramTester histogram_tester;
  FullscreenBrowserAgent::CreateForBrowser(browser_.get());
  FullscreenBrowserAgent* agent =
      FullscreenBrowserAgent::FromBrowser(browser_.get());

  TestFullscreenBrowserAgentObserver base_observer;
  agent->AddObserver(&base_observer);

  // Initialize ranges. Top delta = 40.
  RangeTestFullscreenBrowserAgentObserver observer1(UIRectEdgeTop, 10.0, 50.0);
  agent->AddObserver(&observer1);
  agent->InvalidateInsetRange();

  EXPECT_EQ(1.0, agent->top_progress());

  // Reset observer flags.
  base_observer.will_update_called_ = false;
  base_observer.did_update_called_ = false;

  // Enter Fullscreen.
  agent->EnterFullscreen(PassKey(),
                         FullscreenModeTransitionTrigger::kForcedByCode,
                         /*animated=*/false);
  EXPECT_EQ(0.0, agent->top_progress());
  EXPECT_EQ(10.0, agent->insets().top);
  EXPECT_TRUE(base_observer.will_update_called_);
  EXPECT_TRUE(base_observer.did_update_called_);
  histogram_tester.ExpectUniqueSample(
      kEnterFullscreenModeTransitionTriggerHistogram,
      FullscreenModeTransitionTrigger::kForcedByCode, 1);

  base_observer.will_update_called_ = false;
  base_observer.did_update_called_ = false;

  // Exit Fullscreen.
  agent->ExitFullscreen(PassKey(),
                        FullscreenModeTransitionTrigger::kForcedByCode,
                        /*animated=*/false);
  EXPECT_EQ(1.0, agent->top_progress());
  EXPECT_EQ(50.0, agent->insets().top);
  EXPECT_TRUE(base_observer.will_update_called_);
  EXPECT_TRUE(base_observer.did_update_called_);
  histogram_tester.ExpectUniqueSample(
      kExitFullscreenModeTransitionTriggerHistogram,
      FullscreenModeTransitionTrigger::kForcedByCode, 1);

  agent->RemoveObserver(&base_observer);
  agent->RemoveObserver(&observer1);
}

// Tests that the disabled counter increments and decrements correctly.
TEST_F(FullscreenBrowserAgentTest, DisabledCounter) {
  FullscreenBrowserAgent::CreateForBrowser(browser_.get());
  FullscreenBrowserAgent* agent =
      FullscreenBrowserAgent::FromBrowser(browser_.get());
  EXPECT_EQ(0u, agent->disabled_count());
  agent->IncrementDisabledCounter(PassKey(), true);
  EXPECT_EQ(1u, agent->disabled_count());
  agent->IncrementDisabledCounter(PassKey(), false);
  EXPECT_EQ(2u, agent->disabled_count());
  agent->DecrementDisabledCounter(PassKey());
  EXPECT_EQ(1u, agent->disabled_count());
  agent->DecrementDisabledCounter(PassKey());
  EXPECT_EQ(0u, agent->disabled_count());
  agent->DecrementDisabledCounter(PassKey());  // Should not go below zero.
  EXPECT_EQ(0u, agent->disabled_count());
}

// Tests that FullscreenDidTransition is called correctly.
TEST_F(FullscreenBrowserAgentTest, FullscreenDidTransition) {
  FullscreenBrowserAgent::CreateForBrowser(browser_.get());
  FullscreenBrowserAgent* agent =
      FullscreenBrowserAgent::FromBrowser(browser_.get());

  TestFullscreenBrowserAgentObserver observer;
  agent->AddObserver(&observer);

  // Enter Fullscreen (non-animated).
  agent->EnterFullscreen(PassKey(),
                         FullscreenModeTransitionTrigger::kForcedByCode,
                         /*animated=*/false);
  EXPECT_TRUE(observer.did_transition_called_);
  EXPECT_EQ(FullscreenTransition::kEnterFullscreen, observer.transition_);

  observer.did_transition_called_ = false;

  // Exit Fullscreen (non-animated).
  agent->ExitFullscreen(PassKey(),
                        FullscreenModeTransitionTrigger::kForcedByCode,
                        /*animated=*/false);
  EXPECT_TRUE(observer.did_transition_called_);
  EXPECT_EQ(FullscreenTransition::kExitFullscreen, observer.transition_);

  agent->RemoveObserver(&observer);
}

// Tests that IsEnabled() returns correct values.
TEST_F(FullscreenBrowserAgentTest, IsEnabled) {
  FullscreenBrowserAgent::CreateForBrowser(browser_.get());
  FullscreenBrowserAgent* agent =
      FullscreenBrowserAgent::FromBrowser(browser_.get());

  EXPECT_TRUE(agent->IsEnabled());

  // Disable fullscreen.
  agent->IncrementDisabledCounter(PassKey(), /*animated=*/false);
  EXPECT_FALSE(agent->IsEnabled());

  // Re-enable.
  agent->DecrementDisabledCounter(PassKey());
  EXPECT_TRUE(agent->IsEnabled());
}

// Tests that State() returns correct values.
TEST_F(FullscreenBrowserAgentTest, State) {
  FullscreenBrowserAgent::CreateForBrowser(browser_.get());
  FullscreenBrowserAgent* agent =
      FullscreenBrowserAgent::FromBrowser(browser_.get());

  EXPECT_EQ(FullscreenState::kUIExpanded, agent->State());

  // Enter Fullscreen.
  agent->EnterFullscreen(PassKey(),
                         FullscreenModeTransitionTrigger::kForcedByCode,
                         /*animated=*/false);
  EXPECT_EQ(FullscreenState::kUICollapsed, agent->State());

  // Disable fullscreen (which also exits fullscreen).
  agent->IncrementDisabledCounter(PassKey(), /*animated=*/false);
  EXPECT_EQ(FullscreenState::kUIExpanded, agent->State());
}

// Tests that WillShutDown is called correctly.
TEST_F(FullscreenBrowserAgentTest, WillShutDown) {
  FullscreenBrowserAgent::CreateForBrowser(browser_.get());
  FullscreenBrowserAgent* agent =
      FullscreenBrowserAgent::FromBrowser(browser_.get());

  TestFullscreenBrowserAgentObserver observer;
  agent->AddObserver(&observer);

  EXPECT_FALSE(observer.will_shut_down_called_);

  // Destroy the browser, which destroys the agent.
  browser_.reset();

  EXPECT_TRUE(observer.will_shut_down_called_);
}
