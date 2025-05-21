// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_module.h"

#include <array>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "pdf/page_orientation.h"
#include "pdf/pdf_features.h"
#include "pdf/pdf_ink_brush.h"
#include "pdf/pdf_ink_conversions.h"
#include "pdf/pdf_ink_metrics_handler.h"
#include "pdf/pdf_ink_module_client.h"
#include "pdf/pdf_ink_transform.h"
#include "pdf/pdfium/pdfium_ink_reader.h"
#include "pdf/test/mouse_event_builder.h"
#include "pdf/test/pdf_ink_test_helpers.h"
#include "pdf/ui/thumbnail.h"
#include "printing/units.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/ink/src/ink/brush/brush.h"
#include "third_party/ink/src/ink/brush/type_matchers.h"
#include "third_party/ink/src/ink/geometry/affine_transform.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input_batch.h"
#include "third_party/ink/src/ink/strokes/input/type_matchers.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

using testing::_;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Field;
using testing::InSequence;
using testing::NiceMock;
using testing::Pair;
using testing::Pointwise;
using testing::Return;
using testing::SizeIs;

namespace chrome_pdf {

namespace {

// Some commonly used points with InitializeSimpleSinglePageBasicLayout().
constexpr gfx::PointF kLeftVerticalStrokePoint1(10.0f, 15.0f);
constexpr gfx::PointF kLeftVerticalStrokePoint2(10.0f, 35.0f);
constexpr gfx::PointF kRightVerticalStrokePoint1(40.0f, 15.0f);
constexpr gfx::PointF kRightVerticalStrokePoint2(40.0f, 35.0f);

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
constexpr gfx::PointF kTwoPageVerticalLayoutPoint4InsidePage0(10.0f, 20.0f);
constexpr gfx::PointF kTwoPageVerticalLayoutPoint1InsidePage1(10.0f, 75.0f);
constexpr gfx::PointF kTwoPageVerticalLayoutPoint2InsidePage1(15.0f, 80.0f);
constexpr gfx::PointF kTwoPageVerticalLayoutPoint3InsidePage1(20.0f, 80.0f);

// Canonical points after stroking horizontal & vertical lines with some
// commonly used points.
// Horizontal line uses: kTwoPageVerticalLayoutPoint2InsidePage0 to
//                       kTwoPageVerticalLayoutPoint3InsidePage0
//                   or: kTwoPageVerticalLayoutPoint2InsidePage1 to
//                       kTwoPageVerticalLayoutPoint3InsidePage1
// Vertical line uses:   kTwoPageVerticalLayoutPoint1InsidePage0 to
//                       kTwoPageVerticalLayoutPoint4InsidePage0
constexpr gfx::PointF kTwoPageVerticalLayoutHorzLinePoint0Canonical(10.0f,
                                                                    10.0f);
constexpr gfx::PointF kTwoPageVerticalLayoutHorzLinePoint1Canonical(15.0f,
                                                                    10.0f);
constexpr gfx::PointF kTwoPageVerticalLayoutVertLinePoint0Canonical(5.0f, 5.0f);
constexpr gfx::PointF kTwoPageVerticalLayoutVertLinePoint1Canonical(5.0f,
                                                                    15.0f);

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
    gfx::PointF(10.0f, 0.0f), gfx::PointF(10.0f, 5.0f),
    gfx::PointF(15.0f, 10.0f)};

// The stroke inputs for vertical and horizontal lines in the pages.  The
// `.time` fields intentionally get a common value, to match the behavior of
// `MouseEventBuilder`.
constexpr auto kTwoPageVerticalLayoutHorzLinePage0Inputs =
    std::to_array<PdfInkInputData>({
        {kTwoPageVerticalLayoutHorzLinePoint0Canonical, base::Seconds(0)},
        {kTwoPageVerticalLayoutHorzLinePoint1Canonical, base::Seconds(0)},
    });
constexpr auto kTwoPageVerticalLayoutVertLinePage0Inputs =
    std::to_array<PdfInkInputData>({
        {kTwoPageVerticalLayoutVertLinePoint0Canonical, base::Seconds(0)},
        {kTwoPageVerticalLayoutVertLinePoint1Canonical, base::Seconds(0)},
    });
constexpr auto kTwoPageVerticalLayoutHorzLinePage1Inputs =
    std::to_array<PdfInkInputData>({
        {kTwoPageVerticalLayoutHorzLinePoint0Canonical, base::Seconds(0)},
        {kTwoPageVerticalLayoutHorzLinePoint1Canonical, base::Seconds(0)},
    });

// Commonly used test brush color. The color corresponds to "Yellow 1" for pen
// brushes and "Light Yellow" for highlighter brushes.
constexpr SkColor kYellow = SkColorSetRGB(0xFD, 0xD6, 0x63);

// Commonly used test brush message params. The color corresponds to "Red 1" for
// pen brushes and "Light Red" for highlighter brushes.
constexpr TestAnnotationBrushMessageParams kRedBrushParams{
    SkColorSetRGB(0xF2, 0x8B, 0x82),
    /*size=*/6.0};

// Matcher for ink::Stroke objects against their expected brush and inputs.
MATCHER_P(InkStrokeEq, expected_brush, "") {
  const auto& [actual_stroke, expected_inputs] = arg;
  const auto brush_matcher = ink::BrushEq(expected_brush);
  const auto input_matcher = ink::StrokeInputBatchEq(expected_inputs);
  return testing::Matches(brush_matcher)(actual_stroke->GetBrush()) &&
         testing::Matches(input_matcher)(actual_stroke->GetInputs());
}

// Matcher for ink::Stroke objects against an expected brush color.
MATCHER_P(InkStrokeBrushColorEq, expected_color, "") {
  return chrome_pdf::GetSkColorFromInkBrush(arg.GetBrush()) == expected_color;
}

// Matcher for ink::Stroke objects against an expected brush size.
MATCHER_P(InkStrokeBrushSizeEq, expected_size, "") {
  return arg.GetBrush().GetSize() == expected_size;
}

// Matcher for ink::Stroke objects against an expected drawing brush type.
// A pen is opaque while a highlighter has transparency, so a drawing
// brush type can be deduced from the ink::Stroke's brush coat.
MATCHER_P(InkStrokeDrawingBrushTypeEq, expected_type, "") {
  const ink::Brush& ink_brush = arg.GetBrush();
  const ink::BrushCoat& coat = ink_brush.GetCoats()[0];
  float opacity = coat.tip.opacity_multiplier;
  if (expected_type == PdfInkBrush::Type::kPen) {
    return opacity == 1.0f;
  }

  CHECK(expected_type == PdfInkBrush::Type::kHighlighter);
  return opacity == 0.4f;
}

// Matcher for cursor with a custom bitmap against expected dimensions.
MATCHER_P(CursorBitmapImageSizeEq, dimensions, "") {
  return arg.type() == ui::mojom::CursorType::kCustom &&
         arg.custom_bitmap().dimensions() == dimensions;
}

std::map<int, std::vector<raw_ref<const ink::Stroke>>> CollectVisibleStrokes(
    PdfInkModule::PageInkStrokeIterator strokes_iter) {
  std::map<int, std::vector<raw_ref<const ink::Stroke>>> visible_stroke_shapes;
  for (auto page_stroke = strokes_iter.GetNextStrokeAndAdvance();
       page_stroke.has_value();
       page_stroke = strokes_iter.GetNextStrokeAndAdvance()) {
    visible_stroke_shapes[page_stroke.value().page_index].push_back(
        page_stroke.value().stroke);
  }

  return visible_stroke_shapes;
}

blink::WebMouseEvent CreateMouseMoveEventAtPoint(const gfx::PointF& point) {
  return MouseEventBuilder()
      .SetType(blink::WebInputEvent::Type::kMouseMove)
      .SetPosition(point)
      .Build();
}

blink::WebMouseEvent CreateMouseMoveWithLeftButtonEventAtPoint(
    const gfx::PointF& point) {
  return MouseEventBuilder()
      .SetType(blink::WebInputEvent::Type::kMouseMove)
      .SetPosition(point)
      .SetButton(blink::WebPointerProperties::Button::kLeft)
      .Build();
}

base::Value::Dict CreateGetAnnotationBrushMessageForTesting(
    const std::string& brush_type) {
  auto message = base::Value::Dict()
                     .Set("type", "getAnnotationBrush")
                     .Set("messageId", "foo");
  if (!brush_type.empty()) {
    message.Set("brushType", brush_type);
  }
  return message;
}

blink::WebTouchEvent CreateTouchEvent(blink::WebInputEvent::Type type,
                                      base::span<const gfx::PointF> points) {
  CHECK_LE(points.size(), blink::WebTouchEvent::kTouchesLengthCap);

  constexpr int kNoModifiers = 0;
  blink::WebTouchEvent touch_event(
      type, kNoModifiers, blink::WebInputEvent::GetStaticTimeStampForTests());
  for (size_t i = 0; i < points.size(); ++i) {
    touch_event.touches[i].SetPositionInWidget(points[i]);
  }
  touch_event.touches_length = points.size();
  return touch_event;
}

blink::WebTouchEvent CreatePenEvent(blink::WebInputEvent::Type type,
                                    base::span<const gfx::PointF> points) {
  blink::WebTouchEvent pen_event = CreateTouchEvent(type, points);
  for (size_t i = 0; i < pen_event.touches_length; ++i) {
    pen_event.touches[i].pointer_type =
        blink::WebPointerProperties::PointerType::kPen;
  }
  return pen_event;
}

class FakeClient : public PdfInkModuleClient {
 public:
  FakeClient() = default;
  FakeClient(const FakeClient&) = delete;
  FakeClient& operator=(const FakeClient&) = delete;
  ~FakeClient() override = default;

  // PdfInkModuleClient:
  MOCK_METHOD(void,
              DiscardStroke,
              (int page_index, InkStrokeId id),
              (override));

  MOCK_METHOD(void,
              ExtendSelectionByPoint,
              (const gfx::PointF& point),
              (override));

  MOCK_METHOD(ui::Cursor, GetCursor, (), (override));

  PageOrientation GetOrientation() const override { return orientation_; }

  MOCK_METHOD(std::vector<gfx::Rect>, GetSelectionRects, (), (override));

  gfx::Size GetThumbnailSize(int page_index) override {
    CHECK_GE(page_index, 0);
    CHECK_LT(static_cast<size_t>(page_index), page_layouts_.size());
    return Thumbnail::CalculateImageSize(page_layouts_[page_index].size(),
                                         /*device_pixel_ratio=*/1);
  }

  gfx::Vector2dF GetViewportOriginOffset() override {
    return viewport_origin_offset_;
  }

  gfx::Rect GetPageContentsRect(int page_index) override {
    CHECK_GE(page_index, 0);
    CHECK_LT(static_cast<size_t>(page_index), page_layouts_.size());
    return gfx::ToEnclosedRect(page_layouts_[page_index]);
  }

  gfx::SizeF GetPageSizeInPoints(int page_index) override {
    CHECK_GE(page_index, 0);
    CHECK_LT(static_cast<size_t>(page_index), page_layouts_.size());
    gfx::SizeF page_size = page_layouts_[page_index].size();
    page_size.Scale(printing::kUnitConversionFactorPixelsToPoints);
    return page_size;
  }

  float GetZoom() const override { return zoom_; }

  void Invalidate(const gfx::Rect& rect) override {
    invalidations_.push_back(rect);
  }

  bool IsPageVisible(int page_index) override {
    return base::Contains(visible_page_indices_, page_index);
  }

  MOCK_METHOD(bool,
              IsSelectableTextOrLinkArea,
              (const gfx::PointF& point),
              (override));

  MOCK_METHOD(PdfInkModuleClient::DocumentV2InkPathShapesMap,
              LoadV2InkPathsFromPdf,
              (),
              (override));

  MOCK_METHOD(void,
              OnTextOrLinkAreaClick,
              (const gfx::PointF& point, int click_count),
              (override));

  MOCK_METHOD(int, PageIndexFromPoint, (const gfx::PointF& point), (override));

  MOCK_METHOD(void, PostMessage, (base::Value::Dict message), (override));

  MOCK_METHOD(void,
              RequestThumbnail,
              (int page_index, SendThumbnailCallback callback),
              (override));

  MOCK_METHOD(void,
              StrokeAdded,
              (int page_index, InkStrokeId id, const ink::Stroke& stroke),
              (override));

  void StrokeFinished(bool modified) override {
    if (modified) {
      ++modified_stroke_finished_count_;
    } else {
      ++unmodified_stroke_finished_count_;
    }
  }

  void StrokeStarted() override { ++stroke_started_count_; }

  MOCK_METHOD(void, UpdateInkCursor, (const ui::Cursor&), (override));

  MOCK_METHOD(void,
              UpdateShapeActive,
              (int page_index, InkModeledShapeId id, bool active),
              (override));

  MOCK_METHOD(void,
              UpdateStrokeActive,
              (int page_index, InkStrokeId id, bool active),
              (override));

  int VisiblePageIndexFromPoint(const gfx::PointF& point) override {
    for (size_t i = 0; i < page_layouts_.size(); ++i) {
      if (IsPageVisible(i) && page_layouts_[i].Contains(point)) {
        return i;
      }
    }

    // Point is not over a visible page in the viewer plane.
    return -1;
  }

  int stroke_started_count() const { return stroke_started_count_; }
  int modified_stroke_finished_count() const {
    return modified_stroke_finished_count_;
  }
  int unmodified_stroke_finished_count() const {
    return unmodified_stroke_finished_count_;
  }

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
  int stroke_started_count_ = 0;
  int modified_stroke_finished_count_ = 0;
  int unmodified_stroke_finished_count_ = 0;
  std::vector<gfx::RectF> page_layouts_;
  std::set<int> visible_page_indices_;
  PageOrientation orientation_ = PageOrientation::kOriginal;
  gfx::Vector2dF viewport_origin_offset_;
  float zoom_ = 1.0f;
  std::vector<gfx::Rect> invalidations_;
};

class PdfInkModuleMetricsTestBase {
 protected:
  static constexpr char kHighlighterColorMetric[] =
      "PDF.Ink2StrokeHighlighterColor";
  static constexpr char kHighlighterSizeMetric[] =
      "PDF.Ink2StrokeHighlighterSize";
  static constexpr char kInputDeviceMetric[] = "PDF.Ink2StrokeInputDeviceType";
  static constexpr char kTypeMetric[] = "PDF.Ink2StrokeBrushType";

  base::HistogramTester& histograms() { return histograms_; }

 private:
  base::HistogramTester histograms_;
};

class PdfInkModuleTest : public testing::TestWithParam<InkTestVariation> {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        chrome_pdf::features::kPdfInk2,
        {{features::kPdfInk2TextAnnotations.name,
          base::ToString(UseTextAnnotations())},
         {features::kPdfInk2TextHighlighting.name,
          base::ToString(UseTextHighlighting())}});
    ink_module_ = std::make_unique<PdfInkModule>(client_);
  }

 protected:
  bool UseTextAnnotations() const { return GetParam().use_text_annotations; }
  bool UseTextHighlighting() const { return GetParam().use_text_highlighting; }

  void EnableDrawAnnotationMode() {
    EXPECT_TRUE(ink_module().OnMessage(
        CreateSetAnnotationModeMessageForTesting(InkAnnotationMode::kDraw)));
    EXPECT_TRUE(ink_module().enabled());
  }

  void VerifyAndClearExpectations() {
    testing::Mock::VerifyAndClearExpectations(this);
  }

  FakeClient& client() { return client_; }
  PdfInkModule& ink_module() { return *ink_module_; }
  const PdfInkModule& ink_module() const { return *ink_module_; }

 private:
  base::test::ScopedFeatureList feature_list_;

  NiceMock<FakeClient> client_;
  std::unique_ptr<PdfInkModule> ink_module_;
};

}  // namespace

TEST_P(PdfInkModuleTest, UnknownMessage) {
  EXPECT_FALSE(
      ink_module().OnMessage(base::Value::Dict().Set("type", "nonInkMessage")));
}

// Verify that a get eraser message gets the eraser parameters.
TEST_P(PdfInkModuleTest, HandleGetAnnotationBrushMessageEraser) {
  EnableDrawAnnotationMode();

  EXPECT_CALL(client(), PostMessage)
      .WillOnce([](const base::Value::Dict& dict) {
        auto expected = base::test::ParseJsonDict(R"({
            "type": "getAnnotationBrushReply",
            "messageId": "foo",
            "data": {
              "type": "eraser",
            },
        })");
        EXPECT_THAT(dict, base::test::DictionaryHasValues(expected));
      });

  EXPECT_TRUE(ink_module().OnMessage(
      CreateGetAnnotationBrushMessageForTesting("eraser")));
}

// Verify that a get pen message gets the pen brush parameters.
TEST_P(PdfInkModuleTest, HandleGetAnnotationBrushMessagePen) {
  EnableDrawAnnotationMode();

  EXPECT_CALL(client(), PostMessage)
      .WillOnce([](const base::Value::Dict& dict) {
        auto expected = base::test::ParseJsonDict(R"({
            "type": "getAnnotationBrushReply",
            "messageId": "foo",
            "data": {
              "type": "pen",
              "size": 3.0,
              "color": {
                "r": 0,
                "g": 0,
                "b": 0,
              },
            },
        })");
        EXPECT_THAT(dict, base::test::DictionaryHasValues(expected));
      });

  EXPECT_TRUE(
      ink_module().OnMessage(CreateGetAnnotationBrushMessageForTesting("pen")));
}

// Verify that a get highlighter message gets the highlighter brush parameters.
TEST_P(PdfInkModuleTest, HandleGetAnnotationBrushMessageHighlighter) {
  EnableDrawAnnotationMode();

  EXPECT_CALL(client(), PostMessage)
      .WillOnce([](const base::Value::Dict& dict) {
        auto expected = base::test::ParseJsonDict(R"({
            "type": "getAnnotationBrushReply",
            "messageId": "foo",
            "data": {
              "type": "highlighter",
              "size": 8.0,
              "color": {
                "r": 242,
                "g": 139,
                "b": 130,
              },
            },
        })");
        EXPECT_THAT(dict, base::test::DictionaryHasValues(expected));
      });

  EXPECT_TRUE(ink_module().OnMessage(
      CreateGetAnnotationBrushMessageForTesting("highlighter")));
}

// Verify that a get brush message without a parameter gets the default brush
// parameters.
TEST_P(PdfInkModuleTest, HandleGetAnnotationBrushMessageDefault) {
  EnableDrawAnnotationMode();

  EXPECT_CALL(client(), PostMessage)
      .WillOnce([](const base::Value::Dict& dict) {
        auto expected = base::test::ParseJsonDict(R"({
            "type": "getAnnotationBrushReply",
            "messageId": "foo",
            "data": {
              "type": "pen",
              "size": 3.0,
              "color": {
                "r": 0,
                "g": 0,
                "b": 0,
              },
            },
        })");
        EXPECT_THAT(dict, base::test::DictionaryHasValues(expected));
      });

  EXPECT_TRUE(
      ink_module().OnMessage(CreateGetAnnotationBrushMessageForTesting("")));
}

// Verify that a get brush message without a parameter gets the current brush
// parameters if the brush has changed from the default brush.
TEST_P(PdfInkModuleTest, HandleGetAnnotationBrushMessageCurrent) {
  EnableDrawAnnotationMode();

  // Set the brush to eraser.
  EXPECT_TRUE(ink_module().OnMessage(
      CreateSetAnnotationBrushMessageForTesting("eraser", nullptr)));

  EXPECT_CALL(client(), PostMessage)
      .WillOnce([](const base::Value::Dict& dict) {
        auto expected = base::test::ParseJsonDict(R"({
            "type": "getAnnotationBrushReply",
            "messageId": "foo",
            "data": {
              "type": "eraser",
            },
        })");
        EXPECT_THAT(dict, base::test::DictionaryHasValues(expected));
      });

  EXPECT_TRUE(
      ink_module().OnMessage(CreateGetAnnotationBrushMessageForTesting("")));
}

// Verify that a set eraser message sets the annotation brush to an eraser. i.e.
// There is no `PdfInkBrush`.
TEST_P(PdfInkModuleTest, HandleSetAnnotationBrushMessageEraser) {
  EnableDrawAnnotationMode();

  base::Value::Dict message =
      CreateSetAnnotationBrushMessageForTesting("eraser", nullptr);
  EXPECT_TRUE(ink_module().OnMessage(message));

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  EXPECT_FALSE(brush);
}

