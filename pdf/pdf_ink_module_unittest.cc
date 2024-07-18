// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_module.h"

#include <set>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
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
using testing::ElementsAreArray;
using testing::Pair;

namespace chrome_pdf {

namespace {

// Optional parameters that the `setAnnotationBrushMessage` may have, depending
// on the brush type.
struct AnnotationBrushMessageParams {
  int color_r;
  int color_g;
  int color_b;
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

// The inputs for a stroke that starts in first page, leaves the bounds of that
// page, but then moves back into the page results in one stroke with two
// segments.
constexpr gfx::PointF kTwoPageVerticalLayoutPageExitAndReentryPoints[] = {
    gfx::PointF(10.0f, 5.0f), gfx::PointF(10.0f, 0.0f),
    gfx::PointF(15.0f, 0.0f), gfx::PointF(15.0f, 5.0f),
    gfx::PointF(15.0f, 10.0f)};
// The two segments created by the inputs above.
constexpr gfx::PointF kTwoPageVerticalLayoutPageExitAndReentrySegment1[] = {
    gfx::PointF(5.0f, 5.0f), gfx::PointF(5.0f, 0.0f)};
constexpr gfx::PointF kTwoPageVerticalLayoutPageExitAndReentrySegment2[] = {
    gfx::PointF(10.0f, 0.0f), gfx::PointF(10.0f, 5.0f)};

class FakeClient : public PdfInkModule::Client {
 public:
  FakeClient() = default;
  FakeClient(const FakeClient&) = delete;
  FakeClient& operator=(const FakeClient&) = delete;
  ~FakeClient() override = default;

  // PdfInkModule::Client:
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

  void Invalidate(const gfx::Rect& rect) override {
    invalidations_.push_back(rect);
  }

  bool IsPageVisible(int index) override {
    return base::Contains(visible_page_indices_, index);
  }

  void StrokeFinished() override { ++stroke_finished_count_; }

  int VisiblePageIndexFromPoint(const gfx::PointF& point) override {
    for (size_t i = 0; i < page_layouts_.size(); ++i) {
      if (IsPageVisible(i) && page_layouts_[i].Contains(point)) {
        return i;
      }
    }

    // Point is not over a visible page in the viewer plane.
    return -1;
  }

  int stroke_finished_count() const { return stroke_finished_count_; }

  const std::vector<gfx::Rect>& invalidations() const { return invalidations_; }

  // Provide the sequence of pages and the coordinates and dimensions for how
  // they are laid out in a viewer plane.  It is upon the caller to ensure the
  // positioning makes sense (e.g., pages do not overlap).
  void set_page_layouts(base::span<const gfx::RectF> page_layouts) {
    page_layouts_ = base::ToVector(page_layouts);
  }

  // Marks pages as visible or not. The caller is responsible for making sure
  // the values makes sense.
  void set_page_visibility(int index, bool visible) {
    if (visible) {
      visible_page_indices_.insert(index);
    } else {
      visible_page_indices_.erase(index);
    }
  }

  void set_orientation(PageOrientation orientation) {
    orientation_ = orientation;
  }

  void set_viewport_origin_offset(const gfx::Vector2dF& offset) {
    viewport_origin_offset_ = offset;
  }

  void set_zoom(float zoom) { zoom_ = zoom; }

 private:
  int stroke_finished_count_ = 0;
  std::vector<gfx::RectF> page_layouts_;
  std::set<int> visible_page_indices_;
  PageOrientation orientation_ = PageOrientation::kOriginal;
  gfx::Vector2dF viewport_origin_offset_;
  float zoom_ = 1.0f;
  std::vector<gfx::Rect> invalidations_;
};

class PdfInkModuleTest : public testing::Test {
 protected:
  base::Value::Dict CreateSetAnnotationBrushMessage(
      const std::string& type,
      double size,
      const AnnotationBrushMessageParams* params) {
    base::Value::Dict message;
    message.Set("type", "setAnnotationBrush");

    base::Value::Dict data;
    data.Set("type", type);
    data.Set("size", size);
    if (params) {
      base::Value::Dict color;
      color.Set("r", params->color_r);
      color.Set("g", params->color_g);
      color.Set("b", params->color_b);
      data.Set("color", std::move(color));
    }
    message.Set("data", std::move(data));
    return message;
  }

