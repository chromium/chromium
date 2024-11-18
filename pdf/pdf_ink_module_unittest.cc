// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_module.h"

#include <array>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/cfi_buildflags.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "pdf/pdf_features.h"
#include "pdf/pdf_ink_brush.h"
#include "pdf/pdf_ink_conversions.h"
#include "pdf/pdf_ink_module_client.h"
#include "pdf/pdf_ink_transform.h"
#include "pdf/pdfium/pdfium_ink_reader.h"
#include "pdf/test/mouse_event_builder.h"
#include "pdf/test/pdf_ink_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/ink/src/ink/brush/brush.h"
#include "third_party/ink/src/ink/brush/type_matchers.h"
#include "third_party/ink/src/ink/geometry/affine_transform.h"
#include "third_party/ink/src/ink/strokes/input/type_matchers.h"
#include "third_party/skia/include/core/SkBitmap.h"
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
using testing::Pair;
using testing::Pointwise;
using testing::Return;
using testing::SizeIs;

namespace chrome_pdf {

namespace {

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

// Matcher for ink::Stroke objects against their expected brush and inputs.
MATCHER_P(InkStrokeEq, expected_brush, "") {
  const auto& [actual_stroke, expected_inputs] = arg;
  const auto brush_matcher = ink::BrushEq(expected_brush);
  const auto input_matcher = ink::StrokeInputBatchEq(expected_inputs);
  return testing::Matches(brush_matcher)(actual_stroke->GetBrush()) &&
         testing::Matches(input_matcher)(actual_stroke->GetInputs());
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

base::Value::Dict CreateGetAnnotationBrushMessageForTesting(
    const std::string& brush_type) {
  base::Value::Dict message;
  message.Set("type", "getAnnotationBrush");
  message.Set("messageId", "foo");
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

class FakeClient : public PdfInkModuleClient {
 public:
  FakeClient() = default;
  FakeClient(const FakeClient&) = delete;
  FakeClient& operator=(const FakeClient&) = delete;
  ~FakeClient() override = default;

  // PdfInkModuleClient:
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

  bool IsPageVisible(int page_index) override {
    return base::Contains(visible_page_indices_, page_index);
  }

  MOCK_METHOD(PdfInkModuleClient::DocumentV2InkPathShapesMap,
              LoadV2InkPathsFromPdf,
              (),
              (override));

  MOCK_METHOD(void, PostMessage, (base::Value::Dict message), (override));

  MOCK_METHOD(void,
              StrokeAdded,
              (int page_index, InkStrokeId id, const ink::Stroke& stroke),
              (override));

  void StrokeFinished() override { ++stroke_finished_count_; }

  MOCK_METHOD(void, UpdateInkCursorImage, (SkBitmap bitmap), (override));

  MOCK_METHOD(void,
              UpdateShapeActive,
              (int page_index, InkModeledShapeId id, bool active),
              (override));

  MOCK_METHOD(void,
              UpdateStrokeActive,
              (int page_index, InkStrokeId id, bool active),
              (override));

  MOCK_METHOD(void,
              DiscardStroke,
              (int page_index, InkStrokeId id),
              (override));

  void UpdateThumbnail(int page_index) override {
    updated_thumbnail_page_indices_.push_back(page_index);
  }

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

  const std::vector<int>& updated_thumbnail_page_indices() const {
    return updated_thumbnail_page_indices_;
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
  int stroke_finished_count_ = 0;
  std::vector<int> updated_thumbnail_page_indices_;
  std::vector<gfx::RectF> page_layouts_;
  std::set<int> visible_page_indices_;
  PageOrientation orientation_ = PageOrientation::kOriginal;
  gfx::Vector2dF viewport_origin_offset_;
  float zoom_ = 1.0f;
  std::vector<gfx::Rect> invalidations_;
};

class PdfInkModuleTest : public testing::Test {
 protected:
  void EnableAnnotationMode() {
    EXPECT_TRUE(
        ink_module().OnMessage(CreateSetAnnotationModeMessageForTesting(true)));
  }

  FakeClient& client() { return client_; }
  PdfInkModule& ink_module() { return ink_module_; }
  const PdfInkModule& ink_module() const { return ink_module_; }

 private:
  base::test::ScopedFeatureList feature_list_{features::kPdfInk2};

  FakeClient client_;
  PdfInkModule ink_module_{client_};
};

}  // namespace

TEST_F(PdfInkModuleTest, UnknownMessage) {
  base::Value::Dict message;
  message.Set("type", "nonInkMessage");
  EXPECT_FALSE(ink_module().OnMessage(message));
}

// Verify that a get eraser message gets the eraser parameters.
TEST_F(PdfInkModuleTest, HandleGetAnnotationBrushMessageEraser) {
  EnableAnnotationMode();
  EXPECT_TRUE(ink_module().enabled());

  EXPECT_CALL(client(), PostMessage)
      .WillOnce([](const base::Value::Dict& dict) {
        auto expected = base::test::ParseJsonDict(R"({
            "type": "getAnnotationBrushReply",
            "messageId": "foo",
            "data": {
              "type": "eraser",
              "size": 3.0,
            },
        })");
        EXPECT_THAT(dict, base::test::DictionaryHasValues(expected));
      });

  EXPECT_TRUE(ink_module().OnMessage(
      CreateGetAnnotationBrushMessageForTesting("eraser")));
}

// Verify that a get pen message gets the pen brush parameters.
TEST_F(PdfInkModuleTest, HandleGetAnnotationBrushMessagePen) {
  EnableAnnotationMode();
  EXPECT_TRUE(ink_module().enabled());

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
TEST_F(PdfInkModuleTest, HandleGetAnnotationBrushMessageHighlighter) {
  EnableAnnotationMode();
  EXPECT_TRUE(ink_module().enabled());

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
TEST_F(PdfInkModuleTest, HandleGetAnnotationBrushMessageDefault) {
  EnableAnnotationMode();
  EXPECT_TRUE(ink_module().enabled());

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
TEST_F(PdfInkModuleTest, HandleGetAnnotationBrushMessageCurrent) {
  EnableAnnotationMode();
  EXPECT_TRUE(ink_module().enabled());

  // Set the brush to eraser.
  EXPECT_TRUE(ink_module().OnMessage(CreateSetAnnotationBrushMessageForTesting(
      "eraser", /*size=*/4.5, nullptr)));

  EXPECT_CALL(client(), PostMessage)
      .WillOnce([](const base::Value::Dict& dict) {
        auto expected = base::test::ParseJsonDict(R"({
            "type": "getAnnotationBrushReply",
            "messageId": "foo",
            "data": {
              "type": "eraser",
              "size": 4.5,
            },
        })");
        EXPECT_THAT(dict, base::test::DictionaryHasValues(expected));
      });

  EXPECT_TRUE(
      ink_module().OnMessage(CreateGetAnnotationBrushMessageForTesting("")));
}

// Verify that a set eraser message sets the annotation brush to an eraser.
TEST_F(PdfInkModuleTest, HandleSetAnnotationBrushMessageEraser) {
  EnableAnnotationMode();
  EXPECT_TRUE(ink_module().enabled());

  base::Value::Dict message = CreateSetAnnotationBrushMessageForTesting(
      "eraser", /*size=*/2.5, nullptr);
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
  EXPECT_TRUE(ink_module().enabled());

  TestAnnotationBrushMessageParams message_params{/*color_r=*/10,
                                                  /*color_g=*/255,
                                                  /*color_b=*/50};
  base::Value::Dict message = CreateSetAnnotationBrushMessageForTesting(
      "pen", /*size=*/8.0, &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  ASSERT_TRUE(brush);

  const ink::Brush& ink_brush = brush->ink_brush();
  EXPECT_EQ(SkColorSetRGB(10, 255, 50), GetSkColorFromInkBrush(ink_brush));
  EXPECT_EQ(8.0f, ink_brush.GetSize());
  ASSERT_EQ(1u, ink_brush.CoatCount());
  const ink::BrushCoat& coat = ink_brush.GetCoats()[0];
  ASSERT_EQ(1u, coat.tips.size());
  EXPECT_EQ(1.0f, coat.tips[0].corner_rounding);
  EXPECT_EQ(1.0f, coat.tips[0].opacity_multiplier);
}

// Verify that a set highlighter message sets the annotation brush to a
// highlighter, with the given params.
TEST_F(PdfInkModuleTest, HandleSetAnnotationBrushMessageHighlighter) {
  EnableAnnotationMode();
  EXPECT_TRUE(ink_module().enabled());

  TestAnnotationBrushMessageParams message_params{/*color_r=*/240,
                                                  /*color_g=*/133,
                                                  /*color_b=*/0};
  base::Value::Dict message = CreateSetAnnotationBrushMessageForTesting(
      "highlighter", /*size=*/4.5, &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  ASSERT_TRUE(brush);

  const ink::Brush& ink_brush = brush->ink_brush();
  EXPECT_EQ(SkColorSetRGB(240, 133, 0), GetSkColorFromInkBrush(ink_brush));
  EXPECT_EQ(4.5f, ink_brush.GetSize());
  ASSERT_EQ(1u, ink_brush.CoatCount());
  const ink::BrushCoat& coat = ink_brush.GetCoats()[0];
  ASSERT_EQ(1u, coat.tips.size());
  EXPECT_EQ(0.0f, coat.tips[0].corner_rounding);
  EXPECT_EQ(0.4f, coat.tips[0].opacity_multiplier);
}

// Verify that brushes with zero color values can be set as the annotation
// brush.
TEST_F(PdfInkModuleTest, HandleSetAnnotationBrushMessageColorZero) {
  EnableAnnotationMode();
  EXPECT_TRUE(ink_module().enabled());

  TestAnnotationBrushMessageParams message_params{/*color_r=*/0, /*color_g=*/0,
                                                  /*color_b=*/0};
  base::Value::Dict message = CreateSetAnnotationBrushMessageForTesting(
      "pen", /*size=*/4.5, &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  const PdfInkBrush* brush = ink_module().GetPdfInkBrushForTesting();
  ASSERT_TRUE(brush);

  const ink::Brush& ink_brush = brush->ink_brush();
  EXPECT_EQ(SK_ColorBLACK, GetSkColorFromInkBrush(ink_brush));
  EXPECT_EQ(4.5f, ink_brush.GetSize());
  ASSERT_EQ(1u, ink_brush.CoatCount());
  const ink::BrushCoat& coat = ink_brush.GetCoats()[0];
  ASSERT_EQ(1u, coat.tips.size());
  EXPECT_EQ(1.0f, coat.tips[0].corner_rounding);
  EXPECT_EQ(1.0f, coat.tips[0].opacity_multiplier);
}

TEST_F(PdfInkModuleTest, HandleSetAnnotationModeMessage) {
  EXPECT_CALL(client(), LoadV2InkPathsFromPdf())
      .WillOnce(Return(PdfInkModuleClient::DocumentV2InkPathShapesMap{
          {0,
           PdfInkModuleClient::PageV2InkPathShapesMap{
               {InkModeledShapeId(0), ink::ModeledShape()},
               {InkModeledShapeId(1), ink::ModeledShape()}}},
          {3,
           PdfInkModuleClient::PageV2InkPathShapesMap{
               {InkModeledShapeId(2), ink::ModeledShape()}}},
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
      CreateSetAnnotationModeMessageForTesting(/*enable=*/false);

  EXPECT_TRUE(ink_module().OnMessage(message));
  EXPECT_FALSE(ink_module().enabled());
  EXPECT_TRUE(ink_module().loaded_v2_shapes_.empty());

  message.Set("enable", true);
  EXPECT_TRUE(ink_module().OnMessage(message));
  EXPECT_TRUE(ink_module().enabled());
  EXPECT_THAT(ink_module().loaded_v2_shapes_, kShapeMapMatcher);

  message.Set("enable", false);
  EXPECT_TRUE(ink_module().OnMessage(message));
  EXPECT_FALSE(ink_module().enabled());
  EXPECT_THAT(ink_module().loaded_v2_shapes_, kShapeMapMatcher);
}

TEST_F(PdfInkModuleTest, MaybeSetCursorWhenTogglingAnnotationMode) {
  EXPECT_FALSE(ink_module().enabled());

  EXPECT_CALL(client(), UpdateInkCursorImage(_))
      .WillOnce(
          [this](SkBitmap bitmap) { EXPECT_TRUE(ink_module().enabled()); });

  base::Value::Dict message =
      CreateSetAnnotationModeMessageForTesting(/*enable=*/true);
  EXPECT_TRUE(ink_module().OnMessage(message));
  EXPECT_TRUE(ink_module().enabled());

  message.Set("enable", false);
  EXPECT_TRUE(ink_module().OnMessage(message));
  EXPECT_FALSE(ink_module().enabled());
}

TEST_F(PdfInkModuleTest, MaybeSetCursorWhenChangingBrushes) {
  {
    InSequence seq;
    EXPECT_CALL(client(), UpdateInkCursorImage(_))
        .WillOnce([](SkBitmap bitmap) {
          EXPECT_EQ(6, bitmap.width());
          EXPECT_EQ(6, bitmap.height());
        });
    EXPECT_CALL(client(), UpdateInkCursorImage(_))
        .WillOnce([](SkBitmap bitmap) {
          EXPECT_EQ(20, bitmap.width());
          EXPECT_EQ(20, bitmap.height());
        });
    EXPECT_CALL(client(), UpdateInkCursorImage(_))
        .WillOnce([](SkBitmap bitmap) {
          EXPECT_EQ(10, bitmap.width());
          EXPECT_EQ(10, bitmap.height());
        });
  }

  EnableAnnotationMode();
  EXPECT_TRUE(ink_module().enabled());

  TestAnnotationBrushMessageParams message_params{/*color_r=*/0,
                                                  /*color_g=*/255,
                                                  /*color_b=*/0};
  base::Value::Dict message = CreateSetAnnotationBrushMessageForTesting(
      "pen", /*size=*/16.0, &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  message = CreateSetAnnotationBrushMessageForTesting("eraser", /*size=*/8.0,
                                                      nullptr);
  EXPECT_TRUE(ink_module().OnMessage(message));
}

TEST_F(PdfInkModuleTest, MaybeSetCursorWhenChangingZoom) {
  {
    InSequence seq;
    EXPECT_CALL(client(), UpdateInkCursorImage(_))
        .WillOnce([](SkBitmap bitmap) {
          EXPECT_EQ(6, bitmap.width());
          EXPECT_EQ(6, bitmap.height());
        });
    EXPECT_CALL(client(), UpdateInkCursorImage(_))
        .WillOnce([](SkBitmap bitmap) {
          EXPECT_EQ(20, bitmap.width());
          EXPECT_EQ(20, bitmap.height());
        });
    EXPECT_CALL(client(), UpdateInkCursorImage(_))
        .WillOnce([](SkBitmap bitmap) {
          EXPECT_EQ(10, bitmap.width());
          EXPECT_EQ(10, bitmap.height());
        });
  }

  EnableAnnotationMode();
  EXPECT_TRUE(ink_module().enabled());

  TestAnnotationBrushMessageParams message_params{/*color_r=*/0,
                                                  /*color_g=*/255,
                                                  /*color_b=*/0};
  base::Value::Dict message = CreateSetAnnotationBrushMessageForTesting(
      "pen", /*size=*/16.0, &message_params);
  EXPECT_TRUE(ink_module().OnMessage(message));

  client().set_zoom(0.5f);
  ink_module().OnGeometryChanged();
}

TEST_F(PdfInkModuleTest, ContentFocusedPostMessage) {
  EnableAnnotationMode();
  EXPECT_TRUE(ink_module().enabled());

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

class PdfInkModuleStrokeTest : public PdfInkModuleTest {
 protected:
  // Mouse locations used for `RunStrokeCheckTest()`.
  // Touch events may use the same coordinates.
  static constexpr gfx::PointF kMouseDownPoint = gfx::PointF(10.0f, 15.0f);
  static constexpr gfx::PointF kMouseMovePoint = gfx::PointF(20.0f, 25.0f);
  static constexpr gfx::PointF kMouseUpPoint = gfx::PointF(30.0f, 17.0f);
  static constexpr gfx::PointF kMousePoints[] = {
      kMouseDownPoint, kMouseMovePoint, kMouseUpPoint};

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
    EXPECT_TRUE(ink_module().OnMessage(
        CreateSetAnnotationModeMessageForTesting(annotation_mode_enabled)));
    EXPECT_EQ(annotation_mode_enabled, ink_module().enabled());

    ApplyStrokeWithMouseAtPointsMaybeHandled(
        kMouseDownPoint, base::span_from_ref(kMouseMovePoint), kMouseUpPoint,
        /*expect_mouse_events_handled=*/annotation_mode_enabled);

    const int expected_count = annotation_mode_enabled ? 1 : 0;
    EXPECT_EQ(expected_count, client().stroke_finished_count());
    const std::vector<int>& updated_thumbnail_page_indices =
        client().updated_thumbnail_page_indices();
    if (annotation_mode_enabled) {
      EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0));
    } else {
      EXPECT_TRUE(updated_thumbnail_page_indices.empty());
    }
  }

  void ApplyStrokeWithTouchAtPoints(
      base::span<const gfx::PointF> touch_start_points,
      std::vector<base::span<const gfx::PointF>> all_touch_move_points,
      base::span<const gfx::PointF> touch_end_points) {
    ApplyStrokeWithTouchAtPointsMaybeHandled(
        touch_start_points, all_touch_move_points, touch_end_points,
        /*expect_touch_events_handled=*/true);
  }

  // TODO(crbug.com/377733396): Consider refactoring to combine with
  // RunStrokeCheckTest().
  void RunStrokeTouchCheckTest(bool annotation_mode_enabled) {
    EXPECT_TRUE(ink_module().OnMessage(
        CreateSetAnnotationModeMessageForTesting(annotation_mode_enabled)));
    EXPECT_EQ(annotation_mode_enabled, ink_module().enabled());

    const std::vector<base::span<const gfx::PointF>> all_touch_move_points{
        base::span_from_ref(kMouseMovePoint),
    };
    ApplyStrokeWithTouchAtPointsMaybeHandled(
        base::span_from_ref(kMouseDownPoint), all_touch_move_points,
        base::span_from_ref(kMouseUpPoint),
        /*expect_touch_events_handled=*/annotation_mode_enabled);

    const int expected_count = annotation_mode_enabled ? 1 : 0;
    EXPECT_EQ(expected_count, client().stroke_finished_count());
    const std::vector<int>& updated_thumbnail_page_indices =
        client().updated_thumbnail_page_indices();
    if (annotation_mode_enabled) {
      EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0));
    } else {
      EXPECT_TRUE(updated_thumbnail_page_indices.empty());
    }
  }

  // TODO(crbug.com/377733396): Consider refactoring to combine with
  // RunStrokeCheckTest().
  //
  // Note that currently multi-touch is not handled, so the test expectations
  // are different from the ones in RunStrokeTouchCheckTest().
  void RunStrokeMultiTouchCheckTest(bool annotation_mode_enabled) {
    EXPECT_TRUE(ink_module().OnMessage(
        CreateSetAnnotationModeMessageForTesting(annotation_mode_enabled)));
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

    EXPECT_EQ(0, client().stroke_finished_count());
    const std::vector<int>& updated_thumbnail_page_indices =
        client().updated_thumbnail_page_indices();
    EXPECT_TRUE(updated_thumbnail_page_indices.empty());
  }

  void SelectEraserToolOfSize(float size) {
    EXPECT_TRUE(ink_module().OnMessage(
        CreateSetAnnotationBrushMessageForTesting("eraser", size, nullptr)));
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

  void VerifyAndClearExpectations() {
    testing::Mock::VerifyAndClearExpectations(this);
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
};

TEST_F(PdfInkModuleStrokeTest, NoAnnotationWithMouseIfNotEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/false);
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kMouse));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
}

TEST_F(PdfInkModuleStrokeTest, AnnotationWithMouseIfEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);
  EXPECT_EQ(3, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kMouse));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
}

TEST_F(PdfInkModuleStrokeTest, NoAnnotationWithTouchIfNotEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeTouchCheckTest(/*annotation_mode_enabled=*/false);
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kMouse));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
}

TEST_F(PdfInkModuleStrokeTest, AnnotationWithTouchIfEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeTouchCheckTest(/*annotation_mode_enabled=*/true);
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kMouse));
  EXPECT_EQ(3, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
}

TEST_F(PdfInkModuleStrokeTest, NoAnnotationWithMultiTouchIfNotEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeMultiTouchCheckTest(/*annotation_mode_enabled=*/false);
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kMouse));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
}

