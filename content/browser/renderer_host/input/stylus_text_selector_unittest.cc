// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/stylus_text_selector.h"

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/motion_event_test_utils.h"
#include "ui/events/velocity_tracker/motion_event.h"
#include "ui/gfx/geometry/rect_f.h"

using ui::MotionEvent;
using ui::test::MockMotionEvent;

namespace content {

class StylusTextSelectorTest : public testing::Test,
                               public StylusTextSelectorClient {
 public:
  StylusTextSelectorTest() {}
  ~StylusTextSelectorTest() override {}

  // Test implementation.
  void SetUp() override {
    selector_ = std::make_unique<StylusTextSelector>(this);
    event_log_.clear();
  }

  void TearDown() override {
    selector_.reset();
    event_log_.clear();
  }

  // StylusTextSelectorClient implementation.
  void OnStylusSelectBegin(float x0, float y0, float x1, float y1) override {
    std::stringstream ss;
    ss << "Begin(" << x0 << ", " << y0 << ", " << x1 << ", " << y1 << ")";
    event_log_.push_back(ss.str());
  }

  void OnStylusSelectUpdate(float x, float y) override {
    std::stringstream ss;
    ss << "Update(" << x << ", " << y << ")";
    event_log_.push_back(ss.str());
  }

  void OnStylusSelectEnd(float x, float y) override {
    event_log_.push_back("End");
  }

  void OnStylusSelectTap(base::TimeTicks time, float x, float y) override {
    event_log_.push_back("Tap");
  }

 protected:
  std::unique_ptr<StylusTextSelector> selector_;
  std::vector<std::string> event_log_;
};

TEST_F(StylusTextSelectorTest, ShouldStartTextSelection) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  {  // Touched with a finger.
    MockMotionEvent e(MotionEvent::Action::DOWN, event_time, 50.0f, 50.0f);
    e.SetToolType(0, MotionEvent::ToolType::FINGER);
    e.set_button_state(0);
    EXPECT_FALSE(selector_->ShouldStartTextSelection(e));
  }

  {  // Touched with a stylus, but no button pressed.
    MockMotionEvent e(MotionEvent::Action::DOWN, event_time, 50.0f, 50.0f);
    e.SetToolType(0, MotionEvent::ToolType::STYLUS);
    e.set_button_state(0);
    EXPECT_FALSE(selector_->ShouldStartTextSelection(e));
  }

  {  // Touched with a stylus, with first button (BUTTON_SECONDARY) pressed.
     // For Android version < M, this stylus state is BUTTON_SECONDARY.
    MockMotionEvent e(MotionEvent::Action::DOWN, event_time, 50.0f, 50.0f);
    e.SetToolType(0, MotionEvent::ToolType::STYLUS);
    e.set_button_state(MotionEvent::BUTTON_SECONDARY);
    EXPECT_TRUE(selector_->ShouldStartTextSelection(e));
  }

  {  // Touched with a stylus, with first button (BUTTON_STYLUS_PRIMARY)
     // pressed. From Android M, this stylus state has been changed to
     // BUTTON_STYLUS_PRIMARY.
    MockMotionEvent e(MotionEvent::Action::DOWN, event_time, 50.0f, 50.0f);
    e.SetToolType(0, MotionEvent::ToolType::STYLUS);
    e.set_button_state(MotionEvent::BUTTON_STYLUS_PRIMARY);
    EXPECT_TRUE(selector_->ShouldStartTextSelection(e));
  }

  {  // Touched with a stylus, with two buttons pressed.
     // For Android version < M, these states are BUTTON_SECONDARY,
     // BUTTON_TERTIARY.
    MockMotionEvent e(MotionEvent::Action::DOWN, event_time, 50.0f, 50.0f);
    e.SetToolType(0, MotionEvent::ToolType::STYLUS);
    e.set_button_state(MotionEvent::BUTTON_SECONDARY |
                       MotionEvent::BUTTON_TERTIARY);
    EXPECT_FALSE(selector_->ShouldStartTextSelection(e));
  }

  {  // Touched with a stylus, with two buttons pressed.
     // From Android M, these state are BUTTON_STYLUS_PRIMARY,
     // BUTTON_STYLUS_SECONDARY.
    MockMotionEvent e(MotionEvent::Action::DOWN, event_time, 50.0f, 50.0f);
    e.SetToolType(0, MotionEvent::ToolType::STYLUS);
    e.set_button_state(MotionEvent::BUTTON_STYLUS_PRIMARY |
                       MotionEvent::BUTTON_STYLUS_SECONDARY);
    EXPECT_FALSE(selector_->ShouldStartTextSelection(e));
  }
}