  base::Value::Dict CreateSetAnnotationModeMessage(bool enable) {
    base::Value::Dict message;
    message.Set("type", "setAnnotationMode");
    message.Set("enable", enable);
    return message;
  }

  void EnableAnnotationMode() {
    EXPECT_TRUE(ink_module().OnMessage(CreateSetAnnotationModeMessage(true)));
  }

  FakeClient& client() { return client_; }
  PdfInkModule& ink_module() { return ink_module_; }
  const PdfInkModule& ink_module() const { return ink_module_; }

 private:
  base::test::ScopedFeatureList feature_list_{features::kPdfInk2};

  FakeClient client_;
  PdfInkModule ink_module_{client_};
};

TEST_F(PdfInkModuleTest, UnknownMessage) {
  base::Value::Dict message;
  message.Set("type", "nonInkMessage");
  EXPECT_FALSE(ink_module().OnMessage(message));
}

// Verify that a set eraser message sets the annotation brush to an eraser.
TEST_F(PdfInkModuleTest, HandleSetAnnotationBrushMessageEraser) {
  EnableAnnotationMode();
  EXPECT_EQ(true, ink_module().enabled());

  base::Value::Dict message =
      CreateSetAnnotationBrushMessage("eraser", /*size=*/2.5, nullptr);
  EXPECT_TRUE(ink_module().OnMessage(message));

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  EXPECT_FALSE(brush);
  std::optional<float> eraser = ink_module().GetEraserSizeForTesting();
  EXPECT_THAT(eraser, testing::Optional(2.5f));
}

// Verify that a set pen message sets the annotation brush to a pen, with the
// given params.
TEST_F(PdfInkModuleTest, HandleSetAnnotationBrushMessagePen) {
  EnableAnnotationMode();
  EXPECT_EQ(true, ink_module().enabled());

  AnnotationBrushMessageParams message_params{/*color_r=*/10, /*color_g=*/255,
                                              /*color_b=*/50};
  base::Value::Dict message =
      CreateSetAnnotationBrushMessage("pen", /*size=*/8.0, &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  ASSERT_TRUE(brush);

  const InkBrush& ink_brush = brush->GetInkBrush();
  EXPECT_EQ(SkColorSetRGB(10, 255, 50), ink_brush.GetColor());
  EXPECT_EQ(8.0f, ink_brush.GetSize());
  EXPECT_EQ(1.0f, ink_brush.GetOpacityForTesting());
}

// Verify that a set highlighter message sets the annotation brush to a
// highlighter, with the given params.
TEST_F(PdfInkModuleTest, HandleSetAnnotationBrushMessageHighlighter) {
  EnableAnnotationMode();
  EXPECT_EQ(true, ink_module().enabled());

  AnnotationBrushMessageParams message_params{/*color_r=*/240, /*color_g=*/133,
                                              /*color_b=*/0};
  base::Value::Dict message = CreateSetAnnotationBrushMessage(
      "highlighter", /*size=*/4.5, &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  ASSERT_TRUE(brush);

  const InkBrush& ink_brush = brush->GetInkBrush();
  EXPECT_EQ(SkColorSetRGB(240, 133, 0), ink_brush.GetColor());
  EXPECT_EQ(4.5f, ink_brush.GetSize());
  EXPECT_EQ(0.4f, ink_brush.GetOpacityForTesting());
}

// Verify that brushes with zero color values can be set as the annotation
// brush.
TEST_F(PdfInkModuleTest, HandleSetAnnotationBrushMessageColorZero) {
  EnableAnnotationMode();
  EXPECT_EQ(true, ink_module().enabled());

  AnnotationBrushMessageParams message_params{/*color_r=*/0, /*color_g=*/0,
                                              /*color_b=*/0};
  base::Value::Dict message =
      CreateSetAnnotationBrushMessage("pen", /*size=*/4.5, &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  ASSERT_TRUE(brush);

  const InkBrush& ink_brush = brush->GetInkBrush();
  EXPECT_EQ(SkColorSetRGB(0, 0, 0), ink_brush.GetColor());
  EXPECT_EQ(4.5f, ink_brush.GetSize());
  EXPECT_EQ(1.0f, ink_brush.GetOpacityForTesting());
}

TEST_F(PdfInkModuleTest, HandleSetAnnotationModeMessage) {
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

class PdfInkModuleStrokeTest : public PdfInkModuleTest {
 protected:
  // Mouse locations used for `RunStrokeCheckTest()`.
  static constexpr gfx::PointF kMouseDownPoint = gfx::PointF(10.0f, 15.0f);
  static constexpr gfx::PointF kMouseMovePoint = gfx::PointF(20.0f, 25.0f);
  static constexpr gfx::PointF kMouseUpPoint = gfx::PointF(30.0f, 17.0f);

  void InitializeSimpleSinglePageBasicLayout() {
    // Single page layout that matches visible area.
    constexpr gfx::RectF kPage(0.0f, 0.0f, 50.0f, 60.0f);
    client().set_page_layouts(base::span_from_ref(kPage));
    client().set_page_visibility(0, true);
  }

  void InitializeVerticalTwoPageLayout() {
    // Page 2 is below page 1. Not side-by-side.
    client().set_page_layouts(kVerticalLayout2Pages);
    client().set_page_visibility(0, true);
    client().set_page_visibility(1, true);
  }

  void ApplyStrokeWithMouseAtPoints(
      const gfx::PointF& mouse_down_point,
      base::span<const gfx::PointF> mouse_move_points,
      const gfx::PointF& mouse_up_point) {
    ApplyStrokeWithMouseAtPointsMaybeHandled(
        mouse_down_point, mouse_move_points, mouse_up_point,
        /*expect_mouse_events_handled=*/true);
  }
  void ApplyStrokeWithMouseAtPointsNotHandled(
      const gfx::PointF& mouse_down_point,
      base::span<const gfx::PointF> mouse_move_points,
      const gfx::PointF& mouse_up_point) {
    ApplyStrokeWithMouseAtPointsMaybeHandled(
        mouse_down_point, mouse_move_points, mouse_up_point,
        /*expect_mouse_events_handled=*/false);
  }

  void RunStrokeCheckTest(bool annotation_mode_enabled) {
    EXPECT_TRUE(ink_module().OnMessage(
        CreateSetAnnotationModeMessage(annotation_mode_enabled)));
    EXPECT_EQ(annotation_mode_enabled, ink_module().enabled());

    ApplyStrokeWithMouseAtPointsMaybeHandled(
        kMouseDownPoint, base::span_from_ref(kMouseMovePoint), kMouseUpPoint,
        /*expect_mouse_events_handled=*/annotation_mode_enabled);

    const int expected_count = annotation_mode_enabled ? 1 : 0;
    EXPECT_EQ(expected_count, client().stroke_finished_count());
  }

  void SelectEraserTool() {
    // TODO(crbug.com/352720912): Test multiple eraser sizes.
    EXPECT_TRUE(ink_module().OnMessage(
        CreateSetAnnotationBrushMessage("eraser", /*size=*/3.0, nullptr)));
  }

  PdfInkModule::DocumentStrokeInputPointsMap StrokeInputPositions() const {
    return ink_module().GetStrokesInputPositionsForTesting();
  }
  PdfInkModule::DocumentStrokeInputPointsMap VisibleStrokeInputPositions()
      const {
    return ink_module().GetVisibleStrokesInputPositionsForTesting();
  }

 private:
  void ApplyStrokeWithMouseAtPointsMaybeHandled(
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
};

TEST_F(PdfInkModuleStrokeTest, NoAnnotationIfNotEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/false);
}

TEST_F(PdfInkModuleStrokeTest, AnnotationIfEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);
}

TEST_F(PdfInkModuleStrokeTest, CanonicalAnnotationPoints) {
  // Setup to support examining the page stroke points for a layout that is
  // more complicated than what is provide by
  // `InitializeSimpleSinglePageBasicLayout()`.  Include viewport offset,
  // scroll, rotation, and zoom.
  constexpr gfx::SizeF kPageSize(100.0f, 120.0f);
  constexpr gfx::PointF kPageOrigin(5.0f, -15.0f);
  constexpr gfx::RectF kPageLayout(kPageOrigin, kPageSize);
  client().set_page_layouts(base::span_from_ref(kPageLayout));
  client().set_page_visibility(0, true);
  client().set_orientation(PageOrientation::kClockwise180);
  client().set_zoom(2.0f);

  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  // There should be two points collected, for mouse down and a single mouse
  // move.  Verify that the collected points match a canonical position for
  // the PdfInkModule::Client setup.
  constexpr gfx::PointF kCanonicalMouseDownPosition(47.0f, 44.5f);
  constexpr gfx::PointF kCanonicalMouseMovePosition(42.0f, 39.5f);
  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(Pair(0, PdfInkModule::PageStrokeInputPoints{
                                      {kCanonicalMouseDownPosition,
                                       kCanonicalMouseMovePosition}})));
}

TEST_F(PdfInkModuleStrokeTest, DrawRenderTransform) {
  // Simulate a viewport that is wider than page to be rendered, and has the
  // page centered within that.  The page is positioned at top of viewport with
  // no vertical padding.
  constexpr gfx::SizeF kPageSize(50.0f, 60.0f);
  constexpr gfx::PointF kPageOrigin(0.0f, -15.0f);
  constexpr gfx::RectF kPageLayout(kPageOrigin, kPageSize);
  constexpr gfx::Vector2dF kViewportOrigin(5.0f, 0.0f);
  client().set_page_layouts(base::span_from_ref(kPageLayout));
  client().set_page_visibility(0, true);
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

  // But if the one and only page is not visible, then Draw() does no transform
  // calculations.
  draw_render_transforms.clear();
  client().set_page_visibility(0, false);
  ink_module().Draw(canvas);
  EXPECT_TRUE(draw_render_transforms.empty());
}

TEST_F(PdfInkModuleStrokeTest, InvalidationsFromStroke) {
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

TEST_F(PdfInkModuleStrokeTest, StrokeOutsidePage) {
  EnableAnnotationMode();
  InitializeVerticalTwoPageLayout();

  // Start out without any strokes.
  EXPECT_TRUE(StrokeInputPositions().empty());

  // A stroke that starts outside of any page does not generate a stroke, even
  // if it crosses into a page.
  ApplyStrokeWithMouseAtPointsNotHandled(
      kTwoPageVerticalLayoutPointOutsidePages,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage0),
      kTwoPageVerticalLayoutPoint3InsidePage0);

  EXPECT_TRUE(StrokeInputPositions().empty());
}

TEST_F(PdfInkModuleStrokeTest, StrokeInsidePages) {
  EnableAnnotationMode();
  InitializeVerticalTwoPageLayout();

  // Start out without any strokes.
  EXPECT_TRUE(StrokeInputPositions().empty());

  // A stroke in the first page generates a stroke only for that page.
  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint1InsidePage0,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage0),
      kTwoPageVerticalLayoutPoint3InsidePage0);

  EXPECT_THAT(StrokeInputPositions(), ElementsAre(Pair(0, testing::SizeIs(1))));

  // A stroke in the second page generates a stroke only for that page.
  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint1InsidePage1,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage1),
      kTwoPageVerticalLayoutPoint3InsidePage1);