// Verify that a set pen message sets the annotation brush to a pen, with the
// given params.
TEST_P(PdfInkModuleTest, HandleSetAnnotationBrushMessagePen) {
  EnableDrawAnnotationMode();

  // Select the "Yellow 1" color.
  TestAnnotationBrushMessageParams message_params{kYellow,
                                                  /*size=*/8.0};
  base::Value::Dict message =
      CreateSetAnnotationBrushMessageForTesting("pen", &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  ASSERT_TRUE(brush);

  const ink::Brush& ink_brush = brush->ink_brush();
  EXPECT_EQ(kYellow, GetSkColorFromInkBrush(ink_brush));
  EXPECT_EQ(8.0f, ink_brush.GetSize());
  ASSERT_EQ(1u, ink_brush.CoatCount());
  const ink::BrushCoat& coat = ink_brush.GetCoats()[0];
  EXPECT_EQ(1.0f, coat.tip.corner_rounding);
  EXPECT_EQ(1.0f, coat.tip.opacity_multiplier);
}

// Verify that a set highlighter message sets the annotation brush to a
// highlighter, with the given params.
TEST_P(PdfInkModuleTest, HandleSetAnnotationBrushMessageHighlighter) {
  EnableDrawAnnotationMode();

  // Select the "Light Yellow" color.
  TestAnnotationBrushMessageParams message_params{kYellow,
                                                  /*size=*/4.5};
  base::Value::Dict message =
      CreateSetAnnotationBrushMessageForTesting("highlighter", &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  ASSERT_TRUE(brush);

  const ink::Brush& ink_brush = brush->ink_brush();
  EXPECT_EQ(kYellow, GetSkColorFromInkBrush(ink_brush));
  EXPECT_EQ(4.5f, ink_brush.GetSize());
  ASSERT_EQ(1u, ink_brush.CoatCount());
  const ink::BrushCoat& coat = ink_brush.GetCoats()[0];
  EXPECT_EQ(0.0f, coat.tip.corner_rounding);
  EXPECT_EQ(0.4f, coat.tip.opacity_multiplier);
}

// Verify that brushes with zero color values can be set as the annotation
// brush.
TEST_P(PdfInkModuleTest, HandleSetAnnotationBrushMessageColorZero) {
  EnableDrawAnnotationMode();

  TestAnnotationBrushMessageParams message_params{
      SkColorSetRGB(0x00, 0x00, 0x00),
      /*size=*/4.5};
  base::Value::Dict message =
      CreateSetAnnotationBrushMessageForTesting("pen", &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  ASSERT_TRUE(brush);

  const ink::Brush& ink_brush = brush->ink_brush();
  EXPECT_EQ(SK_ColorBLACK, GetSkColorFromInkBrush(ink_brush));
  EXPECT_EQ(4.5f, ink_brush.GetSize());
  ASSERT_EQ(1u, ink_brush.CoatCount());
  const ink::BrushCoat& coat = ink_brush.GetCoats()[0];
  EXPECT_EQ(1.0f, coat.tip.corner_rounding);
  EXPECT_EQ(1.0f, coat.tip.opacity_multiplier);
}

TEST_P(PdfInkModuleTest, HandleSetAnnotationModeMessage) {
  EXPECT_CALL(client(), LoadV2InkPathsFromPdf())
      .WillOnce(Return(PdfInkModuleClient::DocumentV2InkPathShapesMap{
          {0,
           PdfInkModuleClient::PageV2InkPathShapesMap{
               {InkModeledShapeId(0), ink::PartitionedMesh()},
               {InkModeledShapeId(1), ink::PartitionedMesh()}}},
          {3,
           PdfInkModuleClient::PageV2InkPathShapesMap{
               {InkModeledShapeId(2), ink::PartitionedMesh()}}},
      }));

  const auto kShapeMapMatcher = ElementsAre(
      Pair(0, ElementsAre(Field(&PdfInkModule::LoadedV2ShapeState::id,
                                InkModeledShapeId(0)),
                          Field(&PdfInkModule::LoadedV2ShapeState::id,
                                InkModeledShapeId(1)))),
      Pair(3, ElementsAre(Field(&PdfInkModule::LoadedV2ShapeState::id,
                                InkModeledShapeId(2)))));

  EXPECT_FALSE(ink_module().enabled());

  base::Value::Dict message =
      CreateSetAnnotationModeMessageForTesting(InkAnnotationMode::kOff);

  EXPECT_TRUE(ink_module().OnMessage(message));
  EXPECT_FALSE(ink_module().enabled());
  EXPECT_TRUE(ink_module().loaded_v2_shapes_.empty());

  message.Set("mode", "draw");
  EXPECT_TRUE(ink_module().OnMessage(message));
  EXPECT_TRUE(ink_module().enabled());
  EXPECT_THAT(ink_module().loaded_v2_shapes_, kShapeMapMatcher);

  message.Set("mode", "off");
  EXPECT_TRUE(ink_module().OnMessage(message));
  EXPECT_FALSE(ink_module().enabled());
  EXPECT_THAT(ink_module().loaded_v2_shapes_, kShapeMapMatcher);

  if (UseTextAnnotations()) {
    message.Set("mode", "text");
    EXPECT_TRUE(ink_module().OnMessage(message));
    EXPECT_TRUE(ink_module().enabled());
    EXPECT_THAT(ink_module().loaded_v2_shapes_, kShapeMapMatcher);

    message.Set("mode", "off");
    EXPECT_TRUE(ink_module().OnMessage(message));
    EXPECT_FALSE(ink_module().enabled());
    EXPECT_THAT(ink_module().loaded_v2_shapes_, kShapeMapMatcher);
  }
}

TEST_P(PdfInkModuleTest, MaybeSetCursorWhenTogglingAnnotationMode) {
  // Toggle between annotations off vs. drawing.
  EXPECT_FALSE(ink_module().enabled());

  EXPECT_CALL(client(), UpdateInkCursor(_)).WillOnce([this]() {
    EXPECT_TRUE(ink_module().enabled());
  });

  base::Value::Dict message =
      CreateSetAnnotationModeMessageForTesting(InkAnnotationMode::kDraw);
  EXPECT_TRUE(ink_module().OnMessage(message));
  EXPECT_TRUE(ink_module().enabled());

  message.Set("mode", "off");
  EXPECT_TRUE(ink_module().OnMessage(message));
  EXPECT_FALSE(ink_module().enabled());
  VerifyAndClearExpectations();

  if (UseTextAnnotations()) {
    // Toggle between annotations off vs. text markup.
    EXPECT_CALL(client(), UpdateInkCursor(_));

    message.Set("mode", "text");
    EXPECT_TRUE(ink_module().OnMessage(message));
    EXPECT_TRUE(ink_module().enabled());

    message.Set("mode", "off");
    EXPECT_TRUE(ink_module().OnMessage(message));
    EXPECT_FALSE(ink_module().enabled());
    VerifyAndClearExpectations();

    // Toggle between annotations drawing vs. text markup.
    EXPECT_CALL(client(), UpdateInkCursor(_)).Times(3);

    message.Set("mode", "draw");
    EXPECT_TRUE(ink_module().OnMessage(message));
    EXPECT_TRUE(ink_module().enabled());

    message.Set("mode", "text");
    EXPECT_TRUE(ink_module().OnMessage(message));
    EXPECT_TRUE(ink_module().enabled());

    message.Set("mode", "draw");
    EXPECT_TRUE(ink_module().OnMessage(message));
    EXPECT_TRUE(ink_module().enabled());
  }
}

TEST_P(PdfInkModuleTest, MaybeSetCursorWhenChangingBrushes) {
  {
    InSequence seq;
    EXPECT_CALL(client(), UpdateInkCursor(_))
        .WillOnce([](const ui::Cursor& cursor) {
          ASSERT_EQ(ui::mojom::CursorType::kCustom, cursor.type());
          const SkBitmap& bitmap = cursor.custom_bitmap();
          EXPECT_EQ(6, bitmap.width());
          EXPECT_EQ(6, bitmap.height());
        });
    EXPECT_CALL(client(), UpdateInkCursor(_))
        .WillOnce([](const ui::Cursor& cursor) {
          ASSERT_EQ(ui::mojom::CursorType::kCustom, cursor.type());
          const SkBitmap& bitmap = cursor.custom_bitmap();
          EXPECT_EQ(20, bitmap.width());
          EXPECT_EQ(20, bitmap.height());
        });
    EXPECT_CALL(client(), UpdateInkCursor(_))
        .WillOnce([](const ui::Cursor& cursor) {
          ASSERT_EQ(ui::mojom::CursorType::kCustom, cursor.type());
          const SkBitmap& bitmap = cursor.custom_bitmap();
          EXPECT_EQ(6, bitmap.width());
          EXPECT_EQ(6, bitmap.height());
        });
  }

  EnableDrawAnnotationMode();

  TestAnnotationBrushMessageParams message_params{
      SkColorSetRGB(0x00, 0xFF, 0x00),
      /*size=*/16.0};
  base::Value::Dict message =
      CreateSetAnnotationBrushMessageForTesting("pen", &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  message = CreateSetAnnotationBrushMessageForTesting("eraser", nullptr);
  EXPECT_TRUE(ink_module().OnMessage(message));
}

TEST_P(PdfInkModuleTest, MaybeSetCursorWhenChangingZoom) {
  {
    InSequence seq;
    EXPECT_CALL(client(), UpdateInkCursor(_))
        .WillOnce([](const ui::Cursor& cursor) {
          ASSERT_EQ(ui::mojom::CursorType::kCustom, cursor.type());
          const SkBitmap& bitmap = cursor.custom_bitmap();
          EXPECT_EQ(6, bitmap.width());
          EXPECT_EQ(6, bitmap.height());
        });
    EXPECT_CALL(client(), UpdateInkCursor(_))
        .WillOnce([](const ui::Cursor& cursor) {
          ASSERT_EQ(ui::mojom::CursorType::kCustom, cursor.type());
          const SkBitmap& bitmap = cursor.custom_bitmap();
          EXPECT_EQ(20, bitmap.width());
          EXPECT_EQ(20, bitmap.height());
        });
    EXPECT_CALL(client(), UpdateInkCursor(_))
        .WillOnce([](const ui::Cursor& cursor) {
          ASSERT_EQ(ui::mojom::CursorType::kCustom, cursor.type());
          const SkBitmap& bitmap = cursor.custom_bitmap();
          EXPECT_EQ(10, bitmap.width());
          EXPECT_EQ(10, bitmap.height());
        });
  }

  EnableDrawAnnotationMode();

  TestAnnotationBrushMessageParams message_params{
      SkColorSetRGB(0x00, 0xFF, 0x00),
      /*size=*/16.0};
  base::Value::Dict message =
      CreateSetAnnotationBrushMessageForTesting("pen", &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  client().set_zoom(0.5f);
  ink_module().OnGeometryChanged();
}

TEST_P(PdfInkModuleTest, ContentFocusedWithMouseWillPostMessage) {
  EnableDrawAnnotationMode();

  blink::WebMouseEvent mouse_down_event =
      MouseEventBuilder().CreateLeftClickAtPosition(gfx::PointF()).Build();

  EXPECT_CALL(client(), PostMessage)
      .WillOnce([](const base::Value::Dict& dict) {
        auto expected = base::test::ParseJsonDict(R"({
            "type": "contentFocused",
        })");
        EXPECT_THAT(dict, base::test::DictionaryHasValues(expected));
      });

  ink_module().HandleInputEvent(mouse_down_event);
}

TEST_P(PdfInkModuleTest, ContentFocusedWithTouchWillPostMessage) {
  EnableDrawAnnotationMode();

  blink::WebTouchEvent touch_start_event =
      CreateTouchEvent(blink::WebInputEvent::Type::kTouchStart,
                       base::span_from_ref(gfx::PointF()));

  EXPECT_CALL(client(), PostMessage)
      .WillOnce([](const base::Value::Dict& dict) {
        auto expected = base::test::ParseJsonDict(R"({
            "type": "contentFocused",
        })");
        EXPECT_THAT(dict, base::test::DictionaryHasValues(expected));
      });

  ink_module().HandleInputEvent(touch_start_event);
}

class PdfInkModuleStrokeTest : public PdfInkModuleTest {
 protected:
  // Mouse locations used for `RunStrokeCheckTest()`.
  // Touch events may use the same coordinates.
  static constexpr gfx::PointF kMouseDownPoint = gfx::PointF(10.0f, 15.0f);
  static constexpr gfx::PointF kMouseMovePoint = gfx::PointF(20.0f, 25.0f);
  static constexpr gfx::PointF kMouseUpPoint = gfx::PointF(30.0f, 17.0f);
  static constexpr gfx::PointF kMousePoints[] = {
      kMouseDownPoint, kMouseMovePoint, kMouseUpPoint};

  // PdfInkModuleTest:
  void SetUp() override {
    PdfInkModuleTest::SetUp();

    EXPECT_CALL(client(), PostMessage)
        .WillRepeatedly([&](const base::Value::Dict& dict) {
          const std::string* type = dict.FindString("type");
          ASSERT_TRUE(type);
          if (*type != "updateInk2Thumbnail") {
            return;
          }

          std::optional<int> page_number = dict.FindInt("pageNumber");
          ASSERT_TRUE(page_number.has_value());

          std::optional<bool> is_ink = dict.FindBool("isInk");
          ASSERT_TRUE(is_ink.has_value());
          auto& updated = is_ink.value() ? updated_ink_thumbnail_page_indices_
                                         : updated_pdf_thumbnail_page_indices_;
          updated.push_back(page_number.value() - 1);
        });
  }

  void InitializeSimpleSinglePageBasicLayout() {
    // Single page layout that matches visible area.
    constexpr gfx::RectF kPage(0.0f, 0.0f, 50.0f, 60.0f);
    client().set_page_layouts(base::span_from_ref(kPage));
    client().set_page_visibility(0, true);
  }

  void InitializeScaledLandscapeSinglePageBasicLayout() {
    // Single page layout that matches visible area.
    constexpr gfx::RectF kPage(0.0f, 0.0f, 120.0f, 100.0f);
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
    EXPECT_TRUE(ink_module().OnMessage(CreateSetAnnotationModeMessageForTesting(
        annotation_mode_enabled ? InkAnnotationMode::kDraw
                                : InkAnnotationMode::kOff)));
    EXPECT_EQ(annotation_mode_enabled, ink_module().enabled());

    ApplyStrokeWithMouseAtPointsMaybeHandled(
        kMouseDownPoint, base::span_from_ref(kMouseMovePoint), kMouseUpPoint,
        /*expect_mouse_events_handled=*/annotation_mode_enabled);

    ValidateRunStrokeCheckTest(
        /*expect_stroke_success=*/annotation_mode_enabled);
  }

  void ApplyStrokeWithMouseAtMouseDownPoint() {
    ApplyStrokeWithMouseAtPoints(
        kMouseDownPoint, base::span_from_ref(kMouseDownPoint), kMouseDownPoint);
  }

  void ApplyStrokeWithTouchAtPoints(
      base::span<const gfx::PointF> touch_start_points,
      std::vector<base::span<const gfx::PointF>> all_touch_move_points,
      base::span<const gfx::PointF> touch_end_points) {
    ApplyStrokeWithTouchAtPointsMaybeHandled(
        touch_start_points, all_touch_move_points, touch_end_points,
        /*expect_touch_events_handled=*/true);
  }

  void ApplyStrokeWithTouchAtPointsNotHandled(
      base::span<const gfx::PointF> touch_start_points,
      std::vector<base::span<const gfx::PointF>> all_touch_move_points,
      base::span<const gfx::PointF> touch_end_points) {
    ApplyStrokeWithTouchAtPointsMaybeHandled(
        touch_start_points, all_touch_move_points, touch_end_points,
        /*expect_touch_events_handled=*/false);
  }

  // TODO(crbug.com/377733396): Consider refactoring to combine with
  // RunStrokeCheckTest().
  void RunStrokeTouchCheckTest(bool annotation_mode_enabled) {
    EXPECT_TRUE(ink_module().OnMessage(CreateSetAnnotationModeMessageForTesting(
        annotation_mode_enabled ? InkAnnotationMode::kDraw
                                : InkAnnotationMode::kOff)));
    EXPECT_EQ(annotation_mode_enabled, ink_module().enabled());

    const std::vector<base::span<const gfx::PointF>> all_touch_move_points{
        base::span_from_ref(kMouseMovePoint),
    };
    ApplyStrokeWithTouchAtPointsMaybeHandled(
        base::span_from_ref(kMouseDownPoint), all_touch_move_points,
        base::span_from_ref(kMouseUpPoint),
        /*expect_touch_events_handled=*/annotation_mode_enabled);

    ValidateRunStrokeCheckTest(
        /*expect_stroke_success=*/annotation_mode_enabled);
  }

  // TODO(crbug.com/377733396): Consider refactoring to combine with
  // RunStrokeCheckTest().
  //
  // Note that currently multi-touch is not handled, so the test expectations
  // are different from the ones in RunStrokeTouchCheckTest().
  void RunStrokeMultiTouchCheckTest(bool annotation_mode_enabled) {
    EXPECT_TRUE(ink_module().OnMessage(CreateSetAnnotationModeMessageForTesting(
        annotation_mode_enabled ? InkAnnotationMode::kDraw
                                : InkAnnotationMode::kOff)));
    EXPECT_EQ(annotation_mode_enabled, ink_module().enabled());

    const std::vector<gfx::PointF> touch_start_points{kMouseDownPoint,
                                                      kMouseDownPoint};
    const std::vector<gfx::PointF> touch_move_points{kMouseMovePoint,
                                                     kMouseMovePoint};
    const std::vector<base::span<const gfx::PointF>> all_touch_move_points{
        touch_move_points,
    };
    const std::vector<gfx::PointF> touch_end_points{kMouseUpPoint,
                                                    kMouseUpPoint};
    ApplyStrokeWithTouchAtPointsMaybeHandled(
        touch_start_points, all_touch_move_points, touch_end_points,
        /*expect_touch_events_handled=*/false);

    ValidateRunStrokeCheckTest(/*expect_stroke_success=*/false);
  }

  void ApplyStrokeWithPenAtPoints(
      base::span<const gfx::PointF> pen_start_points,
      std::vector<base::span<const gfx::PointF>> all_pen_move_points,
      base::span<const gfx::PointF> pen_end_points) {
    ApplyStrokeWithPenAtPointsMaybeHandled(pen_start_points,
                                           all_pen_move_points, pen_end_points,
                                           /*expect_pen_events_handled=*/true);
  }

  // TODO(crbug.com/377733396): Consider refactoring to combine with
  // RunStrokeCheckTest().
  void RunStrokePenCheckTest(bool annotation_mode_enabled) {
    EXPECT_TRUE(ink_module().OnMessage(CreateSetAnnotationModeMessageForTesting(
        annotation_mode_enabled ? InkAnnotationMode::kDraw
                                : InkAnnotationMode::kOff)));
    EXPECT_EQ(annotation_mode_enabled, ink_module().enabled());

    const std::vector<base::span<const gfx::PointF>> all_pen_move_points{
        base::span_from_ref(kMouseMovePoint),
    };
    ApplyStrokeWithPenAtPointsMaybeHandled(
        base::span_from_ref(kMouseDownPoint), all_pen_move_points,
        base::span_from_ref(kMouseUpPoint),
        /*expect_pen_events_handled=*/annotation_mode_enabled);

    ValidateRunStrokeCheckTest(
        /*expect_stroke_success=*/annotation_mode_enabled);
  }

  void RunStrokeMissedEndEventThenMouseMoveTest() {
    {
      // Start a drawing or erase action.
      blink::WebMouseEvent mouse_down_event =
          MouseEventBuilder()
              .CreateLeftClickAtPosition(kMouseDownPoint)
              .Build();
      EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));

      // Simulate scenario where another view has taken the focus and consumed
      // the mouse up event, such that subsequent mouse moves don't show the
      // left mouse button being pressed.  This should be handled, as it treats
      // it as a signal to terminate the prior stroke.
      blink::WebMouseEvent mouse_move_event =
          MouseEventBuilder()
              .SetType(blink::WebInputEvent::Type::kMouseMove)
              .SetPosition(kMouseMovePoint)
              .SetButton(blink::WebPointerProperties::Button::kNoButton)
              .Build();
      EXPECT_TRUE(ink_module().HandleInputEvent(mouse_move_event));
    }

    {
      // Another mouse move event without the button down is no longer handled
      // since there is no longer any active drawing or erasing.
      constexpr gfx::PointF kMouseMovePoint2 = gfx::PointF(21.0f, 26.0f);
      blink::WebMouseEvent mouse_move_event =
          MouseEventBuilder()
              .SetType(blink::WebInputEvent::Type::kMouseMove)
              .SetPosition(kMouseMovePoint2)
              .SetButton(blink::WebPointerProperties::Button::kNoButton)
              .Build();
      EXPECT_FALSE(ink_module().HandleInputEvent(mouse_move_event));
    }

    {
      // Start another stroke with a new mouse down event, which is handled.
      blink::WebMouseEvent mouse_down_event =
          MouseEventBuilder()
              .CreateLeftClickAtPosition(kMouseDownPoint)
              .Build();
      EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));

      blink::WebMouseEvent mouse_up_event =
          MouseEventBuilder()
              .CreateLeftMouseUpAtPosition(kMouseUpPoint)
              .Build();
      EXPECT_TRUE(ink_module().HandleInputEvent(mouse_up_event));
    }
  }

  void SelectBrushTool(PdfInkBrush::Type type,
                       const TestAnnotationBrushMessageParams& params) {
    EXPECT_TRUE(
        ink_module().OnMessage(CreateSetAnnotationBrushMessageForTesting(
            PdfInkBrush::TypeToString(type), &params)));
  }

  void SelectEraserTool() {
    EXPECT_TRUE(ink_module().OnMessage(
        CreateSetAnnotationBrushMessageForTesting("eraser", nullptr)));
  }

  PdfInkModule::DocumentStrokeInputPointsMap StrokeInputPositions() const {
    return ink_module().GetStrokesInputPositionsForTesting();
  }
  PdfInkModule::DocumentStrokeInputPointsMap VisibleStrokeInputPositions()
      const {
    return ink_module().GetVisibleStrokesInputPositionsForTesting();
  }

  void ExpectStrokesAdded(int strokes_affected) {
    CHECK_GT(strokes_affected, 0);
    EXPECT_CALL(client(), StrokeAdded(_, _, _)).Times(strokes_affected);
  }

  void ExpectNoStrokeAdded() {
    EXPECT_CALL(client(), StrokeAdded(_, _, _)).Times(0);
  }

  void ExpectUpdateStrokesActive(int strokes_affected, bool expected_active) {
    CHECK_GT(strokes_affected, 0);
    EXPECT_CALL(client(), UpdateStrokeActive(_, _, expected_active))
        .Times(strokes_affected);
  }

  void ExpectNoUpdateStrokeActive() {
    EXPECT_CALL(client(), UpdateStrokeActive(_, _, _)).Times(0);
  }

  const std::vector<int>& updated_ink_thumbnail_page_indices() const {
    return updated_ink_thumbnail_page_indices_;
  }
  const std::vector<int>& updated_pdf_thumbnail_page_indices() const {
    return updated_pdf_thumbnail_page_indices_;
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
          CreateMouseMoveWithLeftButtonEventAtPoint(mouse_move_point);
      EXPECT_EQ(expect_mouse_events_handled,
                ink_module().HandleInputEvent(mouse_move_event));
    }

    blink::WebMouseEvent mouse_up_event =
        MouseEventBuilder().CreateLeftMouseUpAtPosition(mouse_up_point).Build();
    EXPECT_EQ(expect_mouse_events_handled,
              ink_module().HandleInputEvent(mouse_up_event));
  }

  void ApplyStrokeWithTouchAtPointsMaybeHandled(
      base::span<const gfx::PointF> touch_start_points,
      std::vector<base::span<const gfx::PointF>> all_touch_move_points,
      base::span<const gfx::PointF> touch_end_points,
      bool expect_touch_events_handled) {
    blink::WebTouchEvent touch_start_event = CreateTouchEvent(
        blink::WebInputEvent::Type::kTouchStart, touch_start_points);
    EXPECT_EQ(expect_touch_events_handled,
              ink_module().HandleInputEvent(touch_start_event));
    for (const auto& touch_move_points : all_touch_move_points) {
      blink::WebTouchEvent touch_move_event = CreateTouchEvent(
          blink::WebInputEvent::Type::kTouchMove, touch_move_points);
      EXPECT_EQ(expect_touch_events_handled,
                ink_module().HandleInputEvent(touch_move_event));
    }

    blink::WebTouchEvent touch_end_event = CreateTouchEvent(
        blink::WebInputEvent::Type::kTouchEnd, touch_end_points);
    EXPECT_EQ(expect_touch_events_handled,
              ink_module().HandleInputEvent(touch_end_event));
  }

  void ApplyStrokeWithPenAtPointsMaybeHandled(
      base::span<const gfx::PointF> pen_start_points,
      std::vector<base::span<const gfx::PointF>> all_pen_move_points,
      base::span<const gfx::PointF> pen_end_points,
      bool expect_pen_events_handled) {
    blink::WebTouchEvent pen_start_event = CreatePenEvent(
        blink::WebInputEvent::Type::kTouchStart, pen_start_points);
    EXPECT_EQ(expect_pen_events_handled,
              ink_module().HandleInputEvent(pen_start_event));
    for (const auto& pen_move_points : all_pen_move_points) {
      blink::WebTouchEvent pen_move_event = CreatePenEvent(
          blink::WebInputEvent::Type::kTouchMove, pen_move_points);
      EXPECT_EQ(expect_pen_events_handled,
                ink_module().HandleInputEvent(pen_move_event));
    }

    blink::WebTouchEvent pen_end_event =
        CreatePenEvent(blink::WebInputEvent::Type::kTouchEnd, pen_end_points);
    EXPECT_EQ(expect_pen_events_handled,
              ink_module().HandleInputEvent(pen_end_event));
  }

  void ValidateRunStrokeCheckTest(bool expect_stroke_success) {
    EXPECT_EQ(expect_stroke_success ? 1 : 0, client().stroke_started_count());
    EXPECT_EQ(expect_stroke_success ? 1 : 0,
              client().modified_stroke_finished_count());
    EXPECT_EQ(0, client().unmodified_stroke_finished_count());
    if (expect_stroke_success) {
      EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0));
    } else {
      EXPECT_TRUE(updated_ink_thumbnail_page_indices().empty());
    }
  }

  std::vector<int> updated_ink_thumbnail_page_indices_;
  std::vector<int> updated_pdf_thumbnail_page_indices_;
};