TEST_F(PdfInkModuleStrokeTest, NoAnnotationWithMultiTouchIfEnabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeMultiTouchCheckTest(/*annotation_mode_enabled=*/true);
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kMouse));
  EXPECT_EQ(0, ink_module().GetInputOfTypeCountForPageForTesting(
                   /*page_index=*/0, ink::StrokeInput::ToolType::kTouch));
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

TEST_F(PdfInkModuleStrokeTest, BasicLayoutInvalidationsFromStroke) {
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

TEST_F(PdfInkModuleStrokeTest, TransformedLayoutInvalidationsFromStroke) {
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

  EXPECT_THAT(StrokeInputPositions(), ElementsAre(Pair(0, SizeIs(1))));

  // A stroke in the second page generates a stroke only for that page.
  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint1InsidePage1,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage1),
      kTwoPageVerticalLayoutPoint3InsidePage1);

  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(Pair(0, SizeIs(1)), Pair(1, SizeIs(1))));
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

  EXPECT_THAT(StrokeInputPositions(), ElementsAre(Pair(0, SizeIs(1))));
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

  EXPECT_THAT(
      StrokeInputPositions(),
      ElementsAre(Pair(
          0, ElementsAre(ElementsAreArray(
                             kTwoPageVerticalLayoutPageExitAndReentrySegment1),
                         ElementsAreArray({gfx::PointF(6.666667f, 0.0f),
                                           gfx::PointF(10.0f, 10.0f)})))));
}

