// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink_module.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "pdf/pdf_features.h"
#include "pdf/test/mouse_event_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/rect_f.h"

namespace chrome_pdf {

namespace {

class FakeClient : public InkModule::Client {
 public:
  FakeClient() = default;
  FakeClient(const FakeClient&) = delete;
  FakeClient& operator=(const FakeClient&) = delete;
  ~FakeClient() override = default;

  // InkModule::Client:
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

  // Provide the sequence of pages and the coordinates and dimensions for how
  // they are laid out in a viewer plane.  It is upon the caller to ensure the
  // positioning makes sense (e.g., pages do not overlap).
  void set_pages_layout(const std::vector<gfx::RectF>& pages_layout) {
    pages_layout_ = pages_layout;
  }

 private:
  std::vector<gfx::RectF> pages_layout_;
};

class InkModuleTest : public testing::Test {
 protected:
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
  void RunStrokeCheckTest(bool annotation_mode_enabled) {
    EXPECT_TRUE(ink_module().OnMessage(
        CreateSetAnnotationModeMessage(annotation_mode_enabled)));
    EXPECT_EQ(annotation_mode_enabled, ink_module().enabled());

    // Single page layout that matches visible area.
    constexpr gfx::RectF kPage(0.0f, 0.0f, 50.0f, 60.0f);
    client().set_pages_layout({kPage});

    constexpr gfx::PointF kMouseDownLocation(10.0f, 15.0f);
    constexpr gfx::PointF kMouseMoveLocation(20.0f, 25.0f);
    constexpr gfx::PointF kMouseUpLocation(30.0f, 17.0f);

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
  }
};

TEST_F(InkModuleStrokeTest, NoAnnotationIfNotEnabled) {
  RunStrokeCheckTest(/*annotation_mode_enabled=*/false);
}

TEST_F(InkModuleStrokeTest, AnnotationIfEnabled) {
  RunStrokeCheckTest(/*annotation_mode_enabled=*/true);
}

}  // namespace

}  // namespace chrome_pdf