TEST_P(PdfInkModuleStrokeTest, NoAnnotationWithMouseIfNotEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/false);
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kMouse));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kStylus));
}

TEST_P(PdfInkModuleStrokeTest, AnnotationWithMouseIfEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);
  EXPECT_EQ(3, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kMouse));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kStylus));
}

TEST_P(PdfInkModuleStrokeTest, NoAnnotationWithTouchIfNotEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeTouchCheckTest(/*annotation_mode_enabled=*/false);
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kMouse));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kStylus));
}

TEST_P(PdfInkModuleStrokeTest, AnnotationWithTouchIfEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeTouchCheckTest(/*annotation_mode_enabled=*/true);
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kMouse));
  EXPECT_EQ(3, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kStylus));
}

TEST_P(PdfInkModuleStrokeTest, NoAnnotationWithMultiTouchIfNotEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeMultiTouchCheckTest(/*annotation_mode_enabled=*/false);
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kMouse));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kStylus));
}

TEST_P(PdfInkModuleStrokeTest, NoAnnotationWithMultiTouchIfEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeMultiTouchCheckTest(/*annotation_mode_enabled=*/true);
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kMouse));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kStylus));
}

TEST_P(PdfInkModuleStrokeTest, NoAnnotationWithPenIfNotEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokePenCheckTest(/*annotation_mode_enabled=*/false);
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kMouse));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kStylus));
}

TEST_P(PdfInkModuleStrokeTest, AnnotationWithPenIfEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokePenCheckTest(/*annotation_mode_enabled=*/true);
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kMouse));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
  EXPECT_EQ(3, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kStylus));
}

TEST_P(PdfInkModuleStrokeTest, IgnoreTouchEventsAfterPenEvent) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  const std::vector<base::span<const gfx::PointF>> all_move_points{
      base::span_from_ref(kMouseMovePoint),
  };
  ApplyStrokeWithTouchAtPoints(base::span_from_ref(kMouseDownPoint),
                               all_move_points,
                               base::span_from_ref(kMouseUpPoint));
  EXPECT_EQ(3, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));

  ApplyStrokeWithTouchAtPoints(base::span_from_ref(kMouseDownPoint),
                               all_move_points,
                               base::span_from_ref(kMouseUpPoint));
  EXPECT_EQ(6, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kStylus));

  ApplyStrokeWithPenAtPoints(base::span_from_ref(kMouseDownPoint),
                             all_move_points,
                             base::span_from_ref(kMouseUpPoint));
  EXPECT_EQ(6, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
  EXPECT_EQ(3, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kStylus));

  ApplyStrokeWithTouchAtPointsNotHandled(base::span_from_ref(kMouseDownPoint),
                                         all_move_points,
                                         base::span_from_ref(kMouseUpPoint));
  EXPECT_EQ(6, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
  EXPECT_EQ(3, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kStylus));

  ApplyStrokeWithPenAtPoints(base::span_from_ref(kMouseDownPoint),
                             all_move_points,
                             base::span_from_ref(kMouseUpPoint));
  EXPECT_EQ(6, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
  EXPECT_EQ(6, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kStylus));

  ApplyStrokeWithTouchAtPointsNotHandled(base::span_from_ref(kMouseDownPoint),
                                         all_move_points,
                                         base::span_from_ref(kMouseUpPoint));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kMouse));
  EXPECT_EQ(6, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
  EXPECT_EQ(6, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kStylus));
}

TEST_P(PdfInkModuleStrokeTest, AnnotationWithMouseInterruptedByPenEvents) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  blink::WebMouseEvent mouse_down_event =
      MouseEventBuilder().CreateLeftClickAtPosition(kMouseDownPoint).Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));

  blink::WebMouseEvent mouse_move_event =
      CreateMouseMoveWithLeftButtonEventAtPoint(kMouseMovePoint);
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_move_event));

  // Per manual testing on a Windows laptop, pen input causes mouse events to
  // lose their left-button down state while the pen is active.
  blink::WebMouseEvent mouse_move_no_left_button_event =
      MouseEventBuilder()
          .SetType(blink::WebInputEvent::Type::kMouseMove)
          .SetPosition(kMouseMovePoint)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_move_no_left_button_event));

  const std::vector<base::span<const gfx::PointF>> all_move_points{
      base::span_from_ref(kMouseMovePoint),
  };
  ApplyStrokeWithPenAtPoints(base::span_from_ref(kMouseDownPoint),
                             all_move_points,
                             base::span_from_ref(kMouseUpPoint));

  // Per manual testing on a Windows laptop, after the pen inputs finish, the
  // mouse events regain their left-button down state. PdfInkModule treats these
  // as spurious events, and ignores them.
  EXPECT_FALSE(ink_module().HandleInputEvent(mouse_move_event));

  blink::WebMouseEvent mouse_up_event =
      MouseEventBuilder().CreateLeftMouseUpAtPosition(kMouseUpPoint).Build();
  EXPECT_FALSE(ink_module().HandleInputEvent(mouse_up_event));

  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_EQ(2, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kMouse));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
  EXPECT_EQ(3, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kStylus));
}

TEST_P(PdfInkModuleStrokeTest, AnnotationWithPenIgnoresMouseEvents) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  blink::WebTouchEvent pen_start_event =
      CreatePenEvent(blink::WebInputEvent::Type::kTouchStart,
                     base::span_from_ref(kMouseDownPoint));
  EXPECT_TRUE(ink_module().HandleInputEvent(pen_start_event));

  blink::WebMouseEvent mouse_move_event =
      CreateMouseMoveWithLeftButtonEventAtPoint(kMouseMovePoint);
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_move_event));

  blink::WebTouchEvent pen_end_event =
      CreatePenEvent(blink::WebInputEvent::Type::kTouchEnd,
                     base::span_from_ref(kMouseUpPoint));
  EXPECT_TRUE(ink_module().HandleInputEvent(pen_end_event));

  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kMouse));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
  EXPECT_EQ(2, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kStylus));
}

TEST_P(PdfInkModuleStrokeTest, CanonicalAnnotationPoints) {
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

  // There should be 3 points collected, for the mouse down, move, and up
  // events. Verify that the collected points match a canonical position for
  // the PdfInkModuleClient setup.
  constexpr gfx::PointF kCanonicalMouseDownPosition(47.0f, 44.5f);
  constexpr gfx::PointF kCanonicalMouseMovePosition(42.0f, 39.5f);
  constexpr gfx::PointF kCanonicalMouseUpPosition(37.0f, 43.5f);
  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(Pair(0, PdfInkModule::PageStrokeInputPoints{
                                      {kCanonicalMouseDownPosition,
                                       kCanonicalMouseMovePosition,
                                       kCanonicalMouseUpPosition}})));
}

TEST_P(PdfInkModuleStrokeTest, BasicLayoutInvalidationsFromStroke) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  // The default brush param size is 3.0.
  const gfx::Rect kInvalidationAreaMouseDown(gfx::Point(8.0f, 13.0f),
                                             gfx::Size(4.0f, 4.0f));
  const gfx::Rect kInvalidationAreaMouseMove(gfx::Point(8.0f, 13.0f),
                                             gfx::Size(14.0f, 14.0f));
  const gfx::Rect kInvalidationAreaMouseUp(gfx::Point(18.0f, 15.0f),
                                           gfx::Size(14.0f, 12.0f));
  const gfx::Rect kInvalidationAreaFinishedStroke(8.0f, 13.0f, 25.0f, 7.0f);
  EXPECT_THAT(
      client().invalidations(),
      ElementsAre(kInvalidationAreaMouseDown, kInvalidationAreaMouseMove,
                  kInvalidationAreaMouseUp, kInvalidationAreaFinishedStroke));
}

TEST_P(PdfInkModuleStrokeTest, TransformedLayoutInvalidationsFromStroke) {
  // Setup to support examining the invalidation areas from page stroke points
  // for a layout that is more complicated than what is provide by
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

  // The default brush param size is 3.0.
  const gfx::Rect kInvalidationAreaMouseDown(gfx::Point(8.0f, 13.0f),
                                             gfx::Size(4.0f, 4.0f));
  const gfx::Rect kInvalidationAreaMouseMove(gfx::Point(8.0f, 13.0f),
                                             gfx::Size(14.0f, 14.0f));
  const gfx::Rect kInvalidationAreaMouseUp(gfx::Point(18.0f, 15.0f),
                                           gfx::Size(14.0f, 12.0f));
  const gfx::Rect kInvalidationAreaFinishedStroke(7.0f, 12.0f, 27.0f, 9.0f);
  EXPECT_THAT(
      client().invalidations(),
      ElementsAre(kInvalidationAreaMouseDown, kInvalidationAreaMouseMove,
                  kInvalidationAreaMouseUp, kInvalidationAreaFinishedStroke));
}

TEST_P(PdfInkModuleStrokeTest, StrokeOutsidePage) {
  EnableDrawAnnotationMode();
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

TEST_P(PdfInkModuleStrokeTest, StrokeInsidePages) {
  EnableDrawAnnotationMode();
  InitializeVerticalTwoPageLayout();

  // Start out without any strokes.
  EXPECT_TRUE(StrokeInputPositions().empty());

  // A stroke in the first page generates a stroke only for that page.
  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint1InsidePage0,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage0),
      kTwoPageVerticalLayoutPoint3InsidePage0);

  EXPECT_THAT(StrokeInputPositions(), ElementsAre(Pair(0, SizeIs(1))));

  // A stroke in the second page generates a stroke only for that page.
  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint1InsidePage1,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage1),
      kTwoPageVerticalLayoutPoint3InsidePage1);

  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(Pair(0, SizeIs(1)), Pair(1, SizeIs(1))));
}

TEST_P(PdfInkModuleStrokeTest, StrokeAcrossPages) {
  EnableDrawAnnotationMode();
  InitializeVerticalTwoPageLayout();

  // Start out without any strokes.
  EXPECT_TRUE(StrokeInputPositions().empty());

  // A stroke that starts in first page and ends in the second page only
  // generates one stroke in the first page.
  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint1InsidePage0,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage1),
      kTwoPageVerticalLayoutPoint3InsidePage1);

  EXPECT_THAT(StrokeInputPositions(), ElementsAre(Pair(0, SizeIs(1))));
}

TEST_P(PdfInkModuleStrokeTest, StrokePageExitAndReentry) {
  EnableDrawAnnotationMode();
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

TEST_P(PdfInkModuleStrokeTest, StrokePageExitAndReentryWithQuickMoves) {
  EnableDrawAnnotationMode();
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

  EXPECT_THAT(
      StrokeInputPositions(),
      ElementsAre(Pair(
          0, ElementsAre(ElementsAreArray(
                             kTwoPageVerticalLayoutPageExitAndReentrySegment1),
                         ElementsAreArray({gfx::PointF(6.666667f, 0.0f),
                                           gfx::PointF(10.0f, 10.0f)})))));
}

TEST_P(PdfInkModuleStrokeTest, EraseStroke) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  // Check that there are now some visible strokes.
  EXPECT_THAT(
      VisibleStrokeInputPositions(),
      ElementsAre(Pair(0, ElementsAre(ElementsAreArray(kMousePoints)))));
  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0));

  // Stroke with the eraser tool.
  SelectEraserTool();
  ApplyStrokeWithMouseAtMouseDownPoint();

  // Now there are no visible strokes left.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Erasing increments the modified stroke count.
  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0));

  // Stroke again. The stroke that have already been erased should stay erased.
  ApplyStrokeWithMouseAtMouseDownPoint();

  // Still no visible strokes.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Nothing got erased, so the modified count stays the same, and the
  // unmodified stroke count goes up by 1 instead.
  EXPECT_EQ(3, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0));

  // PDF thumbnail never needed to be updated.
  EXPECT_TRUE(updated_pdf_thumbnail_page_indices().empty());
}

TEST_P(PdfInkModuleStrokeTest, EraseOnPageWithoutStrokes) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  // Verify there are no visible strokes to start with.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());

  // Stroke with the eraser tool when there are no strokes on the page.
  SelectEraserTool();
  ApplyStrokeWithMouseAtMouseDownPoint();

  // Verify there are still no visible strokes and the StrokeFinished() call is
  // for being unmodified.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(0, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  EXPECT_TRUE(updated_ink_thumbnail_page_indices().empty());
}

TEST_P(PdfInkModuleStrokeTest, EraseStrokeEntirelyOffPage) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  // Check that there are now some visible strokes.
  EXPECT_THAT(
      VisibleStrokeInputPositions(),
      ElementsAre(Pair(0, ElementsAre(ElementsAreArray(kMousePoints)))));
  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0));

  // Stroke with the eraser tool outside of the page.
  SelectEraserTool();
  constexpr gfx::PointF kOffPagePoint(99.0f, 99.0f);
  ApplyStrokeWithMouseAtPointsNotHandled(
      kOffPagePoint, base::span_from_ref(kOffPagePoint), kOffPagePoint);

  // Check that the visible strokes remain, and StrokeFinished() did not get
  // called again.
  EXPECT_THAT(
      VisibleStrokeInputPositions(),
      ElementsAre(Pair(0, ElementsAre(ElementsAreArray(kMousePoints)))));
  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0));
}