  EXPECT_THAT(StrokeInputPositions(), ElementsAre(Pair(0, testing::SizeIs(1)),
                                                  Pair(1, testing::SizeIs(1))));
}

TEST_F(PdfInkModuleStrokeTest, StrokeAcrossPages) {
  EnableAnnotationMode();
  InitializeVerticalTwoPageLayout();

  // Start out without any strokes.
  EXPECT_TRUE(StrokeInputPositions().empty());

  // A stroke that starts in first page and ends in the second page only
  // generates one stroke in the first page.
  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint1InsidePage0,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage1),
      kTwoPageVerticalLayoutPoint3InsidePage1);

  EXPECT_THAT(StrokeInputPositions(), ElementsAre(Pair(0, testing::SizeIs(1))));
}

TEST_F(PdfInkModuleStrokeTest, StrokePageExitAndReentry) {
  EnableAnnotationMode();
  InitializeVerticalTwoPageLayout();

  // Start out without any strokes.
  EXPECT_TRUE(StrokeInputPositions().empty());

  ApplyStrokeWithMouseAtPoints(kTwoPageVerticalLayoutPoint1InsidePage0,
                               kTwoPageVerticalLayoutPageExitAndReentryPoints,
                               kTwoPageVerticalLayoutPoint3InsidePage0);

  EXPECT_THAT(
      StrokeInputPositions(),
      ElementsAre(Pair(
          0,
          ElementsAre(ElementsAreArray(
                          kTwoPageVerticalLayoutPageExitAndReentrySegment1),
                      ElementsAreArray(
                          kTwoPageVerticalLayoutPageExitAndReentrySegment2)))));
}

