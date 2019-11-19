// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

#define EXPECT_WHEEL_BUCKET(reason, count)     \
  histogram_tester.ExpectBucketCount(          \
      "Renderer4.MainThreadWheelScrollReason", \
      GetBucketIndex(cc::MainThreadScrollingReason::reason), count);

#define EXPECT_TOUCH_BUCKET(reason, count)       \
  histogram_tester.ExpectBucketCount(            \
      "Renderer4.MainThreadGestureScrollReason", \
      GetBucketIndex(cc::MainThreadScrollingReason::reason), count);

#define EXPECT_WHEEL_TOTAL(count)                                            \
  histogram_tester.ExpectTotalCount("Renderer4.MainThreadWheelScrollReason", \
                                    count);

#define EXPECT_TOUCH_TOTAL(count)                                              \
  histogram_tester.ExpectTotalCount("Renderer4.MainThreadGestureScrollReason", \
                                    count);

namespace blink {

namespace {

class ScrollMetricsTest : public SimTest {
 public:
  void SetUpHtml(const char*);
  void Scroll(Element*, const WebGestureDevice);
  void UpdateAllLifecyclePhases() {
    GetDocument().View()->UpdateAllLifecyclePhases(
        DocumentLifecycle::LifecycleUpdateReason::kTest);
  }
};

class NonCompositedMainThreadScrollingReasonRecordTest
    : public ScrollMetricsTest {
 protected:
  int GetBucketIndex(uint32_t reason);
};

class ScrollBeginEventBuilder : public WebGestureEvent {
 public:
  ScrollBeginEventBuilder(FloatPoint position,
                          FloatPoint delta,
                          WebGestureDevice device)
      : WebGestureEvent(WebInputEvent::kGestureScrollBegin,
                        WebInputEvent::kNoModifiers,
                        base::TimeTicks::Now(),
                        device) {
    SetPositionInWidget(position);
    SetPositionInScreen(position);
    data.scroll_begin.delta_y_hint = delta.Y();
    frame_scale_ = 1;
  }
};

class ScrollUpdateEventBuilder : public WebGestureEvent {
 public:
  ScrollUpdateEventBuilder() : WebGestureEvent() {
    type_ = WebInputEvent::kGestureScrollUpdate;
    data.scroll_update.delta_x = 0.0f;
    data.scroll_update.delta_y = 1.0f;
    data.scroll_update.velocity_x = 0;
    data.scroll_update.velocity_y = 1;
    frame_scale_ = 1;
  }
};

class ScrollEndEventBuilder : public WebGestureEvent {
 public:
  ScrollEndEventBuilder() : WebGestureEvent() {
    type_ = WebInputEvent::kGestureScrollEnd;
    frame_scale_ = 1;
  }
};

int NonCompositedMainThreadScrollingReasonRecordTest::GetBucketIndex(
    uint32_t reason) {
  int index = 1;
  while (!(reason & 1)) {
    reason >>= 1;
    ++index;
  }
  DCHECK_EQ(reason, 1u);
  return index;
}

void ScrollMetricsTest::Scroll(Element* element,
                               const WebGestureDevice device) {
  DCHECK(element);
  DCHECK(element->getBoundingClientRect());
  DOMRect* rect = element->getBoundingClientRect();
  ScrollBeginEventBuilder scroll_begin(
      FloatPoint(rect->left() + rect->width() / 2,
                 rect->top() + rect->height() / 2),
      FloatPoint(0.f, 1.f), device);
  ScrollUpdateEventBuilder scroll_update;
  ScrollEndEventBuilder scroll_end;
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(scroll_begin);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(scroll_update);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(scroll_end);
  ASSERT_GT(scroll_update.DeltaYInRootFrame(), 0);
}

void ScrollMetricsTest::SetUpHtml(const char* html_content) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(html_content);
  Compositor().BeginFrame();
}

TEST_F(NonCompositedMainThreadScrollingReasonRecordTest,
       TouchAndWheelGeneralTest) {
  SetUpHtml(R"HTML(
    <style>
     .box { overflow:scroll; width: 100px; height: 100px; }
     .translucent { opacity: 0.5; }
     .spacer { height: 1000px; }
    </style>
    <div id='box' class='translucent box'>
     <div class='spacer'></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  Element* box = GetDocument().getElementById("box");
  HistogramTester histogram_tester;

  // Test touch scroll.
  Scroll(box, WebGestureDevice::kTouchscreen);
  EXPECT_TOUCH_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_TOUCH_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);

  Scroll(box, WebGestureDevice::kTouchscreen);
  EXPECT_TOUCH_BUCKET(kHasOpacityAndLCDText, 2);
  EXPECT_TOUCH_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 2);
  EXPECT_TOUCH_TOTAL(4);

