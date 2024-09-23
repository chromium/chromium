// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/delegated_ink/delegated_ink_trail_presenter.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ink_trail_style.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "ui/gfx/delegated_ink_metadata.h"

namespace blink {
namespace {

class TestDelegatedInkMetadata {
 public:
  explicit TestDelegatedInkMetadata(gfx::DelegatedInkMetadata* metadata)
      : point_(metadata->point()),
        color_(metadata->color()),
        diameter_(metadata->diameter()),
        area_(metadata->presentation_area()),
        is_hovering_(metadata->is_hovering()) {}
  explicit TestDelegatedInkMetadata(gfx::RectF area,
                                    float device_pixel_ratio = 1.0)
      : area_(area) {
    area_.Scale(device_pixel_ratio);
  }
  TestDelegatedInkMetadata() = default;

  void ExpectEqual(TestDelegatedInkMetadata actual) const {
    // LayoutUnits cast floats to ints, causing the actual point and area to be
    // off a small amount from what is expected.
    EXPECT_NEAR(point_.x(), actual.point_.x(), LayoutUnit::Epsilon());
    EXPECT_NEAR(point_.y(), actual.point_.y(), LayoutUnit::Epsilon());
    EXPECT_EQ(color_, actual.color_);
    EXPECT_EQ(diameter_, actual.diameter_);
    EXPECT_NEAR(area_.x(), actual.area_.x(), LayoutUnit::Epsilon());
    EXPECT_NEAR(area_.y(), actual.area_.y(), LayoutUnit::Epsilon());
    EXPECT_NEAR(area_.width(), actual.area_.width(), LayoutUnit::Epsilon());
    EXPECT_NEAR(area_.height(), actual.area_.height(), LayoutUnit::Epsilon());
    EXPECT_EQ(is_hovering_, actual.is_hovering_);
  }

  void SetPoint(gfx::PointF pt) { point_ = pt; }
  void SetColor(SkColor color) { color_ = color; }
  void SetDiameter(double diameter) { diameter_ = diameter; }
  void SetArea(gfx::RectF area) { area_ = area; }
  void SetHovering(bool hovering) { is_hovering_ = hovering; }

 private:
  gfx::PointF point_;
  SkColor color_;
  double diameter_;
  gfx::RectF area_;
  bool is_hovering_;
};

DelegatedInkTrailPresenter* CreatePresenter(Element* element,
                                            LocalFrame* frame) {
  return MakeGarbageCollected<DelegatedInkTrailPresenter>(element, frame);
}

}  // namespace

class DelegatedInkTrailPresenterUnitTest : public SimTest {
 public:
  void SetWebViewSize(float width, float height) {
    WebView().MainFrameViewWidget()->Resize(gfx::Size(width, height));
  }

  void SetWebViewSizeGreaterThanCanvas(float width, float height) {
    // The presentation area is intersected with the visible content rect, so
    // make sure that the page size is larger than the canvas to ensure it
    // isn't clipped. Adding 1 to the height and width is enough to ensure that
    // doesn't happen.
    SetWebViewSize(width + 1, height + 1);
  }

  PointerEvent* CreatePointerMoveEvent(gfx::PointF pt, bool hovering) {
    PointerEventInit* init = PointerEventInit::Create();
    init->setClientX(pt.x());
    init->setClientY(pt.y());
    if (!hovering) {
      init->setButtons(MouseEvent::WebInputEventModifiersToButtons(
          WebInputEvent::Modifiers::kLeftButtonDown));
    }
    init->setView(&Window());
    PointerEvent* event =
        PointerEvent::Create(event_type_names::kPointermove, init);
    event->SetTrusted(true);
    return event;
  }

  TestDelegatedInkMetadata GetActualMetadata() {
    return TestDelegatedInkMetadata(WebView()
                                        .MainFrameViewWidget()
                                        ->LayerTreeHostForTesting()
                                        ->DelegatedInkMetadataForTesting());
  }

