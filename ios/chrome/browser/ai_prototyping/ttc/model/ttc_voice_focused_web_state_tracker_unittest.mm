// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_voice_focused_web_state_tracker.h"

#import <Foundation/Foundation.h>

#import <memory>
#import <utility>
#import <vector>

#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

// Records every active `WebState` change reported by
// `TtcVoiceFocusedWebStateTracker`.
class RecordingTrackerObserver
    : public TtcVoiceFocusedWebStateTracker::Observer {
 public:
  explicit RecordingTrackerObserver(TtcVoiceFocusedWebStateTracker& tracker) {
    observation_.Observe(&tracker);
  }
  ~RecordingTrackerObserver() override = default;

  void OnVoiceFocusedWebStateChanged(web::WebState* web_state) override {
    changes_.push_back(web_state);
  }

  // Returns and clears the recorded active `WebState` changes.
  std::vector<web::WebState*> TakeChanges() {
    return std::exchange(changes_, {});
  }

 private:
  std::vector<web::WebState*> changes_;
  base::ScopedObservation<TtcVoiceFocusedWebStateTracker,
                          TtcVoiceFocusedWebStateTracker::Observer>
      observation_{this};
};

}  // namespace

// Test fixture for `TtcVoiceFocusedWebStateTracker`.
class TtcVoiceFocusedWebStateTrackerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    browser_list_ = BrowserListFactory::GetForProfile(profile_.get());
    first_scene_state_ = [[SceneState alloc] init];
    first_scene_state_.activationLevel = SceneActivationLevelForegroundActive;
    first_browser_ =
        std::make_unique<TestBrowser>(profile_.get(), first_scene_state_);
    browser_list_->AddBrowser(first_browser_.get());
  }

  void TearDown() override {
    if (first_browser_) {
      browser_list_->RemoveBrowser(first_browser_.get());
      first_browser_.reset();
    }
    first_scene_state_ = nil;
    browser_list_ = nullptr;
    profile_.reset();
    PlatformTest::TearDown();
  }

  // Inserts and activates a fake tab in `browser`, returning a non-owning
  // pointer to it.
  web::FakeWebState* InsertActiveTab(Browser* browser) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web::FakeWebState* ptr = web_state.get();
    browser->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    return ptr;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<BrowserList> browser_list_ = nullptr;
  SceneState* first_scene_state_ = nil;
  std::unique_ptr<TestBrowser> first_browser_;
};

// Tests that the tracker synchronously binds to the initial active tab on
// construction.
TEST_F(TtcVoiceFocusedWebStateTrackerTest, InitialActiveTab) {
  web::FakeWebState* tab = InsertActiveTab(first_browser_.get());
  TtcVoiceFocusedWebStateTracker tracker(browser_list_);
  RecordingTrackerObserver observer(tracker);

  EXPECT_EQ(tracker.GetActiveWebState(), tab);
  EXPECT_THAT(observer.TakeChanges(), IsEmpty());
}

// Tests that switching tabs within the bound browser updates the active tab and
// notifies observers.
TEST_F(TtcVoiceFocusedWebStateTrackerTest, TabSwitchInBoundBrowser) {
  web::FakeWebState* first_tab = InsertActiveTab(first_browser_.get());
  TtcVoiceFocusedWebStateTracker tracker(browser_list_);
  RecordingTrackerObserver observer(tracker);

  // Inserting an inactive tab should not change the active tab or notify
  // observers.
  first_browser_->GetWebStateList()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic());
  EXPECT_EQ(tracker.GetActiveWebState(), first_tab);
  EXPECT_THAT(observer.TakeChanges(), IsEmpty());

  web::FakeWebState* second_tab = InsertActiveTab(first_browser_.get());
  EXPECT_EQ(tracker.GetActiveWebState(), second_tab);
  EXPECT_THAT(observer.TakeChanges(), ElementsAre(second_tab));

  first_browser_->GetWebStateList()->ActivateWebStateAt(0);
  EXPECT_EQ(tracker.GetActiveWebState(), first_tab);
  EXPECT_THAT(observer.TakeChanges(), ElementsAre(first_tab));
}

// Tests that closing the active tab in the bound browser moves to the remaining
// tab and then to `nullptr` when all tabs close.
TEST_F(TtcVoiceFocusedWebStateTrackerTest, CloseActiveTab) {
  web::FakeWebState* first_tab = InsertActiveTab(first_browser_.get());
  InsertActiveTab(first_browser_.get());
  TtcVoiceFocusedWebStateTracker tracker(browser_list_);
  RecordingTrackerObserver observer(tracker);

  WebStateList* web_state_list = first_browser_->GetWebStateList();
  web_state_list->CloseWebStateAt(web_state_list->active_index(),
                                  WebStateList::ClosingReason::kDefault);
  EXPECT_EQ(tracker.GetActiveWebState(), first_tab);
  EXPECT_THAT(observer.TakeChanges(), ElementsAre(first_tab));

  web_state_list->CloseWebStateAt(0, WebStateList::ClosingReason::kDefault);
  EXPECT_EQ(tracker.GetActiveWebState(), nullptr);
  EXPECT_THAT(observer.TakeChanges(), ElementsAre(nullptr));
}

// Tests that `Disconnect()` clears the tracked `WebState` and stops notifying
// observers on subsequent tab changes, and that a `nullptr` `BrowserList` is
// safe.
TEST_F(TtcVoiceFocusedWebStateTrackerTest, DisconnectStopsObservation) {
  TtcVoiceFocusedWebStateTracker null_tracker(nullptr);
  EXPECT_EQ(null_tracker.GetActiveWebState(), nullptr);

  web::FakeWebState* first_tab = InsertActiveTab(first_browser_.get());
  TtcVoiceFocusedWebStateTracker tracker(browser_list_);
  RecordingTrackerObserver observer(tracker);
  EXPECT_EQ(tracker.GetActiveWebState(), first_tab);

  tracker.Disconnect();
  EXPECT_EQ(tracker.GetActiveWebState(), nullptr);

  InsertActiveTab(first_browser_.get());
  EXPECT_THAT(observer.TakeChanges(), IsEmpty());
}