TEST_P(PdfInkModuleStrokeTest, EraseStrokeErasesTwoStrokes) {
  InitializeSimpleSinglePageBasicLayout();
  ExpectStrokesAdded(/*strokes_affected=*/2);
  ExpectNoUpdateStrokeActive();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  // Draw a second stroke.
  constexpr gfx::PointF kMouseDownPoint2 = gfx::PointF(10.0f, 30.0f);
  constexpr gfx::PointF kMouseUpPoint2 = gfx::PointF(30.0f, 30.0f);
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint2, base::span_from_ref(kMouseMovePoint), kMouseUpPoint2);

  // Check that there are now some visible strokes.
  const auto kStroke2Matcher =
      ElementsAre(kMouseDownPoint2, kMouseMovePoint, kMouseUpPoint2);
  const auto kVisibleStrokesMatcher = ElementsAre(
      Pair(0, ElementsAre(ElementsAreArray(kMousePoints), kStroke2Matcher)));
  EXPECT_THAT(VisibleStrokeInputPositions(), kVisibleStrokesMatcher);
  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0));

  // Stroke with the eraser tool at `kMouseMovePoint`, where it should
  // intersect with both strokes, but does not because InkStrokeModeler modeled
  // the "V" shaped input into an input with a much gentler line slope.
  SelectEraserTool();
  ApplyStrokeWithMouseAtPoints(
      kMouseMovePoint, base::span_from_ref(kMouseMovePoint), kMouseMovePoint);

  // Check that the visible strokes are still there since the eraser tool missed
  // the strokes. This third stroke causes the unmodified stroke finished count
  // to go up by 1.
  EXPECT_THAT(VisibleStrokeInputPositions(), kVisibleStrokesMatcher);
  EXPECT_EQ(3, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0));

  // Stroke with the eraser tool again, but follow the stroke inputs. This will
  // intersect with both strokes and erase them.
  SelectEraserTool();
  VerifyAndClearExpectations();
  ExpectNoStrokeAdded();
  ExpectUpdateStrokesActive(/*strokes_affected=*/2, /*expected_active=*/false);
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint, base::span_from_ref(kMouseMovePoint), kMouseUpPoint);
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint2, base::span_from_ref(kMouseMovePoint), kMouseUpPoint2);

  // Check that there are now no visible strokes.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  EXPECT_EQ(5, client().stroke_started_count());
  EXPECT_EQ(4, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0, 0, 0));
}

TEST_P(PdfInkModuleStrokeTest, EraseStrokesAcrossTwoPages) {
  EnableDrawAnnotationMode();
  InitializeVerticalTwoPageLayout();

  // Start out without any strokes.
  EXPECT_TRUE(StrokeInputPositions().empty());
  EXPECT_EQ(0, client().stroke_started_count());
  EXPECT_EQ(0, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_TRUE(updated_ink_thumbnail_page_indices().empty());

  ExpectStrokesAdded(/*strokes_affected=*/2);
  ExpectNoUpdateStrokeActive();

  // A stroke in the first page generates a stroke only for that page.
  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint1InsidePage0,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage0),
      kTwoPageVerticalLayoutPoint3InsidePage0);
  EXPECT_THAT(StrokeInputPositions(), ElementsAre(Pair(0, SizeIs(1))));
  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0));

  // A stroke in the second page generates a stroke only for that page.
  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint1InsidePage1,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage1),
      kTwoPageVerticalLayoutPoint3InsidePage1);
  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(Pair(0, SizeIs(1)), Pair(1, SizeIs(1))));
  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 1));

  // Erasing across the two pages should erase everything.
  SelectEraserTool();
  VerifyAndClearExpectations();
  ExpectNoStrokeAdded();
  ExpectUpdateStrokesActive(/*strokes_affected=*/2, /*expected_active=*/false);
  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint1InsidePage0,
      std::vector<gfx::PointF>{kTwoPageVerticalLayoutPoint2InsidePage0,
                               kTwoPageVerticalLayoutPoint1InsidePage1},
      kTwoPageVerticalLayoutPoint3InsidePage1);
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  EXPECT_EQ(3, client().stroke_started_count());
  EXPECT_EQ(3, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 1, 0, 1));
}

TEST_P(PdfInkModuleStrokeTest, EraseStrokePageExitAndReentry) {
  EnableDrawAnnotationMode();
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
  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0));

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
  // Erasing increments the modified stroke count.
  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0));
}

TEST_P(PdfInkModuleStrokeTest, EraseStrokeWithTouch) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeTouchCheckTest(/*annotation_mode_enabled=*/true);

  // Check that there are now some visible strokes.
  EXPECT_THAT(
      VisibleStrokeInputPositions(),
      ElementsAre(Pair(0, ElementsAre(ElementsAreArray(kMousePoints)))));
  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0));

  // Stroke with the eraser tool.
  SelectEraserTool();
  const std::vector<base::span<const gfx::PointF>> touch_move_points{
      base::span_from_ref(kMouseMovePoint),
  };
  ApplyStrokeWithTouchAtPoints(base::span_from_ref(kMouseDownPoint),
                               touch_move_points,
                               base::span_from_ref(kMouseDownPoint));

  // Now there are no visible strokes left.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Erasing increments the modified stroke count.
  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0));

  // Stroke again. The stroke that have already been erased should stay erased.
  ApplyStrokeWithTouchAtPoints(base::span_from_ref(kMouseDownPoint),
                               touch_move_points,
                               base::span_from_ref(kMouseDownPoint));

  // Still no visible strokes.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Nothing got erased, so the modified count stays the same, and the
  // unmodified stroke count goes up by 1 instead.
  EXPECT_EQ(3, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0));

  // Stroke again with the mouse gets the same results.
  ApplyStrokeWithMouseAtMouseDownPoint();

  // Still no visible strokes.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Nothing got erased, so the modified count stays the same, and the
  // unmodified stroke count goes up by 1 instead.
  EXPECT_EQ(4, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(2, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0));
}

TEST_P(PdfInkModuleStrokeTest, EraseStrokeWithPen) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokePenCheckTest(/*annotation_mode_enabled=*/true);

  // Check that there are now some visible strokes.
  EXPECT_THAT(
      VisibleStrokeInputPositions(),
      ElementsAre(Pair(0, ElementsAre(ElementsAreArray(kMousePoints)))));
  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0));

  // Stroke with the eraser tool.
  SelectEraserTool();
  const std::vector<base::span<const gfx::PointF>> pen_move_points{
      base::span_from_ref(kMouseMovePoint),
  };
  ApplyStrokeWithPenAtPoints(base::span_from_ref(kMouseDownPoint),
                             pen_move_points,
                             base::span_from_ref(kMouseDownPoint));

  // Now there are no visible strokes left.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Erasing increments the modified stroke count.
  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0));

  // Stroke again. The stroke that have already been erased should stay erased.
  ApplyStrokeWithPenAtPoints(base::span_from_ref(kMouseDownPoint),
                             pen_move_points,
                             base::span_from_ref(kMouseDownPoint));

  // Still no visible strokes.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Nothing got erased, so the modified count stays the same, and the
  // unmodified stroke count goes up by 1 instead.
  EXPECT_EQ(3, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0));

  // Stroke again with the mouse gets the same results.
  ApplyStrokeWithMouseAtMouseDownPoint();

  // Still no visible strokes.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Nothing got erased, so the modified count stays the same, and the
  // unmodified stroke count goes up by 1 instead.
  EXPECT_EQ(4, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(2, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0));
}

TEST_P(PdfInkModuleStrokeTest, StrokeMissedEndEventThenMouseDown) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  blink::WebMouseEvent mouse_down_event =
      MouseEventBuilder().CreateLeftClickAtPosition(kMouseDownPoint).Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));

  blink::WebMouseEvent mouse_move_event =
      CreateMouseMoveWithLeftButtonEventAtPoint(kMouseMovePoint);
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_move_event));

  // If the mouse up event went missing during stroking, the next mouse down
  // event should not cause a crash.
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));
}

TEST_P(PdfInkModuleStrokeTest, StrokeMissedEndEventThenMouseMoveDuringDrawing) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  // No need to distinguish between pen or highlighter here.
  EXPECT_TRUE(
      ink_module().OnMessage(CreateGetAnnotationBrushMessageForTesting("pen")));

  RunStrokeMissedEndEventThenMouseMoveTest();
}

TEST_P(PdfInkModuleStrokeTest, StrokeMissedEndEventThenMouseMoveDuringErasing) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectEraserTool();

  RunStrokeMissedEndEventThenMouseMoveTest();
}

TEST_P(PdfInkModuleStrokeTest, ChangeBrushColorDuringDrawing) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  // Start drawing a stroke with a black pen.  The stroke will not finish
  // until the mouse-up event.
  EXPECT_CALL(client(), StrokeAdded(_, _, _)).Times(0);
  TestAnnotationBrushMessageParams black_pen_message_params{
      SkColorSetRGB(0x00, 0x00, 0x00),
      /*size=*/3.0};
  SelectBrushTool(PdfInkBrush::Type::kPen, black_pen_message_params);

  blink::WebMouseEvent mouse_down_event =
      MouseEventBuilder()
          .CreateLeftClickAtPosition(kLeftVerticalStrokePoint1)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));

  // While the stroke is still in progress, change the pen color.  This has no
  // immediate effect on the in-progress stroke.
  TestAnnotationBrushMessageParams red_pen_message_params{
      SkColorSetRGB(0xF2, 0x8B, 0x82),
      /*size=*/3.0};
  SelectBrushTool(PdfInkBrush::Type::kPen, red_pen_message_params);
  VerifyAndClearExpectations();

  // Continue with mouse movement and then mouse up at a new location.  Notice
  // that the events are handled and the new stroke is added.
  static constexpr int kPageIndex = 0;
  EXPECT_CALL(client(), StrokeAdded(kPageIndex, InkStrokeId(0),
                                    InkStrokeBrushColorEq(SK_ColorBLACK)));
  blink::WebMouseEvent mouse_move_event =
      CreateMouseMoveWithLeftButtonEventAtPoint(kLeftVerticalStrokePoint2);
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_move_event));
  blink::WebMouseEvent mouse_up_event =
      MouseEventBuilder()
          .CreateLeftMouseUpAtPosition(kLeftVerticalStrokePoint2)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_up_event));
  VerifyAndClearExpectations();

  // Do another stroke.  Notice that the changed pen color is in effect for
  // the new stroke that is added.
  EXPECT_CALL(
      client(),
      StrokeAdded(kPageIndex, InkStrokeId(1),
                  InkStrokeBrushColorEq(SkColorSetRGB(0xF2, 0x8B, 0x82))));
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_up_event));
}

TEST_P(PdfInkModuleStrokeTest, ChangeBrushSizeDuringDrawing) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  // Start drawing a stroke with a black pen.  The stroke will not finish
  // until the mouse-up event.  The cursor image will be updated only when
  // there is not a stroke in progress.
  EXPECT_CALL(client(), StrokeAdded(_, _, _)).Times(0);
  EXPECT_CALL(client(),
              UpdateInkCursor(CursorBitmapImageSizeEq(SkISize(6, 6))));
  TestAnnotationBrushMessageParams message_params{
      SkColorSetRGB(0x00, 0x00, 0x00),
      /*size=*/2.0};
  SelectBrushTool(PdfInkBrush::Type::kPen, message_params);

  blink::WebMouseEvent mouse_down_event =
      MouseEventBuilder()
          .CreateLeftClickAtPosition(kLeftVerticalStrokePoint1)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));

  // While the stroke is still in progress, change the pen size.  This has no
  // immediate effect on the in-progress stroke.
  message_params.size = 6.0f;
  SelectBrushTool(PdfInkBrush::Type::kPen, message_params);
  VerifyAndClearExpectations();

  // Continue with mouse movement and then mouse up at a new location.  Notice
  // that the events are handled and the new stroke is added.  The cursor image
  // also gets updated once the stroke has started.
  // also gets updated once the stroke has finished.
  static constexpr int kPageIndex = 0;
  {
    InSequence seq;
    EXPECT_CALL(client(), StrokeAdded(kPageIndex, InkStrokeId(0),
                                      InkStrokeBrushSizeEq(2.0f)));
    EXPECT_CALL(client(),
                UpdateInkCursor(CursorBitmapImageSizeEq(SkISize(8, 8))));
  }
  blink::WebMouseEvent mouse_move_event =
      CreateMouseMoveWithLeftButtonEventAtPoint(kLeftVerticalStrokePoint2);
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_move_event));
  blink::WebMouseEvent mouse_up_event =
      MouseEventBuilder()
          .CreateLeftMouseUpAtPosition(kLeftVerticalStrokePoint2)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_up_event));
  VerifyAndClearExpectations();

  // Do another stroke.  Notice that the changed pen color has now taken
  // effect for the new stroke that is added.
  EXPECT_CALL(client(), StrokeAdded(kPageIndex, InkStrokeId(1),
                                    InkStrokeBrushSizeEq(6.0f)));
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_up_event));
}

TEST_P(PdfInkModuleStrokeTest, ChangeToEraserDuringDrawing) {
  InitializeSimpleSinglePageBasicLayout();

  // Draw an initial stroke.
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  // Start drawing another stroke.
  static constexpr int kPageIndex = 0;
  EXPECT_CALL(client(), StrokeAdded(kPageIndex, InkStrokeId(1), _));
  EXPECT_CALL(client(), UpdateStrokeActive(_, _, _)).Times(0);
  blink::WebMouseEvent mouse_down_event =
      MouseEventBuilder()
          .CreateLeftClickAtPosition(kRightVerticalStrokePoint1)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));

  // While the stroke is still in progress, change to the eraser tool.  This
  // causes the in-progress stroke to finish even before the mouse-up event.
  SelectEraserTool();
  VerifyAndClearExpectations();

  // Continue with mouse movement and then mouse up at a new location.  Notice
  // that the events are not handled and there is no further effect for adding
  // or erasing strokes, since the prior stroke was already started.
  // or erasing strokes, since the prior stroke was already finished.
  EXPECT_CALL(client(), StrokeAdded(_, _, _)).Times(0);
  EXPECT_CALL(client(), UpdateStrokeActive(_, _, _)).Times(0);
  blink::WebMouseEvent mouse_move_event =
      CreateMouseMoveWithLeftButtonEventAtPoint(kRightVerticalStrokePoint2);
  EXPECT_FALSE(ink_module().HandleInputEvent(mouse_move_event));
  blink::WebMouseEvent mouse_up_event =
      MouseEventBuilder()
          .CreateLeftMouseUpAtPosition(kRightVerticalStrokePoint2)
          .Build();
  EXPECT_FALSE(ink_module().HandleInputEvent(mouse_up_event));
  VerifyAndClearExpectations();

  // Do a simple stroke in the same place where the last stroke was added.
  // Notice that the changed tool type has taken effect and the recently added
  // stroke is erased.
  EXPECT_CALL(client(), StrokeAdded(_, _, _)).Times(0);
  EXPECT_CALL(client(),
              UpdateStrokeActive(kPageIndex, InkStrokeId(1), /*active=*/false));
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_up_event));
}

TEST_P(PdfInkModuleStrokeTest, ChangeToDrawingDuringErasing) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  // Initialize to have two strokes, so there is something to erase.
  static constexpr int kPageIndex = 0;
  EXPECT_CALL(client(), StrokeAdded(kPageIndex, InkStrokeId(0), _));
  EXPECT_CALL(client(), StrokeAdded(kPageIndex, InkStrokeId(1), _));
  EXPECT_CALL(client(), UpdateStrokeActive(_, _, _)).Times(0);

  ApplyStrokeWithMouseAtPoints(kLeftVerticalStrokePoint1,
                               base::span_from_ref(kLeftVerticalStrokePoint2),
                               kLeftVerticalStrokePoint2);

  ApplyStrokeWithMouseAtPoints(kRightVerticalStrokePoint1,
                               base::span_from_ref(kRightVerticalStrokePoint2),
                               kRightVerticalStrokePoint2);

  // Set up for erasing.
  SelectEraserTool();
  VerifyAndClearExpectations();

  // Start erasing from where the first stroke was added.
  EXPECT_CALL(client(), StrokeAdded(_, _, _)).Times(0);
  EXPECT_CALL(client(), UpdateStrokeActive(kPageIndex, InkStrokeId(0), _));
  blink::WebMouseEvent mouse_down_event =
      MouseEventBuilder()
          .CreateLeftClickAtPosition(kLeftVerticalStrokePoint1)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));

  // While the stroke is still in progress, change the input tool type to a
  // pen.  Note that this causes the in-progress erase stroke to finish even
  // before the mouse-up event.
  TestAnnotationBrushMessageParams message_params{
      SkColorSetRGB(0x00, 0x00, 0x00),
      /*size=*/8.0};
  SelectBrushTool(PdfInkBrush::Type::kPen, message_params);
  VerifyAndClearExpectations();

  // Continue with mouse movement and then mouse up at a new location.  Notice
  // that the events are not handled and there is no further effect for adding
  // or erasing strokes, since the prior stroke was already started.
  // or erasing strokes, since the prior stroke was already finished.
  EXPECT_CALL(client(), StrokeAdded(_, _, _)).Times(0);
  EXPECT_CALL(client(), UpdateStrokeActive(_, _, _)).Times(0);
  blink::WebMouseEvent mouse_move_event =
      CreateMouseMoveWithLeftButtonEventAtPoint(kRightVerticalStrokePoint2);
  EXPECT_FALSE(ink_module().HandleInputEvent(mouse_move_event));
  blink::WebMouseEvent mouse_up_event =
      MouseEventBuilder()
          .CreateLeftMouseUpAtPosition(kRightVerticalStrokePoint1)
          .Build();
  EXPECT_FALSE(ink_module().HandleInputEvent(mouse_up_event));
  VerifyAndClearExpectations();

  // Do another stroke.  Notice that the changed tool type has taken effect
  // and a new stroke is added.
  EXPECT_CALL(client(), StrokeAdded(kPageIndex, InkStrokeId(2), _));
  EXPECT_CALL(client(), UpdateStrokeActive(_, _, _)).Times(0);
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_up_event));
}

TEST_P(PdfInkModuleStrokeTest, ChangeDrawingBrushTypeDuringDrawing) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  // Start drawing a stroke with a black pen.  The stroke will not finish
  // until the mouse-up event.  The cursor image will be updated only if a
  // stroke is not in progress.
  EXPECT_CALL(client(), StrokeAdded(_, _, _)).Times(0);
  EXPECT_CALL(client(),
              UpdateInkCursor(CursorBitmapImageSizeEq(SkISize(6, 6))));
  TestAnnotationBrushMessageParams pen_message_params{
      SkColorSetRGB(0x00, 0x00, 0x00),
      /*size=*/2.0};
  SelectBrushTool(PdfInkBrush::Type::kPen, pen_message_params);

  blink::WebMouseEvent mouse_down_event =
      MouseEventBuilder()
          .CreateLeftClickAtPosition(kLeftVerticalStrokePoint1)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));

  // While the stroke is still in progress, change the input tool type to a
  // highlighter.  The entire stroke changes to this new type.
  TestAnnotationBrushMessageParams highlighter_message_params{
      SkColorSetRGB(0xDD, 0xF3, 0x00),
      /*size=*/8.0};
  SelectBrushTool(PdfInkBrush::Type::kHighlighter, highlighter_message_params);
  VerifyAndClearExpectations();

  // Continue with mouse movement and then mouse up at a new location.  Notice
  // that the events are handled and the new stroke is added.  The cursor gets
  // updated to reflect the tool change to highlighter only after the stroke
  // is completed.
  static constexpr int kPageIndex = 0;
  {
    InSequence seq;
    EXPECT_CALL(
        client(),
        StrokeAdded(kPageIndex, InkStrokeId(0),
                    InkStrokeDrawingBrushTypeEq(PdfInkBrush::Type::kPen)));
    EXPECT_CALL(client(),
                UpdateInkCursor(CursorBitmapImageSizeEq(SkISize(10, 10))));
  }
  blink::WebMouseEvent mouse_move_event =
      CreateMouseMoveWithLeftButtonEventAtPoint(kLeftVerticalStrokePoint2);
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_move_event));
  blink::WebMouseEvent mouse_up_event =
      MouseEventBuilder()
          .CreateLeftMouseUpAtPosition(kLeftVerticalStrokePoint2)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_up_event));
  VerifyAndClearExpectations();

  // Do another stroke.  Notice that the changed input tool type has taken
  // effect for the new stroke that is added.
  EXPECT_CALL(client(), StrokeAdded(kPageIndex, InkStrokeId(1),
                                    InkStrokeDrawingBrushTypeEq(
                                        PdfInkBrush::Type::kHighlighter)));
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_up_event));
}