  void SetLayoutZoomFactor(const float zoom) {
    GetDocument().GetFrame()->SetLayoutZoomFactor(zoom);
  }
};

// Test scenarios with the presentation area extending beyond the edges of the
// window to confirm it gets clipped correctly.
class DelegatedInkTrailPresenterCanvasBeyondViewport
    : public DelegatedInkTrailPresenterUnitTest,
      public testing::WithParamInterface<bool> {
 public:
  bool CanvasShouldBePastViewport() { return GetParam(); }
  float GetViewportWidth() const { return kViewportWidth; }
  float GetViewportHeight() const { return kViewportHeight; }
  void SetWebViewSize() {
    DelegatedInkTrailPresenterUnitTest::SetWebViewSize(kViewportWidth,
                                                       kViewportHeight);
  }

 private:
  const float kViewportWidth = 175.f;
  const float kViewportHeight = 180.f;
};

// Confirm that all the information is collected and transformed correctly, if
// necessary. Numbers and color used were chosen arbitrarily.
TEST_P(DelegatedInkTrailPresenterCanvasBeyondViewport,
       CollectAndPropagateMetadata) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    canvas {
      width: 191px;
      height: 234px;
    }
    </style>
    <canvas id='canvas'></canvas>
  )HTML");

  Compositor().BeginFrame();

  const float kCanvasWidth = 191.f;
  const float kCanvasHeight = 234.f;

  TestDelegatedInkMetadata expected_metadata;

  if (!CanvasShouldBePastViewport()) {
    SetWebViewSizeGreaterThanCanvas(kCanvasWidth, kCanvasHeight);
    expected_metadata.SetArea(gfx::RectF(0, 0, kCanvasWidth, kCanvasHeight));
  } else {
    SetWebViewSize();
    expected_metadata.SetArea(
        gfx::RectF(0, 0, GetViewportWidth(), GetViewportHeight()));
  }

  DelegatedInkTrailPresenter* presenter =
      CreatePresenter(GetDocument().getElementById(AtomicString("canvas")),
                      GetDocument().GetFrame());
  DCHECK(presenter);

  InkTrailStyle* style = MakeGarbageCollected<InkTrailStyle>();
  style->setDiameter(5);
  style->setColor("blue");
  expected_metadata.SetDiameter(style->diameter());
  expected_metadata.SetColor(SK_ColorBLUE);

  DummyExceptionStateForTesting exception_state;
  gfx::PointF pt(100, 100);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(GetDocument().GetFrame()),
      CreatePointerMoveEvent(pt, /*hovering*/ true), style, exception_state);
  expected_metadata.SetHovering(true);
  expected_metadata.SetPoint(pt);

  expected_metadata.ExpectEqual(GetActualMetadata());
}

// Confirm that everything is still calculated correctly when the
// DevicePixelRatio is not 1. Numbers and color used were chosen arbitrarily.
TEST_P(DelegatedInkTrailPresenterCanvasBeyondViewport,
       NotDefaultDevicePixelRatio) {
  const float kZoom = 1.7;
  SetLayoutZoomFactor(kZoom);

  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    canvas {
      width: 281px;
      height: 190px;
    }
    </style>
    <canvas id='canvas'></canvas>
  )HTML");

  Compositor().BeginFrame();

  const float kCanvasWidth = 281.f;
  const float kCanvasHeight = 190.f;

  TestDelegatedInkMetadata expected_metadata;

  if (!CanvasShouldBePastViewport()) {
    SetWebViewSizeGreaterThanCanvas(kCanvasWidth * kZoom,
                                    kCanvasHeight * kZoom);
    expected_metadata = TestDelegatedInkMetadata(
        gfx::RectF(0, 0, kCanvasWidth, kCanvasHeight), kZoom);
  } else {
    SetWebViewSize();
    expected_metadata.SetArea(
        gfx::RectF(0, 0, GetViewportWidth(), GetViewportHeight()));
  }

  DelegatedInkTrailPresenter* presenter =
      CreatePresenter(GetDocument().getElementById(AtomicString("canvas")),
                      GetDocument().GetFrame());
  DCHECK(presenter);

  InkTrailStyle* style = MakeGarbageCollected<InkTrailStyle>();
  style->setDiameter(101.5);
  style->setColor("magenta");
  expected_metadata.SetDiameter(style->diameter() * kZoom);
  expected_metadata.SetColor(SK_ColorMAGENTA);

  DummyExceptionStateForTesting exception_state;
  gfx::PointF pt(87, 113);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(GetDocument().GetFrame()),
      CreatePointerMoveEvent(pt, /*hovering*/ true), style, exception_state);
  expected_metadata.SetHovering(true);
  pt.Scale(kZoom);
  expected_metadata.SetPoint(pt);

  expected_metadata.ExpectEqual(GetActualMetadata());
}