TEST_F(PdfInkModuleStrokeTest, StrokePageExitAndReentryWithQuickMoves) {
  EnableAnnotationMode();
  InitializeVerticalTwoPageLayout();

  // Start out without any strokes.
  EXPECT_TRUE(StrokeInputPositions().empty());

  // When the mouse cursor moves quickly, PdfInkModule gets fewer input events.
  // Simulate that here with fewer movement inputs compared to
  // `kTwoPageVerticalLayoutPageExitAndReentryPoints`.
  constexpr gfx::PointF kQuickPageExitAndReentryPoints[] = {
      kTwoPageVerticalLayoutPointOutsidePages,
      kTwoPageVerticalLayoutPoint2InsidePage0};
  ApplyStrokeWithMouseAtPoints(kTwoPageVerticalLayoutPoint1InsidePage0,
                               kQuickPageExitAndReentryPoints,
                               kTwoPageVerticalLayoutPoint2InsidePage0);

  // TODO(crbug.com/352578791): The strokes should be:
  // 1) `kTwoPageVerticalLayoutPageExitAndReentrySegment1`
  // 2) {gfx::PointF(6.666667f, 0.0f), gfx::PointF(10.0f, 10.0f)}
  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(Pair(
                  0, ElementsAre(ElementsAre(gfx::PointF(5.0f, 5.0f)),
                                 ElementsAre(gfx::PointF(10.0f, 10.0f))))));
}

