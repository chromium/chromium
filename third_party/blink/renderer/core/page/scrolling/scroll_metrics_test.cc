// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/features.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

#define EXPECT_WHEEL_BUCKET(index, count)                        \
  do {                                                           \
    SCOPED_TRACE("EXPECT_WHEEL_BUCKET");                         \
    histogram_tester->ExpectBucketCount(                         \
        "Renderer4.MainThreadWheelScrollReason2", index, count); \
  } while (false)

#define EXPECT_TOUCH_BUCKET(index, count)                          \
  do {                                                             \
    SCOPED_TRACE("EXPECT_TOUCH_BUCKET");                           \
    histogram_tester->ExpectBucketCount(                           \
        "Renderer4.MainThreadGestureScrollReason2", index, count); \
  } while (false)

#define EXPECT_WHEEL_TOTAL(count)                         \
  do {                                                    \
    SCOPED_TRACE("EXPECT_WHEEL_TOTAL");                   \
    histogram_tester->ExpectTotalCount(                   \
        "Renderer4.MainThreadWheelScrollReason2", count); \
  } while (false)

#define EXPECT_TOUCH_TOTAL(count)                           \
  do {                                                      \
    SCOPED_TRACE("EXPECT_TOUCH_TOTAL");                     \
    histogram_tester->ExpectTotalCount(                     \
        "Renderer4.MainThreadGestureScrollReason2", count); \
  } while (false)