// Confirm that everything is still calculated correctly when the
// PageScaleFactor is not 1. Numbers and color used were chosen arbitrarily.
TEST_P(DelegatedInkTrailPresenterCanvasBeyondViewport,
       NotDefaultPageScaleFactor) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    canvas {
      width: 281px;
      height: 190px;
    }
    </style>
    <canvas id='canvas'></canvas>
  )HTML");

  Compositor().BeginFrame();

  const float kCanvasWidth = 281.f;
  const float kCanvasHeight = 190.f;

  TestDelegatedInkMetadata expected_metadata;
  const float kScale = 2.5;
  if (!CanvasShouldBePastViewport()) {
    SetWebViewSizeGreaterThanCanvas(kCanvasWidth * kScale,
                                    kCanvasHeight * kScale);
    expected_metadata = TestDelegatedInkMetadata(
        gfx::RectF(0, 0, kCanvasWidth, kCanvasHeight), kScale);
  } else {
    SetWebViewSize();
    expected_metadata.SetArea(
        gfx::RectF(0, 0, GetViewportWidth(), GetViewportHeight()));
  }
  GetDocument().GetPage()->SetPageScaleFactor(kScale);

  DelegatedInkTrailPresenter* presenter =
      CreatePresenter(GetDocument().getElementById(AtomicString("canvas")),
                      GetDocument().GetFrame());
  DCHECK(presenter);

  InkTrailStyle* style = MakeGarbageCollected<InkTrailStyle>();
  style->setDiameter(101.5);
  style->setColor("magenta");
  expected_metadata.SetDiameter(style->diameter() * kScale);
  expected_metadata.SetColor(SK_ColorMAGENTA);

  DummyExceptionStateForTesting exception_state;
  gfx::PointF pt(87, 113);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(GetDocument().GetFrame()),
      CreatePointerMoveEvent(pt, /*hovering*/ true), style, exception_state);
  expected_metadata.SetHovering(true);
  pt.Scale(kScale);
  expected_metadata.SetPoint(pt);

  expected_metadata.ExpectEqual(GetActualMetadata());
}

// Confirm that everything is still calculated correctly when the
// PageScaleFactor is not 1 and a scroll offset is applied.
TEST_P(DelegatedInkTrailPresenterCanvasBeyondViewport,
       NotDefaultPageScaleFactorNonZeroOffset) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    canvas {
      width: 281px;
      height: 190px;
    }
    </style>
    <canvas id='canvas'></canvas>
  )HTML");

  Compositor().BeginFrame();

  const float kCanvasWidth = 281.f;
  const float kCanvasHeight = 190.f;
  const float kOffsetX = 55.f;
  const float kOffsetY = 41.f;
  const float kScale = 2.5;

  TestDelegatedInkMetadata expected_metadata;

  if (!CanvasShouldBePastViewport()) {
    SetWebViewSizeGreaterThanCanvas(kCanvasWidth * kScale,
                                    kCanvasHeight * kScale);
    expected_metadata = TestDelegatedInkMetadata(
        gfx::RectF(0, 0, kCanvasWidth - kOffsetX, kCanvasHeight - kOffsetY),
        kScale);
  } else {
    SetWebViewSize();
    expected_metadata.SetArea(
        gfx::RectF(0, 0, GetViewportWidth(), GetViewportHeight()));
  }

  GetDocument().GetPage()->SetPageScaleFactor(kScale);
  WebView().SetVisualViewportOffset(gfx::PointF(kOffsetX, kOffsetY));

  DelegatedInkTrailPresenter* presenter =
      CreatePresenter(GetDocument().getElementById(AtomicString("canvas")),
                      GetDocument().GetFrame());
  DCHECK(presenter);

  DummyExceptionStateForTesting exception_state;
  InkTrailStyle* style = MakeGarbageCollected<InkTrailStyle>();
  style->setDiameter(101.5);
  style->setColor("magenta");
  gfx::PointF pt(87, 113);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(GetDocument().GetFrame()),
      CreatePointerMoveEvent(pt, /*hovering*/ true), style, exception_state);

  gfx::PointF expected_point = pt;
  expected_point.Offset(-kOffsetX, -kOffsetY);
  expected_point.Scale(kScale);
  expected_metadata.SetPoint(expected_point);
  expected_metadata.SetHovering(true);
  expected_metadata.SetDiameter(style->diameter() * kScale);
  expected_metadata.SetColor(SK_ColorMAGENTA);

  expected_metadata.ExpectEqual(GetActualMetadata());
}