TEST_F(PdfInkModuleStrokeTest, EraseStroke) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  // Check that there are now some visible strokes.
  EXPECT_THAT(VisibleStrokeInputPositions(),
              ElementsAre(Pair(0, ElementsAre(ElementsAre(kMouseDownPoint,
                                                          kMouseMovePoint)))));
  EXPECT_EQ(1, client().stroke_finished_count());

  // Stroke with the eraser tool.
  SelectEraserTool();
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint, base::span_from_ref(kMouseDownPoint), kMouseDownPoint);

  // Now there are no visible strokes left.
  // TODO(crbug.com/352720912): Update the test expectations when the Ink
  // library is no longer just a stub.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Erasing counts as another stroke action.
  EXPECT_EQ(2, client().stroke_finished_count());

  // Stroke again. The stroke that have already been erased should stay erased.
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint, base::span_from_ref(kMouseDownPoint), kMouseDownPoint);

  // Still no visible strokes.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Nothing got erased, so the count stays at 2.
  EXPECT_EQ(2, client().stroke_finished_count());
}

TEST_F(PdfInkModuleStrokeTest, EraseOnPageWithoutStrokes) {
  EnableAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  // Verify there are no visible strokes to start with.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());

  // Stroke with the eraser tool when there are no strokes on the page.
  SelectEraserTool();
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint, base::span_from_ref(kMouseDownPoint), kMouseDownPoint);

  // Verify there are still no visible strokes and StrokeFinished() never got
  // called.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  EXPECT_EQ(0, client().stroke_finished_count());
}

TEST_F(PdfInkModuleStrokeTest, EraseStrokeEntirelyOffPage) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  // Check that there are now some visible strokes.
  EXPECT_THAT(VisibleStrokeInputPositions(),
              ElementsAre(Pair(0, ElementsAre(ElementsAre(kMouseDownPoint,
                                                          kMouseMovePoint)))));
  EXPECT_EQ(1, client().stroke_finished_count());

  // Stroke with the eraser tool outside of the page.
  SelectEraserTool();
  constexpr gfx::PointF kOffPagePoint(99.0f, 99.0f);
  ApplyStrokeWithMouseAtPointsNotHandled(
      kOffPagePoint, base::span_from_ref(kOffPagePoint), kOffPagePoint);

  // Check that the visible strokes remain, and StrokeFinished() did not get
  // called again.
  EXPECT_THAT(VisibleStrokeInputPositions(),
              ElementsAre(Pair(0, ElementsAre(ElementsAre(kMouseDownPoint,
                                                          kMouseMovePoint)))));
  EXPECT_EQ(1, client().stroke_finished_count());
}

