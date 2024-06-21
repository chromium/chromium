// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink_module.h"

#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "pdf/ink/ink_affine_transform.h"
#include "pdf/ink/ink_brush.h"
#include "pdf/pdf_features.h"
#include "pdf/pdf_ink_brush.h"
#include "pdf/pdf_ink_transform.h"
#include "pdf/test/mouse_event_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

using testing::ElementsAre;
using testing::Pair;

namespace chrome_pdf {

namespace {

// Optional parameters that the `setAnnotationBrushMessage` may have, depending
// on the brush type.
struct AnnotationBrushMessageParams {
  int color_r;
  int color_g;
  int color_b;
  double size;
};

// Constants to support a layout of 2 pages, arranged vertically with a small
// gap between them.
constexpr gfx::RectF kVerticalLayout2Pages[] = {
    gfx::RectF(/*x=*/5.0f,
               /*y=*/5.0f,
               /*width=*/50.0f,
               /*height=*/60.0f),
    gfx::RectF(/*x=*/5.0f,
               /*y=*/70.0f,
               /*width=*/50.0f,
               /*height=*/60.0f),
};

// Some commonly used points in relation to `kVerticalLayout2Pages`.
constexpr gfx::PointF kTwoPageVerticalLayoutPointOutsidePages(10.0f, 0.0f);
constexpr gfx::PointF kTwoPageVerticalLayoutPoint1InsidePage0(10.0f, 10.0f);
constexpr gfx::PointF kTwoPageVerticalLayoutPoint2InsidePage0(15.0f, 15.0f);
constexpr gfx::PointF kTwoPageVerticalLayoutPoint3InsidePage0(20.0f, 15.0f);
constexpr gfx::PointF kTwoPageVerticalLayoutPoint1InsidePage1(10.0f, 75.0f);
constexpr gfx::PointF kTwoPageVerticalLayoutPoint2InsidePage1(15.0f, 80.0f);
constexpr gfx::PointF kTwoPageVerticalLayoutPoint3InsidePage1(20.0f, 80.0f);

class FakeClient : public InkModule::Client {
 public:
  FakeClient() = default;
  FakeClient(const FakeClient&) = delete;
  FakeClient& operator=(const FakeClient&) = delete;
  ~FakeClient() override = default;

  // InkModule::Client:
  PageOrientation GetOrientation() const override { return orientation_; }

  gfx::Vector2dF GetViewportOriginOffset() override {
    return viewport_origin_offset_;
  }

  gfx::Rect GetPageContentsRect(int index) override {
    CHECK_GE(index, 0);
    CHECK_LT(static_cast<size_t>(index), page_layouts_.size());
    return gfx::ToEnclosedRect(page_layouts_[index]);
  }

  float GetZoom() const override { return zoom_; }

  void InkStrokeFinished() override { ++ink_stroke_finished_count_; }

  void Invalidate(const gfx::Rect& rect) override {
    invalidations_.push_back(rect);
  }

  int VisiblePageIndexFromPoint(const gfx::PointF& point) override {
    // Assumes that all pages are visible.
    for (size_t i = 0; i < page_layouts_.size(); ++i) {
      if (page_layouts_[i].Contains(point)) {
        return i;
      }
    }

    // Point is not over a page in the viewer plane.
    return -1;
  }

  int ink_stroke_finished_count() const { return ink_stroke_finished_count_; }

  const std::vector<gfx::Rect>& invalidations() const { return invalidations_; }

  // Provide the sequence of pages and the coordinates and dimensions for how
  // they are laid out in a viewer plane.  It is upon the caller to ensure the
  // positioning makes sense (e.g., pages do not overlap).
  void set_page_layouts(base::span<const gfx::RectF> page_layouts) {
    page_layouts_ = base::ToVector(page_layouts);
  }

  void set_orientation(PageOrientation orientation) {
    orientation_ = orientation;
  }

  void set_viewport_origin_offset(const gfx::Vector2dF& offset) {
    viewport_origin_offset_ = offset;
  }

  void set_zoom(float zoom) { zoom_ = zoom; }