TEST_F(PdfInkModuleStrokeTest, EraseStroke) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  // Check that there are now some visible strokes.
  EXPECT_THAT(
      VisibleStrokeInputPositions(),
      ElementsAre(Pair(0, ElementsAre(ElementsAreArray(kMousePoints)))));
  EXPECT_EQ(1, client().stroke_finished_count());
  const std::vector<int>& updated_thumbnail_page_indices =
      client().updated_thumbnail_page_indices();
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0));

  // Stroke with the eraser tool.
  SelectEraserToolOfSize(3.0f);
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint, base::span_from_ref(kMouseDownPoint), kMouseDownPoint);

  // Now there are no visible strokes left.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Erasing counts as another stroke action.
  EXPECT_EQ(2, client().stroke_finished_count());
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0, 0));

  // Stroke again. The stroke that have already been erased should stay erased.
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint, base::span_from_ref(kMouseDownPoint), kMouseDownPoint);

  // Still no visible strokes.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Nothing got erased, so the count stays at 2.
  EXPECT_EQ(2, client().stroke_finished_count());
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0, 0));
}

TEST_F(PdfInkModuleStrokeTest, EraseOnPageWithoutStrokes) {
  EnableAnnotationMode();
  InitializeSimpleSinglePageBasicLayout();

  // Verify there are no visible strokes to start with.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());

  // Stroke with the eraser tool when there are no strokes on the page.
  SelectEraserToolOfSize(3.0f);
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint, base::span_from_ref(kMouseDownPoint), kMouseDownPoint);

  // Verify there are still no visible strokes and StrokeFinished() never got
  // called.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  EXPECT_EQ(0, client().stroke_finished_count());
  EXPECT_TRUE(client().updated_thumbnail_page_indices().empty());
}