TEST_F(PdfInkModuleStrokeTest, EraseStrokeErasesTwoStrokes) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  // Draw a second stroke.
  constexpr gfx::PointF kMouseDownPoint2 = gfx::PointF(10.0f, 30.0f);
  constexpr gfx::PointF kMouseUpPoint2 = gfx::PointF(30.0f, 30.0f);
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint2, base::span_from_ref(kMouseMovePoint), kMouseUpPoint2);

  // Check that there are now some visible strokes.
  EXPECT_THAT(
      VisibleStrokeInputPositions(),
      ElementsAre(Pair(
          0, ElementsAre(ElementsAre(kMouseDownPoint, kMouseMovePoint),
                         ElementsAre(kMouseDownPoint2, kMouseMovePoint)))));
  EXPECT_EQ(2, client().stroke_finished_count());

  // Stroke with the eraser tool at `kMouseMovePoint`, where it will
  // intersect with both strokes.
  SelectEraserTool();
  ApplyStrokeWithMouseAtPoints(
      kMouseMovePoint, base::span_from_ref(kMouseMovePoint), kMouseMovePoint);

  // Check that there are now no visible strokes.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  EXPECT_EQ(3, client().stroke_finished_count());
}

TEST_F(PdfInkModuleStrokeTest, EraseStrokePageExitAndReentry) {
  EnableAnnotationMode();
  InitializeVerticalTwoPageLayout();

  // Start out without any strokes.
  EXPECT_TRUE(StrokeInputPositions().empty());

  ApplyStrokeWithMouseAtPoints(kTwoPageVerticalLayoutPoint1InsidePage0,
                               kTwoPageVerticalLayoutPageExitAndReentryPoints,
                               kTwoPageVerticalLayoutPoint3InsidePage0);

  EXPECT_THAT(
      StrokeInputPositions(),
      ElementsAre(Pair(
          0,
          ElementsAre(ElementsAreArray(
                          kTwoPageVerticalLayoutPageExitAndReentrySegment1),
                      ElementsAreArray(
                          kTwoPageVerticalLayoutPageExitAndReentrySegment2)))));
  EXPECT_EQ(1, client().stroke_finished_count());

  // Select the eraser tool and call ApplyStrokeWithMouseAtPoints() again with
  // the same arguments.
  SelectEraserTool();
  ApplyStrokeWithMouseAtPoints(kTwoPageVerticalLayoutPoint1InsidePage0,
                               kTwoPageVerticalLayoutPageExitAndReentryPoints,
                               kTwoPageVerticalLayoutPoint3InsidePage0);

  // The strokes are all still there, but none of them are visible.
  EXPECT_THAT(
      StrokeInputPositions(),
      ElementsAre(Pair(
          0,
          ElementsAre(ElementsAreArray(
                          kTwoPageVerticalLayoutPageExitAndReentrySegment1),
                      ElementsAreArray(
                          kTwoPageVerticalLayoutPageExitAndReentrySegment2)))));
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Erasing counts as another stroke action.
  EXPECT_EQ(2, client().stroke_finished_count());
}

class PdfInkModuleUndoRedoTest : public PdfInkModuleStrokeTest {
 protected:
  void PerformUndo() {
    EXPECT_TRUE(ink_module().OnMessage(
        CreateAnnotationUndoRedoMessage("annotationUndo")));
  }
  void PerformRedo() {
    EXPECT_TRUE(ink_module().OnMessage(
        CreateAnnotationUndoRedoMessage("annotationRedo")));
  }

 private:
  base::Value::Dict CreateAnnotationUndoRedoMessage(std::string_view type) {
    base::Value::Dict message;
    message.Set("type", type);
    return message;
  }
};