 private:
  int ink_stroke_finished_count_ = 0;
  std::vector<gfx::RectF> page_layouts_;
  PageOrientation orientation_ = PageOrientation::kOriginal;
  gfx::Vector2dF viewport_origin_offset_;
  float zoom_ = 1.0f;
  std::vector<gfx::Rect> invalidations_;
};

class InkModuleTest : public testing::Test {
 protected:
  base::Value::Dict CreateSetAnnotationBrushMessage(
      const std::string& type,
      const AnnotationBrushMessageParams* params) {
    base::Value::Dict message;
    message.Set("type", "setAnnotationBrush");
    message.Set("brushType", type);
    if (params) {
      message.Set("colorR", params->color_r);
      message.Set("colorG", params->color_g);
      message.Set("colorB", params->color_b);
      message.Set("size", params->size);
    }
    return message;
  }

  base::Value::Dict CreateSetAnnotationModeMessage(bool enable) {
    base::Value::Dict message;
    message.Set("type", "setAnnotationMode");
    message.Set("enable", enable);
    return message;
  }

  FakeClient& client() { return client_; }
  InkModule& ink_module() { return ink_module_; }

 private:
  base::test::ScopedFeatureList feature_list_{features::kPdfInk2};

  FakeClient client_;
  InkModule ink_module_{client_};
};

TEST_F(InkModuleTest, UnknownMessage) {
  base::Value::Dict message;
  message.Set("type", "nonInkMessage");
  EXPECT_FALSE(ink_module().OnMessage(message));
}

// Verify that a set eraser message sets the annotation brush to an eraser.
TEST_F(InkModuleTest, HandleSetAnnotationBrushMessageEraser) {
  EXPECT_TRUE(ink_module().OnMessage(CreateSetAnnotationModeMessage(true)));
  EXPECT_EQ(true, ink_module().enabled());

  base::Value::Dict message =
      CreateSetAnnotationBrushMessage("eraser", nullptr);
  EXPECT_TRUE(ink_module().OnMessage(message));

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  EXPECT_FALSE(brush);
}

// Verify that a set pen message sets the annotation brush to a pen, with the
// given params.
TEST_F(InkModuleTest, HandleSetAnnotationBrushMessagePen) {
  EXPECT_TRUE(ink_module().OnMessage(CreateSetAnnotationModeMessage(true)));
  EXPECT_EQ(true, ink_module().enabled());

  AnnotationBrushMessageParams message_params{/*color_r=*/10, /*color_g=*/255,
                                              /*color_b=*/50, /*size=*/1.0};
  base::Value::Dict message =
      CreateSetAnnotationBrushMessage("pen", &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  ASSERT_TRUE(brush);

  const InkBrush& ink_brush = brush->GetInkBrush();
  EXPECT_EQ(SkColorSetRGB(10, 255, 50), ink_brush.GetColorForTesting());
  EXPECT_EQ(8.0f, ink_brush.GetSize());
  EXPECT_EQ(1.0f, ink_brush.GetOpacityForTesting());
}

// Verify that a set highlighter message sets the annotation brush to a
// highlighter, with the given params.
TEST_F(InkModuleTest, HandleSetAnnotationBrushMessageHighlighter) {
  EXPECT_TRUE(ink_module().OnMessage(CreateSetAnnotationModeMessage(true)));
  EXPECT_EQ(true, ink_module().enabled());

  AnnotationBrushMessageParams message_params{/*color_r=*/240, /*color_g=*/133,
                                              /*color_b=*/0, /*size=*/0.5};
  base::Value::Dict message =
      CreateSetAnnotationBrushMessage("highlighter", &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  ASSERT_TRUE(brush);

  const InkBrush& ink_brush = brush->GetInkBrush();
  EXPECT_EQ(SkColorSetRGB(240, 133, 0), ink_brush.GetColorForTesting());
  EXPECT_EQ(4.5f, ink_brush.GetSize());
  EXPECT_EQ(0.4f, ink_brush.GetOpacityForTesting());
}

// Verify that brushes with zero color values can be set as the annotation
// brush.
TEST_F(InkModuleTest, HandleSetAnnotationBrushMessageColorZero) {
  EXPECT_TRUE(ink_module().OnMessage(CreateSetAnnotationModeMessage(true)));
  EXPECT_EQ(true, ink_module().enabled());

  AnnotationBrushMessageParams message_params{/*color_r=*/0, /*color_g=*/0,
                                              /*color_b=*/0, /*size=*/0.5};
  base::Value::Dict message =
      CreateSetAnnotationBrushMessage("pen", &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  ASSERT_TRUE(brush);

  const InkBrush& ink_brush = brush->GetInkBrush();
  EXPECT_EQ(SkColorSetRGB(0, 0, 0), ink_brush.GetColorForTesting());
  EXPECT_EQ(4.5f, ink_brush.GetSize());
  EXPECT_EQ(1.0f, ink_brush.GetOpacityForTesting());
}

// Verify that the size of the brush is translated when the size is 0. This
// is needed because the PDF extension allows for a brush size of 0, but
// `InkBrush` cannot have a size of 0.
TEST_F(InkModuleTest, HandleSetAnnotationBrushMessageSizeZeroTranslation) {
  EXPECT_TRUE(ink_module().OnMessage(CreateSetAnnotationModeMessage(true)));
  EXPECT_EQ(true, ink_module().enabled());

  AnnotationBrushMessageParams message_params{/*color_r=*/255, /*color_g=*/255,
                                              /*color_b=*/255, /*size=*/0.0};
  base::Value::Dict message =
      CreateSetAnnotationBrushMessage("highlighter", &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  ASSERT_TRUE(brush);

  const InkBrush& ink_brush = brush->GetInkBrush();
  EXPECT_EQ(SkColorSetRGB(255, 255, 255), ink_brush.GetColorForTesting());
  EXPECT_EQ(1.0f, ink_brush.GetSize());
  EXPECT_EQ(0.4f, ink_brush.GetOpacityForTesting());
}

// Verify that the size of the brush is properly translated. The PDF extension's
// max brush size is 1, while the max for `InkBrush` will be 8.
TEST_F(InkModuleTest, HandleSetAnnotationBrushMessageSizeOneTranslation) {
  EXPECT_TRUE(ink_module().OnMessage(CreateSetAnnotationModeMessage(true)));
  EXPECT_EQ(true, ink_module().enabled());

  AnnotationBrushMessageParams message_params{/*color_r=*/255, /*color_g=*/255,
                                              /*color_b=*/255, /*size=*/1.0};
  base::Value::Dict message =
      CreateSetAnnotationBrushMessage("highlighter", &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  ASSERT_TRUE(brush);

  const InkBrush& ink_brush = brush->GetInkBrush();
  EXPECT_EQ(SkColorSetRGB(255, 255, 255), ink_brush.GetColorForTesting());
  EXPECT_EQ(8.0f, ink_brush.GetSize());
  EXPECT_EQ(0.4f, ink_brush.GetOpacityForTesting());
}

TEST_F(InkModuleTest, HandleSetAnnotationModeMessage) {
  EXPECT_FALSE(ink_module().enabled());

  base::Value::Dict message = CreateSetAnnotationModeMessage(/*enable=*/false);

  EXPECT_TRUE(ink_module().OnMessage(message));
  EXPECT_FALSE(ink_module().enabled());

  message.Set("enable", true);
  EXPECT_TRUE(ink_module().OnMessage(message));
  EXPECT_TRUE(ink_module().enabled());

  message.Set("enable", false);
  EXPECT_TRUE(ink_module().OnMessage(message));
  EXPECT_FALSE(ink_module().enabled());
}

class InkModuleStrokeTest : public InkModuleTest {
 protected:
  // Mouse locations used for `RunStrokeCheckTest()`.
  static constexpr gfx::PointF kMouseDownLocation = gfx::PointF(10.0f, 15.0f);
  static constexpr gfx::PointF kMouseMoveLocation = gfx::PointF(20.0f, 25.0f);
  static constexpr gfx::PointF kMouseUpLocation = gfx::PointF(30.0f, 17.0f);

  void InitializeSimpleSinglePageBasicLayout() {
    // Single page layout that matches visible area.
    constexpr gfx::RectF kPage(0.0f, 0.0f, 50.0f, 60.0f);
    client().set_page_layouts(base::span_from_ref(kPage));
  }

  void ApplyInkStrokeWithMousePoints(
      const gfx::PointF& mouse_down_point,
      base::span<const gfx::PointF> mouse_move_points,
      const gfx::PointF& mouse_up_point,
      bool expect_mouse_events_handled) {
    blink::WebMouseEvent mouse_down_event =
        MouseEventBuilder().CreateLeftClickAtPosition(mouse_down_point).Build();
    EXPECT_EQ(expect_mouse_events_handled,
              ink_module().HandleInputEvent(mouse_down_event));

    for (const gfx::PointF& mouse_move_point : mouse_move_points) {
      blink::WebMouseEvent mouse_move_event =
          MouseEventBuilder()
              .SetType(blink::WebInputEvent::Type::kMouseMove)
              .SetPosition(mouse_move_point)
              .Build();
      EXPECT_EQ(expect_mouse_events_handled,
                ink_module().HandleInputEvent(mouse_move_event));
    }

    blink::WebMouseEvent mouse_up_event =
        MouseEventBuilder()
            .SetType(blink::WebInputEvent::Type::kMouseUp)
            .SetPosition(mouse_up_point)
            .SetButton(blink::WebPointerProperties::Button::kLeft)
            .SetClickCount(1)
            .Build();
    EXPECT_EQ(expect_mouse_events_handled,
              ink_module().HandleInputEvent(mouse_up_event));
  }

  void RunStrokeCheckTest(bool annotation_mode_enabled) {
    EXPECT_TRUE(ink_module().OnMessage(
        CreateSetAnnotationModeMessage(annotation_mode_enabled)));
    EXPECT_EQ(annotation_mode_enabled, ink_module().enabled());

    ApplyInkStrokeWithMousePoints(
        kMouseDownLocation, base::span_from_ref(kMouseMoveLocation),
        kMouseUpLocation,
        /*expect_mouse_events_handled=*/annotation_mode_enabled);

    const int expected_count = annotation_mode_enabled ? 1 : 0;
    EXPECT_EQ(expected_count, client().ink_stroke_finished_count());
  }
};

TEST_F(InkModuleStrokeTest, NoAnnotationIfNotEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/false);
}

TEST_F(InkModuleStrokeTest, AnnotationIfEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);
}

TEST_F(InkModuleStrokeTest, CanonicalAnnotationPoints) {
  // Setup to support examining the page stroke points for a layout that is
  // more complicated than what is provide by
  // `InitializeSimpleSinglePageBasicLayout()`.  Include viewport offset,
  // scroll, rotation, and zoom.
  constexpr gfx::SizeF kPageSize(100.0f, 120.0f);
  constexpr gfx::PointF kPageOrigin(5.0f, -15.0f);
  constexpr gfx::RectF kPageLayout(kPageOrigin, kPageSize);
  client().set_page_layouts(base::span_from_ref(kPageLayout));
  client().set_orientation(PageOrientation::kClockwise180);
  client().set_zoom(2.0f);

  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  // There should be two points collected, for mouse down and a single mouse
  // move.  Verify that the collected points match a canonical position for
  // the InkModule::Client setup.
  constexpr gfx::PointF kCanonicalMouseDownPosition(47.0f, 44.5f);
  constexpr gfx::PointF kCanonicalMouseMovePosition(42.0f, 39.5f);
  const InkModule::DocumentInkStrokeInputPointsMap document_strokes_positions =
      ink_module().GetInkStrokesInputPositionsForTesting();
  EXPECT_THAT(document_strokes_positions,
              ElementsAre(Pair(0, InkModule::PageInkStrokeInputPoints{
                                      {kCanonicalMouseDownPosition,
                                       kCanonicalMouseMovePosition}})));
}

TEST_F(InkModuleStrokeTest, DrawRenderTransform) {
  // Simulate a viewport that is wider than page to be rendered, and has the
  // page centered within that.  The page is positioned at top of viewport with
  // no vertical padding.
  constexpr gfx::SizeF kPageSize(50.0f, 60.0f);
  constexpr gfx::PointF kPageOrigin(0.0f, -15.0f);
  constexpr gfx::RectF kPageLayout(kPageOrigin, kPageSize);
  constexpr gfx::Vector2dF kViewportOrigin(5.0f, 0.0f);
  client().set_page_layouts(base::span_from_ref(kPageLayout));
  client().set_orientation(PageOrientation::kClockwise180);
  client().set_viewport_origin_offset(kViewportOrigin);

  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  // Simulate drawing the strokes, and verify that the expected transform was
  // used.
  std::vector<InkAffineTransform> draw_render_transforms;
  ink_module().SetDrawRenderTransformCallbackForTesting(
      base::BindLambdaForTesting([&](const InkAffineTransform& transform) {
        draw_render_transforms.push_back(transform);
      }));
  SkCanvas canvas;
  ink_module().Draw(canvas);
  const InkAffineTransform kDrawTransform = {-1.0f, 0.0f,  54.0f,
                                             0.0f,  -1.0f, 44.0f};
  // Just one transform provided, to match the captured stroke.
  EXPECT_THAT(draw_render_transforms, ElementsAre(kDrawTransform));
}

TEST_F(InkModuleStrokeTest, InvalidationsFromStroke) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  // The default brush param size is 1.0.
  const gfx::Rect kInvalidationAreaMouseDown(gfx::Point(9.0f, 14.0f),
                                             gfx::Size(2.0f, 2.0f));
  const gfx::Rect kInvalidationAreaMouseMove(gfx::Point(9.0f, 14.0f),
                                             gfx::Size(12.0f, 12.0f));
  EXPECT_THAT(
      client().invalidations(),
      ElementsAre(kInvalidationAreaMouseDown, kInvalidationAreaMouseMove));
}

TEST_F(InkModuleStrokeTest, StrokeOutsidePage) {
  EXPECT_TRUE(
      ink_module().OnMessage(CreateSetAnnotationModeMessage(/*enable=*/true)));

  client().set_page_layouts(kVerticalLayout2Pages);

  // Start out without any strokes.
  EXPECT_TRUE(ink_module().GetInkStrokesInputPositionsForTesting().empty());

  // A stroke that starts outside of any page does not generate a stroke, even
  // if it crosses into a page.
  ApplyInkStrokeWithMousePoints(
      kTwoPageVerticalLayoutPointOutsidePages,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage0),
      kTwoPageVerticalLayoutPoint3InsidePage0,
      /*expect_mouse_events_handled=*/false);

  EXPECT_TRUE(ink_module().GetInkStrokesInputPositionsForTesting().empty());
}

TEST_F(InkModuleStrokeTest, StrokeInsidePages) {
  EXPECT_TRUE(
      ink_module().OnMessage(CreateSetAnnotationModeMessage(/*enable=*/true)));

  client().set_page_layouts(kVerticalLayout2Pages);

  // Start out without any strokes.
  EXPECT_TRUE(ink_module().GetInkStrokesInputPositionsForTesting().empty());

  // A stroke in the first page generates a stroke only for that page.
  ApplyInkStrokeWithMousePoints(
      kTwoPageVerticalLayoutPoint1InsidePage0,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage0),
      kTwoPageVerticalLayoutPoint3InsidePage0,
      /*expect_mouse_events_handled=*/true);

  const InkModule::DocumentInkStrokeInputPointsMap document_strokes_positions =
      ink_module().GetInkStrokesInputPositionsForTesting();
  EXPECT_THAT(document_strokes_positions,
              ElementsAre(Pair(0, testing::SizeIs(1))));

  // A stroke in the second page generates a stroke only for that page.
  ApplyInkStrokeWithMousePoints(
      kTwoPageVerticalLayoutPoint1InsidePage1,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage1),
      kTwoPageVerticalLayoutPoint3InsidePage1,
      /*expect_mouse_events_handled=*/true);