TEST_F(PdfInkModuleStrokeTest, EraseStrokeEntirelyOffPage) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  // Check that there are now some visible strokes.
  EXPECT_THAT(
      VisibleStrokeInputPositions(),
      ElementsAre(Pair(0, ElementsAre(ElementsAreArray(kMousePoints)))));
  EXPECT_EQ(1, client().stroke_finished_count());
  const std::vector<int>& updated_thumbnail_page_indices =
      client().updated_thumbnail_page_indices();
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0));

  // Stroke with the eraser tool outside of the page.
  SelectEraserToolOfSize(3.0f);
  constexpr gfx::PointF kOffPagePoint(99.0f, 99.0f);
  ApplyStrokeWithMouseAtPointsNotHandled(
      kOffPagePoint, base::span_from_ref(kOffPagePoint), kOffPagePoint);

  // Check that the visible strokes remain, and StrokeFinished() did not get
  // called again.
  EXPECT_THAT(
      VisibleStrokeInputPositions(),
      ElementsAre(Pair(0, ElementsAre(ElementsAreArray(kMousePoints)))));
  EXPECT_EQ(1, client().stroke_finished_count());
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0));
}

TEST_F(PdfInkModuleStrokeTest, EraseStrokeErasesTwoStrokes) {
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
  EXPECT_EQ(2, client().stroke_finished_count());
  const std::vector<int>& updated_thumbnail_page_indices =
      client().updated_thumbnail_page_indices();
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0, 0));

  // Stroke with the eraser tool at `kMouseMovePoint`, where it should
  // intersect with both strokes, but does not because InkStrokeModeler modeled
  // the "V" shaped input into an input with a much gentler line slope.
  SelectEraserToolOfSize(3.0f);
  ApplyStrokeWithMouseAtPoints(
      kMouseMovePoint, base::span_from_ref(kMouseMovePoint), kMouseMovePoint);

  // Check that the visible strokes are still there since the eraser tool missed
  // the strokes.
  EXPECT_THAT(VisibleStrokeInputPositions(), kVisibleStrokesMatcher);
  EXPECT_EQ(2, client().stroke_finished_count());
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0, 0));

  // Stroke with the eraser tool again at `kMousePoints`, but now with a much
  // bigger eraser size. This will actually intersect with both strokes.
  SelectEraserToolOfSize(8.0f);
  VerifyAndClearExpectations();
  ExpectNoStrokeAdded();
  ExpectUpdateStrokesActive(/*strokes_affected=*/2, /*expected_active=*/false);
  ApplyStrokeWithMouseAtPoints(
      kMouseMovePoint, base::span_from_ref(kMouseMovePoint), kMouseMovePoint);

  // Check that there are now no visible strokes.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  EXPECT_EQ(3, client().stroke_finished_count());
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0, 0, 0));
}