class PdfInkModuleUndoRedoTest : public PdfInkModuleStrokeTest {
 protected:
  void PerformUndo() {
    EXPECT_TRUE(
        ink_module().OnMessage(CreateSetAnnotationUndoRedoMessageForTesting(
            TestAnnotationUndoRedoMessageType::kUndo)));
  }
  void PerformRedo() {
    EXPECT_TRUE(
        ink_module().OnMessage(CreateSetAnnotationUndoRedoMessageForTesting(
            TestAnnotationUndoRedoMessageType::kRedo)));
  }
};

TEST_P(PdfInkModuleUndoRedoTest, UndoRedoEmpty) {
  InitializeSimpleSinglePageBasicLayout();
  EnableDrawAnnotationMode();

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

TEST_P(PdfInkModuleUndoRedoTest, UndoRedoBasic) {
  InitializeSimpleSinglePageBasicLayout();
  ExpectStrokesAdded(/*strokes_affected=*/1);
  ExpectUpdateStrokesActive(/*strokes_affected=*/1, /*expect_active=*/false);
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  const auto kMatcher =
      ElementsAre(Pair(0, ElementsAre(ElementsAreArray(kMousePoints))));
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_THAT(VisibleStrokeInputPositions(), kMatcher);
  // RunStrokeCheckTest() performed the only stroke.
  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0));

  PerformUndo();
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Undo/redo here and below do not trigger StrokeFinished().
  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0));

  // Spurious undo message is a no-op.
  VerifyAndClearExpectations();
  ExpectNoStrokeAdded();
  ExpectNoUpdateStrokeActive();
  PerformUndo();
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0));

  VerifyAndClearExpectations();
  ExpectNoStrokeAdded();
  ExpectUpdateStrokesActive(/*strokes_affected=*/1, /*expect_active=*/true);
  PerformRedo();
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_THAT(VisibleStrokeInputPositions(), kMatcher);
  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0, 0));

  // Spurious redo message is a no-op.
  VerifyAndClearExpectations();
  ExpectNoStrokeAdded();
  ExpectNoUpdateStrokeActive();
  PerformRedo();
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_THAT(VisibleStrokeInputPositions(), kMatcher);
  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0, 0));
}

TEST_P(PdfInkModuleUndoRedoTest, UndoRedoInvalidationsBasic) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  // The default brush param size is 3.0.  Invalidation areas are in screen
  // coordinates.
  const gfx::Rect kInvalidationAreaMouseDown(gfx::Point(8.0f, 13.0f),
                                             gfx::Size(4.0f, 4.0f));
  const gfx::Rect kInvalidationAreaMouseMove(gfx::Point(8.0f, 13.0f),
                                             gfx::Size(14.0f, 14.0f));
  const gfx::Rect kInvalidationAreaMouseUp(gfx::Point(18.0f, 15.0f),
                                           gfx::Size(14.0f, 12.0f));
  // This size is smaller than the area of the merged invalidation constants
  // above because InkStrokeModeler modeled the "V" shaped input into an input
  // with a much gentler line slope.
  const gfx::Rect kInvalidationAreaEntireStroke(gfx::Point(8.0f, 13.0f),
                                                gfx::Size(25.0f, 7.0f));
  EXPECT_THAT(
      client().invalidations(),
      ElementsAre(kInvalidationAreaMouseDown, kInvalidationAreaMouseMove,
                  kInvalidationAreaMouseUp, kInvalidationAreaEntireStroke));

  PerformUndo();
  EXPECT_THAT(
      client().invalidations(),
      ElementsAre(kInvalidationAreaMouseDown, kInvalidationAreaMouseMove,
                  kInvalidationAreaMouseUp, kInvalidationAreaEntireStroke,
                  kInvalidationAreaEntireStroke));

  PerformRedo();
  EXPECT_THAT(
      client().invalidations(),
      ElementsAre(kInvalidationAreaMouseDown, kInvalidationAreaMouseMove,
                  kInvalidationAreaMouseUp, kInvalidationAreaEntireStroke,
                  kInvalidationAreaEntireStroke,
                  kInvalidationAreaEntireStroke));
}

TEST_P(PdfInkModuleUndoRedoTest, UndoRedoInvalidationsScaledRotated90) {
  InitializeScaledLandscapeSinglePageBasicLayout();
  client().set_orientation(PageOrientation::kClockwise90);
  client().set_zoom(2.0f);
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  // The default brush param size is 3.0.  Invalidation areas are in screen
  // coordinates.
  const gfx::Rect kInvalidationAreaMouseDown(gfx::Point(8.0f, 13.0f),
                                             gfx::Size(4.0f, 4.0f));
  const gfx::Rect kInvalidationAreaMouseMove(gfx::Point(8.0f, 13.0f),
                                             gfx::Size(14.0f, 14.0f));
  const gfx::Rect kInvalidationAreaMouseUp(gfx::Point(18.0f, 15.0f),
                                           gfx::Size(14.0f, 12.0f));
  // This size is smaller than the area of the merged invalidation constants
  // above because InkStrokeModeler modeled the "V" shaped input into an input
  // with a much gentler line slope.
  const gfx::Rect kInvalidationAreaEntireStroke(gfx::Point(7.0f, 12.0f),
                                                gfx::Size(27.0f, 9.0f));
  EXPECT_THAT(
      client().invalidations(),
      ElementsAre(kInvalidationAreaMouseDown, kInvalidationAreaMouseMove,
                  kInvalidationAreaMouseUp, kInvalidationAreaEntireStroke));

  PerformUndo();
  EXPECT_THAT(
      client().invalidations(),
      ElementsAre(kInvalidationAreaMouseDown, kInvalidationAreaMouseMove,
                  kInvalidationAreaMouseUp, kInvalidationAreaEntireStroke,
                  kInvalidationAreaEntireStroke));

  PerformRedo();
  EXPECT_THAT(
      client().invalidations(),
      ElementsAre(kInvalidationAreaMouseDown, kInvalidationAreaMouseMove,
                  kInvalidationAreaMouseUp, kInvalidationAreaEntireStroke,
                  kInvalidationAreaEntireStroke,
                  kInvalidationAreaEntireStroke));
}

TEST_P(PdfInkModuleUndoRedoTest, UndoRedoAnnotationModeDisabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  const auto kMatcher =
      ElementsAre(Pair(0, ElementsAre(ElementsAreArray(kMousePoints))));
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_THAT(VisibleStrokeInputPositions(), kMatcher);
  // RunStrokeCheckTest() performed the only stroke.
  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0));

  // Disable annotation mode. Undo/redo should still work.
  EXPECT_TRUE(ink_module().OnMessage(
      CreateSetAnnotationModeMessageForTesting(InkAnnotationMode::kOff)));
  EXPECT_EQ(false, ink_module().enabled());

  PerformUndo();
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0));

  PerformRedo();
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_THAT(VisibleStrokeInputPositions(), kMatcher);
  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0, 0));
}

TEST_P(PdfInkModuleUndoRedoTest, UndoRedoBetweenDraws) {
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
      ElementsAre(kMouseDownPoint, kMouseMovePoint, kMouseUpPoint),
      ElementsAre(kMouseDownPoint1, kMouseMovePoint1, kMouseUpPoint1),
      ElementsAre(kMouseDownPoint2, kMouseMovePoint2, kMouseUpPoint2),
      ElementsAre(kMouseDownPoint3, kMouseMovePoint3, kMouseUpPoint3)};
  const auto kInitial4StrokeMatchersSpan = base::span(kInitial4StrokeMatchers);
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

  constexpr int kPageIndex = 0;
  EXPECT_CALL(client(), DiscardStroke(kPageIndex, InkStrokeId(2)));
  EXPECT_CALL(client(), DiscardStroke(kPageIndex, InkStrokeId(3)));

  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint3, base::span_from_ref(kMouseMovePoint3), kMouseUpPoint3);
  VerifyAndClearExpectations();

  EXPECT_CALL(client(), DiscardStroke(_, _)).Times(0);

  // The 2 strokes that were undone have been discarded, and the newly drawn
  // stroke takes their place.
  const auto kNext3StrokeMatchers = {
      ElementsAre(kMouseDownPoint, kMouseMovePoint, kMouseUpPoint),
      ElementsAre(kMouseDownPoint1, kMouseMovePoint1, kMouseUpPoint1),
      ElementsAre(kMouseDownPoint3, kMouseMovePoint3, kMouseUpPoint3)};
  const auto kNext3StrokeMatchersSpan = base::span(kNext3StrokeMatchers);
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
  VerifyAndClearExpectations();

  EXPECT_CALL(client(), DiscardStroke(kPageIndex, InkStrokeId(0)));
  EXPECT_CALL(client(), DiscardStroke(kPageIndex, InkStrokeId(1)));
  EXPECT_CALL(client(), DiscardStroke(kPageIndex, InkStrokeId(2)));

  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint2, base::span_from_ref(kMouseMovePoint2), kMouseUpPoint2);

  // All strokes were undone, so they all got discarded. The newly drawn stroke
  // is the only one remaining.
  const auto kFinal1StrokeMatcher =
      ElementsAre(kMouseDownPoint2, kMouseMovePoint2, kMouseUpPoint2);
  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(Pair(0, ElementsAre(kFinal1StrokeMatcher))));
  EXPECT_THAT(VisibleStrokeInputPositions(),
              ElementsAre(Pair(0, ElementsAre(kFinal1StrokeMatcher))));
}

TEST_P(PdfInkModuleUndoRedoTest, UndoRedoOnTwoPages) {
  EnableDrawAnnotationMode();
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
                                      gfx::PointF(10.0f, 10.0f),
                                      gfx::PointF(15.0f, 10.0f))));
  const auto kPage1Matcher =
      Pair(1, ElementsAre(ElementsAre(gfx::PointF(5.0f, 5.0f),
                                      gfx::PointF(10.0f, 10.0f),
                                      gfx::PointF(15.0f, 10.0f))));
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

TEST_P(PdfInkModuleUndoRedoTest, UndoRedoEraseLoadedV2Shapes) {
  constexpr int kPageIndex = 0;
  constexpr InkModeledShapeId kShapeId0(0);
  constexpr InkModeledShapeId kShapeId1(1);

  const auto ink_points = base::ToVector(
      kMousePoints,
      [](const gfx::PointF& point) { return InkPointFromGfxPoint(point); });
  std::optional<ink::Mesh> mesh0 =
      CreateInkMeshFromPolylineForTesting(ink_points);
  ASSERT_TRUE(mesh0.has_value());
  auto shape0 =
      ink::PartitionedMesh::FromMeshes(base::span_from_ref(mesh0.value()));
  ASSERT_TRUE(shape0.ok());

  constexpr ink::Point kCornerPoints[] = {
      {49, 59},
      {48, 59},
      {48, 58},
  };
  std::optional<ink::Mesh> mesh1 =
      CreateInkMeshFromPolylineForTesting(kCornerPoints);
  ASSERT_TRUE(mesh1.has_value());
  auto shape1 =
      ink::PartitionedMesh::FromMeshes(base::span_from_ref(mesh1.value()));
  ASSERT_TRUE(shape1.ok());

  EXPECT_CALL(client(), LoadV2InkPathsFromPdf())
      .WillOnce(Return(PdfInkModuleClient::DocumentV2InkPathShapesMap{
          {kPageIndex, PdfInkModuleClient::PageV2InkPathShapesMap{
                           {kShapeId0, *shape0},
                           {kShapeId1, *shape1},
                       }}}));
  ExpectNoStrokeAdded();
  ExpectNoUpdateStrokeActive();
  EXPECT_CALL(client(), UpdateShapeActive(_, _, _)).Times(0);

  InitializeSimpleSinglePageBasicLayout();
  EnableDrawAnnotationMode();
  EXPECT_TRUE(updated_ink_thumbnail_page_indices().empty());
  EXPECT_TRUE(updated_pdf_thumbnail_page_indices().empty());

  EXPECT_CALL(client(), RequestThumbnail)
      .WillRepeatedly([&](int page_index, SendThumbnailCallback callback) {
        std::move(callback).Run(
            Thumbnail(gfx::SizeF(50, 25), /*device_pixel_ratio=*/1));
      });

  // Stroke with the eraser tool in the corner opposite from `kCornerPoints`,
  // which does nothing.
  SelectEraserTool();
  ApplyStrokeWithMouseAtPoints(
      gfx::PointF(), base::span_from_ref(gfx::PointF()), gfx::PointF());
  VerifyAndClearExpectations();
  EXPECT_TRUE(updated_pdf_thumbnail_page_indices().empty());

  // Stroke twice where `shape0` is, and that should deactivate only that shape
  // and only once.
  ExpectNoStrokeAdded();
  ExpectNoUpdateStrokeActive();
  EXPECT_CALL(client(),
              UpdateShapeActive(kPageIndex, kShapeId0, /*active=*/false));
  EXPECT_CALL(client(), UpdateShapeActive(_, kShapeId1, _)).Times(0);
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint, base::span_from_ref(kMouseMovePoint), kMouseUpPoint);
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint, base::span_from_ref(kMouseMovePoint), kMouseUpPoint);
  VerifyAndClearExpectations();
  EXPECT_THAT(updated_pdf_thumbnail_page_indices(), ElementsAre(0));

  // Undo should reactivate `shape0`.
  ExpectNoStrokeAdded();
  ExpectNoUpdateStrokeActive();
  EXPECT_CALL(client(),
              UpdateShapeActive(kPageIndex, kShapeId0, /*active=*/true));
  EXPECT_CALL(client(), UpdateShapeActive(_, kShapeId1, _)).Times(0);
  PerformUndo();
  VerifyAndClearExpectations();
  EXPECT_THAT(updated_pdf_thumbnail_page_indices(), ElementsAre(0, 0));

  // Redo should deactivate `shape0`.
  ExpectNoStrokeAdded();
  ExpectNoUpdateStrokeActive();
  EXPECT_CALL(client(),
              UpdateShapeActive(kPageIndex, kShapeId0, /*active=*/false));
  EXPECT_CALL(client(), UpdateShapeActive(_, kShapeId1, _)).Times(0);
  PerformRedo();
  EXPECT_TRUE(updated_ink_thumbnail_page_indices().empty());
  EXPECT_THAT(updated_pdf_thumbnail_page_indices(), ElementsAre(0, 0, 0));
}

// Regression test for crbug.com/378724153.
TEST_P(PdfInkModuleUndoRedoTest, StrokeStrokeUndoStroke) {
  InitializeSimpleSinglePageBasicLayout();

  // Draw stroke 1.
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  // Draw stroke 2.
  constexpr gfx::PointF kMouseDownPoint2 = gfx::PointF(11.0f, 15.0f);
  constexpr gfx::PointF kMouseMovePoint2 = gfx::PointF(21.0f, 25.0f);
  constexpr gfx::PointF kMouseUpPoint2 = gfx::PointF(31.0f, 17.0f);
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint2, base::span_from_ref(kMouseMovePoint2), kMouseUpPoint2);

  // Strokes 1 and 2 should be visible.
  const auto kInitialStrokeMatchers = {
      ElementsAre(kMouseDownPoint, kMouseMovePoint, kMouseUpPoint),
      ElementsAre(kMouseDownPoint2, kMouseMovePoint2, kMouseUpPoint2)};
  const auto kInitialStrokeMatchersSpan = base::span(kInitialStrokeMatchers);
  EXPECT_THAT(
      StrokeInputPositions(),
      ElementsAre(Pair(0, ElementsAreArray(kInitialStrokeMatchersSpan))));
  EXPECT_THAT(
      VisibleStrokeInputPositions(),
      ElementsAre(Pair(0, ElementsAreArray(kInitialStrokeMatchersSpan))));

  // Undo makes 1 stroke visible.
  PerformUndo();
  EXPECT_THAT(
      StrokeInputPositions(),
      ElementsAre(Pair(0, ElementsAreArray(kInitialStrokeMatchersSpan))));
  EXPECT_THAT(VisibleStrokeInputPositions(),
              ElementsAre(Pair(
                  0, ElementsAreArray(kInitialStrokeMatchersSpan.first(1u)))));

  // Stroke IDs are 0-indexed, so stroke 2 has a stroke ID of 1.
  EXPECT_CALL(client(), DiscardStroke(/*page_index=*/0, InkStrokeId(1)));

  // Draw stroke 3. Stroke 2 was undone and should be discarded.
  constexpr gfx::PointF kMouseDownPoint3 = gfx::PointF(12.0f, 15.0f);
  constexpr gfx::PointF kMouseMovePoint3 = gfx::PointF(22.0f, 25.0f);
  constexpr gfx::PointF kMouseUpPoint3 = gfx::PointF(32.0f, 17.0f);
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint3, base::span_from_ref(kMouseMovePoint3), kMouseUpPoint3);

  // Strokes 1 and 3 should be visible.
  const auto kNextStrokeMatchers = {
      ElementsAre(kMouseDownPoint, kMouseMovePoint, kMouseUpPoint),
      ElementsAre(kMouseDownPoint3, kMouseMovePoint3, kMouseUpPoint3)};
  const auto kNextStrokeMatchersSpan = base::span(kNextStrokeMatchers);
  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(Pair(0, ElementsAreArray(kNextStrokeMatchersSpan))));
  EXPECT_THAT(VisibleStrokeInputPositions(),
              ElementsAre(Pair(0, ElementsAreArray(kNextStrokeMatchersSpan))));
}

using PdfInkModuleGetVisibleStrokesTest = PdfInkModuleStrokeTest;

TEST_P(PdfInkModuleGetVisibleStrokesTest, NoPageStrokes) {
  std::map<int, std::vector<raw_ref<const ink::Stroke>>>
      collected_stroke_shapes =
          CollectVisibleStrokes(ink_module().GetVisibleStrokesIterator());
  ASSERT_EQ(collected_stroke_shapes.size(), 0u);
}

TEST_P(PdfInkModuleGetVisibleStrokesTest, MultiplePageStrokes) {
  EnableDrawAnnotationMode();
  InitializeVerticalTwoPageLayout();

  // Multiple strokes on one page, plus a stroke on another page.
  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint2InsidePage0,
      base::span_from_ref(kTwoPageVerticalLayoutPoint3InsidePage0),
      kTwoPageVerticalLayoutPoint3InsidePage0);
  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint1InsidePage0,
      base::span_from_ref(kTwoPageVerticalLayoutPoint4InsidePage0),
      kTwoPageVerticalLayoutPoint4InsidePage0);
  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint2InsidePage1,
      base::span_from_ref(kTwoPageVerticalLayoutPoint3InsidePage1),
      kTwoPageVerticalLayoutPoint3InsidePage1);

  std::optional<ink::StrokeInputBatch> expected_page0_horz_line_input_batch =
      CreateInkInputBatch(kTwoPageVerticalLayoutHorzLinePage0Inputs);
  ASSERT_TRUE(expected_page0_horz_line_input_batch.has_value());
  std::optional<ink::StrokeInputBatch> expected_page0_vert_line_input_batch =
      CreateInkInputBatch(kTwoPageVerticalLayoutVertLinePage0Inputs);
  ASSERT_TRUE(expected_page0_vert_line_input_batch.has_value());
  std::optional<ink::StrokeInputBatch> expected_page1_horz_line_input_batch =
      CreateInkInputBatch(kTwoPageVerticalLayoutHorzLinePage1Inputs);
  ASSERT_TRUE(expected_page1_horz_line_input_batch.has_value());

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  ASSERT_TRUE(brush);

  std::map<int, std::vector<raw_ref<const ink::Stroke>>> collected_strokes =
      CollectVisibleStrokes(ink_module().GetVisibleStrokesIterator());
  EXPECT_THAT(
      collected_strokes,
      ElementsAre(
          Pair(0, Pointwise(InkStrokeEq(brush->ink_brush()),
                            {expected_page0_horz_line_input_batch.value(),
                             expected_page0_vert_line_input_batch.value()})),
          Pair(1, Pointwise(InkStrokeEq(brush->ink_brush()),
                            {expected_page1_horz_line_input_batch.value()}))));
}

class PdfInkModuleMetricsTest : public PdfInkModuleMetricsTestBase,
                                public PdfInkModuleUndoRedoTest {
 protected:
  static constexpr char kPenColorMetric[] = "PDF.Ink2StrokePenColor";
  static constexpr char kPenSizeMetric[] = "PDF.Ink2StrokePenSize";
};

