/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Tests for the ScrollAnimator class.

#include "third_party/blink/renderer/core/scroll/scroll_animator.h"

#include "base/single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

using testing::AtLeast;
using testing::Return;
using testing::_;

static double g_mocked_time = 0.0;

static double GetMockedTime() {
  return g_mocked_time;
}

namespace {

class MockScrollableAreaForAnimatorTest
    : public GarbageCollectedFinalized<MockScrollableAreaForAnimatorTest>,
      public ScrollableArea {
  USING_GARBAGE_COLLECTED_MIXIN(MockScrollableAreaForAnimatorTest);

 public:
  static MockScrollableAreaForAnimatorTest* Create(
      bool scroll_animator_enabled,
      const ScrollOffset& min_offset,
      const ScrollOffset& max_offset) {
    return new MockScrollableAreaForAnimatorTest(scroll_animator_enabled,
                                                 min_offset, max_offset);
  }

  MOCK_CONST_METHOD0(VisualRectForScrollbarParts, LayoutRect());
  MOCK_CONST_METHOD0(IsActive, bool());
  MOCK_CONST_METHOD0(IsThrottled, bool());
  MOCK_CONST_METHOD1(ScrollSize, int(ScrollbarOrientation));
  MOCK_CONST_METHOD0(IsScrollCornerVisible, bool());
  MOCK_CONST_METHOD0(ScrollCornerRect, IntRect());
  MOCK_METHOD2(UpdateScrollOffset, void(const ScrollOffset&, ScrollType));
  MOCK_METHOD0(ScrollControlWasSetNeedsPaintInvalidation, void());
  MOCK_CONST_METHOD0(EnclosingScrollableArea, ScrollableArea*());
  MOCK_CONST_METHOD1(VisibleContentRect, IntRect(IncludeScrollbarsInRect));
  MOCK_CONST_METHOD0(ContentsSize, IntSize());
  MOCK_CONST_METHOD0(ScrollbarsCanBeActive, bool());
  MOCK_CONST_METHOD0(ScrollableAreaBoundingBox, IntRect());
  MOCK_METHOD0(RegisterForAnimation, void());
  MOCK_METHOD0(ScheduleAnimation, bool());

  bool UserInputScrollable(ScrollbarOrientation) const override { return true; }
  bool ShouldPlaceVerticalScrollbarOnLeft() const override { return false; }
  IntSize ScrollOffsetInt() const override { return IntSize(); }
  int VisibleHeight() const override { return 768; }
  int VisibleWidth() const override { return 1024; }
  CompositorElementId GetCompositorElementId() const override {
    return CompositorElementId();
  }
  bool ScrollAnimatorEnabled() const override {
    return scroll_animator_enabled_;
  }
  int PageStep(ScrollbarOrientation) const override { return 0; }
  IntSize MinimumScrollOffsetInt() const override {
    return FlooredIntSize(min_offset_);
  }
  IntSize MaximumScrollOffsetInt() const override {
    return FlooredIntSize(max_offset_);
  }

  void SetScrollAnimator(ScrollAnimator* scroll_animator) {
    animator = scroll_animator;
  }

  ScrollOffset GetScrollOffset() const override {
    if (animator)
      return animator->CurrentOffset();
    return ScrollableArea::GetScrollOffset();
  }

  void SetScrollOffset(
      const ScrollOffset& offset,
      ScrollType type,
      ScrollBehavior behavior = kScrollBehaviorInstant) override {
    if (animator)
      animator->SetCurrentOffset(offset);
    ScrollableArea::SetScrollOffset(offset, type, behavior);
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetTimerTaskRunner() const final {
    if (!timer_task_runner_) {
      timer_task_runner_ =
          blink::scheduler::GetSingleThreadTaskRunnerForTesting();
    }
    return timer_task_runner_;
  }

  ScrollbarTheme& GetPageScrollbarTheme() const override {
    return ScrollbarTheme::DeprecatedStaticGetTheme();
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(animator);
    ScrollableArea::Trace(visitor);
  }

 private:
  explicit MockScrollableAreaForAnimatorTest(bool scroll_animator_enabled,
                                             const ScrollOffset& min_offset,
                                             const ScrollOffset& max_offset)
      : scroll_animator_enabled_(scroll_animator_enabled),
        min_offset_(min_offset),
        max_offset_(max_offset) {}

  bool scroll_animator_enabled_;
  ScrollOffset min_offset_;
  ScrollOffset max_offset_;
  Member<ScrollAnimator> animator;
  mutable scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner_;
};

class TestScrollAnimator : public ScrollAnimator {
 public:
  TestScrollAnimator(ScrollableArea* scrollable_area,
                     WTF::TimeFunction timing_function)
      : ScrollAnimator(scrollable_area, timing_function) {}
  ~TestScrollAnimator() override = default;

  void SetShouldSendToCompositor(bool send) {
    should_send_to_compositor_ = send;
  }

  bool SendAnimationToCompositor() override {
    if (should_send_to_compositor_) {
      run_state_ =
          ScrollAnimatorCompositorCoordinator::RunState::kRunningOnCompositor;
      compositor_animation_id_ = 1;
      return true;
    }
    return false;
  }

 protected:
  void AbortAnimation() override {}

 private:
  bool should_send_to_compositor_ = false;
};

}  // namespace

static void Reset(ScrollAnimator& scroll_animator) {
  scroll_animator.ScrollToOffsetWithoutAnimation(ScrollOffset());
}

// TODO(skobes): Add unit tests for composited scrolling paths.

TEST(ScrollAnimatorTest, MainThreadStates) {
  MockScrollableAreaForAnimatorTest* scrollable_area =
      MockScrollableAreaForAnimatorTest::Create(true, ScrollOffset(),
                                                ScrollOffset(1000, 1000));
  ScrollAnimator* scroll_animator =
      new ScrollAnimator(scrollable_area, GetMockedTime);

  EXPECT_CALL(*scrollable_area, UpdateScrollOffset(_, _)).Times(2);
  // Once from userScroll, once from updateCompositorAnimations.
  EXPECT_CALL(*scrollable_area, RegisterForAnimation()).Times(2);
  EXPECT_CALL(*scrollable_area, ScheduleAnimation())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  // Idle
  EXPECT_FALSE(scroll_animator->HasAnimationThatRequiresService());
  EXPECT_EQ(scroll_animator->run_state_,
            ScrollAnimatorCompositorCoordinator::RunState::kIdle);

  // WaitingToSendToCompositor
  scroll_animator->UserScroll(kScrollByLine, FloatSize(10, 0));
  EXPECT_EQ(scroll_animator->run_state_,
            ScrollAnimatorCompositorCoordinator::RunState::
                kWaitingToSendToCompositor);

  // RunningOnMainThread
  g_mocked_time += 0.05;
  scroll_animator->UpdateCompositorAnimations();
  EXPECT_EQ(
      scroll_animator->run_state_,
      ScrollAnimatorCompositorCoordinator::RunState::kRunningOnMainThread);
  scroll_animator->TickAnimation(GetMockedTime());
  EXPECT_EQ(
      scroll_animator->run_state_,
      ScrollAnimatorCompositorCoordinator::RunState::kRunningOnMainThread);

  // PostAnimationCleanup
  scroll_animator->CancelAnimation();
  EXPECT_EQ(
      scroll_animator->run_state_,
      ScrollAnimatorCompositorCoordinator::RunState::kPostAnimationCleanup);

  // Idle
  scroll_animator->UpdateCompositorAnimations();
  scroll_animator->TickAnimation(GetMockedTime());
  EXPECT_EQ(scroll_animator->run_state_,
            ScrollAnimatorCompositorCoordinator::RunState::kIdle);

  Reset(*scroll_animator);

  // Forced GC in order to finalize objects depending on the mock object.
  ThreadState::Current()->CollectAllGarbage();
}

TEST(ScrollAnimatorTest, MainThreadEnabled) {
  MockScrollableAreaForAnimatorTest* scrollable_area =
      MockScrollableAreaForAnimatorTest::Create(true, ScrollOffset(),
                                                ScrollOffset(1000, 1000));
  ScrollAnimator* scroll_animator =
      new ScrollAnimator(scrollable_area, GetMockedTime);

  EXPECT_CALL(*scrollable_area, UpdateScrollOffset(_, _)).Times(9);
  EXPECT_CALL(*scrollable_area, RegisterForAnimation()).Times(6);
  EXPECT_CALL(*scrollable_area, ScheduleAnimation())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_FALSE(scroll_animator->HasAnimationThatRequiresService());

  ScrollResult result =
      scroll_animator->UserScroll(kScrollByLine, FloatSize(-100, 0));
  EXPECT_FALSE(scroll_animator->HasAnimationThatRequiresService());
  EXPECT_FALSE(result.did_scroll_x);
  EXPECT_FLOAT_EQ(-100.0f, result.unused_scroll_delta_x);

  result = scroll_animator->UserScroll(kScrollByLine, FloatSize(100, 0));
  EXPECT_TRUE(scroll_animator->HasAnimationThatRequiresService());
  EXPECT_TRUE(result.did_scroll_x);
  EXPECT_FLOAT_EQ(0.0, result.unused_scroll_delta_x);

  g_mocked_time += 0.05;
  scroll_animator->UpdateCompositorAnimations();
  scroll_animator->TickAnimation(GetMockedTime());

  EXPECT_NE(100, scroll_animator->CurrentOffset().Width());
  EXPECT_NE(0, scroll_animator->CurrentOffset().Width());
  EXPECT_EQ(0, scroll_animator->CurrentOffset().Height());
  Reset(*scroll_animator);

  scroll_animator->UserScroll(kScrollByPage, FloatSize(100, 0));
  EXPECT_TRUE(scroll_animator->HasAnimationThatRequiresService());

  g_mocked_time += 0.05;
  scroll_animator->UpdateCompositorAnimations();
  scroll_animator->TickAnimation(GetMockedTime());

  EXPECT_NE(100, scroll_animator->CurrentOffset().Width());
  EXPECT_NE(0, scroll_animator->CurrentOffset().Width());
  EXPECT_EQ(0, scroll_animator->CurrentOffset().Height());
  Reset(*scroll_animator);

  scroll_animator->UserScroll(kScrollByPixel, FloatSize(100, 0));
  EXPECT_TRUE(scroll_animator->HasAnimationThatRequiresService());

  g_mocked_time += 0.05;
  scroll_animator->UpdateCompositorAnimations();
  scroll_animator->TickAnimation(GetMockedTime());

  EXPECT_NE(100, scroll_animator->CurrentOffset().Width());
  EXPECT_NE(0, scroll_animator->CurrentOffset().Width());
  EXPECT_EQ(0, scroll_animator->CurrentOffset().Height());

  g_mocked_time += 1.0;
  scroll_animator->UpdateCompositorAnimations();
  scroll_animator->TickAnimation(GetMockedTime());

  g_mocked_time += 0.05;
  scroll_animator->UpdateCompositorAnimations();
  EXPECT_FALSE(scroll_animator->HasAnimationThatRequiresService());
  EXPECT_EQ(100, scroll_animator->CurrentOffset().Width());

  Reset(*scroll_animator);

  scroll_animator->UserScroll(kScrollByPrecisePixel, FloatSize(100, 0));
  EXPECT_FALSE(scroll_animator->HasAnimationThatRequiresService());

  EXPECT_EQ(100, scroll_animator->CurrentOffset().Width());
  EXPECT_NE(0, scroll_animator->CurrentOffset().Width());
  EXPECT_EQ(0, scroll_animator->CurrentOffset().Height());
  Reset(*scroll_animator);
}

// Test that a smooth scroll offset animation is aborted when followed by a
// non-smooth scroll offset animation.
TEST(ScrollAnimatorTest, AnimatedScrollAborted) {
  MockScrollableAreaForAnimatorTest* scrollable_area =
      MockScrollableAreaForAnimatorTest::Create(true, ScrollOffset(),
                                                ScrollOffset(1000, 1000));
  ScrollAnimator* scroll_animator =
      new ScrollAnimator(scrollable_area, GetMockedTime);

  EXPECT_CALL(*scrollable_area, UpdateScrollOffset(_, _)).Times(3);
  EXPECT_CALL(*scrollable_area, RegisterForAnimation()).Times(2);
  EXPECT_CALL(*scrollable_area, ScheduleAnimation())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_FALSE(scroll_animator->HasAnimationThatRequiresService());

  // Smooth scroll.
  ScrollResult result =
      scroll_animator->UserScroll(kScrollByLine, FloatSize(100, 0));
  EXPECT_TRUE(scroll_animator->HasAnimationThatRequiresService());
  EXPECT_TRUE(result.did_scroll_x);
  EXPECT_FLOAT_EQ(0.0, result.unused_scroll_delta_x);
  EXPECT_TRUE(scroll_animator->HasRunningAnimation());

  g_mocked_time += 0.05;
  scroll_animator->UpdateCompositorAnimations();
  scroll_animator->TickAnimation(GetMockedTime());

  EXPECT_NE(100, scroll_animator->CurrentOffset().Width());
  EXPECT_NE(0, scroll_animator->CurrentOffset().Width());
  EXPECT_EQ(0, scroll_animator->CurrentOffset().Height());

  float x = scroll_animator->CurrentOffset().Width();

  // Instant scroll.
  result =
      scroll_animator->UserScroll(kScrollByPrecisePixel, FloatSize(100, 0));
  EXPECT_TRUE(result.did_scroll_x);
  g_mocked_time += 0.05;
  scroll_animator->UpdateCompositorAnimations();
  EXPECT_FALSE(scroll_animator->HasRunningAnimation());
  EXPECT_EQ(x + 100, scroll_animator->CurrentOffset().Width());
  EXPECT_EQ(0, scroll_animator->CurrentOffset().Height());

  Reset(*scroll_animator);
}

// Test that a smooth scroll offset animation running on the compositor is
// completed on the main thread.
TEST(ScrollAnimatorTest, AnimatedScrollTakeover) {
  MockScrollableAreaForAnimatorTest* scrollable_area =
      MockScrollableAreaForAnimatorTest::Create(true, ScrollOffset(),
                                                ScrollOffset(1000, 1000));
  TestScrollAnimator* scroll_animator =
      new TestScrollAnimator(scrollable_area, GetMockedTime);

  EXPECT_CALL(*scrollable_area, UpdateScrollOffset(_, _)).Times(2);
  // Called from userScroll, updateCompositorAnimations, then
  // takeOverCompositorAnimation (to re-register after RunningOnCompositor).
  EXPECT_CALL(*scrollable_area, RegisterForAnimation()).Times(3);
  EXPECT_CALL(*scrollable_area, ScheduleAnimation())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_FALSE(scroll_animator->HasAnimationThatRequiresService());

  // Smooth scroll.
  ScrollResult result =
      scroll_animator->UserScroll(kScrollByLine, FloatSize(100, 0));
  EXPECT_TRUE(scroll_animator->HasAnimationThatRequiresService());
  EXPECT_TRUE(result.did_scroll_x);
  EXPECT_FLOAT_EQ(0.0, result.unused_scroll_delta_x);
  EXPECT_TRUE(scroll_animator->HasRunningAnimation());

  // Update compositor animation.
  g_mocked_time += 0.05;
  scroll_animator->SetShouldSendToCompositor(true);
  scroll_animator->UpdateCompositorAnimations();
  EXPECT_EQ(
      scroll_animator->run_state_,
      ScrollAnimatorCompositorCoordinator::RunState::kRunningOnCompositor);

  // Takeover.
  scroll_animator->TakeOverCompositorAnimation();
  EXPECT_EQ(scroll_animator->run_state_,
            ScrollAnimatorCompositorCoordinator::RunState::
                kRunningOnCompositorButNeedsTakeover);

  // Animation should now be running on the main thread.
  scroll_animator->SetShouldSendToCompositor(false);
  scroll_animator->UpdateCompositorAnimations();
  EXPECT_EQ(
      scroll_animator->run_state_,
      ScrollAnimatorCompositorCoordinator::RunState::kRunningOnMainThread);
  scroll_animator->TickAnimation(GetMockedTime());
  EXPECT_NE(100, scroll_animator->CurrentOffset().Width());
  EXPECT_NE(0, scroll_animator->CurrentOffset().Width());
  EXPECT_EQ(0, scroll_animator->CurrentOffset().Height());
  Reset(*scroll_animator);
}

TEST(ScrollAnimatorTest, Disabled) {
  MockScrollableAreaForAnimatorTest* scrollable_area =
      MockScrollableAreaForAnimatorTest::Create(false, ScrollOffset(),
                                                ScrollOffset(1000, 1000));
  ScrollAnimator* scroll_animator =
      new ScrollAnimator(scrollable_area, GetMockedTime);

  EXPECT_CALL(*scrollable_area, UpdateScrollOffset(_, _)).Times(8);
  EXPECT_CALL(*scrollable_area, RegisterForAnimation()).Times(0);

  scroll_animator->UserScroll(kScrollByLine, FloatSize(100, 0));
  EXPECT_EQ(100, scroll_animator->CurrentOffset().Width());
  EXPECT_EQ(0, scroll_animator->CurrentOffset().Height());
  Reset(*scroll_animator);

  scroll_animator->UserScroll(kScrollByPage, FloatSize(100, 0));
  EXPECT_EQ(100, scroll_animator->CurrentOffset().Width());
  EXPECT_EQ(0, scroll_animator->CurrentOffset().Height());
  Reset(*scroll_animator);

  scroll_animator->UserScroll(kScrollByDocument, FloatSize(100, 0));
  EXPECT_EQ(100, scroll_animator->CurrentOffset().Width());
  EXPECT_EQ(0, scroll_animator->CurrentOffset().Height());
  Reset(*scroll_animator);

  scroll_animator->UserScroll(kScrollByPixel, FloatSize(100, 0));
  EXPECT_EQ(100, scroll_animator->CurrentOffset().Width());
  EXPECT_EQ(0, scroll_animator->CurrentOffset().Height());
  Reset(*scroll_animator);
}

// Test that cancelling an animation resets the animation state.
// See crbug.com/598548.
TEST(ScrollAnimatorTest, CancellingAnimationResetsState) {
  MockScrollableAreaForAnimatorTest* scrollable_area =
      MockScrollableAreaForAnimatorTest::Create(true, ScrollOffset(),
                                                ScrollOffset(1000, 1000));
  ScrollAnimator* scroll_animator =
      new ScrollAnimator(scrollable_area, GetMockedTime);

  // Called from first userScroll, setCurrentOffset, and second userScroll.
  EXPECT_CALL(*scrollable_area, UpdateScrollOffset(_, _)).Times(3);
  // Called from userScroll, updateCompositorAnimations.
  EXPECT_CALL(*scrollable_area, RegisterForAnimation()).Times(4);
  EXPECT_CALL(*scrollable_area, ScheduleAnimation())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_EQ(0, scroll_animator->CurrentOffset().Width());
  EXPECT_EQ(0, scroll_animator->CurrentOffset().Height());

  // WaitingToSendToCompositor
  scroll_animator->UserScroll(kScrollByLine, FloatSize(10, 0));
  EXPECT_EQ(scroll_animator->run_state_,
            ScrollAnimatorCompositorCoordinator::RunState::
                kWaitingToSendToCompositor);

  // RunningOnMainThread
  g_mocked_time += 0.05;
  scroll_animator->UpdateCompositorAnimations();
  EXPECT_EQ(
      scroll_animator->run_state_,
      ScrollAnimatorCompositorCoordinator::RunState::kRunningOnMainThread);
  scroll_animator->TickAnimation(GetMockedTime());
  EXPECT_EQ(
      scroll_animator->run_state_,
      ScrollAnimatorCompositorCoordinator::RunState::kRunningOnMainThread);

  // Amount scrolled so far.
  float offset_x = scroll_animator->CurrentOffset().Width();

  // Interrupt user scroll.
  scroll_animator->CancelAnimation();
  EXPECT_EQ(
      scroll_animator->run_state_,
      ScrollAnimatorCompositorCoordinator::RunState::kPostAnimationCleanup);

  // Another userScroll after modified scroll offset.
  scroll_animator->SetCurrentOffset(ScrollOffset(offset_x + 15, 0));
  scroll_animator->UserScroll(kScrollByLine, FloatSize(10, 0));
  EXPECT_EQ(scroll_animator->run_state_,
            ScrollAnimatorCompositorCoordinator::RunState::
                kWaitingToSendToCompositor);

  // Finish scroll animation.
  g_mocked_time += 1.0;
  scroll_animator->UpdateCompositorAnimations();
  scroll_animator->TickAnimation(GetMockedTime());
  EXPECT_EQ(
      scroll_animator->run_state_,
      ScrollAnimatorCompositorCoordinator::RunState::kPostAnimationCleanup);

  EXPECT_EQ(offset_x + 15 + 10, scroll_animator->CurrentOffset().Width());
  EXPECT_EQ(0, scroll_animator->CurrentOffset().Height());
  Reset(*scroll_animator);
}

// Test the behavior when in WaitingToCancelOnCompositor and a new user scroll
// happens.
TEST(ScrollAnimatorTest, CancellingCompositorAnimation) {
  MockScrollableAreaForAnimatorTest* scrollable_area =
      MockScrollableAreaForAnimatorTest::Create(true, ScrollOffset(),
                                                ScrollOffset(1000, 1000));
  TestScrollAnimator* scroll_animator =
      new TestScrollAnimator(scrollable_area, GetMockedTime);

  // Called when reset, not setting anywhere else.
  EXPECT_CALL(*scrollable_area, UpdateScrollOffset(_, _)).Times(1);
  // Called from userScroll, and first update.
  EXPECT_CALL(*scrollable_area, RegisterForAnimation()).Times(4);
  EXPECT_CALL(*scrollable_area, ScheduleAnimation())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  EXPECT_FALSE(scroll_animator->HasAnimationThatRequiresService());

  // First user scroll.
  ScrollResult result =
      scroll_animator->UserScroll(kScrollByLine, FloatSize(100, 0));
  EXPECT_TRUE(scroll_animator->HasAnimationThatRequiresService());
  EXPECT_TRUE(result.did_scroll_x);
  EXPECT_FLOAT_EQ(0.0, result.unused_scroll_delta_x);
  EXPECT_TRUE(scroll_animator->HasRunningAnimation());
  EXPECT_EQ(100, scroll_animator->DesiredTargetOffset().Width());
  EXPECT_EQ(0, scroll_animator->DesiredTargetOffset().Height());

  // Update compositor animation.
  g_mocked_time += 0.05;
  scroll_animator->SetShouldSendToCompositor(true);
  scroll_animator->UpdateCompositorAnimations();
  EXPECT_EQ(
      scroll_animator->run_state_,
      ScrollAnimatorCompositorCoordinator::RunState::kRunningOnCompositor);

  // Cancel
  scroll_animator->CancelAnimation();
  EXPECT_EQ(scroll_animator->run_state_,
            ScrollAnimatorCompositorCoordinator::RunState::
                kWaitingToCancelOnCompositor);

  // Unrelated scroll offset update.
  scroll_animator->SetCurrentOffset(ScrollOffset(50, 0));

  // Desired target offset should be that of the second scroll.
  result = scroll_animator->UserScroll(kScrollByLine, FloatSize(100, 0));
  EXPECT_TRUE(scroll_animator->HasAnimationThatRequiresService());
  EXPECT_TRUE(result.did_scroll_x);
  EXPECT_FLOAT_EQ(0.0, result.unused_scroll_delta_x);
  EXPECT_EQ(scroll_animator->run_state_,
            ScrollAnimatorCompositorCoordinator::RunState::
                kWaitingToCancelOnCompositorButNewScroll);
  EXPECT_EQ(150, scroll_animator->DesiredTargetOffset().Width());
  EXPECT_EQ(0, scroll_animator->DesiredTargetOffset().Height());

  // Update compositor animation.
  g_mocked_time += 0.05;
  scroll_animator->UpdateCompositorAnimations();
  EXPECT_EQ(
      scroll_animator->run_state_,
      ScrollAnimatorCompositorCoordinator::RunState::kRunningOnCompositor);

  // Third user scroll after compositor update updates the target.
  result = scroll_animator->UserScroll(kScrollByLine, FloatSize(100, 0));
  EXPECT_TRUE(scroll_animator->HasAnimationThatRequiresService());
  EXPECT_TRUE(result.did_scroll_x);
  EXPECT_FLOAT_EQ(0.0, result.unused_scroll_delta_x);
  EXPECT_EQ(scroll_animator->run_state_,
            ScrollAnimatorCompositorCoordinator::RunState::
                kRunningOnCompositorButNeedsUpdate);
  EXPECT_EQ(250, scroll_animator->DesiredTargetOffset().Width());
  EXPECT_EQ(0, scroll_animator->DesiredTargetOffset().Height());
  Reset(*scroll_animator);

  // Forced GC in order to finalize objects depending on the mock object.
  ThreadState::Current()->CollectAllGarbage();
}

// This test verifies that impl only animation updates get cleared once they
// are pushed to compositor animation host.
TEST(ScrollAnimatorTest, ImplOnlyAnimationUpdatesCleared) {
  MockScrollableAreaForAnimatorTest* scrollable_area =
      MockScrollableAreaForAnimatorTest::Create(true, ScrollOffset(),
                                                ScrollOffset(1000, 1000));
  TestScrollAnimator* animator =
      new TestScrollAnimator(scrollable_area, GetMockedTime);

  // From calls to adjust/takeoverImplOnlyScrollOffsetAnimation.
  EXPECT_CALL(*scrollable_area, RegisterForAnimation()).Times(3);

  // Verify that the adjustment update is cleared.
  EXPECT_EQ(animator->run_state_,
            ScrollAnimatorCompositorCoordinator::RunState::kIdle);
  EXPECT_FALSE(animator->HasAnimationThatRequiresService());
  EXPECT_TRUE(animator->ImplOnlyAnimationAdjustmentForTesting().IsZero());

  animator->AdjustImplOnlyScrollOffsetAnimation(IntSize(100, 100));
  animator->AdjustImplOnlyScrollOffsetAnimation(IntSize(10, -10));

  EXPECT_TRUE(animator->HasAnimationThatRequiresService());
  EXPECT_EQ(IntSize(110, 90),
            animator->ImplOnlyAnimationAdjustmentForTesting());

  animator->UpdateCompositorAnimations();

  EXPECT_EQ(animator->run_state_,
            ScrollAnimatorCompositorCoordinator::RunState::kIdle);
  EXPECT_FALSE(animator->HasAnimationThatRequiresService());
  EXPECT_TRUE(animator->ImplOnlyAnimationAdjustmentForTesting().IsZero());

  // Verify that the takeover update is cleared.
  animator->TakeOverImplOnlyScrollOffsetAnimation();
  EXPECT_FALSE(animator->HasAnimationThatRequiresService());

  // Forced GC in order to finalize objects depending on the mock object.
  ThreadState::Current()->CollectAllGarbage();
}

TEST(ScrollAnimatorTest, MainThreadAnimationTargetAdjustment) {
  MockScrollableAreaForAnimatorTest* scrollable_area =
      MockScrollableAreaForAnimatorTest::Create(true, ScrollOffset(-100, -100),
                                                ScrollOffset(1000, 1000));
  ScrollAnimator* animator = new ScrollAnimator(scrollable_area, GetMockedTime);
  scrollable_area->SetScrollAnimator(animator);

  // Twice from tickAnimation, once from reset, and twice from
  // adjustAnimationAndSetScrollOffset.
  EXPECT_CALL(*scrollable_area, UpdateScrollOffset(_, _)).Times(5);
  // One from call to userScroll and one from updateCompositorAnimations.
  EXPECT_CALL(*scrollable_area, RegisterForAnimation()).Times(2);
  EXPECT_CALL(*scrollable_area, ScheduleAnimation())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  // Idle
  EXPECT_FALSE(animator->HasAnimationThatRequiresService());
  EXPECT_EQ(ScrollOffset(), animator->CurrentOffset());

  // WaitingToSendToCompositor
  animator->UserScroll(kScrollByLine, ScrollOffset(100, 100));

  // RunningOnMainThread
  g_mocked_time += 0.05;
  animator->UpdateCompositorAnimations();
  animator->TickAnimation(GetMockedTime());
  ScrollOffset offset = animator->CurrentOffset();
  EXPECT_EQ(ScrollOffset(100, 100), animator->DesiredTargetOffset());
  EXPECT_GT(offset.Width(), 0);
  EXPECT_GT(offset.Height(), 0);

  // Adjustment
  ScrollOffset new_offset = offset + ScrollOffset(10, -10);
  animator->AdjustAnimationAndSetScrollOffset(new_offset, kAnchoringScroll);
  EXPECT_EQ(ScrollOffset(110, 90), animator->DesiredTargetOffset());

  // Adjusting after finished animation should do nothing.
  g_mocked_time += 1.0;
  animator->UpdateCompositorAnimations();
  animator->TickAnimation(GetMockedTime());
  EXPECT_EQ(
      animator->RunStateForTesting(),
      ScrollAnimatorCompositorCoordinator::RunState::kPostAnimationCleanup);
  new_offset = animator->CurrentOffset() + ScrollOffset(10, -10);
  animator->AdjustAnimationAndSetScrollOffset(new_offset, kAnchoringScroll);
  EXPECT_EQ(
      animator->RunStateForTesting(),
      ScrollAnimatorCompositorCoordinator::RunState::kPostAnimationCleanup);
  EXPECT_EQ(ScrollOffset(110, 90), animator->DesiredTargetOffset());

  Reset(*animator);

  // Forced GC in order to finalize objects depending on the mock object.
  ThreadState::Current()->CollectAllGarbage();
}

}  // namespace blink