// Confirm that the offset is correct. Numbers and color used were chosen
// arbitrarily.
TEST_P(DelegatedInkTrailPresenterCanvasBeyondViewport, CanvasNotAtOrigin) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    canvas {
      width: 250px;
      height: 350px;
      position: fixed;
      top: 59px;
      left: 16px;
    }
    </style>
    <canvas id='canvas'></canvas>
  )HTML");

  Compositor().BeginFrame();

  const float kCanvasWidth = 250.f;
  const float kCanvasHeight = 350.f;
  const float kCanvasTopOffset = 59.f;
  const float kCanvasLeftOffset = 16.f;

  TestDelegatedInkMetadata expected_metadata;

  if (!CanvasShouldBePastViewport()) {
    SetWebViewSizeGreaterThanCanvas(kCanvasWidth + kCanvasLeftOffset,
                                    kCanvasHeight + kCanvasTopOffset);
    expected_metadata.SetArea(gfx::RectF(kCanvasLeftOffset, kCanvasTopOffset,
                                         kCanvasWidth, kCanvasHeight));
  } else {
    SetWebViewSize();
    expected_metadata.SetArea(
        gfx::RectF(kCanvasLeftOffset, kCanvasTopOffset,
                   GetViewportWidth() - kCanvasLeftOffset,
                   GetViewportHeight() - kCanvasTopOffset));
  }

  DelegatedInkTrailPresenter* presenter =
      CreatePresenter(GetDocument().getElementById(AtomicString("canvas")),
                      GetDocument().GetFrame());
  DCHECK(presenter);

  InkTrailStyle* style = MakeGarbageCollected<InkTrailStyle>();
  style->setDiameter(8.6);
  style->setColor("red");
  expected_metadata.SetDiameter(style->diameter());
  expected_metadata.SetColor(SK_ColorRED);

  DummyExceptionStateForTesting exception_state;
  gfx::PointF pt(380, 175);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(GetDocument().GetFrame()),
      CreatePointerMoveEvent(pt, /*hovering*/ false), style, exception_state);
  expected_metadata.SetHovering(false);
  expected_metadata.SetPoint(pt);

  expected_metadata.ExpectEqual(GetActualMetadata());
}

// Confirm that values, specifically offsets, are transformed correctly when
// the canvas is in an iframe. Numbers and color used were chosen arbitrarily.
TEST_P(DelegatedInkTrailPresenterCanvasBeyondViewport, CanvasInIFrame) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    #iframe {
      width: 500px;
      height: 500px;
      position: fixed;
      top: 26px;
      left: 57px;
    }
    </style>
    <iframe id='iframe' src='https://example.com/iframe.html'>
    </iframe>
  )HTML");

  frame_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    canvas {
      width: 250px;
      height: 250px;
      position: fixed;
      top: 33px;
      left: 16px;
    }
    </style>
    <canvas id='canvas'></canvas>
  )HTML");

  Compositor().BeginFrame();

  // When creating the expected metadata, we have to take into account the
  // offsets that are applied to the iframe that the canvas is in, and the 2px
  // border around the iframe.
  const float kIframeBorder = 2.f;
  const float kIframeLeftOffset = 57.f + kIframeBorder;
  const float kIframeTopOffset = 26.f + kIframeBorder;
  const float kIframeHeight = 500.f;
  const float kIframeWidth = 500.f;
  const float kCanvasLeftOffset = 16.f;
  const float kCanvasTopOffset = 33.f;
  const float kCanvasHeight = 250.f;
  const float kCanvasWidth = 250.f;

  TestDelegatedInkMetadata expected_metadata;

  if (!CanvasShouldBePastViewport()) {
    SetWebViewSizeGreaterThanCanvas(kIframeWidth + kIframeLeftOffset,
                                    kIframeHeight + kIframeTopOffset);
    expected_metadata.SetArea(gfx::RectF(kIframeLeftOffset + kCanvasLeftOffset,
                                         kIframeTopOffset + kCanvasTopOffset,
                                         kCanvasWidth, kCanvasHeight));
  } else {
    SetWebViewSize();
    expected_metadata.SetArea(gfx::RectF(
        kIframeLeftOffset + kCanvasLeftOffset,
        kIframeTopOffset + kCanvasTopOffset,
        GetViewportWidth() - (kIframeLeftOffset + kCanvasLeftOffset),
        GetViewportHeight() - (kIframeTopOffset + kCanvasTopOffset)));
  }

  auto* iframe_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("iframe")));
  auto* iframe_localframe = To<LocalFrame>(iframe_element->ContentFrame());
  Document* iframe_document = iframe_element->contentDocument();

  DelegatedInkTrailPresenter* presenter = CreatePresenter(
      iframe_localframe->GetDocument()->getElementById(AtomicString("canvas")),
      iframe_document->GetFrame());
  DCHECK(presenter);

  InkTrailStyle* style = MakeGarbageCollected<InkTrailStyle>();
  style->setDiameter(0.3);
  style->setColor("cyan");
  expected_metadata.SetDiameter(style->diameter());
  expected_metadata.SetColor(SK_ColorCYAN);

  DummyExceptionStateForTesting exception_state;
  gfx::PointF pt(380, 375);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(iframe_document->GetFrame()),
      CreatePointerMoveEvent(pt, /*hovering*/ false), style, exception_state);
  expected_metadata.SetHovering(false);
  expected_metadata.SetPoint(
      gfx::PointF(pt.x() + kIframeLeftOffset, pt.y() + kIframeTopOffset));

  expected_metadata.ExpectEqual(GetActualMetadata());
}