TEST_P(PdfInkModuleMetricsTest, StrokeUndoRedoDoesNotAffectMetrics) {
  InitializeSimpleSinglePageBasicLayout();

  // Draw a pen stroke.
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  histograms().ExpectUniqueSample(kTypeMetric, StrokeMetricBrushType::kPen, 1);
  histograms().ExpectUniqueSample(kInputDeviceMetric,
                                  StrokeMetricInputDeviceType::kMouse, 1);
  histograms().ExpectUniqueSample(kPenSizeMetric,
                                  StrokeMetricBrushSize::kMedium, 1);
  histograms().ExpectUniqueSample(kPenColorMetric, StrokeMetricPenColor::kBlack,
                                  1);

  // Undo and redo.
  PerformUndo();
  PerformRedo();

  // The metrics should stay the same.
  histograms().ExpectUniqueSample(kTypeMetric, StrokeMetricBrushType::kPen, 1);
  histograms().ExpectUniqueSample(kInputDeviceMetric,
                                  StrokeMetricInputDeviceType::kMouse, 1);
  histograms().ExpectUniqueSample(kPenSizeMetric,
                                  StrokeMetricBrushSize::kMedium, 1);
  histograms().ExpectUniqueSample(kPenColorMetric, StrokeMetricPenColor::kBlack,
                                  1);
}

TEST_P(PdfInkModuleMetricsTest, StrokeBrushColorPen) {
  InitializeSimpleSinglePageBasicLayout();

  RunStrokeCheckTest(/*annotation_mode_enabled=*/false);

  histograms().ExpectTotalCount(kPenColorMetric, 0);

  // Draw a stroke with the default black color.
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  histograms().ExpectUniqueSample(kPenColorMetric, StrokeMetricPenColor::kBlack,
                                  1);

  // Draw a stroke with "Red 1" color.
  TestAnnotationBrushMessageParams params = kRedBrushParams;
  SelectBrushTool(PdfInkBrush::Type::kPen, params);
  ApplyStrokeWithMouseAtMouseDownPoint();

  histograms().ExpectBucketCount(kPenColorMetric, StrokeMetricPenColor::kRed1,
                                 1);
  histograms().ExpectTotalCount(kPenColorMetric, 2);

  // Draw a stroke with "Tan 3" color.
  params.color = SkColorSetRGB(0x88, 0x59, 0x45);
  SelectBrushTool(PdfInkBrush::Type::kPen, params);
  ApplyStrokeWithMouseAtMouseDownPoint();

  histograms().ExpectBucketCount(kPenColorMetric, StrokeMetricPenColor::kTan3,
                                 1);
  histograms().ExpectTotalCount(kPenColorMetric, 3);
  histograms().ExpectTotalCount(kHighlighterColorMetric, 0);
}

TEST_P(PdfInkModuleMetricsTest, StrokeBrushColorHighlighter) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  // Draw a stroke with "Light Red" color.
  TestAnnotationBrushMessageParams params = kRedBrushParams;
  SelectBrushTool(PdfInkBrush::Type::kHighlighter, params);
  ApplyStrokeWithMouseAtMouseDownPoint();

  histograms().ExpectBucketCount(kHighlighterColorMetric,
                                 StrokeMetricHighlighterColor::kLightRed, 1);
  histograms().ExpectTotalCount(kHighlighterColorMetric, 1);

  // Draw a stroke with "Orange" color.
  params.color = SkColorSetRGB(0xFF, 0x63, 0x0C);
  SelectBrushTool(PdfInkBrush::Type::kHighlighter, params);
  ApplyStrokeWithMouseAtMouseDownPoint();

  histograms().ExpectBucketCount(kHighlighterColorMetric,
                                 StrokeMetricHighlighterColor::kOrange, 1);
  histograms().ExpectTotalCount(kHighlighterColorMetric, 2);
  histograms().ExpectTotalCount(kPenColorMetric, 0);
}

TEST_P(PdfInkModuleMetricsTest, StrokeBrushSizePen) {
  InitializeSimpleSinglePageBasicLayout();

  // Draw a stroke.
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  histograms().ExpectUniqueSample(kPenSizeMetric,
                                  StrokeMetricBrushSize::kMedium, 1);

  TestAnnotationBrushMessageParams params = {SkColorSetRGB(0xF2, 0x8B, 0x82),
                                             /*size=*/1.0};
  SelectBrushTool(PdfInkBrush::Type::kPen, params);
  ApplyStrokeWithMouseAtMouseDownPoint();

  histograms().ExpectBucketCount(kPenSizeMetric,
                                 StrokeMetricBrushSize::kExtraThin, 1);
  histograms().ExpectTotalCount(kPenSizeMetric, 2);

  params.size = 8.0f;
  SelectBrushTool(PdfInkBrush::Type::kPen, params);
  ApplyStrokeWithMouseAtMouseDownPoint();

  histograms().ExpectBucketCount(kPenSizeMetric,
                                 StrokeMetricBrushSize::kExtraThick, 1);
  histograms().ExpectTotalCount(kPenSizeMetric, 3);
  histograms().ExpectTotalCount(kHighlighterSizeMetric, 0);
}

TEST_P(PdfInkModuleMetricsTest, StrokeBrushSizeHighlighter) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  // Draw a stroke with medium size.
  TestAnnotationBrushMessageParams params = {SkColorSetRGB(0xF2, 0x8B, 0x82),
                                             /*size=*/8.0};
  SelectBrushTool(PdfInkBrush::Type::kHighlighter, params);
  ApplyStrokeWithMouseAtMouseDownPoint();

  histograms().ExpectUniqueSample(kHighlighterSizeMetric,
                                  StrokeMetricBrushSize::kMedium, 1);

  // Draw a stroke with extra thin size.
  params.size = 4.0f;
  SelectBrushTool(PdfInkBrush::Type::kHighlighter, params);
  ApplyStrokeWithMouseAtMouseDownPoint();

  histograms().ExpectBucketCount(kHighlighterSizeMetric,
                                 StrokeMetricBrushSize::kExtraThin, 1);
  histograms().ExpectTotalCount(kHighlighterSizeMetric, 2);

  // Draw a stroke with extra thick size.
  params.size = 16.0f;
  SelectBrushTool(PdfInkBrush::Type::kHighlighter, params);
  ApplyStrokeWithMouseAtMouseDownPoint();

  histograms().ExpectBucketCount(kHighlighterSizeMetric,
                                 StrokeMetricBrushSize::kExtraThick, 1);
  histograms().ExpectTotalCount(kPenSizeMetric, 0);
  histograms().ExpectTotalCount(kHighlighterSizeMetric, 3);
}

TEST_P(PdfInkModuleMetricsTest, StrokeBrushType) {
  InitializeSimpleSinglePageBasicLayout();

  RunStrokeCheckTest(/*annotation_mode_enabled=*/false);

  histograms().ExpectTotalCount(kTypeMetric, 0);

  // Draw a pen stroke.
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  histograms().ExpectUniqueSample(kTypeMetric, StrokeMetricBrushType::kPen, 1);

  // Draw a highlighter stroke.
  TestAnnotationBrushMessageParams params = kRedBrushParams;
  SelectBrushTool(PdfInkBrush::Type::kHighlighter, params);
  ApplyStrokeWithMouseAtMouseDownPoint();

  histograms().ExpectBucketCount(kTypeMetric,
                                 StrokeMetricBrushType::kHighlighter, 1);
  histograms().ExpectTotalCount(kTypeMetric, 2);

  // Draw an eraser stroke.
  SelectEraserTool();
  ApplyStrokeWithMouseAtMouseDownPoint();

  histograms().ExpectBucketCount(kTypeMetric, StrokeMetricBrushType::kEraser,
                                 1);
  histograms().ExpectTotalCount(kTypeMetric, 3);

  // Draw an eraser stroke at a different point that does not erase any other
  // strokes. The metric should stay the same.
  ApplyStrokeWithMouseAtPoints(
      kMouseUpPoint, base::span_from_ref(kMouseUpPoint), kMouseUpPoint);

  histograms().ExpectBucketCount(kTypeMetric, StrokeMetricBrushType::kEraser,
                                 1);
  histograms().ExpectTotalCount(kTypeMetric, 3);

  // Draw another pen stroke.
  params.size = 3.0f;
  SelectBrushTool(PdfInkBrush::Type::kPen, params);
  ApplyStrokeWithMouseAtMouseDownPoint();

  histograms().ExpectBucketCount(kTypeMetric, StrokeMetricBrushType::kPen, 2);
  histograms().ExpectTotalCount(kTypeMetric, 4);
}

TEST_P(PdfInkModuleMetricsTest, StrokeInputDeviceMouse) {
  InitializeSimpleSinglePageBasicLayout();

  RunStrokeCheckTest(/*annotation_mode_enabled=*/false);

  histograms().ExpectTotalCount(kInputDeviceMetric, 0);

  // Draw a stroke with a mouse.
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  histograms().ExpectUniqueSample(kInputDeviceMetric,
                                  StrokeMetricInputDeviceType::kMouse, 1);

  // Draw an eraser stroke with a mouse that erases the first stroke.
  SelectEraserTool();
  ApplyStrokeWithMouseAtMouseDownPoint();

  histograms().ExpectUniqueSample(kInputDeviceMetric,
                                  StrokeMetricInputDeviceType::kMouse, 2);

  // Draw another eraser stroke with a mouse that erases nothing.
  ApplyStrokeWithMouseAtMouseDownPoint();

  histograms().ExpectUniqueSample(kInputDeviceMetric,
                                  StrokeMetricInputDeviceType::kMouse, 2);
}

TEST_P(PdfInkModuleMetricsTest, StrokeInputDeviceTouch) {
  InitializeSimpleSinglePageBasicLayout();

  RunStrokeTouchCheckTest(/*annotation_mode_enabled=*/false);

  histograms().ExpectTotalCount(kInputDeviceMetric, 0);

  // Draw a stroke with touch.
  RunStrokeTouchCheckTest(/*annotation_mode_enabled=*/true);

  histograms().ExpectUniqueSample(kInputDeviceMetric,
                                  StrokeMetricInputDeviceType::kTouch, 1);

  // Draw an eraser stroke with touch that erases the first stroke.
  SelectEraserTool();
  const std::vector<base::span<const gfx::PointF>> move_point{
      base::span_from_ref(kMouseDownPoint),
  };
  ApplyStrokeWithTouchAtPoints(base::span_from_ref(kMouseDownPoint), move_point,
                               base::span_from_ref(kMouseDownPoint));

  histograms().ExpectUniqueSample(kInputDeviceMetric,
                                  StrokeMetricInputDeviceType::kTouch, 2);

  // Draw another eraser stroke with touch that erases nothing.
  ApplyStrokeWithTouchAtPoints(base::span_from_ref(kMouseDownPoint), move_point,
                               base::span_from_ref(kMouseDownPoint));

  histograms().ExpectUniqueSample(kInputDeviceMetric,
                                  StrokeMetricInputDeviceType::kTouch, 2);
}

TEST_P(PdfInkModuleMetricsTest, StrokeInputDevicePen) {
  InitializeSimpleSinglePageBasicLayout();

  RunStrokePenCheckTest(/*annotation_mode_enabled=*/false);

  histograms().ExpectTotalCount(kInputDeviceMetric, 0);

  // Draw a stroke with a pen.
  RunStrokePenCheckTest(/*annotation_mode_enabled=*/true);

  histograms().ExpectUniqueSample(kInputDeviceMetric,
                                  StrokeMetricInputDeviceType::kPen, 1);

  // Draw an eraser stroke with a pen that erases the first stroke.
  SelectEraserTool();
  const std::vector<base::span<const gfx::PointF>> move_point{
      base::span_from_ref(kMouseDownPoint),
  };
  ApplyStrokeWithPenAtPoints(base::span_from_ref(kMouseDownPoint), move_point,
                             base::span_from_ref(kMouseDownPoint));

  histograms().ExpectUniqueSample(kInputDeviceMetric,
                                  StrokeMetricInputDeviceType::kPen, 2);

  // Draw another eraser stroke with a pen that erases nothing.
  ApplyStrokeWithPenAtPoints(base::span_from_ref(kMouseDownPoint), move_point,
                             base::span_from_ref(kMouseDownPoint));

  histograms().ExpectUniqueSample(kInputDeviceMetric,
                                  StrokeMetricInputDeviceType::kPen, 2);
}

class PdfInkModuleTextHighlightTest : public PdfInkModuleUndoRedoTest {
 public:
  static constexpr TestAnnotationBrushMessageParams kOrangeBrushParams{
      SkColorSetRGB(0xFF, 0x63, 0x0C),
      /*size=*/6.0};
  static constexpr gfx::Rect kHorizontalSelection{10, 15, 30, 10};
  static constexpr gfx::Rect kVerticalSelection{10, 15, 6, 10};
  static constexpr gfx::PointF kStartPointInsidePage0{10.0, 10.0};
  static constexpr gfx::PointF kEndPointInsidePage0{15.0, 10.0};
  static constexpr SkColor kOrangeColor = SkColorSetRGB(0xFF, 0x63, 0x0C);

 protected:
  // Helper method for running a simple text highlighting test using text
  // selected by mouse with a single selection rect on page zero.
  void RunSingleSelectionWithMouseTest(
      const gfx::Rect& selection_rect,
      base::span<const PdfInkInputData> expected_inputs,
      float expected_size) {
    SetUpSingleSelectionTest(selection_rect);

    // Apply a text highlight stroke at the given points.
    ApplyStrokeWithMouseAtPoints(kStartPointInsidePage0, {kEndPointInsidePage0},
                                 kEndPointInsidePage0);

    VerifySingleSelectionTest(expected_inputs, expected_size);
  }

  // Sets the selection rects that will be given by the client.
  void SetSelectionRects(base::span<const gfx::Rect> selection_rects) {
    EXPECT_CALL(client(), GetSelectionRects())
        .WillRepeatedly(Return(base::ToVector(selection_rects)));
  }

  // Sets `points` as selectable text areas. Any points not included will be
  // considered non-selectable.
  void SetTextAreaPoints(base::span<const gfx::PointF> points) {
    EXPECT_CALL(client(), IsSelectableTextOrLinkArea(_))
        .WillRepeatedly(Return(false));
    for (const auto& point : points) {
      EXPECT_CALL(client(), IsSelectableTextOrLinkArea(point))
          .WillRepeatedly(Return(true));
    }
  }

  // Sets up single selection test expectations before text selection strokes
  // have been applied.
  void SetUpSingleSelectionTest(const gfx::Rect& selection_rect) {
    EnableDrawAnnotationMode();
    InitializeSimpleSinglePageBasicLayout();

    SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);

    SetSelectionRects(base::span_from_ref(selection_rect));
    SetTextAreaPoints(base::span_from_ref(kStartPointInsidePage0));

    EXPECT_CALL(client(), OnTextOrLinkAreaClick(kStartPointInsidePage0,
                                                /*click_count=*/1));
    EXPECT_CALL(client(), ExtendSelectionByPoint(kEndPointInsidePage0));
  }

  // Verifies single selection test results after applying text selection
  // strokes.
  void VerifySingleSelectionTest(
      base::span<const PdfInkInputData> expected_inputs,
      float expected_size) {
    EXPECT_EQ(1, client().stroke_started_count());
    EXPECT_EQ(1, client().modified_stroke_finished_count());
    EXPECT_EQ(0, client().unmodified_stroke_finished_count());
    EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0));

    std::optional<ink::StrokeInputBatch> expected_batch =
        CreateInkInputBatch(expected_inputs);
    ASSERT_TRUE(expected_batch.has_value());

    std::map<int, std::vector<raw_ref<const ink::Stroke>>> collected_strokes =
        CollectVisibleStrokes(ink_module().GetVisibleStrokesIterator());
    const PdfInkBrush expected_brush(PdfInkBrush::Type::kHighlighter,
                                     kOrangeColor, expected_size);
    EXPECT_THAT(
        collected_strokes,
        ElementsAre(Pair(0, Pointwise(InkStrokeEq(expected_brush.ink_brush()),
                                      {expected_batch.value()}))));

    // The current brush should remain a highlighter.
    const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
    ASSERT_TRUE(brush);
    const ink::Brush& ink_brush = brush->ink_brush();
    ASSERT_EQ(1u, ink_brush.CoatCount());
    const ink::BrushCoat& coat = ink_brush.GetCoats()[0];

    EXPECT_EQ(kOrangeColor, GetSkColorFromInkBrush(ink_brush));
    EXPECT_EQ(6.0f, ink_brush.GetSize());
    EXPECT_EQ(0.4f, coat.tip.opacity_multiplier);
  }

  void ClickTextAtPoint(const gfx::PointF& point, int click_count) {
    blink::WebMouseEvent mouse_down_event =
        MouseEventBuilder()
            .CreateLeftClickAtPosition(point)
            .SetClickCount(click_count)
            .Build();
    EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));

    // Do not create a mouse move event, as it would reset the click count.

    blink::WebMouseEvent mouse_up_event =
        MouseEventBuilder()
            .CreateLeftMouseUpAtPosition(point)
            .SetClickCount(click_count)
            .Build();
    EXPECT_TRUE(ink_module().HandleInputEvent(mouse_up_event));
  }
};

TEST_P(PdfInkModuleTextHighlightTest, PenDoesNotSelectText) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  // Select the pen tool with a "Light Red" color.
  SelectBrushTool(PdfInkBrush::Type::kPen, kRedBrushParams);

  EXPECT_CALL(client(), GetSelectionRects()).Times(0);
  SetTextAreaPoints(base::span_from_ref(kStartPointInsidePage0));

  EXPECT_CALL(client(), OnTextOrLinkAreaClick(_, _)).Times(0);
  EXPECT_CALL(client(), ExtendSelectionByPoint(_)).Times(0);

  // Apply a pen stroke at the given points.
  ApplyStrokeWithMouseAtPoints(kStartPointInsidePage0, {kEndPointInsidePage0},
                               kEndPointInsidePage0);

  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0));

  // The stroke inputs should match exactly.
  std::optional<ink::StrokeInputBatch> expected_batch =
      CreateInkInputBatch({PdfInkInputData(kStartPointInsidePage0),
                           PdfInkInputData(kEndPointInsidePage0)});
  ASSERT_TRUE(expected_batch.has_value());

  std::map<int, std::vector<raw_ref<const ink::Stroke>>> collected_strokes =
      CollectVisibleStrokes(ink_module().GetVisibleStrokesIterator());

  // The stroke should be a pen stroke.
  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  ASSERT_TRUE(brush);

  EXPECT_THAT(collected_strokes,
              ElementsAre(Pair(0, Pointwise(InkStrokeEq(brush->ink_brush()),
                                            {expected_batch.value()}))));
}

TEST_P(PdfInkModuleTextHighlightTest, SingleHorizontalSelection) {
  RunSingleSelectionWithMouseTest(
      /*selection_rect=*/kHorizontalSelection,
      /*expected_inputs=*/
      {PdfInkInputData(gfx::PointF(15.0, 20.0)),
       PdfInkInputData(gfx::PointF(35.0, 20.0))},
      /*expected_size=*/10.0);
}

TEST_P(PdfInkModuleTextHighlightTest, SingleVerticalSelection) {
  RunSingleSelectionWithMouseTest(
      /*selection_rect=*/kVerticalSelection,
      /*expected_inputs=*/
      {PdfInkInputData(gfx::PointF(13.0, 18.0)),
       PdfInkInputData(gfx::PointF(13.0, 22.0))},
      /*expected_size=*/6.0);
}

TEST_P(PdfInkModuleTextHighlightTest, SingleSquareSelection) {
  RunSingleSelectionWithMouseTest(
      /*selection_rect=*/gfx::Rect(10, 15, 12, 12),
      /*expected_inputs=*/
      {PdfInkInputData(gfx::PointF(16.0, 21.0))},
      /*expected_size=*/12.0);
}

TEST_P(PdfInkModuleTextHighlightTest,
       SingleHorizontalSelectionRotatedClockwise90) {
  client().set_orientation(PageOrientation::kClockwise90);
  RunSingleSelectionWithMouseTest(
      /*selection_rect=*/kHorizontalSelection,
      /*expected_inputs=*/
      {PdfInkInputData(gfx::PointF(20.0, 14.0)),
       PdfInkInputData(gfx::PointF(20.0, 34.0))},
      /*expected_size=*/10.0);
}

