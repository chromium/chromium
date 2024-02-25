// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/document_animations.h"

#include "cc/animation/animation_timeline.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;

namespace blink {

class MockAnimationTimeline : public AnimationTimeline {
 public:
  MockAnimationTimeline(Document* document) : AnimationTimeline(document) {}

  MOCK_METHOD0(Phase, TimelinePhase());
  MOCK_CONST_METHOD0(IsActive, bool());
  MOCK_METHOD0(ZeroTime, AnimationTimeDelta());
  MOCK_METHOD0(InitialStartTimeForAnimations, std::optional<base::TimeDelta>());
  MOCK_METHOD0(NeedsAnimationTimingUpdate, bool());
  MOCK_CONST_METHOD0(HasOutdatedAnimation, bool());
  MOCK_CONST_METHOD0(HasAnimations, bool());
  MOCK_METHOD1(ServiceAnimations, void(TimingUpdateReason));
  MOCK_CONST_METHOD0(AnimationsNeedingUpdateCount, wtf_size_t());
  MOCK_METHOD0(ScheduleNextService, void());
  MOCK_METHOD0(EnsureCompositorTimeline, cc::AnimationTimeline*());

  void Trace(Visitor* visitor) const override {
    AnimationTimeline::Trace(visitor);
  }

 protected:
  MOCK_METHOD0(CurrentPhaseAndTime, PhaseAndTime());
};

class DocumentAnimationsTest : public RenderingTest {
 protected:
  DocumentAnimationsTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
    helper_.Initialize(nullptr, nullptr, nullptr);
    document = helper_.LocalMainFrame()->GetFrame()->GetDocument();
    UpdateAllLifecyclePhasesForTest();
  }

  void TearDown() override {
    document.Release();
    ThreadState::Current()->CollectAllGarbageForTesting();
  }

  void UpdateAllLifecyclePhasesForTest() {
    document->View()->UpdateAllLifecyclePhasesForTest();
  }

  Persistent<Document> document;

 private:
  frame_test_helpers::WebViewHelper helper_;
};

// Test correctness of DocumentAnimations::NeedsAnimationTimingUpdate.
TEST_F(DocumentAnimationsTest, NeedsAnimationTimingUpdate) {
  // 1. Test that if all timelines don't require timing update,
  // DocumentAnimations::NeedsAnimationTimingUpdate returns false.
  MockAnimationTimeline* timeline1 =
      MakeGarbageCollected<MockAnimationTimeline>(document);
  MockAnimationTimeline* timeline2 =
      MakeGarbageCollected<MockAnimationTimeline>(document);

  EXPECT_CALL(*timeline1, HasOutdatedAnimation()).WillOnce(Return(false));
  EXPECT_CALL(*timeline1, NeedsAnimationTimingUpdate()).WillOnce(Return(false));
  EXPECT_CALL(*timeline2, HasOutdatedAnimation()).WillOnce(Return(false));
  EXPECT_CALL(*timeline2, NeedsAnimationTimingUpdate()).WillOnce(Return(false));

  EXPECT_FALSE(document->GetDocumentAnimations().NeedsAnimationTimingUpdate());

  Mock::VerifyAndClearExpectations(timeline1);
  Mock::VerifyAndClearExpectations(timeline2);

  // 2. Test that if at least one timeline requires timing update,
  // DocumentAnimations::NeedsAnimationTimingUpdate returns true.
  EXPECT_CALL(*timeline2, HasOutdatedAnimation()).WillRepeatedly(Return(false));
  EXPECT_CALL(*timeline2, NeedsAnimationTimingUpdate())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*timeline1, HasOutdatedAnimation()).WillRepeatedly(Return(true));
  EXPECT_CALL(*timeline1, NeedsAnimationTimingUpdate())
      .WillRepeatedly(Return(false));

  EXPECT_TRUE(document->GetDocumentAnimations().NeedsAnimationTimingUpdate());
}

// Test correctness of
// DocumentAnimations::UpdateAnimationTimingForAnimationFrame.
TEST_F(DocumentAnimationsTest, UpdateAnimationTimingServicesAllTimelines) {
  // Test that all timelines are traversed to perform timing update.
  MockAnimationTimeline* timeline1 =
      MakeGarbageCollected<MockAnimationTimeline>(document);
  MockAnimationTimeline* timeline2 =
      MakeGarbageCollected<MockAnimationTimeline>(document);

  EXPECT_CALL(*timeline1, ServiceAnimations(_));
  EXPECT_CALL(*timeline2, ServiceAnimations(_));

  document->GetDocumentAnimations().UpdateAnimationTimingForAnimationFrame();
}

// Test correctness of DocumentAnimations::UpdateAnimations.
TEST_F(DocumentAnimationsTest, UpdateAnimationsUpdatesAllTimelines) {
  // Test that all timelines are traversed to schedule next service.
  MockAnimationTimeline* timeline1 =
      MakeGarbageCollected<MockAnimationTimeline>(document);
  MockAnimationTimeline* timeline2 =
      MakeGarbageCollected<MockAnimationTimeline>(document);

  UpdateAllLifecyclePhasesForTest();

  EXPECT_CALL(*timeline1, HasAnimations()).WillOnce(Return(true));
  EXPECT_CALL(*timeline2, HasAnimations()).WillOnce(Return(true));
  EXPECT_CALL(*timeline1, AnimationsNeedingUpdateCount()).WillOnce(Return(3));
  EXPECT_CALL(*timeline2, AnimationsNeedingUpdateCount()).WillOnce(Return(2));
  EXPECT_CALL(*timeline1, ScheduleNextService());
  EXPECT_CALL(*timeline2, ScheduleNextService());

  document->GetFrame()->LocalFrameRoot().View()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  // Verify that animations count is correctly updated on animation host.
  cc::AnimationHost* host = document->View()->GetCompositorAnimationHost();
  EXPECT_EQ(5u, host->MainThreadAnimationsCount());
}

}  // namespace blink