TEST_F(PdfInkModuleStrokeTest, EraseStrokesAcrossTwoPages) {
  EnableAnnotationMode();
  InitializeVerticalTwoPageLayout();

  // Start out without any strokes.
  EXPECT_TRUE(StrokeInputPositions().empty());
  EXPECT_EQ(0, client().stroke_finished_count());
  const std::vector<int>& updated_thumbnail_page_indices =
      client().updated_thumbnail_page_indices();
  EXPECT_TRUE(updated_thumbnail_page_indices.empty());

  ExpectStrokesAdded(/*strokes_affected=*/2);
  ExpectNoUpdateStrokeActive();

  // A stroke in the first page generates a stroke only for that page.
  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint1InsidePage0,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage0),
      kTwoPageVerticalLayoutPoint3InsidePage0);
  EXPECT_THAT(StrokeInputPositions(), ElementsAre(Pair(0, SizeIs(1))));
  EXPECT_EQ(1, client().stroke_finished_count());
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0));

  // A stroke in the second page generates a stroke only for that page.
  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint1InsidePage1,
      base::span_from_ref(kTwoPageVerticalLayoutPoint2InsidePage1),
      kTwoPageVerticalLayoutPoint3InsidePage1);
  EXPECT_THAT(StrokeInputPositions(),
              ElementsAre(Pair(0, SizeIs(1)), Pair(1, SizeIs(1))));
  EXPECT_EQ(2, client().stroke_finished_count());
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0, 1));

  // Erasing across the two pages should erase everything.
  SelectEraserToolOfSize(3.0f);
  VerifyAndClearExpectations();
  ExpectNoStrokeAdded();
  ExpectUpdateStrokesActive(/*strokes_affected=*/2, /*expected_active=*/false);
  ApplyStrokeWithMouseAtPoints(
      kTwoPageVerticalLayoutPoint1InsidePage0,
      std::vector<gfx::PointF>{kTwoPageVerticalLayoutPoint2InsidePage0,
                               kTwoPageVerticalLayoutPoint1InsidePage1},
      kTwoPageVerticalLayoutPoint3InsidePage1);
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  EXPECT_EQ(3, client().stroke_finished_count());
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0, 1, 0, 1));
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
  const std::vector<int>& updated_thumbnail_page_indices =
      client().updated_thumbnail_page_indices();
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0));

  // Select the eraser tool and call ApplyStrokeWithMouseAtPoints() again with
  // the same arguments.
  SelectEraserToolOfSize(3.0f);
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
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0, 0));
}