TEST_P(PdfInkModuleTextHighlightTest,
       SingleVerticalSelectionRotatedClockwise90) {
  client().set_orientation(PageOrientation::kClockwise90);
  RunSingleSelectionWithMouseTest(
      /*selection_rect=*/kVerticalSelection,
      /*expected_inputs=*/
      {PdfInkInputData(gfx::PointF(18.0, 36.0)),
       PdfInkInputData(gfx::PointF(22.0, 36.0))},
      /*expected_size=*/6.0);
}

TEST_P(PdfInkModuleTextHighlightTest,
       SingleHorizontalSelectionRotatedClockwise180) {
  client().set_orientation(PageOrientation::kClockwise180);
  RunSingleSelectionWithMouseTest(
      /*selection_rect=*/kHorizontalSelection,
      /*expected_inputs=*/
      {PdfInkInputData(gfx::PointF(14.0, 39.0)),
       PdfInkInputData(gfx::PointF(34.0, 39.0))},
      /*expected_size=*/10.0);
}

TEST_P(PdfInkModuleTextHighlightTest,
       SingleVerticalSelectionRotatedClockwise180) {
  client().set_orientation(PageOrientation::kClockwise180);
  RunSingleSelectionWithMouseTest(
      /*selection_rect=*/kVerticalSelection,
      /*expected_inputs=*/
      {PdfInkInputData(gfx::PointF(36.0, 37.0)),
       PdfInkInputData(gfx::PointF(36.0, 41.0))},
      /*expected_size=*/6.0);
}

TEST_P(PdfInkModuleTextHighlightTest,
       SingleHorizontalSelectionRotatedClockwise270) {
  client().set_orientation(PageOrientation::kClockwise270);
  RunSingleSelectionWithMouseTest(
      /*selection_rect=*/kHorizontalSelection,
      /*expected_inputs=*/
      {PdfInkInputData(gfx::PointF(39.0, 15.0)),
       PdfInkInputData(gfx::PointF(39.0, 35.0))},
      /*expected_size=*/10.0);
}

TEST_P(PdfInkModuleTextHighlightTest,
       SingleVerticalSelectionRotatedClockwise270) {
  client().set_orientation(PageOrientation::kClockwise270);
  RunSingleSelectionWithMouseTest(
      /*selection_rect=*/kVerticalSelection,
      /*expected_inputs=*/
      {PdfInkInputData(gfx::PointF(37.0, 13.0)),
       PdfInkInputData(gfx::PointF(41.0, 13.0))},
      /*expected_size=*/6.0);
}

TEST_P(PdfInkModuleTextHighlightTest, SingleSelectionZoomedIn) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();
  client().set_zoom(2.0f);

  RunSingleSelectionWithMouseTest(
      /*selection_rect=*/kHorizontalSelection,
      /*expected_inputs=*/
      {PdfInkInputData(gfx::PointF(7.5, 10.0)),
       PdfInkInputData(gfx::PointF(17.5, 10.0))},
      /*expected_size=*/5.0);
}

TEST_P(PdfInkModuleTextHighlightTest, SingleSelectionZoomedOut) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();
  client().set_zoom(0.5f);

  RunSingleSelectionWithMouseTest(
      /*selection_rect=*/kHorizontalSelection,
      /*expected_inputs=*/
      {PdfInkInputData(gfx::PointF(30.0, 40.0)),
       PdfInkInputData(gfx::PointF(70.0, 40.0))},
      /*expected_size=*/20.0);
}

TEST_P(PdfInkModuleTextHighlightTest, MultipleSelection) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);

  SetSelectionRects({kHorizontalSelection, gfx::Rect(15, 25, 10, 5)});
  constexpr gfx::PointF kEndPoint2InsidePage0{25.0, 30.0};
  SetTextAreaPoints({kStartPointInsidePage0, kEndPoint2InsidePage0});

  EXPECT_CALL(client(), OnTextOrLinkAreaClick(kStartPointInsidePage0,
                                              /*click_count=*/1));
  EXPECT_CALL(client(), ExtendSelectionByPoint(kEndPoint2InsidePage0));

  // Apply a text highlight stroke at the given points.
  ApplyStrokeWithMouseAtPoints(kStartPointInsidePage0, {kEndPoint2InsidePage0},
                               kEndPoint2InsidePage0);

  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0));

  std::optional<ink::StrokeInputBatch> expected_selection0_batch =
      CreateInkInputBatch({PdfInkInputData(gfx::PointF(15.0, 20.0)),
                           PdfInkInputData(gfx::PointF(35.0, 20.0))});
  ASSERT_TRUE(expected_selection0_batch.has_value());
  std::optional<ink::StrokeInputBatch> expected_selection1_batch =
      CreateInkInputBatch({PdfInkInputData(gfx::PointF(17.5, 27.0)),
                           PdfInkInputData(gfx::PointF(22.5, 27.0))});
  ASSERT_TRUE(expected_selection1_batch.has_value());

  std::map<int, std::vector<raw_ref<const ink::Stroke>>> collected_strokes =
      CollectVisibleStrokes(ink_module().GetVisibleStrokesIterator());
  ASSERT_EQ(1u, collected_strokes.size());

  std::vector<raw_ref<const ink::Stroke>>& strokes_on_page0 =
      collected_strokes[0];
  ASSERT_EQ(2u, strokes_on_page0.size());

  const PdfInkBrush expected_selection0_brush(PdfInkBrush::Type::kHighlighter,
                                              kOrangeColor, /*size=*/10.0f);

  raw_ref<const ink::Stroke> actual_selection0 = strokes_on_page0[0];
  EXPECT_THAT(actual_selection0->GetBrush(),
              ink::BrushEq(expected_selection0_brush.ink_brush()));
  EXPECT_THAT(actual_selection0->GetInputs(),
              ink::StrokeInputBatchEq(expected_selection0_batch.value()));

  const PdfInkBrush expected_selection1_brush(PdfInkBrush::Type::kHighlighter,
                                              kOrangeColor, /*size=*/5.0f);

  raw_ref<const ink::Stroke> actual_selection1 = strokes_on_page0[1];
  EXPECT_THAT(actual_selection1->GetBrush(),
              ink::BrushEq(expected_selection1_brush.ink_brush()));
  EXPECT_THAT(actual_selection1->GetInputs(),
              ink::StrokeInputBatchEq(expected_selection1_batch.value()));
}

TEST_P(PdfInkModuleTextHighlightTest, OneClickCount) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);

  // There will be no text selection rects.
  SetSelectionRects({});
  SetTextAreaPoints(base::span_from_ref(kStartPointInsidePage0));

  EXPECT_CALL(client(), OnTextOrLinkAreaClick(kStartPointInsidePage0,
                                              /*click_count=*/1));
  EXPECT_CALL(client(), ExtendSelectionByPoint(_)).Times(0);

  ClickTextAtPoint(kStartPointInsidePage0, /*click_count=*/1);

  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(0, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  EXPECT_TRUE(updated_ink_thumbnail_page_indices().empty());

  std::map<int, std::vector<raw_ref<const ink::Stroke>>> collected_strokes =
      CollectVisibleStrokes(ink_module().GetVisibleStrokesIterator());
  EXPECT_TRUE(collected_strokes.empty());
}

TEST_P(PdfInkModuleTextHighlightTest, TwoClickCount) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);
  SetTextAreaPoints(base::span_from_ref(kStartPointInsidePage0));

  ClickTextAtPoint(kStartPointInsidePage0, /*click_count=*/1);

  // The second text click will select the word.
  SetSelectionRects(base::span_from_ref(kHorizontalSelection));

  EXPECT_CALL(client(), OnTextOrLinkAreaClick(kStartPointInsidePage0,
                                              /*click_count=*/2));
  EXPECT_CALL(client(), ExtendSelectionByPoint(_)).Times(0);

  blink::WebMouseEvent mouse_down_event =
      MouseEventBuilder()
          .CreateLeftClickAtPosition(kStartPointInsidePage0)
          .SetClickCount(2)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));

  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0));

  std::optional<ink::StrokeInputBatch> expected_batch =
      CreateInkInputBatch({PdfInkInputData(gfx::PointF(15.0, 20.0)),
                           PdfInkInputData(gfx::PointF(35.0, 20.0))});
  ASSERT_TRUE(expected_batch.has_value());

  std::map<int, std::vector<raw_ref<const ink::Stroke>>> collected_strokes =
      CollectVisibleStrokes(ink_module().GetVisibleStrokesIterator());
  const PdfInkBrush expected_brush(PdfInkBrush::Type::kHighlighter,
                                   kOrangeColor,
                                   /*size=*/10.0f);
  EXPECT_THAT(
      collected_strokes,
      ElementsAre(Pair(0, Pointwise(InkStrokeEq(expected_brush.ink_brush()),
                                    {expected_batch.value()}))));

  // Mousemove and mouseup events will be handled but will not result in any
  // additional strokes.
  EXPECT_TRUE(ink_module().HandleInputEvent(
      CreateMouseMoveWithLeftButtonEventAtPoint(kStartPointInsidePage0)));
  blink::WebMouseEvent mouse_up_event =
      MouseEventBuilder()
          .CreateLeftMouseUpAtPosition(kStartPointInsidePage0)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_up_event));

  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
}

TEST_P(PdfInkModuleTextHighlightTest, ThreeClickCount) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);
  SetTextAreaPoints(base::span_from_ref(kStartPointInsidePage0));

  ClickTextAtPoint(kStartPointInsidePage0, /*click_count=*/1);

  SetSelectionRects(base::span_from_ref(kHorizontalSelection));
  ClickTextAtPoint(kStartPointInsidePage0, /*click_count=*/2);

  // The third text click will remove the original word text highlight and
  // select the line.
  SetSelectionRects(base::span_from_ref(gfx::Rect(5, 15, 45, 12)));

  EXPECT_CALL(client(), OnTextOrLinkAreaClick(kStartPointInsidePage0,
                                              /*click_count=*/3));
  EXPECT_CALL(client(), ExtendSelectionByPoint(_)).Times(0);

  blink::WebMouseEvent mouse_down_event =
      MouseEventBuilder()
          .CreateLeftClickAtPosition(kStartPointInsidePage0)
          .SetClickCount(3)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));

  // Two modified strokes: one from the double-click rect and another from the
  // triple-click rect.
  // One unmodified stroke: From the single-click.
  // Three ink thumbnail updates: one from the double-click rect, one from the
  // undo, and another from the triple-click rect.
  EXPECT_EQ(3, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 0, 0));

  std::optional<ink::StrokeInputBatch> expected_batch =
      CreateInkInputBatch({PdfInkInputData(gfx::PointF(11.0, 21.0)),
                           PdfInkInputData(gfx::PointF(44.0, 21.0))});
  ASSERT_TRUE(expected_batch.has_value());

  std::map<int, std::vector<raw_ref<const ink::Stroke>>> collected_strokes =
      CollectVisibleStrokes(ink_module().GetVisibleStrokesIterator());
  const PdfInkBrush expected_brush(PdfInkBrush::Type::kHighlighter,
                                   kOrangeColor,
                                   /*size=*/12.0f);
  EXPECT_THAT(
      collected_strokes,
      ElementsAre(Pair(0, Pointwise(InkStrokeEq(expected_brush.ink_brush()),
                                    {expected_batch.value()}))));

  // Mousemove and mouseup events will be handled but will not result in any
  // additional strokes.
  EXPECT_TRUE(ink_module().HandleInputEvent(
      CreateMouseMoveWithLeftButtonEventAtPoint(kStartPointInsidePage0)));
  blink::WebMouseEvent mouse_up_event =
      MouseEventBuilder()
          .CreateLeftMouseUpAtPosition(kStartPointInsidePage0)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_up_event));

  EXPECT_EQ(3, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
}

TEST_P(PdfInkModuleTextHighlightTest, MouseUpOnNonSelection) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);

  // Start in a text area.
  SetTextAreaPoints(base::span_from_ref(kStartPointInsidePage0));

  EXPECT_CALL(client(), OnTextOrLinkAreaClick(kStartPointInsidePage0,
                                              /*click_count=*/1));

  blink::WebMouseEvent mouse_down_event =
      MouseEventBuilder()
          .CreateLeftClickAtPosition(kStartPointInsidePage0)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));

  VerifyAndClearExpectations();

  // Move and end in a non-text area. Make the mock selection rect smaller than
  // the distance between the mousedown and mouseup points.
  SetSelectionRects(base::span_from_ref(gfx::Rect(10, 15, 2, 10)));

  EXPECT_CALL(client(), ExtendSelectionByPoint(kEndPointInsidePage0));

  blink::WebMouseEvent mouse_move_event =
      CreateMouseMoveWithLeftButtonEventAtPoint(kEndPointInsidePage0);
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_move_event));

  blink::WebMouseEvent mouse_up_event =
      MouseEventBuilder()
          .CreateLeftMouseUpAtPosition(kEndPointInsidePage0)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_up_event));

  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0));

  std::optional<ink::StrokeInputBatch> expected_batch =
      CreateInkInputBatch({PdfInkInputData(gfx::PointF(11.0, 16.0)),
                           PdfInkInputData(gfx::PointF(11.0, 24.0))});
  ASSERT_TRUE(expected_batch.has_value());

  std::map<int, std::vector<raw_ref<const ink::Stroke>>> collected_strokes =
      CollectVisibleStrokes(ink_module().GetVisibleStrokesIterator());
  const PdfInkBrush expected_brush(PdfInkBrush::Type::kHighlighter,
                                   kOrangeColor,
                                   /*size=*/2.0f);
  EXPECT_THAT(
      collected_strokes,
      ElementsAre(Pair(0, Pointwise(InkStrokeEq(expected_brush.ink_brush()),
                                    {expected_batch.value()}))));
}

TEST_P(PdfInkModuleTextHighlightTest, MultiplePages) {
  EnableDrawAnnotationMode();
  InitializeVerticalTwoPageLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);

  SetTextAreaPoints(
      {kStartPointInsidePage0, kTwoPageVerticalLayoutPoint1InsidePage1});

  // Start on page 0.
  EXPECT_CALL(client(), OnTextOrLinkAreaClick(kStartPointInsidePage0,
                                              /*click_count=*/1));

  blink::WebMouseEvent mouse_down_event =
      MouseEventBuilder()
          .CreateLeftClickAtPosition(kStartPointInsidePage0)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));

  // Move to page 1. Select rects from both pages.
  constexpr gfx::Rect kHorizontalSelectionInPage1{10, 75, 15, 14};
  SetSelectionRects({kHorizontalSelection, kHorizontalSelectionInPage1});
  EXPECT_CALL(client(),
              PageIndexFromPoint(gfx::PointF(kHorizontalSelection.origin())))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(client(), PageIndexFromPoint(
                            gfx::PointF(kHorizontalSelectionInPage1.origin())))
      .WillRepeatedly(Return(1));

  EXPECT_CALL(client(),
              ExtendSelectionByPoint(kTwoPageVerticalLayoutPoint1InsidePage1));

  blink::WebMouseEvent mouse_move_event =
      CreateMouseMoveWithLeftButtonEventAtPoint(
          kTwoPageVerticalLayoutPoint1InsidePage1);
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_move_event));

  blink::WebMouseEvent mouse_up_event =
      MouseEventBuilder()
          .CreateLeftMouseUpAtPosition(kTwoPageVerticalLayoutPoint1InsidePage1)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_up_event));

  // All the selection strokes are considered one stroke.
  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  EXPECT_THAT(updated_ink_thumbnail_page_indices(), ElementsAre(0, 1));

  std::optional<ink::StrokeInputBatch> expected_page0_batch =
      CreateInkInputBatch({PdfInkInputData(gfx::PointF(10.0, 15.0)),
                           PdfInkInputData(gfx::PointF(30.0, 15.0))});
  ASSERT_TRUE(expected_page0_batch.has_value());
  std::optional<ink::StrokeInputBatch> expected_page1_batch =
      CreateInkInputBatch({PdfInkInputData(gfx::PointF(12.0, 12.0)),
                           PdfInkInputData(gfx::PointF(13.0, 12.0))});
  ASSERT_TRUE(expected_page1_batch.has_value());

  std::map<int, std::vector<raw_ref<const ink::Stroke>>> collected_strokes =
      CollectVisibleStrokes(ink_module().GetVisibleStrokesIterator());

  const PdfInkBrush expected_page0_brush(PdfInkBrush::Type::kHighlighter,
                                         kOrangeColor, /*size=*/10.0f);
  const PdfInkBrush expected_page1_brush(PdfInkBrush::Type::kHighlighter,
                                         kOrangeColor, /*size=*/14.0f);
  EXPECT_THAT(
      collected_strokes,
      ElementsAre(
          Pair(0, Pointwise(InkStrokeEq(expected_page0_brush.ink_brush()),
                            {expected_page0_batch.value()})),
          Pair(1, Pointwise(InkStrokeEq(expected_page1_brush.ink_brush()),
                            {expected_page1_batch.value()}))));
}

TEST_P(PdfInkModuleTextHighlightTest,
       StrokeMissedEndEventThenMouseMoveDuringTextSelecting) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);

  SetSelectionRects(base::span_from_ref(gfx::Rect(9, 14, 5, 10)));
  EXPECT_CALL(client(), IsSelectableTextOrLinkArea(_))
      .WillRepeatedly(Return(true));

  // There should not be any text selection extension after the miss, as the
  // initial text selection has been terminated.
  EXPECT_CALL(client(), ExtendSelectionByPoint(_)).Times(0);

  RunStrokeMissedEndEventThenMouseMoveTest();
}

TEST_P(PdfInkModuleTextHighlightTest, TouchSingleHorizontalSelection) {
  SetUpSingleSelectionTest(kHorizontalSelection);

  // Apply a text highlight stroke at the given points.
  ApplyStrokeWithTouchAtPoints(base::span_from_ref(kStartPointInsidePage0),
                               {base::span_from_ref(kEndPointInsidePage0)},
                               base::span_from_ref(kEndPointInsidePage0));

  constexpr auto kExpectedInputs = std::to_array<PdfInkInputData>(
      {PdfInkInputData(gfx::PointF(15.0, 20.0)),
       PdfInkInputData(gfx::PointF(35.0, 20.0))});
  VerifySingleSelectionTest(kExpectedInputs, /*expected_size=*/10.0f);
}

TEST_P(PdfInkModuleTextHighlightTest, TouchOneClickCount) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);

  // There will be no text selection rects.
  SetSelectionRects({});
  SetTextAreaPoints(base::span_from_ref(kStartPointInsidePage0));

  EXPECT_CALL(client(), OnTextOrLinkAreaClick(kStartPointInsidePage0,
                                              /*click_count=*/1));
  EXPECT_CALL(client(), ExtendSelectionByPoint(_)).Times(0);

  blink::WebTouchEvent touch_event =
      CreateTouchEvent(blink::WebInputEvent::Type::kTouchStart,
                       base::span_from_ref(kStartPointInsidePage0));
  EXPECT_TRUE(ink_module().HandleInputEvent(touch_event));

  touch_event = CreateTouchEvent(blink::WebInputEvent::Type::kTouchEnd,
                                 base::span_from_ref(kStartPointInsidePage0));
  EXPECT_TRUE(ink_module().HandleInputEvent(touch_event));

  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(0, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  EXPECT_TRUE(updated_ink_thumbnail_page_indices().empty());

  std::map<int, std::vector<raw_ref<const ink::Stroke>>> collected_strokes =
      CollectVisibleStrokes(ink_module().GetVisibleStrokesIterator());
  EXPECT_TRUE(collected_strokes.empty());
}

TEST_P(PdfInkModuleTextHighlightTest, MultiTouchDoesNotSelectText) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);

  EXPECT_CALL(client(), IsSelectableTextOrLinkArea(_)).Times(0);

  ApplyStrokeWithTouchAtPointsNotHandled(
      {kStartPointInsidePage0, kStartPointInsidePage0},
      {{kEndPointInsidePage0, kEndPointInsidePage0}},
      {kEndPointInsidePage0, kEndPointInsidePage0});
}

