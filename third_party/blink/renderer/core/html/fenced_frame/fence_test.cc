// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fence.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fence_event.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_fenceevent_string.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class FenceTest : private ScopedFencedFramesForTest, public SimTest {
 public:
  FenceTest() : ScopedFencedFramesForTest(true) {
    scoped_feature_list_.InitWithFeatures(
        {{blink::features::kFencedFrames},
         {blink::features::kPrivateAggregationApi}},
        /*disabled_features=*/{});
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FenceTest, ReportPrivateAggregationEvent) {
  const KURL base_url("https://www.example.com/");
  V8TestingScope scope(base_url);
  Fence* fence =
      MakeGarbageCollected<Fence>(*(GetDocument().GetFrame()->DomWindow()));
  fence->reportPrivateAggregationEvent("event", scope.GetExceptionState());

  // We expect this to make it past all the other checks, except for the fenced
  // frame properties check. Since this is loaded in a vacuum and not the result
  // of an ad auction, we expect it to output the reporting metadata error.
  EXPECT_EQ(ConsoleMessages().size(), 1u);
  EXPECT_EQ(ConsoleMessages().front(),
            "This frame was not loaded with a FencedFrameConfig.");
}

TEST_F(FenceTest, ReportPrivateAggregationReservedEvent) {
  const KURL base_url("https://www.example.com/");
  V8TestingScope scope(base_url);
  Fence* fence =
      MakeGarbageCollected<Fence>(*(GetDocument().GetFrame()->DomWindow()));
  fence->reportPrivateAggregationEvent("reserved.event",
                                       scope.GetExceptionState());

  // There should be a "Reserved events cannot be triggered manually." console
  // warning.
  EXPECT_EQ(ConsoleMessages().size(), 1u);
  EXPECT_EQ(ConsoleMessages().front(),
            "Reserved events cannot be triggered manually.");
}

TEST_F(FenceTest, ReportReservedEvent) {
  const KURL base_url("https://www.example.com/");
  V8TestingScope scope(base_url);
  Fence* fence =
      MakeGarbageCollected<Fence>(*(GetDocument().GetFrame()->DomWindow()));
  FenceEvent* event = FenceEvent::Create();
  event->setEventType("reserved.top_navigation");
  V8UnionFenceEventOrString* event_union =
      MakeGarbageCollected<V8UnionFenceEventOrString>(event);
  fence->reportEvent(event_union, scope.GetExceptionState());

  // There should be a "Reserved events cannot be triggered manually." console
  // warning.
  EXPECT_EQ(ConsoleMessages().size(), 1u);
  EXPECT_EQ(ConsoleMessages().front(),
            "Reserved events cannot be triggered manually.");
}

TEST_F(FenceTest, NotifyEventNotFencedFrameRoot) {
  const KURL base_url("https://www.example.com/");
  V8TestingScope scope(base_url);
  Fence* fence =
      MakeGarbageCollected<Fence>(*(GetDocument().GetFrame()->DomWindow()));

  Event* event = Event::Create(AtomicString("click"));
  fence->notifyEvent(event, scope.GetExceptionState());

  histogram_tester_.ExpectBucketCount(
      kNotifyEventOutcome, NotifyEventOutcome::kNotFencedFrameRoot, 1);
}

TEST_F(FenceTest, NotifyEventInvalidEvent) {
  InitializeFencedFrameRoot(
      blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds);
  const KURL base_url("https://www.example.com/");
  V8TestingScope scope(base_url);
  Fence* fence =
      MakeGarbageCollected<Fence>(*(GetDocument().GetFrame()->DomWindow()));

  Event* event = Event::Create(AtomicString("click"));
  fence->notifyEvent(event, scope.GetExceptionState());

  // The event being untrusted will result in an invalid event histogram being
  // logged.
  histogram_tester_.ExpectBucketCount(kNotifyEventOutcome,
                                      NotifyEventOutcome::kInvalidEvent, 1);
}

TEST_F(FenceTest, NotifyEventUnsupportedType) {
  InitializeFencedFrameRoot(
      blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds);
  const KURL base_url("https://www.example.com/");
  V8TestingScope scope(base_url);
  Fence* fence =
      MakeGarbageCollected<Fence>(*(GetDocument().GetFrame()->DomWindow()));

  Event* event = Event::Create(AtomicString("keydown"));
  event->SetTrusted(true);
  event->SetEventPhase(Event::PhaseType::kAtTarget);
  fence->notifyEvent(event, scope.GetExceptionState());

  histogram_tester_.ExpectBucketCount(
      kNotifyEventOutcome, NotifyEventOutcome::kUnsupportedEventType, 1);
}

}  // namespace blink