  // Test wheel scroll.
  Scroll(box, WebGestureDevice::kTouchpad);
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_TOTAL(2);
}

TEST_F(NonCompositedMainThreadScrollingReasonRecordTest,
       CompositedScrollableAreaTest) {
  SetUpHtml(R"HTML(
    <style>
     .box { overflow:scroll; width: 100px; height: 100px; }
     .translucent { opacity: 0.5; }
     .composited { will-change: transform; }
     .spacer { height: 1000px; }
    </style>
    <div id='box' class='translucent box'>
     <div class='spacer'></div>
    </div>
  )HTML");

  GetDocument().View()->SetParentVisible(true);
  GetDocument().View()->SetSelfVisible(true);
  UpdateAllLifecyclePhases();

  Element* box = GetDocument().getElementById("box");
  HistogramTester histogram_tester;

  Scroll(box, WebGestureDevice::kTouchpad);
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_TOTAL(2);

  box->setAttribute("class", "composited translucent box");
  UpdateAllLifecyclePhases();
  Scroll(box, WebGestureDevice::kTouchpad);
  EXPECT_FALSE(ToLayoutBox(box->GetLayoutObject())
                   ->GetScrollableArea()
                   ->GetNonCompositedMainThreadScrollingReasons());
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_TOTAL(2);
}

TEST_F(NonCompositedMainThreadScrollingReasonRecordTest,
       NotScrollableAreaTest) {
  SetUpHtml(R"HTML(
    <style>.box { overflow:scroll; width: 100px; height: 100px; }
     .translucent { opacity: 0.5; }
     .hidden { overflow: hidden; }
     .spacer { height: 1000px; }
    </style>
    <div id='box' class='translucent box'>
     <div class='spacer'></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  Element* box = GetDocument().getElementById("box");
  HistogramTester histogram_tester;

  Scroll(box, WebGestureDevice::kTouchpad);
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_TOTAL(2);

  box->setAttribute("class", "hidden translucent box");
  UpdateAllLifecyclePhases();
  Scroll(box, WebGestureDevice::kTouchpad);
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_TOTAL(2);
}

TEST_F(NonCompositedMainThreadScrollingReasonRecordTest, NestedScrollersTest) {
  SetUpHtml(R"HTML(
    <style>
     .container { overflow:scroll; width: 200px; height: 200px; }
     .box { overflow:scroll; width: 100px; height: 100px; }
     .translucent { opacity: 0.5; }
     .transform { transform: scale(0.8); }
     .spacer { height: 1000px; }
     .composited { will-change: transform; }
    </style>
    <div id='container' class='container with-border-radius'>
      <div class='translucent box'>
        <div id='inner' class='composited transform box'>
          <div class='spacer'></div>
        </div>
        <div class='spacer'></div>
      </div>
      <div class='spacer'></div>
    </div>
  )HTML");

  GetDocument().View()->SetParentVisible(true);
  GetDocument().View()->SetSelfVisible(true);
  UpdateAllLifecyclePhases();

  Element* box = GetDocument().getElementById("inner");
  HistogramTester histogram_tester;

  Scroll(box, WebGestureDevice::kTouchpad);
  // Scrolling the inner box will gather reasons from the scrolling chain. The
  // inner box itself has no reason because it's composited. Other scrollable
  // areas from the chain have corresponding reasons.
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kIsNotStackingContextAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kHasTransformAndLCDText, 0);
  EXPECT_WHEEL_TOTAL(3);
}

}  // namespace

}  // namespace blink