TEST_P(PdfInkModuleTextHighlightTest, PenSingleHorizontalSelection) {
  const std::vector<PdfInkInputData> expected_inputs{
      PdfInkInputData(gfx::PointF(15.0, 20.0)),
      PdfInkInputData(gfx::PointF(35.0, 20.0))};
  SetUpSingleSelectionTest(kHorizontalSelection);

  // Apply a text highlight stroke at the given points.
  ApplyStrokeWithPenAtPoints(base::span_from_ref(kStartPointInsidePage0),
                             {base::span_from_ref(kEndPointInsidePage0)},
                             base::span_from_ref(kEndPointInsidePage0));

  VerifySingleSelectionTest(expected_inputs, /*expected_size=*/10.0f);
}

TEST_P(PdfInkModuleTextHighlightTest, PenOneClickCount) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);

  // There will be no text selection rects.
  SetSelectionRects({});
  SetTextAreaPoints(base::span_from_ref(kStartPointInsidePage0));

  EXPECT_CALL(client(), OnTextOrLinkAreaClick(kStartPointInsidePage0,
                                              /*click_count=*/1));
  EXPECT_CALL(client(), ExtendSelectionByPoint(_)).Times(0);

  blink::WebTouchEvent pen_event =
      CreatePenEvent(blink::WebInputEvent::Type::kTouchStart,
                     base::span_from_ref(kStartPointInsidePage0));
  EXPECT_TRUE(ink_module().HandleInputEvent(pen_event));

  pen_event = CreatePenEvent(blink::WebInputEvent::Type::kTouchEnd,
                             base::span_from_ref(kStartPointInsidePage0));
  EXPECT_TRUE(ink_module().HandleInputEvent(pen_event));

  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(0, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  EXPECT_TRUE(updated_ink_thumbnail_page_indices().empty());

  std::map<int, std::vector<raw_ref<const ink::Stroke>>> collected_strokes =
      CollectVisibleStrokes(ink_module().GetVisibleStrokesIterator());
  EXPECT_TRUE(collected_strokes.empty());
}

TEST_P(PdfInkModuleTextHighlightTest, CursorOnMouseMove) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  // Select the pen tool with a "Light Red" color. The cursor should be the
  // custom pen cursor.
  EXPECT_CALL(client(),
              UpdateInkCursor(CursorBitmapImageSizeEq(SkISize(8, 8))));

  TestAnnotationBrushMessageParams params = kRedBrushParams;
  SelectBrushTool(PdfInkBrush::Type::kPen, params);

  SetTextAreaPoints(base::span_from_ref(kStartPointInsidePage0));

  // Move to a text position. The cursor should remain as the custom pen cursor.
  blink::WebMouseEvent mouse_move_event =
      CreateMouseMoveEventAtPoint(kEndPointInsidePage0);
  // The event will be considered not handled, but the cursor will still update.
  EXPECT_FALSE(ink_module().HandleInputEvent(mouse_move_event));

  VerifyAndClearExpectations();

  // Select the highlighter tool. The cursor should be the custom highlighter
  // cursor.
  EXPECT_CALL(client(),
              UpdateInkCursor(CursorBitmapImageSizeEq(SkISize(10, 10))));
  params.size = 8.0f;
  SelectBrushTool(PdfInkBrush::Type::kHighlighter, params);

  VerifyAndClearExpectations();

  // Move to a text position. The cursor should be an I-beam.
  EXPECT_CALL(client(),
              UpdateInkCursor(ui::Cursor(ui::mojom::CursorType::kIBeam)));
  mouse_move_event = CreateMouseMoveEventAtPoint(kStartPointInsidePage0);
  EXPECT_FALSE(ink_module().HandleInputEvent(mouse_move_event));

  VerifyAndClearExpectations();

  // Move to a non-text position. The cursor should restore to the custom
  // highlighter cursor.
  {
    InSequence seq;
    EXPECT_CALL(client(), GetCursor())
        .WillOnce(Return(ui::mojom::CursorType::kIBeam));
    EXPECT_CALL(client(),
                UpdateInkCursor(CursorBitmapImageSizeEq(SkISize(10, 10))));
  }
  mouse_move_event = CreateMouseMoveEventAtPoint(kEndPointInsidePage0);
  EXPECT_FALSE(ink_module().HandleInputEvent(mouse_move_event));
}

TEST_P(PdfInkModuleTextHighlightTest, CursorOnMouseMoveWhileTextSelecting) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  // Select the highlighter tool. The cursor should be the custom highlighter
  // cursor.
  EXPECT_CALL(client(),
              UpdateInkCursor(CursorBitmapImageSizeEq(SkISize(8, 8))));
  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kRedBrushParams);

  VerifyAndClearExpectations();

  SetTextAreaPoints(base::span_from_ref(kStartPointInsidePage0));

  // Move to a text position. The cursor should be an I-beam.
  EXPECT_CALL(client(),
              UpdateInkCursor(ui::Cursor(ui::mojom::CursorType::kIBeam)));
  blink::WebMouseEvent mouse_move_event =
      CreateMouseMoveEventAtPoint(kStartPointInsidePage0);
  EXPECT_FALSE(ink_module().HandleInputEvent(mouse_move_event));

  VerifyAndClearExpectations();

  // Start text highlighting and move to a non-text position. The cursor should
  // remain as an I-beam.
  EXPECT_CALL(client(), UpdateInkCursor(_)).Times(0);
  blink::WebMouseEvent mouse_down_event =
      MouseEventBuilder()
          .CreateLeftClickAtPosition(kStartPointInsidePage0)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));
  mouse_move_event =
      CreateMouseMoveWithLeftButtonEventAtPoint(kEndPointInsidePage0);
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_move_event));

  VerifyAndClearExpectations();

  // End text highlighting. The cursor should restore to the custom highlighter
  // cursor.
  EXPECT_CALL(client(),
              UpdateInkCursor(CursorBitmapImageSizeEq(SkISize(8, 8))));
  blink::WebMouseEvent mouse_up_event =
      MouseEventBuilder()
          .CreateLeftMouseUpAtPosition(kEndPointInsidePage0)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_up_event));
}

TEST_P(PdfInkModuleTextHighlightTest, CursorOnMouseMoveWhileBrushDrawing) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  // Select the highlighter tool. The cursor should be the custom highlighter
  // cursor.
  EXPECT_CALL(client(),
              UpdateInkCursor(CursorBitmapImageSizeEq(SkISize(8, 8))));
  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kRedBrushParams);

  VerifyAndClearExpectations();

  SetTextAreaPoints(base::span_from_ref(kEndPointInsidePage0));

  // Move to a non-text position. The cursor should remain as the custom
  // highlighter cursor.
  EXPECT_CALL(client(), UpdateInkCursor(_)).Times(0);
  blink::WebMouseEvent mouse_move_event =
      CreateMouseMoveEventAtPoint(kStartPointInsidePage0);
  EXPECT_FALSE(ink_module().HandleInputEvent(mouse_move_event));

  VerifyAndClearExpectations();

  // Start drawing and move to a text position. The cursor should remain as the
  // custom highlighter cursor.
  EXPECT_CALL(client(), UpdateInkCursor(_)).Times(0);
  blink::WebMouseEvent mouse_down_event =
      MouseEventBuilder()
          .CreateLeftClickAtPosition(kStartPointInsidePage0)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_down_event));
  mouse_move_event =
      CreateMouseMoveWithLeftButtonEventAtPoint(kEndPointInsidePage0);
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_move_event));

  VerifyAndClearExpectations();

  // End drawing. The cursor should be an I-beam.
  EXPECT_CALL(client(),
              UpdateInkCursor(ui::Cursor(ui::mojom::CursorType::kIBeam)));
  blink::WebMouseEvent mouse_up_event =
      MouseEventBuilder()
          .CreateLeftMouseUpAtPosition(kEndPointInsidePage0)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_up_event));
}

class PdfInkModuleTextHighlightMetricsTest
    : public PdfInkModuleMetricsTestBase,
      public PdfInkModuleTextHighlightTest {
 protected:
  static constexpr char kTextHighlightColorMetric[] =
      "PDF.Ink2TextHighlighterColor";
  static constexpr char kTextHighlightInputDeviceMetric[] =
      "PDF.Ink2TextHighlightInputDeviceType";
  static constexpr base::TimeDelta kOneMs = base::Milliseconds(1);
  static constexpr base::TimeDelta kTextSelectionClickTimeMs =
      base::Milliseconds(ui::kDoubleClickTimeMs);

  // Helper for moving the mouse and releasing left click at `point` with
  // `click_count` clicks.
  void MouseMoveAndUpAtPoint(const gfx::PointF& point, int click_count) {
    EXPECT_TRUE(ink_module().HandleInputEvent(
        CreateMouseMoveWithLeftButtonEventAtPoint(point)));

    blink::WebMouseEvent mouse_up_event =
        MouseEventBuilder()
            .CreateLeftMouseUpAtPosition(point)
            .SetClickCount(click_count)
            .Build();
    EXPECT_TRUE(ink_module().HandleInputEvent(mouse_up_event));
  }

  // Validates the the total counts of all relevant text highlight metrics.
  void ValidateHighlightMetricCounts(int expected_metric_count) {
    histograms().ExpectTotalCount(kTextHighlightColorMetric,
                                  expected_metric_count);
    histograms().ExpectTotalCount(kTextHighlightInputDeviceMetric,
                                  expected_metric_count);
  }
};

TEST_P(PdfInkModuleTextHighlightMetricsTest,
       StrokeDoesNotAffectTextHighlightMetrics) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(0);
}

TEST_P(PdfInkModuleTextHighlightMetricsTest,
       TextHighlightDoesNotAffectStrokeMetrics) {
  RunSingleSelectionWithMouseTest(
      /*selection_rect=*/kHorizontalSelection,
      /*expected_inputs=*/
      {PdfInkInputData(gfx::PointF(15.0, 20.0)),
       PdfInkInputData(gfx::PointF(35.0, 20.0))},
      /*expected_size=*/10.0);

  histograms().ExpectTotalCount(kHighlighterColorMetric, 0);
  histograms().ExpectTotalCount(kInputDeviceMetric, 0);
  histograms().ExpectTotalCount(kHighlighterSizeMetric, 0);
  histograms().ExpectTotalCount(kTypeMetric, 0);
}

TEST_P(PdfInkModuleTextHighlightMetricsTest,
       TextHighlightUndoRedoDoesNotAffectMetrics) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  RunSingleSelectionWithMouseTest(
      /*selection_rect=*/kHorizontalSelection,
      /*expected_inputs=*/
      {PdfInkInputData(gfx::PointF(15.0, 20.0)),
       PdfInkInputData(gfx::PointF(35.0, 20.0))},
      /*expected_size=*/10.0);

  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(1);

  PerformUndo();
  PerformRedo();

  EXPECT_EQ(1, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(1);
}

TEST_P(PdfInkModuleTextHighlightMetricsTest, Color) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kRedBrushParams);

  histograms().ExpectTotalCount(kTextHighlightColorMetric, 0);

  SetSelectionRects(base::span_from_ref(kHorizontalSelection));
  SetTextAreaPoints({kStartPointInsidePage0, kEndPointInsidePage0});

  ApplyStrokeWithMouseAtPoints(kStartPointInsidePage0, {kEndPointInsidePage0},
                               kEndPointInsidePage0);

  histograms().ExpectUniqueSample(kTextHighlightColorMetric,
                                  StrokeMetricHighlighterColor::kLightRed, 1);

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);
  ApplyStrokeWithMouseAtPoints(kStartPointInsidePage0, {kEndPointInsidePage0},
                               kEndPointInsidePage0);

  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  histograms().ExpectBucketCount(kTextHighlightColorMetric,
                                 StrokeMetricHighlighterColor::kOrange, 1);
  histograms().ExpectTotalCount(kTextHighlightColorMetric, 2);
}

TEST_P(PdfInkModuleTextHighlightMetricsTest, InputDevice) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);

  histograms().ExpectTotalCount(kTextHighlightInputDeviceMetric, 0);

  SetSelectionRects(base::span_from_ref(kHorizontalSelection));
  SetTextAreaPoints({kStartPointInsidePage0, kEndPointInsidePage0});

  // Apply a text highlight stroke with mouse.
  ApplyStrokeWithMouseAtPoints(kStartPointInsidePage0, {kEndPointInsidePage0},
                               kEndPointInsidePage0);

  histograms().ExpectUniqueSample(kTextHighlightInputDeviceMetric,
                                  StrokeMetricInputDeviceType::kMouse, 1);

  // Apply a text highlight stroke with touch.
  ApplyStrokeWithTouchAtPoints(base::span_from_ref(kStartPointInsidePage0),
                               {base::span_from_ref(kEndPointInsidePage0)},
                               base::span_from_ref(kEndPointInsidePage0));

  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  histograms().ExpectBucketCount(kTextHighlightInputDeviceMetric,
                                 StrokeMetricInputDeviceType::kTouch, 1);
  histograms().ExpectTotalCount(kTextHighlightInputDeviceMetric, 2);

  // Apply a text highlight stroke with pen.
  ApplyStrokeWithPenAtPoints(base::span_from_ref(kStartPointInsidePage0),
                             {base::span_from_ref(kEndPointInsidePage0)},
                             base::span_from_ref(kEndPointInsidePage0));

  EXPECT_EQ(3, client().stroke_started_count());
  EXPECT_EQ(3, client().modified_stroke_finished_count());
  EXPECT_EQ(0, client().unmodified_stroke_finished_count());
  histograms().ExpectBucketCount(kTextHighlightInputDeviceMetric,
                                 StrokeMetricInputDeviceType::kPen, 1);
  histograms().ExpectTotalCount(kTextHighlightInputDeviceMetric, 3);
}

TEST_P(PdfInkModuleTextHighlightMetricsTest, TwoClickDelay) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);
  SetTextAreaPoints(base::span_from_ref(kStartPointInsidePage0));

  // Click twice.
  ClickTextAtPoint(kStartPointInsidePage0, /*click_count=*/1);
  SetSelectionRects(base::span_from_ref(kHorizontalSelection));
  ClickTextAtPoint(kStartPointInsidePage0, /*click_count=*/2);

  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(0);

  // Fast forward to just one ms before the timer should fire.
  GetPdfTestTaskEnvironment().FastForwardBy(kTextSelectionClickTimeMs - kOneMs);

  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(0);

  // Fast forward by one ms so the timer fires.
  GetPdfTestTaskEnvironment().FastForwardBy(kOneMs);

  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(1);
}

TEST_P(PdfInkModuleTextHighlightMetricsTest, TwoClickMove) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);
  SetTextAreaPoints(base::span_from_ref(kStartPointInsidePage0));

  // Click once.
  ClickTextAtPoint(kStartPointInsidePage0, /*click_count=*/1);

  // Click the second time, but without mouseup.
  SetSelectionRects(base::span_from_ref(kHorizontalSelection));
  blink::WebMouseEvent mouse_event =
      MouseEventBuilder()
          .CreateLeftClickAtPosition(kStartPointInsidePage0)
          .SetClickCount(2)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_event));

  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(0);

  // Fast forward by the click time duration.
  GetPdfTestTaskEnvironment().FastForwardBy(kTextSelectionClickTimeMs);

  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(1);

  // Now move and release at a new position.
  MouseMoveAndUpAtPoint(kEndPointInsidePage0, /*click_count=*/2);

  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(1);

  // Fast forward by the click time duration.
  GetPdfTestTaskEnvironment().FastForwardBy(kTextSelectionClickTimeMs);

  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(1);
}

TEST_P(PdfInkModuleTextHighlightMetricsTest, TwoClickMoveHighlight) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);
  constexpr gfx::PointF kStartPoint2InsidePage0{12.0f, 10.0f};
  SetTextAreaPoints({kStartPointInsidePage0, kStartPoint2InsidePage0});

  // Click twice.
  ClickTextAtPoint(kStartPointInsidePage0, /*click_count=*/1);
  SetSelectionRects(base::span_from_ref(kHorizontalSelection));
  ClickTextAtPoint(kStartPointInsidePage0, /*click_count=*/2);

  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(0);

  // Text highlight elsewhere.
  EXPECT_CALL(client(), OnTextOrLinkAreaClick(kStartPoint2InsidePage0,
                                              /*click_count=*/1));
  EXPECT_CALL(client(), ExtendSelectionByPoint(kEndPointInsidePage0));
  ApplyStrokeWithMouseAtPoints(kStartPoint2InsidePage0, {kEndPointInsidePage0},
                               kEndPointInsidePage0);

  // There should be reports for the double-click highlight and the new
  // highlight.
  EXPECT_EQ(3, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(2);

  // Fast forward by the click time duration.
  GetPdfTestTaskEnvironment().FastForwardBy(kTextSelectionClickTimeMs);

  EXPECT_EQ(3, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(2);
}

TEST_P(PdfInkModuleTextHighlightMetricsTest, ThreeClickDelay) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);
  SetTextAreaPoints(base::span_from_ref(kStartPointInsidePage0));

  // Click twice.
  ClickTextAtPoint(kStartPointInsidePage0, /*click_count=*/1);
  SetSelectionRects(base::span_from_ref(kHorizontalSelection));
  ClickTextAtPoint(kStartPointInsidePage0, /*click_count=*/2);

  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(0);

  // Fast forward to just one ms before the timer would fire.
  GetPdfTestTaskEnvironment().FastForwardBy(kTextSelectionClickTimeMs - kOneMs);

  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(0);

  // Click the third time.
  ClickTextAtPoint(kStartPointInsidePage0, /*click_count=*/3);

  EXPECT_EQ(3, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(1);

  // Fast forward by the click time duration.
  GetPdfTestTaskEnvironment().FastForwardBy(kTextSelectionClickTimeMs);

  EXPECT_EQ(3, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(1);
}

TEST_P(PdfInkModuleTextHighlightMetricsTest, ThreeClickMove) {
  EnableDrawAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  SelectBrushTool(PdfInkBrush::Type::kHighlighter, kOrangeBrushParams);
  SetTextAreaPoints(base::span_from_ref(kStartPointInsidePage0));

  // Click twice.
  ClickTextAtPoint(kStartPointInsidePage0, /*click_count=*/1);
  SetSelectionRects(base::span_from_ref(kHorizontalSelection));
  ClickTextAtPoint(kStartPointInsidePage0, /*click_count=*/2);

  EXPECT_EQ(2, client().stroke_started_count());
  EXPECT_EQ(1, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(0);

  // Click the third time, but without mouseup.
  blink::WebMouseEvent mouse_event =
      MouseEventBuilder()
          .CreateLeftClickAtPosition(kStartPointInsidePage0)
          .SetClickCount(3)
          .Build();
  EXPECT_TRUE(ink_module().HandleInputEvent(mouse_event));

  EXPECT_EQ(3, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(1);

  // Fast forward by the click time duration.
  GetPdfTestTaskEnvironment().FastForwardBy(kTextSelectionClickTimeMs);

  EXPECT_EQ(3, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(1);

  // Now move and release at a new position.
  MouseMoveAndUpAtPoint(kEndPointInsidePage0, /*click_count=*/3);

  EXPECT_EQ(3, client().stroke_started_count());
  EXPECT_EQ(2, client().modified_stroke_finished_count());
  EXPECT_EQ(1, client().unmodified_stroke_finished_count());
  ValidateHighlightMetricCounts(1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PdfInkModuleTest,
                         testing::ValuesIn(GetAllInkTestVariations()));
INSTANTIATE_TEST_SUITE_P(All,
                         PdfInkModuleStrokeTest,
                         testing::ValuesIn(GetAllInkTestVariations()));
INSTANTIATE_TEST_SUITE_P(All,
                         PdfInkModuleUndoRedoTest,
                         testing::ValuesIn(GetAllInkTestVariations()));
INSTANTIATE_TEST_SUITE_P(All,
                         PdfInkModuleGetVisibleStrokesTest,
                         testing::ValuesIn(GetAllInkTestVariations()));
INSTANTIATE_TEST_SUITE_P(All,
                         PdfInkModuleMetricsTest,
                         testing::ValuesIn(GetAllInkTestVariations()));
INSTANTIATE_TEST_SUITE_P(
    All,
    PdfInkModuleTextHighlightTest,
    testing::ValuesIn(GetInkTestVariationsWithTextHighlighting()));
INSTANTIATE_TEST_SUITE_P(
    All,
    PdfInkModuleTextHighlightMetricsTest,
    testing::ValuesIn(GetInkTestVariationsWithTextHighlighting()));

}  // namespace chrome_pdf