TEST_F(StylusTextSelectorTest, FingerTouch) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  const float x = 50.0f;
  const float y = 30.0f;
  // 1. Touched with a finger: ignored
  MockMotionEvent finger(MotionEvent::Action::DOWN, event_time, x, y);
  finger.SetToolType(0, MotionEvent::ToolType::FINGER);
  EXPECT_FALSE(selector_->OnTouchEvent(finger));
  // We do not consume finger events.
  EXPECT_TRUE(event_log_.empty());
}

// The following Tests cover BUTTON_SECONDARY case, which is the stylus button
// pressed state for Android version < M.
TEST_F(StylusTextSelectorTest, PenDraggingButtonSecondary) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  const float x1 = 50.0f;
  const float y1 = 30.0f;
  const float x2 = 100.0f;
  const float y2 = 90.0f;
  const float x3 = 150.0f;
  const float y3 = 150.0f;
  // 1. Action::DOWN with stylus + button
  event_time += base::Milliseconds(10);
  MockMotionEvent action_down(MotionEvent::Action::DOWN, event_time, x1, y1);
  action_down.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_down.set_button_state(MotionEvent::BUTTON_SECONDARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_down));
  EXPECT_TRUE(event_log_.empty());

  // 2. Action::MOVE
  event_time += base::Milliseconds(10);
  MockMotionEvent action_move(MotionEvent::Action::MOVE, event_time, x2, y2);
  action_move.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_move.set_button_state(MotionEvent::BUTTON_SECONDARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  ASSERT_EQ(1u, event_log_.size());
  EXPECT_STREQ("Begin(50, 30, 100, 90)", event_log_.back().c_str());

  event_time += base::Milliseconds(10);
  action_move = MockMotionEvent(MotionEvent::Action::MOVE, event_time, x3, y3);
  action_move.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_move.set_button_state(MotionEvent::BUTTON_SECONDARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  ASSERT_EQ(2u, event_log_.size());
  EXPECT_STREQ("Update(150, 150)", event_log_.back().c_str());

  // 3. Action::UP
  event_time += base::Milliseconds(10);
  MockMotionEvent action_up(MotionEvent::Action::UP, event_time, x3, y3);
  action_up.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_up.set_button_state(0);
  EXPECT_TRUE(selector_->OnTouchEvent(action_up));
  ASSERT_EQ(3u, event_log_.size());  // NO CHANGE
  EXPECT_STREQ("End", event_log_.back().c_str());
}

TEST_F(StylusTextSelectorTest, PenDraggingButtonSecondaryNotPressed) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  float x = 50.0f;
  float y = 30.0f;

  // 1. Action::DOWN with stylus + button
  event_time += base::Milliseconds(10);
  MockMotionEvent action_down(MotionEvent::Action::DOWN, event_time, x, y);
  action_down.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_down.set_button_state(MotionEvent::BUTTON_SECONDARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_down));
  EXPECT_TRUE(event_log_.empty());

  // 2. Action::MOVE
  event_time += base::Milliseconds(10);
  x += 20;  // 70
  y += 20;  // 50
  MockMotionEvent action_move(MotionEvent::Action::MOVE, event_time, x, y);
  action_move.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_move.set_button_state(MotionEvent::BUTTON_SECONDARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  ASSERT_EQ(1u, event_log_.size());
  EXPECT_STREQ("Begin(50, 30, 70, 50)", event_log_.back().c_str());

  // 3. Action::MOVE with stylus + no button
  event_time += base::Milliseconds(10);
  x += 20;  // 90
  y += 20;  // 70
  action_move = MockMotionEvent(MotionEvent::Action::MOVE, event_time, x, y);
  action_move.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_move.set_button_state(0);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  EXPECT_EQ(1u, event_log_.size());  // NO CHANGE

  // 4. Action::MOVE with stylus + button pressed again
  //    Note that the end action is deferred until the stylus is lifted.
  event_time += base::Milliseconds(10);
  x += 20;  // 110
  y += 20;  // 90
  action_move = MockMotionEvent(MotionEvent::Action::MOVE, event_time, x, y);
  action_move.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_move.set_button_state(MotionEvent::BUTTON_SECONDARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  EXPECT_EQ(2u, event_log_.size());
  EXPECT_STREQ("Begin(90, 70, 110, 90)", event_log_.back().c_str());

  // 5. Action::MOVE with stylus + no button
  event_time += base::Milliseconds(10);
  x += 20;  // 130
  y += 20;  // 110
  action_move = MockMotionEvent(MotionEvent::Action::MOVE, event_time, x, y);
  action_move.SetToolType(0, MotionEvent::ToolType::STYLUS);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  EXPECT_EQ(2u, event_log_.size());  // NO CHANGE

  // 5. Action::UP
  event_time += base::Milliseconds(10);
  MockMotionEvent action_up(MotionEvent::Action::UP, event_time, x, y);
  action_up.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_up.set_button_state(0);
  EXPECT_TRUE(selector_->OnTouchEvent(action_up));
  EXPECT_EQ(3u, event_log_.size());
  EXPECT_STREQ("End", event_log_.back().c_str());
}

TEST_F(StylusTextSelectorTest, TapTriggersLongPressSelection) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  const float x1 = 50.0f;
  const float y1 = 30.0f;
  const float x2 = 51.0f;
  const float y2 = 31.0f;
  // 1. Action::DOWN with stylus + button
  event_time += base::Milliseconds(1);
  MockMotionEvent action_down(MotionEvent::Action::DOWN, event_time, x1, y1);
  action_down.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_down.set_button_state(MotionEvent::BUTTON_SECONDARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_down));
  EXPECT_TRUE(event_log_.empty());

  // 2. Action::MOVE
  event_time += base::Milliseconds(1);
  MockMotionEvent action_move(MotionEvent::Action::MOVE, event_time, x2, y2);
  action_move.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_move.set_button_state(MotionEvent::BUTTON_SECONDARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  EXPECT_TRUE(event_log_.empty());

  // 3. Action::UP
  event_time += base::Milliseconds(1);
  MockMotionEvent action_up(MotionEvent::Action::UP, event_time, x2, y2);
  action_up.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_up.set_button_state(0);
  EXPECT_TRUE(selector_->OnTouchEvent(action_up));
  ASSERT_EQ(1u, event_log_.size());
  EXPECT_STREQ("Tap", event_log_.back().c_str());
}
// End of Tests for BUTTON_SECONDARY case.