TEST_F(PdfInkModuleUndoRedoTest, UndoRedoEmpty) {
  InitializeSimpleSinglePageBasicLayout();
  EnableAnnotationMode();

  EXPECT_TRUE(StrokeInputPositions().empty());
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());

  // Spurious undo message is a no-op.
  PerformUndo();
  EXPECT_TRUE(StrokeInputPositions().empty());
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());

  // Spurious redo message is a no-op.
  PerformRedo();
  EXPECT_TRUE(StrokeInputPositions().empty());
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
}

TEST_F(PdfInkModuleUndoRedoTest, UndoRedoBasic) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  const auto kMatcher = ElementsAre(
      Pair(0, ElementsAre(ElementsAre(kMouseDownPoint, kMouseMovePoint))));
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_THAT(VisibleStrokeInputPositions(), kMatcher);
  // RunStrokeCheckTest() performed the only stroke.
  EXPECT_EQ(1, client().stroke_finished_count());

  PerformUndo();
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Undo/redo here and below do not trigger StrokeFinished().
  EXPECT_EQ(1, client().stroke_finished_count());

  // Spurious undo message is a no-op.
  PerformUndo();
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  EXPECT_EQ(1, client().stroke_finished_count());

  PerformRedo();
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_THAT(VisibleStrokeInputPositions(), kMatcher);
  EXPECT_EQ(1, client().stroke_finished_count());

  // Spurious redo message is a no-op.
  PerformRedo();
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_THAT(VisibleStrokeInputPositions(), kMatcher);
  EXPECT_EQ(1, client().stroke_finished_count());
}

TEST_F(PdfInkModuleUndoRedoTest, UndoRedoBetweenDraws) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  constexpr gfx::PointF kMouseDownPoint1 = gfx::PointF(11.0f, 15.0f);
  constexpr gfx::PointF kMouseMovePoint1 = gfx::PointF(21.0f, 25.0f);
  constexpr gfx::PointF kMouseUpPoint1 = gfx::PointF(31.0f, 17.0f);
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint1, base::span_from_ref(kMouseMovePoint1), kMouseUpPoint1);

  constexpr gfx::PointF kMouseDownPoint2 = gfx::PointF(12.0f, 15.0f);
  constexpr gfx::PointF kMouseMovePoint2 = gfx::PointF(22.0f, 25.0f);
  constexpr gfx::PointF kMouseUpPoint2 = gfx::PointF(32.0f, 17.0f);
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint2, base::span_from_ref(kMouseMovePoint2), kMouseUpPoint2);

  constexpr gfx::PointF kMouseDownPoint3 = gfx::PointF(13.0f, 15.0f);
  constexpr gfx::PointF kMouseMovePoint3 = gfx::PointF(23.0f, 25.0f);
  constexpr gfx::PointF kMouseUpPoint3 = gfx::PointF(33.0f, 17.0f);
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint3, base::span_from_ref(kMouseMovePoint3), kMouseUpPoint3);

  // After drawing 4 strokes above, there should be 4 strokes that are all
  // visible.
  const auto kInitial4StrokeMatchers = {
      ElementsAre(kMouseDownPoint, kMouseMovePoint),
      ElementsAre(kMouseDownPoint1, kMouseMovePoint1),
      ElementsAre(kMouseDownPoint2, kMouseMovePoint2),
      ElementsAre(kMouseDownPoint3, kMouseMovePoint3)};
  const auto kInitial4StrokeMatchersSpan =
      base::make_span(kInitial4StrokeMatchers);
  EXPECT_THAT(
      StrokeInputPositions(),
      ElementsAre(Pair(0, ElementsAreArray(kInitial4StrokeMatchersSpan))));
  EXPECT_THAT(
      VisibleStrokeInputPositions(),
      ElementsAre(Pair(0, ElementsAreArray(kInitial4StrokeMatchersSpan))));

  // Undo makes 3 strokes visible.
  PerformUndo();
  EXPECT_THAT(
      StrokeInputPositions(),
      ElementsAre(Pair(0, ElementsAreArray(kInitial4StrokeMatchersSpan))));
  EXPECT_THAT(VisibleStrokeInputPositions(),
              ElementsAre(Pair(
                  0, ElementsAreArray(kInitial4StrokeMatchersSpan.first(3u)))));

  // Undo again makes 2 strokes visible.
  PerformUndo();
  EXPECT_THAT(
      StrokeInputPositions(),
      ElementsAre(Pair(0, ElementsAreArray(kInitial4StrokeMatchersSpan))));
  EXPECT_THAT(VisibleStrokeInputPositions(),
              ElementsAre(Pair(
                  0, ElementsAreArray(kInitial4StrokeMatchersSpan.first(2u)))));

  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint3, base::span_from_ref(kMouseMovePoint3), kMouseUpPoint3);

  // The 2 strokes that were undone have been discarded, and the newly drawn
  // stroke takes their place.
  const auto kNext3StrokeMatchers = {
      ElementsAre(kMouseDownPoint, kMouseMovePoint),
      ElementsAre(kMouseDownPoint1, kMouseMovePoint1),
      ElementsAre(kMouseDownPoint3, kMouseMovePoint3)};
  const auto kNext3StrokeMatchersSpan = base::make_span(kNext3StrokeMatchers);
  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(Pair(0, ElementsAreArray(kNext3StrokeMatchersSpan))));
  EXPECT_THAT(VisibleStrokeInputPositions(),
              ElementsAre(Pair(0, ElementsAreArray(kNext3StrokeMatchersSpan))));

  // Undo makes 2 strokes visible.
  PerformUndo();
  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(Pair(0, ElementsAreArray(kNext3StrokeMatchersSpan))));
  EXPECT_THAT(VisibleStrokeInputPositions(),
              ElementsAre(Pair(
                  0, ElementsAreArray(kNext3StrokeMatchersSpan.first(2u)))));

  // Undo again makes 1 strokes visible.
  PerformUndo();
  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(Pair(0, ElementsAreArray(kNext3StrokeMatchersSpan))));
  EXPECT_THAT(VisibleStrokeInputPositions(),
              ElementsAre(Pair(
                  0, ElementsAreArray(kNext3StrokeMatchersSpan.first(1u)))));

  // Undo again makes no strokes visible.
  PerformUndo();
  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(Pair(0, ElementsAreArray(kNext3StrokeMatchersSpan))));
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());

  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint2, base::span_from_ref(kMouseMovePoint2), kMouseUpPoint2);

  // All strokes were undone, so they all got discarded. The newly drawn stroke
  // is the only one remaining.
  const auto kFinal1StrokeMatcher =
      ElementsAre(kMouseDownPoint2, kMouseMovePoint2);
  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(Pair(0, ElementsAre(kFinal1StrokeMatcher))));
  EXPECT_THAT(VisibleStrokeInputPositions(),
              ElementsAre(Pair(0, ElementsAre(kFinal1StrokeMatcher))));
}

