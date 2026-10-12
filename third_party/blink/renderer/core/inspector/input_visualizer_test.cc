// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/input_visualizer.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

class InputVisualizerTest : public RenderingTest {
 public:
  InputVisualizerTest()
      : RenderingTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    RenderingTest::SetUp();
    visualizer_ = MakeGarbageCollected<InputVisualizer>(GetFrame());
  }

  void TearDown() override {
    if (visualizer_) {
      visualizer_->Shutdown();
      visualizer_ = nullptr;
    }
    RenderingTest::TearDown();
  }

  size_t PaintAndGetDisplayItemCount(InputVisualizer& visualizer) {
    visualizer.ServiceScriptedAnimations(base::TimeTicks::Now());
    UpdateAllLifecyclePhasesForTest();
    visualizer.UpdatePrePaint();
    PaintController paint_controller;
    paint_controller.UpdateCurrentPaintChunkProperties(
        PropertyTreeState::Root());
    GraphicsContext context(paint_controller);
    visualizer.Paint(context);
    return paint_controller.CommitNewDisplayItems().GetDisplayItemList().size();
  }

  WebMouseEvent CreateMouseDownEvent(
      float x,
      float y,
      int modifiers = WebInputEvent::kNoModifiers) {
    return WebMouseEvent(WebInputEvent::Type::kMouseDown, gfx::PointF(x, y),
                         gfx::PointF(x, y), WebPointerProperties::Button::kLeft,
                         1, modifiers, base::TimeTicks::Now());
  }

  WebPointerEvent CreatePointerDownEvent(float x, float y) {
    return WebPointerEvent(
        WebInputEvent::Type::kPointerDown,
        WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                             WebPointerProperties::Button::kLeft,
                             gfx::PointF(x, y), gfx::PointF(x, y)),
        10.0f, 10.0f);
  }

 protected:
  Persistent<InputVisualizer> visualizer_;
};

TEST_F(InputVisualizerTest, DisabledByDefault) {
  EXPECT_FALSE(visualizer_->IsClicksEnabled());

  visualizer_->HandleInputEvent(CreatePointerDownEvent(100, 100));
  EXPECT_EQ(0u, PaintAndGetDisplayItemCount(*visualizer_));
}

TEST_F(InputVisualizerTest, EnableClicksAndCapturePointerDown) {
  visualizer_->SetClicksEnabled(true);
  EXPECT_TRUE(visualizer_->IsClicksEnabled());

  visualizer_->HandleInputEvent(CreatePointerDownEvent(150, 150));
  EXPECT_EQ(1u, PaintAndGetDisplayItemCount(*visualizer_));
}

TEST_F(InputVisualizerTest, CapturesMouseDownEvents) {
  visualizer_->SetClicksEnabled(true);

  visualizer_->HandleInputEvent(CreateMouseDownEvent(100, 100));
  EXPECT_EQ(1u, PaintAndGetDisplayItemCount(*visualizer_));
}

TEST_F(InputVisualizerTest, IgnoresCompatibilityMouseEventsFromTouch) {
  visualizer_->SetClicksEnabled(true);

  visualizer_->HandleInputEvent(CreateMouseDownEvent(
      100, 100, WebInputEvent::kIsCompatibilityEventForTouch));
  EXPECT_EQ(0u, PaintAndGetDisplayItemCount(*visualizer_));
}

TEST_F(InputVisualizerTest, DisableClicksCleansUp) {
  visualizer_->SetClicksEnabled(true);
  visualizer_->HandleInputEvent(CreatePointerDownEvent(100, 100));
  EXPECT_EQ(1u, PaintAndGetDisplayItemCount(*visualizer_));

  visualizer_->SetClicksEnabled(false);
  EXPECT_FALSE(visualizer_->IsClicksEnabled());
  EXPECT_EQ(0u, PaintAndGetDisplayItemCount(*visualizer_));
}

TEST_F(InputVisualizerTest, PrunesExpiredClicks) {
  visualizer_->SetClicksEnabled(true);
  visualizer_->HandleInputEvent(CreatePointerDownEvent(100, 100));
  EXPECT_EQ(1u, PaintAndGetDisplayItemCount(*visualizer_));

  FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(0u, PaintAndGetDisplayItemCount(*visualizer_));
}

TEST_F(InputVisualizerTest, LimitsMaxConcurrentAnimations) {
  visualizer_->SetClicksEnabled(true);

  for (wtf_size_t i = 0; i < InputVisualizer::kMaxConcurrentAnimations + 5;
       ++i) {
    visualizer_->HandleInputEvent(CreatePointerDownEvent(100 + i, 100));
  }
  EXPECT_EQ(InputVisualizer::kMaxConcurrentAnimations,
            visualizer_->ActiveClicksCountForTesting());
  EXPECT_EQ(1u, PaintAndGetDisplayItemCount(*visualizer_));
}

TEST_F(InputVisualizerTest, Shutdown) {
  visualizer_->SetClicksEnabled(true);
  visualizer_->HandleInputEvent(CreatePointerDownEvent(100, 100));
  visualizer_->Shutdown();
  EXPECT_EQ(0u, PaintAndGetDisplayItemCount(*visualizer_));
}

}  // namespace blink