  const InkModule::DocumentInkStrokeInputPointsMap
      updated_document_strokes_positions =
          ink_module().GetInkStrokesInputPositionsForTesting();
  EXPECT_THAT(
      updated_document_strokes_positions,
      ElementsAre(Pair(0, testing::SizeIs(1)), Pair(1, testing::SizeIs(1))));
}

TEST_F(InkModuleStrokeTest, StrokeAcrossPages) {
  EXPECT_TRUE(
      ink_module().OnMessage(CreateSetAnnotationModeMessage(/*enable=*/true)));

  client().set_page_layouts(kVerticalLayout2Pages);

  // Start out without any strokes.
  EXPECT_TRUE(ink_module().GetInkStrokesInputPositionsForTesting().empty());

  // A stroke that starts in first page and ends in the second page only
  // generates one stroke in the first page.
  ApplyInkStrokeWithMousePoints(
      kTwoPageVerticalLayoutPoint1InsidePage0,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage1),
      kTwoPageVerticalLayoutPoint3InsidePage1,
      /*expect_mouse_events_handled=*/true);

  const InkModule::DocumentInkStrokeInputPointsMap document_strokes_positions =
      ink_module().GetInkStrokesInputPositionsForTesting();
  EXPECT_THAT(document_strokes_positions,
              ElementsAre(Pair(0, testing::SizeIs(1))));
}

}  // namespace

}  // namespace chrome_pdf