// Confirm that values, specifically offsets, are transformed correctly when
// the canvas is in a nested iframe. Numbers and color used were chosen
// arbitrarily.
TEST_P(DelegatedInkTrailPresenterCanvasBeyondViewport, NestedIframe) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  SimRequest frame2_resource("https://example.com/iframe2.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    #OuterIframe {
      width: 500px;
      height: 500px;
      position: fixed;
      top: 26px;
      left: 57px;
    }
    </style>
    <iframe id='OuterIframe' src='https://example.com/iframe.html'>
    </iframe>
  )HTML");

  frame_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    #InnerIframe {
      width: 400px;
      height: 400px;
      position: fixed;
      top: 11px;
      left: 18px;
    }
    </style>
    <iframe id='InnerIframe' src='https://example.com/iframe2.html'>
    </iframe>
    )HTML");

  frame2_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    canvas {
      width: 250px;
      height: 250px;
      position: fixed;
      top: 28px;
      left: 6px;
    }
    </style>
    <canvas id='canvas'></canvas>
  )HTML");

  Compositor().BeginFrame();

  // When creating the expected metadata, we have to take into account the
  // offsets that are applied to the iframe that the canvas is in, and the 2px
  // border around the iframe.
  const float kIframeBorder = 2.f;
  const float kOuterIframeLeftOffset = 57.f + kIframeBorder;
  const float kOuterIframeTopOffset = 26.f + kIframeBorder;
  const float kOuterIframeHeight = 500.f;
  const float kOuterIframeWidth = 500.f;
  const float kInnerIframeLeftOffset =
      kOuterIframeLeftOffset + 18.f + kIframeBorder;
  const float kInnerIframeTopOffset =
      kOuterIframeTopOffset + 11.f + kIframeBorder;
  const float kCanvasLeftOffset = 6.f;
  const float kCanvasTopOffset = 28.f;
  const float kCanvasHeight = 250.f;
  const float kCanvasWidth = 250.f;

  TestDelegatedInkMetadata expected_metadata;

  if (!CanvasShouldBePastViewport()) {
    SetWebViewSizeGreaterThanCanvas(kOuterIframeWidth + kOuterIframeLeftOffset,
                                    kOuterIframeHeight + kOuterIframeTopOffset);
    expected_metadata.SetArea(gfx::RectF(
        kInnerIframeLeftOffset + kCanvasLeftOffset,
        kInnerIframeTopOffset + kCanvasTopOffset, kCanvasWidth, kCanvasHeight));
  } else {
    SetWebViewSize();
    expected_metadata.SetArea(gfx::RectF(
        kInnerIframeLeftOffset + kCanvasLeftOffset,
        kInnerIframeTopOffset + kCanvasTopOffset,
        GetViewportWidth() - (kInnerIframeLeftOffset + kCanvasLeftOffset),
        GetViewportHeight() - (kInnerIframeTopOffset + kCanvasTopOffset)));
  }

  auto* outer_iframe_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("OuterIframe")));
  auto* inner_iframe_element = To<HTMLIFrameElement>(
      outer_iframe_element->contentDocument()->getElementById(
          AtomicString("InnerIframe")));
  auto* iframe_localframe =
      To<LocalFrame>(inner_iframe_element->ContentFrame());
  Document* iframe_document = inner_iframe_element->contentDocument();

  DelegatedInkTrailPresenter* presenter = CreatePresenter(
      iframe_localframe->GetDocument()->getElementById(AtomicString("canvas")),
      iframe_document->GetFrame());
  DCHECK(presenter);

  InkTrailStyle* style = MakeGarbageCollected<InkTrailStyle>();
  style->setDiameter(100000.3);
  style->setColor("yellow");
  expected_metadata.SetDiameter(style->diameter());
  expected_metadata.SetColor(SK_ColorYELLOW);

  DummyExceptionStateForTesting exception_state;
  gfx::PointF pt(350, 375);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(iframe_document->GetFrame()),
      CreatePointerMoveEvent(pt, /*hovering*/ true), style, exception_state);
  expected_metadata.SetHovering(true);
  expected_metadata.SetPoint(gfx::PointF(pt.x() + kInnerIframeLeftOffset,
                                         pt.y() + kInnerIframeTopOffset));

  expected_metadata.ExpectEqual(GetActualMetadata());
}