namespace blink {

namespace {

class ScrollMetricsTest : public PaintTestConfigurations, public SimTest {
 public:
  void SetUpHtml(const char*);
  void Scroll(Element*, const WebGestureDevice);
  void UpdateAllLifecyclePhases() {
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(ScrollMetricsTest);

class ScrollBeginEventBuilder : public WebGestureEvent {
 public:
  ScrollBeginEventBuilder(gfx::PointF position,
                          gfx::PointF delta,
                          WebGestureDevice device)
      : WebGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        device) {
    SetPositionInWidget(position);
    SetPositionInScreen(position);
    data.scroll_begin.delta_y_hint = delta.y();
    frame_scale_ = 1;
  }
};

class ScrollUpdateEventBuilder : public WebGestureEvent {
 public:
  explicit ScrollUpdateEventBuilder(WebGestureDevice device)
      : WebGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        device) {
    data.scroll_update.delta_x = 0.0f;
    data.scroll_update.delta_y = -1.0f;
    data.scroll_update.velocity_x = 0;
    data.scroll_update.velocity_y = -1;
    frame_scale_ = 1;
  }
};

class ScrollEndEventBuilder : public WebGestureEvent {
 public:
  explicit ScrollEndEventBuilder(WebGestureDevice device)
      : WebGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        device) {
    frame_scale_ = 1;
  }
};

int BucketIndex(uint32_t reason) {
  return cc::MainThreadScrollingReason::BucketIndexForTesting(reason);
}

void ScrollMetricsTest::Scroll(Element* element,
                               const WebGestureDevice device) {
  DCHECK(element);
  DCHECK(element->getBoundingClientRect());
  DOMRect* rect = element->getBoundingClientRect();
  ScrollBeginEventBuilder scroll_begin(
      gfx::PointF(rect->left() + rect->width() / 2,
                  rect->top() + rect->height() / 2),
      gfx::PointF(0.f, -1.f), device);
  ScrollUpdateEventBuilder scroll_update(device);
  ScrollEndEventBuilder scroll_end(device);
  GetWebFrameWidget().DispatchThroughCcInputHandler(scroll_begin);
  GetWebFrameWidget().DispatchThroughCcInputHandler(scroll_update);
  GetWebFrameWidget().DispatchThroughCcInputHandler(scroll_end);

  // Negative delta in the gesture event corresponds to positive delta to the
  // scroll offset (see CreateScrollStateForGesture).
  ASSERT_LT(scroll_update.DeltaYInRootFrame(), 0);
}

void ScrollMetricsTest::SetUpHtml(const char* html_content) {
  ResizeView(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(html_content);
  Compositor().BeginFrame();

  GetDocument().View()->SetParentVisible(true);
  GetDocument().View()->SetSelfVisible(true);
  UpdateAllLifecyclePhases();
}

TEST_P(ScrollMetricsTest, TouchAndWheelGeneralTest) {
  SetUpHtml(R"HTML(
    <style>
     .box { overflow:scroll; width: 100px; height: 100px; }
     .spacer { height: 1000px; }
    </style>
    <div id='box' class='box'>
     <div class='spacer'></div>
    </div>
  )HTML");

  Element* box = GetDocument().getElementById("box");
  absl::optional<HistogramTester> histogram_tester;
  histogram_tester.emplace();

  // Test touch scroll.
  Scroll(box, WebGestureDevice::kTouchscreen);

  // The below reasons are reported because #box is not composited.
  EXPECT_TOUCH_BUCKET(
      BucketIndex(cc::MainThreadScrollingReason::kNonFastScrollableRegion), 1);
  EXPECT_TOUCH_BUCKET(
      BucketIndex(cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText),
      1);
  EXPECT_TOUCH_BUCKET(
      cc::MainThreadScrollingReason::kScrollingOnMainForAnyReason, 1);
  EXPECT_TOUCH_TOTAL(3);

  // Reset histogram tester.
  histogram_tester.emplace();

  // Test wheel scroll.
  Scroll(box, WebGestureDevice::kTouchpad);

  // The below reasons are reported because #box is not composited.
  EXPECT_WHEEL_BUCKET(
      BucketIndex(cc::MainThreadScrollingReason::kNonFastScrollableRegion), 1);
  EXPECT_WHEEL_BUCKET(
      BucketIndex(cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText),
      1);
  EXPECT_WHEEL_BUCKET(
      cc::MainThreadScrollingReason::kScrollingOnMainForAnyReason, 1);
  EXPECT_WHEEL_TOTAL(3);
}

TEST_P(ScrollMetricsTest, CompositedScrollableAreaTest) {
  SetUpHtml(R"HTML(
    <style>
     .box { overflow:scroll; width: 100px; height: 100px; }
     .composited { will-change: transform; }
     .spacer { height: 1000px; }
    </style>
    <div id='box' class='box'>
     <div class='spacer'></div>
    </div>
  )HTML");

  Element* box = GetDocument().getElementById("box");
  absl::optional<HistogramTester> histogram_tester;
  histogram_tester.emplace();

  Scroll(box, WebGestureDevice::kTouchpad);

  // The below reasons are reported because #box is not composited.
  EXPECT_WHEEL_BUCKET(
      BucketIndex(cc::MainThreadScrollingReason::kNonFastScrollableRegion), 1);
  EXPECT_WHEEL_BUCKET(
      BucketIndex(cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText),
      1);
  EXPECT_WHEEL_BUCKET(
      cc::MainThreadScrollingReason::kScrollingOnMainForAnyReason, 1);
  EXPECT_WHEEL_TOTAL(3);

  // Reset histogram tester.
  histogram_tester.emplace();

  box->setAttribute("class", "composited transform box");
  Compositor().BeginFrame();
  Scroll(box, WebGestureDevice::kTouchpad);
  if (!RuntimeEnabledFeatures::CompositeScrollAfterPaintEnabled()) {
    EXPECT_FALSE(To<LayoutBox>(box->GetLayoutObject())
                     ->GetScrollableArea()
                     ->GetNonCompositedMainThreadScrollingReasons());
  }

  // Now that #box is composited, cc reports that we do not scroll on main.
  EXPECT_WHEEL_BUCKET(cc::MainThreadScrollingReason::kNotScrollingOnMain, 1);
  EXPECT_WHEEL_TOTAL(1);
}

TEST_P(ScrollMetricsTest, NotScrollableAreaTest) {
  SetUpHtml(R"HTML(
    <style>.box { overflow:scroll; width: 100px; height: 100px; }
     .hidden { overflow: hidden; }
     .spacer { height: 1000px; }
    </style>
    <div id='box' class='box'>
     <div class='spacer'></div>
    </div>
  )HTML");

  Element* box = GetDocument().getElementById("box");
  absl::optional<HistogramTester> histogram_tester;
  histogram_tester.emplace();

  Scroll(box, WebGestureDevice::kTouchpad);

  // The below reasons are reported because #box is not composited.
  EXPECT_WHEEL_BUCKET(
      BucketIndex(cc::MainThreadScrollingReason::kNonFastScrollableRegion), 1);
  EXPECT_WHEEL_BUCKET(
      BucketIndex(cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText),
      1);
  EXPECT_WHEEL_BUCKET(
      cc::MainThreadScrollingReason::kScrollingOnMainForAnyReason, 1);
  EXPECT_WHEEL_TOTAL(3);

  // Reset histogram tester.
  histogram_tester.emplace();

  box->setAttribute("class", "hidden transform box");
  UpdateAllLifecyclePhases();
  Scroll(box, WebGestureDevice::kTouchpad);

  // The overflow: hidden element is still a non-fast scroll region, so cc
  // reports the following for the second scroll:
  //   kNonFastScrollableRegion
  //   kScrollingOnMainForAnyReason
  //
  // Since #box is overflow: hidden, the hit test returns the viewport, and
  // so we do not log kNoScrollingLayer again.
  EXPECT_WHEEL_BUCKET(
      BucketIndex(cc::MainThreadScrollingReason::kNonFastScrollableRegion), 1);
  EXPECT_WHEEL_BUCKET(
      cc::MainThreadScrollingReason::kScrollingOnMainForAnyReason, 1);
  EXPECT_WHEEL_TOTAL(2);
}

TEST_P(ScrollMetricsTest, NestedScrollersTest) {
  SetUpHtml(R"HTML(
    <style>
     .container { overflow:scroll; width: 200px; height: 200px; }
     .box { overflow:scroll; width: 100px; height: 100px; }
     /* to prevent the mock overlay scrollbar from affecting compositing. */
     .box::-webkit-scrollbar { display: none; }
     .spacer { height: 1000px; }
     .composited { will-change: transform; }
    </style>
    <div id='container' class='container with-border-radius'>
      <div class='box'>
        <div id='inner' class='composited box'>
          <div class='spacer'></div>
        </div>
        <div class='spacer'></div>
      </div>
      <div class='spacer'></div>
    </div>
  )HTML");

  Element* box = GetDocument().getElementById("inner");
  absl::optional<HistogramTester> histogram_tester;
  histogram_tester.emplace();

  Scroll(box, WebGestureDevice::kTouchpad);

  if (base::FeatureList::IsEnabled(::features::kScrollUnification)) {
    // The gesture latches to #inner, which is composited.
    EXPECT_WHEEL_BUCKET(cc::MainThreadScrollingReason::kNotScrollingOnMain, 1);
    EXPECT_WHEEL_TOTAL(1);

    histogram_tester.emplace();
    box->scrollBy(0, 1000);
    Compositor().BeginFrame();
    Scroll(box, WebGestureDevice::kTouchpad);

    // The second scroll latches to the non-composited parent.
    EXPECT_WHEEL_BUCKET(
        BucketIndex(cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText),
        1);
    EXPECT_WHEEL_BUCKET(
        cc::MainThreadScrollingReason::kScrollingOnMainForAnyReason, 1);
    EXPECT_WHEEL_TOTAL(2);
  } else {
    // Scrolling the inner box will gather reasons from the scrolling chain. The
    // inner box itself has no reason because it's composited. Other scrollable
    // areas from the chain have corresponding reasons.
    //
    // cc reports the following reasons:
    //   kNoScrollingLayer (because the parent is not composited)
    //   kScrollingOnMainForAnyReason
    //
    // Then main reports these reasons when handling the forwarded event:
    //   kNotOpaqueForTextAndLCDText (because ancestors are not composited)
    //
    EXPECT_WHEEL_BUCKET(
        BucketIndex(cc::MainThreadScrollingReason::kNoScrollingLayer), 1);
    EXPECT_WHEEL_BUCKET(
        BucketIndex(cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText),
        1);
    EXPECT_WHEEL_BUCKET(
        cc::MainThreadScrollingReason::kScrollingOnMainForAnyReason, 1);
    EXPECT_WHEEL_TOTAL(3);
  }
}

}  // namespace

}  // namespace blink