TEST_F(PdfInkModuleUndoRedoTest, UndoRedoOnTwoPages) {
  EnableAnnotationMode();
  InitializeVerticalTwoPageLayout();

  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint1InsidePage0,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage0),
      kTwoPageVerticalLayoutPoint3InsidePage0);
  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint1InsidePage1,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage1),
      kTwoPageVerticalLayoutPoint3InsidePage1);

  // Canonical coordinates.
  const auto kPage0Matcher =
      Pair(0, ElementsAre(ElementsAre(gfx::PointF(5.0f, 5.0f),
                                      gfx::PointF(10.0f, 10.0f))));
  const auto kPage1Matcher =
      Pair(1, ElementsAre(ElementsAre(gfx::PointF(5.0f, 5.0f),
                                      gfx::PointF(10.0f, 10.0f))));
  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(kPage0Matcher, kPage1Matcher));
  EXPECT_THAT(VisibleStrokeInputPositions(),
              ElementsAre(kPage0Matcher, kPage1Matcher));

  PerformUndo();
  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(kPage0Matcher, kPage1Matcher));
  EXPECT_THAT(VisibleStrokeInputPositions(), ElementsAre(kPage0Matcher));

  PerformUndo();
  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(kPage0Matcher, kPage1Matcher));
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());

  PerformRedo();
  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(kPage0Matcher, kPage1Matcher));
  EXPECT_THAT(VisibleStrokeInputPositions(), ElementsAre(kPage0Matcher));

  PerformRedo();
  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(kPage0Matcher, kPage1Matcher));
  EXPECT_THAT(VisibleStrokeInputPositions(),
              ElementsAre(kPage0Matcher, kPage1Matcher));
}

}  // namespace

}  // namespace chrome_pdf