// Confirm that values are correct when an iframe is used and presentation area
// isn't provided. Numbers and color used were chosen arbitrarily.
TEST_P(DelegatedInkTrailPresenterCanvasBeyondViewport,
       IFrameNoPresentationArea) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    #iframe {
      width: 500px;
      height: 500px;
      position: fixed;
      top: 56px;
      left: 72px;
    }
    </style>
    <iframe id='iframe' src='https://example.com/iframe.html'>
    </iframe>
  )HTML");

  frame_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    </style>
  )HTML");

  Compositor().BeginFrame();

  // When creating the expected metadata, we have to take into account the
  // offsets that are applied to the iframe, and the 2px border.
  const float kIframeBorder = 2.f;
  const float kIframeLeftOffset = 72.f + kIframeBorder;
  const float kIframeTopOffset = 56.f + kIframeBorder;
  const float kIframeHeight = 500.f;
  const float kIframeWidth = 500.f;

  TestDelegatedInkMetadata expected_metadata;

  if (!CanvasShouldBePastViewport()) {
    SetWebViewSizeGreaterThanCanvas(kIframeWidth + kIframeLeftOffset,
                                    kIframeHeight + kIframeTopOffset);
    expected_metadata.SetArea(gfx::RectF(kIframeLeftOffset, kIframeTopOffset,
                                         kIframeWidth, kIframeHeight));
  } else {
    SetWebViewSize();
    expected_metadata.SetArea(
        gfx::RectF(kIframeLeftOffset, kIframeTopOffset,
                   GetViewportWidth() - kIframeLeftOffset,
                   GetViewportHeight() - kIframeTopOffset));
  }

  Document* iframe_document =
      To<HTMLIFrameElement>(
          GetDocument().getElementById(AtomicString("iframe")))
          ->contentDocument();

  DelegatedInkTrailPresenter* presenter =
      CreatePresenter(nullptr, iframe_document->GetFrame());
  DCHECK(presenter);

  InkTrailStyle* style = MakeGarbageCollected<InkTrailStyle>();
  style->setDiameter(0.01);
  style->setColor("white");
  expected_metadata.SetDiameter(style->diameter());
  expected_metadata.SetColor(SK_ColorWHITE);

  DummyExceptionStateForTesting exception_state;
  gfx::PointF pt(380, 375);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(iframe_document->GetFrame()),
      CreatePointerMoveEvent(pt, /*hovering*/ true), style, exception_state);
  expected_metadata.SetHovering(true);
  expected_metadata.SetPoint(
      gfx::PointF(pt.x() + kIframeLeftOffset, pt.y() + kIframeTopOffset));

  expected_metadata.ExpectEqual(GetActualMetadata());
}

INSTANTIATE_TEST_SUITE_P(,
                         DelegatedInkTrailPresenterCanvasBeyondViewport,
                         testing::Bool());

// Confirm that presentation area defaults to the size of the viewport.
// Numbers and color used were chosen arbitrarily.
TEST_F(DelegatedInkTrailPresenterUnitTest, PresentationAreaNotProvided) {
  LoadURL("about:blank");
  Compositor().BeginFrame();

  const int kViewportHeight = 555;
  const int kViewportWidth = 333;
  SetWebViewSize(kViewportWidth, kViewportHeight);

  DelegatedInkTrailPresenter* presenter =
      CreatePresenter(nullptr, GetDocument().GetFrame());
  DCHECK(presenter);

  TestDelegatedInkMetadata expected_metadata(
      gfx::RectF(0, 0, kViewportWidth, kViewportHeight));

  InkTrailStyle* style = MakeGarbageCollected<InkTrailStyle>();
  style->setDiameter(3.6);
  style->setColor("yellow");
  expected_metadata.SetDiameter(style->diameter());
  expected_metadata.SetColor(SK_ColorYELLOW);

  DummyExceptionStateForTesting exception_state;
  gfx::PointF pt(70, 109);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(GetDocument().GetFrame()),
      CreatePointerMoveEvent(pt, /*hovering*/ false), style, exception_state);
  expected_metadata.SetHovering(false);
  expected_metadata.SetPoint(pt);

  expected_metadata.ExpectEqual(GetActualMetadata());
}