// The following Tests cover BUTTON_STYLUS_PRIMARY case, which is the stylus
// button pressed state from Android M.
TEST_F(StylusTextSelectorTest, PenDraggingButtonStylusPrimary) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  const float x1 = 50.0f;
  const float y1 = 30.0f;
  const float x2 = 100.0f;
  const float y2 = 90.0f;
  const float x3 = 150.0f;
  const float y3 = 150.0f;
  // 1. Action::DOWN with stylus + button
  event_time += base::Milliseconds(10);
  MockMotionEvent action_down(MotionEvent::Action::DOWN, event_time, x1, y1);
  action_down.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_down.set_button_state(MotionEvent::BUTTON_STYLUS_PRIMARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_down));
  EXPECT_TRUE(event_log_.empty());

  // 2. Action::MOVE
  event_time += base::Milliseconds(10);
  MockMotionEvent action_move(MotionEvent::Action::MOVE, event_time, x2, y2);
  action_move.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_move.set_button_state(MotionEvent::BUTTON_STYLUS_PRIMARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  ASSERT_EQ(1u, event_log_.size());
  EXPECT_STREQ("Begin(50, 30, 100, 90)", event_log_.back().c_str());

  event_time += base::Milliseconds(10);
  action_move = MockMotionEvent(MotionEvent::Action::MOVE, event_time, x3, y3);
  action_move.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_move.set_button_state(MotionEvent::BUTTON_STYLUS_PRIMARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  ASSERT_EQ(2u, event_log_.size());
  EXPECT_STREQ("Update(150, 150)", event_log_.back().c_str());

  // 3. Action::UP
  event_time += base::Milliseconds(10);
  MockMotionEvent action_up(MotionEvent::Action::UP, event_time, x3, y3);
  action_up.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_up.set_button_state(0);
  EXPECT_TRUE(selector_->OnTouchEvent(action_up));
  ASSERT_EQ(3u, event_log_.size());  // NO CHANGE
  EXPECT_STREQ("End", event_log_.back().c_str());
}

TEST_F(StylusTextSelectorTest, PenDraggingButtonStylusPrimaryNotPressed) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  float x = 50.0f;
  float y = 30.0f;

  // 1. Action::DOWN with stylus + button
  event_time += base::Milliseconds(10);
  MockMotionEvent action_down(MotionEvent::Action::DOWN, event_time, x, y);
  action_down.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_down.set_button_state(MotionEvent::BUTTON_STYLUS_PRIMARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_down));
  EXPECT_TRUE(event_log_.empty());

  // 2. Action::MOVE
  event_time += base::Milliseconds(10);
  x += 20;  // 70
  y += 20;  // 50
  MockMotionEvent action_move(MotionEvent::Action::MOVE, event_time, x, y);
  action_move.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_move.set_button_state(MotionEvent::BUTTON_STYLUS_PRIMARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  ASSERT_EQ(1u, event_log_.size());
  EXPECT_STREQ("Begin(50, 30, 70, 50)", event_log_.back().c_str());

  // 3. Action::MOVE with stylus + no button
  event_time += base::Milliseconds(10);
  x += 20;  // 90
  y += 20;  // 70
  action_move = MockMotionEvent(MotionEvent::Action::MOVE, event_time, x, y);
  action_move.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_move.set_button_state(0);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  EXPECT_EQ(1u, event_log_.size());  // NO CHANGE

  // 4. Action::MOVE with stylus + button pressed again
  //    Note that the end action is deferred until the stylus is lifted.
  event_time += base::Milliseconds(10);
  x += 20;  // 110
  y += 20;  // 90
  action_move = MockMotionEvent(MotionEvent::Action::MOVE, event_time, x, y);
  action_move.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_move.set_button_state(MotionEvent::BUTTON_STYLUS_PRIMARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  EXPECT_EQ(2u, event_log_.size());
  EXPECT_STREQ("Begin(90, 70, 110, 90)", event_log_.back().c_str());

  // 5. Action::MOVE with stylus + no button
  event_time += base::Milliseconds(10);
  x += 20;  // 130
  y += 20;  // 110
  action_move = MockMotionEvent(MotionEvent::Action::MOVE, event_time, x, y);
  action_move.SetToolType(0, MotionEvent::ToolType::STYLUS);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  EXPECT_EQ(2u, event_log_.size());  // NO CHANGE

  // 5. Action::UP
  event_time += base::Milliseconds(10);
  MockMotionEvent action_up(MotionEvent::Action::UP, event_time, x, y);
  action_up.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_up.set_button_state(0);
  EXPECT_TRUE(selector_->OnTouchEvent(action_up));
  EXPECT_EQ(3u, event_log_.size());
  EXPECT_STREQ("End", event_log_.back().c_str());
}

TEST_F(StylusTextSelectorTest, TapTriggersLongPressSelection2) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  const float x1 = 50.0f;
  const float y1 = 30.0f;
  const float x2 = 51.0f;
  const float y2 = 31.0f;
  // 1. Action::DOWN with stylus + button
  event_time += base::Milliseconds(1);
  MockMotionEvent action_down(MotionEvent::Action::DOWN, event_time, x1, y1);
  action_down.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_down.set_button_state(MotionEvent::BUTTON_STYLUS_PRIMARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_down));
  EXPECT_TRUE(event_log_.empty());

  // 2. Action::MOVE
  event_time += base::Milliseconds(1);
  MockMotionEvent action_move(MotionEvent::Action::MOVE, event_time, x2, y2);
  action_move.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_move.set_button_state(MotionEvent::BUTTON_STYLUS_PRIMARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  EXPECT_TRUE(event_log_.empty());

  // 3. Action::UP
  event_time += base::Milliseconds(1);
  MockMotionEvent action_up(MotionEvent::Action::UP, event_time, x2, y2);
  action_up.SetToolType(0, MotionEvent::ToolType::STYLUS);
  action_up.set_button_state(0);
  EXPECT_TRUE(selector_->OnTouchEvent(action_up));
  ASSERT_EQ(1u, event_log_.size());
  EXPECT_STREQ("Tap", event_log_.back().c_str());
}
// End of tests for BUTTON_STLUS_PRIMARY case.

}  // namespace content