TEST_F(PdfInkModuleStrokeTest, EraseStrokeWithTouch) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeTouchCheckTest(/*annotation_mode_enabled=*/true);

  // Check that there are now some visible strokes.
  EXPECT_THAT(
      VisibleStrokeInputPositions(),
      ElementsAre(Pair(0, ElementsAre(ElementsAreArray(kMousePoints)))));
  EXPECT_EQ(1, client().stroke_finished_count());
  const std::vector<int>& updated_thumbnail_page_indices =
      client().updated_thumbnail_page_indices();
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0));

  // Stroke with the eraser tool.
  SelectEraserToolOfSize(3.0f);
  const std::vector<base::span<const gfx::PointF>> touch_move_points{
      base::span_from_ref(kMouseMovePoint),
  };
  ApplyStrokeWithTouchAtPoints(base::span_from_ref(kMouseDownPoint),
                               touch_move_points,
                               base::span_from_ref(kMouseDownPoint));

  // Now there are no visible strokes left.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Erasing counts as another stroke action.
  EXPECT_EQ(2, client().stroke_finished_count());
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0, 0));

  // Stroke again. The stroke that have already been erased should stay erased.
  ApplyStrokeWithTouchAtPoints(base::span_from_ref(kMouseDownPoint),
                               touch_move_points,
                               base::span_from_ref(kMouseDownPoint));

  // Still no visible strokes.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Nothing got erased, so the count stays at 2.
  EXPECT_EQ(2, client().stroke_finished_count());
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0, 0));

  // Stroke again with the mouse gets the same results.
  ApplyStrokeWithMouseAtPoints(
      kMouseDownPoint, base::span_from_ref(kMouseDownPoint), kMouseDownPoint);

  // Still no visible strokes.
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Nothing got erased, so the count stays at 2.
  EXPECT_EQ(2, client().stroke_finished_count());
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0, 0));
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
  ExpectStrokesAdded(/*strokes_affected=*/1);
  ExpectUpdateStrokesActive(/*strokes_affected=*/1, /*expect_active=*/false);
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  const auto kMatcher =
      ElementsAre(Pair(0, ElementsAre(ElementsAreArray(kMousePoints))));
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_THAT(VisibleStrokeInputPositions(), kMatcher);
  // RunStrokeCheckTest() performed the only stroke.
  EXPECT_EQ(1, client().stroke_finished_count());
  const std::vector<int>& updated_thumbnail_page_indices =
      client().updated_thumbnail_page_indices();
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0));

  PerformUndo();
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  // Undo/redo here and below do not trigger StrokeFinished().
  EXPECT_EQ(1, client().stroke_finished_count());
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0, 0));

  // Spurious undo message is a no-op.
  VerifyAndClearExpectations();
  ExpectNoStrokeAdded();
  ExpectNoUpdateStrokeActive();
  PerformUndo();
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  EXPECT_EQ(1, client().stroke_finished_count());
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0, 0));

  VerifyAndClearExpectations();
  ExpectNoStrokeAdded();
  ExpectUpdateStrokesActive(/*strokes_affected=*/1, /*expect_active=*/true);
  PerformRedo();
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_THAT(VisibleStrokeInputPositions(), kMatcher);
  EXPECT_EQ(1, client().stroke_finished_count());
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0, 0, 0));

  // Spurious redo message is a no-op.
  VerifyAndClearExpectations();
  ExpectNoStrokeAdded();
  ExpectNoUpdateStrokeActive();
  PerformRedo();
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_THAT(VisibleStrokeInputPositions(), kMatcher);
  EXPECT_EQ(1, client().stroke_finished_count());
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0, 0, 0));
}