// Test that the presentation area is clipped correctly by the dimensions of
// the iframe, even when the iframe and canvas each fit entirely within the
// visual viewport.
TEST_F(DelegatedInkTrailPresenterUnitTest, CanvasExtendsOutsideOfIframe) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    #iframe {
      width: 150px;
      height: 150px;
      position: fixed;
      top: 13px;
      left: 19px;
    }
    </style>
    <iframe id='iframe' src='https://example.com/iframe.html'>
    </iframe>
  )HTML");

  frame_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    canvas {
      width: 199px;
      height: 202px;
    }
    </style>
    <canvas id='canvas'></canvas>
  )HTML");

  Compositor().BeginFrame();

  // When creating the expected metadata, we have to take into account the
  // offsets that are applied to the iframe that the canvas is in, and the 2px
  // border around the iframe.
  const float kIframeBorder = 2.f;
  const float kIframeLeftOffset = 19.f + kIframeBorder;
  const float kIframeTopOffset = 13.f + kIframeBorder;
  const float kIframeHeight = 150.f;
  const float kIframeWidth = 150.f;
  const float kCanvasHeight = 202.f;
  const float kCanvasWidth = 199.f;

  // Ensure that the webpage is larger than the iframe and canvas.
  SetWebViewSize(kCanvasWidth + kIframeLeftOffset + 1,
                 kCanvasHeight + kIframeTopOffset + 1);

  TestDelegatedInkMetadata expected_metadata(gfx::RectF(
      kIframeLeftOffset, kIframeTopOffset, kIframeWidth, kIframeHeight));

  auto* iframe_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("iframe")));
  auto* iframe_localframe = To<LocalFrame>(iframe_element->ContentFrame());
  Document* iframe_document = iframe_element->contentDocument();

  DelegatedInkTrailPresenter* presenter = CreatePresenter(
      iframe_localframe->GetDocument()->getElementById(AtomicString("canvas")),
      iframe_document->GetFrame());
  DCHECK(presenter);

  InkTrailStyle* style = MakeGarbageCollected<InkTrailStyle>();
  style->setDiameter(99.999);
  style->setColor("lime");
  expected_metadata.SetDiameter(style->diameter());
  expected_metadata.SetColor(SK_ColorGREEN);

  DummyExceptionStateForTesting exception_state;
  gfx::PointF pt(102, 67);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(iframe_document->GetFrame()),
      CreatePointerMoveEvent(pt, /*hovering*/ false), style, exception_state);
  expected_metadata.SetHovering(false);
  expected_metadata.SetPoint(
      gfx::PointF(pt.x() + kIframeLeftOffset, pt.y() + kIframeTopOffset));

  expected_metadata.ExpectEqual(GetActualMetadata());
}

// Test that the presentation area is clipped correctly when it is offset left
// and above the iframe boundaries.
TEST_F(DelegatedInkTrailPresenterUnitTest, CanvasLeftAndAboveIframeBoundaries) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    #iframe {
      width: 300px;
      height: 301px;
      position: fixed;
      top: 13px;
      left: 19px;
    }
    </style>
    <iframe id='iframe' src='https://example.com/iframe.html'>
    </iframe>
  )HTML");

  frame_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    canvas {
      width: 189px;
      height: 145px;
      position: fixed;
      top: -70px;
      left: -99px;
    }
    </style>
    <canvas id='canvas'></canvas>
  )HTML");

  Compositor().BeginFrame();

  // When creating the expected metadata, we have to take into account the
  // offsets that are applied to the iframe that the canvas is in, and the 2px
  // border around the iframe.
  const float kIframeBorder = 2.f;
  const float kIframeLeftOffset = 19.f + kIframeBorder;
  const float kIframeTopOffset = 13.f + kIframeBorder;
  const float kIframeHeight = 301.f;
  const float kIframeWidth = 300.f;
  const float kCanvasHeight = 145.f;
  const float kCanvasWidth = 189.f;
  const float kCanvasLeftOffset = -99.f;
  const float kCanvasTopOffset = -70.f;

  // Ensure that the webpage is larger than the iframe.
  SetWebViewSize(kIframeHeight + kIframeLeftOffset + 1,
                 kIframeWidth + kIframeTopOffset + 1);

  TestDelegatedInkMetadata expected_metadata(gfx::RectF(
      kIframeLeftOffset, kIframeTopOffset, kCanvasWidth + kCanvasLeftOffset,
      kCanvasHeight + kCanvasTopOffset));

  auto* iframe_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("iframe")));
  auto* iframe_localframe = To<LocalFrame>(iframe_element->ContentFrame());
  Document* iframe_document = iframe_element->contentDocument();

  DelegatedInkTrailPresenter* presenter = CreatePresenter(
      iframe_localframe->GetDocument()->getElementById(AtomicString("canvas")),
      iframe_document->GetFrame());
  DCHECK(presenter);

  InkTrailStyle* style = MakeGarbageCollected<InkTrailStyle>();
  style->setDiameter(99.999);
  style->setColor("lime");
  expected_metadata.SetDiameter(style->diameter());
  expected_metadata.SetColor(SK_ColorGREEN);

  DummyExceptionStateForTesting exception_state;
  gfx::PointF pt(102, 67);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(iframe_document->GetFrame()),
      CreatePointerMoveEvent(pt, /*hovering*/ true), style, exception_state);
  expected_metadata.SetHovering(true);
  expected_metadata.SetPoint(
      gfx::PointF(pt.x() + kIframeLeftOffset, pt.y() + kIframeTopOffset));

  expected_metadata.ExpectEqual(GetActualMetadata());
}

