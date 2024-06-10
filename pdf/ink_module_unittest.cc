// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink_module.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "pdf/ink/ink_brush.h"
#include "pdf/pdf_features.h"
#include "pdf/pdf_ink_brush.h"
#include "pdf/test/mouse_event_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/rect_f.h"

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

class FakeClient : public InkModule::Client {
 public:
  FakeClient() = default;
  FakeClient(const FakeClient&) = delete;
  FakeClient& operator=(const FakeClient&) = delete;
  ~FakeClient() override = default;

  // InkModule::Client:
  void InkStrokeFinished() override { ++ink_stroke_finished_count_; }

  int VisiblePageIndexFromPoint(const gfx::PointF& point) override {
    // Assumes that all pages are visible.
    for (size_t i = 0; i < pages_layout_.size(); ++i) {
      if (pages_layout_[i].Contains(point)) {
        return i;
      }
    }

    // Point is not over a page in the viewer plane.
    return -1;
  }

  int ink_stroke_finished_count() const { return ink_stroke_finished_count_; }

  // Provide the sequence of pages and the coordinates and dimensions for how
  // they are laid out in a viewer plane.  It is upon the caller to ensure the
  // positioning makes sense (e.g., pages do not overlap).
  void set_pages_layout(const std::vector<gfx::RectF>& pages_layout) {
    pages_layout_ = pages_layout;
  }

 private:
  int ink_stroke_finished_count_ = 0;
  std::vector<gfx::RectF> pages_layout_;
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
  EXPECT_EQ(8.0f, ink_brush.GetSizeForTesting());
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
  EXPECT_EQ(4.5f, ink_brush.GetSizeForTesting());
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
  EXPECT_EQ(4.5f, ink_brush.GetSizeForTesting());
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
  EXPECT_EQ(1.0f, ink_brush.GetSizeForTesting());
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
  EXPECT_EQ(8.0f, ink_brush.GetSizeForTesting());
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
    client().set_pages_layout({kPage});
  }

  void RunStrokeCheckTest(bool annotation_mode_enabled) {
    EXPECT_TRUE(ink_module().OnMessage(
        CreateSetAnnotationModeMessage(annotation_mode_enabled)));
    EXPECT_EQ(annotation_mode_enabled, ink_module().enabled());

    // Mouse events should only be handled when annotation mode is enabled.
    blink::WebMouseEvent mouse_down_event =
        MouseEventBuilder()
            .CreateLeftClickAtPosition(kMouseDownLocation)
            .Build();
    EXPECT_EQ(annotation_mode_enabled,
              ink_module().HandleInputEvent(mouse_down_event));

    blink::WebMouseEvent mouse_move_event =
        MouseEventBuilder()
            .SetType(blink::WebInputEvent::Type::kMouseMove)
            .SetPosition(kMouseMoveLocation)
            .Build();
    EXPECT_EQ(annotation_mode_enabled,
              ink_module().HandleInputEvent(mouse_move_event));

    blink::WebMouseEvent mouse_up_event =
        MouseEventBuilder()
            .SetType(blink::WebInputEvent::Type::kMouseUp)
            .SetPosition(kMouseUpLocation)
            .SetButton(blink::WebPointerProperties::Button::kLeft)
            .SetClickCount(1)
            .Build();
    EXPECT_EQ(annotation_mode_enabled,
              ink_module().HandleInputEvent(mouse_up_event));

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

}  // namespace

}  // namespace chrome_pdf