TEST_F(PdfInkModuleUndoRedoTest, UndoRedoInvalidationsBasic) {
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

TEST_F(PdfInkModuleUndoRedoTest, UndoRedoInvalidationsScaledRotated90) {
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

TEST_F(PdfInkModuleUndoRedoTest, UndoRedoAnnotationModeDisabled) {
  InitializeSimpleSinglePageBasicLayout();
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);

  const auto kMatcher =
      ElementsAre(Pair(0, ElementsAre(ElementsAreArray(kMousePoints))));
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_THAT(VisibleStrokeInputPositions(), kMatcher);
  // RunStrokeCheckTest() performed the only stroke.
  EXPECT_EQ(1, client().stroke_finished_count());
  const std::vector<int>& updated_thumbnail_page_indices =
      client().updated_thumbnail_page_indices();
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0));

  // Disable annotation mode. Undo/redo should still work.
  EXPECT_TRUE(
      ink_module().OnMessage(CreateSetAnnotationModeMessageForTesting(false)));
  EXPECT_EQ(false, ink_module().enabled());

  PerformUndo();
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_TRUE(VisibleStrokeInputPositions().empty());
  EXPECT_EQ(1, client().stroke_finished_count());
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0, 0));

  PerformRedo();
  EXPECT_THAT(StrokeInputPositions(), kMatcher);
  EXPECT_THAT(VisibleStrokeInputPositions(), kMatcher);
  EXPECT_EQ(1, client().stroke_finished_count());
  EXPECT_THAT(updated_thumbnail_page_indices, ElementsAre(0, 0, 0));
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
      ElementsAre(kMouseDownPoint, kMouseMovePoint, kMouseUpPoint),
      ElementsAre(kMouseDownPoint1, kMouseMovePoint1, kMouseUpPoint1),
      ElementsAre(kMouseDownPoint2, kMouseMovePoint2, kMouseUpPoint2),
      ElementsAre(kMouseDownPoint3, kMouseMovePoint3, kMouseUpPoint3)};
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