// Confirm that values, specifically presentation area, are transformed
// correctly when the iframe that the canvas is in is clipped by its parent
// iframe. Numbers and color used were chosen arbitrarily.
TEST_F(DelegatedInkTrailPresenterUnitTest, OuterIframeClipsInnerIframe) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  SimRequest frame2_resource("https://example.com/iframe2.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    #OuterIframe {
      width: 500px;
      height: 500px;
      position: fixed;
      top: 26px;
      left: 57px;
    }
    </style>
    <iframe id='OuterIframe' src='https://example.com/iframe.html'>
    </iframe>
  )HTML");

  frame_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    #InnerIframe {
      width: 400px;
      height: 400px;
      position: fixed;
      top: 311px;
      left: 334px;
    }
    </style>
    <iframe id='InnerIframe' src='https://example.com/iframe2.html'>
    </iframe>
    )HTML");

  frame2_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    canvas {
      width: 250px;
      height: 250px;
      position: fixed;
      top: 1px;
      left: 2px;
    }
    </style>
    <canvas id='canvas'></canvas>
  )HTML");

  Compositor().BeginFrame();

  // When creating the expected metadata, we have to take into account the
  // offsets that are applied to the iframe that the canvas is in, and the 2px
  // border around the iframe.
  const float kIframeBorder = 2.f;
  const float kOuterIframeLeftOffset = 57.f + kIframeBorder;
  const float kOuterIframeTopOffset = 26.f + kIframeBorder;
  const float kOuterIframeHeight = 500.f;
  const float kOuterIframeWidth = 500.f;
  const float kInnerIframeLeftOffset =
      kOuterIframeLeftOffset + 334.f + kIframeBorder;
  const float kInnerIframeTopOffset =
      kOuterIframeTopOffset + 311.f + kIframeBorder;
  const float kCanvasLeftOffset = 2.f;
  const float kCanvasTopOffset = 1.f;

  // Ensure that the webpage is larger than the iframe and canvas.
  const float kViewportWidth = kOuterIframeWidth + kOuterIframeLeftOffset + 1.f;
  const float kViewportHeight =
      kOuterIframeHeight + kOuterIframeTopOffset + 1.f;
  SetWebViewSize(kViewportWidth, kViewportHeight);

  TestDelegatedInkMetadata expected_metadata(
      gfx::RectF(kInnerIframeLeftOffset + kCanvasLeftOffset,
                 kInnerIframeTopOffset + kCanvasTopOffset,
                 kOuterIframeWidth + kOuterIframeLeftOffset -
                     kInnerIframeLeftOffset - kCanvasLeftOffset,
                 kOuterIframeHeight + kOuterIframeTopOffset -
                     kInnerIframeTopOffset - kCanvasTopOffset));

  auto* outer_iframe_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("OuterIframe")));
  auto* inner_iframe_element = To<HTMLIFrameElement>(
      outer_iframe_element->contentDocument()->getElementById(
          AtomicString("InnerIframe")));
  auto* iframe_localframe =
      To<LocalFrame>(inner_iframe_element->ContentFrame());
  Document* iframe_document = inner_iframe_element->contentDocument();

  DelegatedInkTrailPresenter* presenter = CreatePresenter(
      iframe_localframe->GetDocument()->getElementById(AtomicString("canvas")),
      iframe_document->GetFrame());
  DCHECK(presenter);

  InkTrailStyle* style = MakeGarbageCollected<InkTrailStyle>();
  style->setDiameter(19);
  style->setColor("red");
  expected_metadata.SetDiameter(style->diameter());
  expected_metadata.SetColor(SK_ColorRED);

  DummyExceptionStateForTesting exception_state;
  gfx::PointF pt(357, 401);
  presenter->updateInkTrailStartPoint(
      ToScriptStateForMainWorld(iframe_document->GetFrame()),
      CreatePointerMoveEvent(pt, /*hovering*/ false), style, exception_state);
  expected_metadata.SetHovering(false);
  expected_metadata.SetPoint(gfx::PointF(pt.x() + kInnerIframeLeftOffset,
                                         pt.y() + kInnerIframeTopOffset));

  expected_metadata.ExpectEqual(GetActualMetadata());
}

}  // namespace blink
