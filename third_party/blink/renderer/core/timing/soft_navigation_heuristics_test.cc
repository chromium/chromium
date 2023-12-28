// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

class SoftNavigationHeuristicsTest : public testing::Test {
 protected:
  void SetUp() override {
    page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  }

  SoftNavigationHeuristics* CreateSoftNavigationHeuristicsForTest() {
    ScriptState* script_state = GetScriptStateForTest();

    LocalDOMWindow* window = LocalDOMWindow::From(script_state);

    SoftNavigationHeuristics* heuristics =
        SoftNavigationHeuristics::From(*window);

    return heuristics;
  }

  ScriptState* GetScriptStateForTest() {
    return ToScriptStateForMainWorld(page_holder_->GetDocument().GetFrame());
  }

 private:
  std::unique_ptr<DummyPageHolder> page_holder_;
};

// TODO(crbug.com/1503284): This test validates that the renderer does not crash
// when presented with an unset timestamp. Figure out whether it is possible to
// void ever calling InteractionCallbackCalled in that situation instead.
TEST_F(SoftNavigationHeuristicsTest,
       EarlyReturnOnInvalidPendingInteractionTimestamp) {
  auto* test_heuristics = CreateSoftNavigationHeuristicsForTest();
  test_heuristics->InteractionCallbackCalled(
      GetScriptStateForTest(), SoftNavigationHeuristics::EventScopeType::Click,
      true);
  // Since pending_interaction_timestamp_ is not set, the execution of
  // InteractionCallbackCalled would return early. As a result, the
  // initial_interaction_encountered_ would not be set true.
  ASSERT_FALSE(test_heuristics->GetInitialInteractionEncounteredForTest());
}

TEST_F(SoftNavigationHeuristicsTest, UmaHistogramRecording) {
  base::HistogramTester histogram_tester;

  // Test case where user interaction timestamp and reference monotonic
  // timestamp are both null.
  base::TimeTicks user_interaction_ts;
  base::TimeTicks reference_ts;
  internal::
      RecordUmaForPageLoadInternalSoftNavigationFromReferenceInvalidTiming(
          user_interaction_ts, reference_ts);

  histogram_tester.ExpectBucketCount(
      internal::kPageLoadInternalSoftNavigationFromReferenceInvalidTiming,
      internal::SoftNavigationFromReferenceInvalidTimingReasons::
          kUserInteractionTsAndReferenceTsBothNull,
      1);

  // Test case where both user interaction timestamp is not null and reference
  // monotonic timestamp is null.
  user_interaction_ts = base::TimeTicks() + base::Milliseconds(1);

  internal::
      RecordUmaForPageLoadInternalSoftNavigationFromReferenceInvalidTiming(
          user_interaction_ts, reference_ts);

  histogram_tester.ExpectBucketCount(
      internal::kPageLoadInternalSoftNavigationFromReferenceInvalidTiming,
      internal::SoftNavigationFromReferenceInvalidTimingReasons::
          kNullReferenceTsAndNotNullUserInteractionTs,
      1);

  // Test case where user interaction timestamp is null and reference
  // monotonic timestamp is not null.
  user_interaction_ts = base::TimeTicks();
  reference_ts = base::TimeTicks() + base::Milliseconds(1);

  internal::
      RecordUmaForPageLoadInternalSoftNavigationFromReferenceInvalidTiming(
          user_interaction_ts, reference_ts);

  histogram_tester.ExpectBucketCount(
      internal::kPageLoadInternalSoftNavigationFromReferenceInvalidTiming,
      internal::SoftNavigationFromReferenceInvalidTimingReasons::
          kNullUserInteractionTsAndNotNullReferenceTs,
      1);

  // Test case where user interaction timestamp and reference monotonic
  // timestamp are both not null.
  user_interaction_ts = base::TimeTicks() + base::Milliseconds(1);
  reference_ts = base::TimeTicks() + base::Milliseconds(2);

  internal::
      RecordUmaForPageLoadInternalSoftNavigationFromReferenceInvalidTiming(
          user_interaction_ts, reference_ts);

  histogram_tester.ExpectBucketCount(
      internal::kPageLoadInternalSoftNavigationFromReferenceInvalidTiming,
      internal::SoftNavigationFromReferenceInvalidTimingReasons::
          kUserInteractionTsAndReferenceTsBothNotNull,
      1);
}
}  // namespace blink