// TODO(crbug.com/377704081): Enable test for CFI.
#if BUILDFLAG(CFI_ICALL_CHECK)
#define MAYBE_UndoRedoEraseLoadedV2Shapes DISABLED_UndoRedoEraseLoadedV2Shapes
#else
#define MAYBE_UndoRedoEraseLoadedV2Shapes UndoRedoEraseLoadedV2Shapes
#endif
TEST_F(PdfInkModuleUndoRedoTest, MAYBE_UndoRedoEraseLoadedV2Shapes) {
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
      ink::ModeledShape::FromMeshes(base::span_from_ref(mesh0.value()));
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
      ink::ModeledShape::FromMeshes(base::span_from_ref(mesh1.value()));
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
  EnableAnnotationMode();
  ASSERT_TRUE(ink_module().enabled());

  // Stroke with the eraser tool in the corner opposite from `kCornerPoints`,
  // which does nothing.
  SelectEraserToolOfSize(3.0f);
  ApplyStrokeWithMouseAtPoints(
      gfx::PointF(), base::span_from_ref(gfx::PointF()), gfx::PointF());
  VerifyAndClearExpectations();

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

  // Undo should reactivate `shape0`.
  ExpectNoStrokeAdded();
  ExpectNoUpdateStrokeActive();
  EXPECT_CALL(client(),
              UpdateShapeActive(kPageIndex, kShapeId0, /*active=*/true));
  EXPECT_CALL(client(), UpdateShapeActive(_, kShapeId1, _)).Times(0);
  PerformUndo();
  VerifyAndClearExpectations();

  // Redo should deactivate `shape0`.
  ExpectNoStrokeAdded();
  ExpectNoUpdateStrokeActive();
  EXPECT_CALL(client(),
              UpdateShapeActive(kPageIndex, kShapeId0, /*active=*/false));
  EXPECT_CALL(client(), UpdateShapeActive(_, kShapeId1, _)).Times(0);
  PerformRedo();
}

using PdfInkModuleGetVisibleStrokesTest = PdfInkModuleStrokeTest;

TEST_F(PdfInkModuleGetVisibleStrokesTest, NoPageStrokes) {
  std::map<int, std::vector<raw_ref<const ink::Stroke>>>
      collected_stroke_shapes =
          CollectVisibleStrokes(ink_module().GetVisibleStrokesIterator());
  ASSERT_EQ(collected_stroke_shapes.size(), 0u);
}

TEST_F(PdfInkModuleGetVisibleStrokesTest, MultiplePageStrokes) {
  EnableAnnotationMode();
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

}  // namespace chrome_pdf
