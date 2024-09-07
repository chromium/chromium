// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/frame_overlay.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/widget/device_emulation_params.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/skia/include/core/SkCanvas.h"

using testing::ElementsAre;
using testing::Property;

namespace blink {
namespace {

// FrameOverlay that paints a solid color.
class SolidColorOverlay : public FrameOverlay::Delegate {
 public:
  SolidColorOverlay(Color color) : color_(color) {}

  void PaintFrameOverlay(const FrameOverlay& frame_overlay,
                         GraphicsContext& graphics_context,
                         const gfx::Size& size) const override {
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            graphics_context, frame_overlay, DisplayItem::kFrameOverlay))
      return;
    gfx::RectF rect(0, 0, size.width(), size.height());
    DrawingRecorder recorder(graphics_context, frame_overlay,
                             DisplayItem::kFrameOverlay, gfx::Rect(size));
    graphics_context.FillRect(rect, color_, AutoDarkMode::Disabled());
  }

 private:
  Color color_;
};

class FrameOverlayTest : public testing::Test, public PaintTestConfigurations {
 protected:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;

  FrameOverlayTest() {
    helper_.Initialize(nullptr, nullptr, nullptr);
    GetWebView()->MainFrameViewWidget()->Resize(
        gfx::Size(kViewportWidth, kViewportHeight));
    GetWebView()->MainFrameViewWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  WebViewImpl* GetWebView() const { return helper_.GetWebView(); }

  FrameOverlay* CreateSolidYellowOverlay() {
    return MakeGarbageCollected<FrameOverlay>(
        GetWebView()->MainFrameImpl()->GetFrame(),
        std::make_unique<SolidColorOverlay>(
            Color::FromSkColor(SK_ColorYELLOW)));
  }

  template <typename OverlayType>
  void RunFrameOverlayTestWithAcceleratedCompositing();

 private:
  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper helper_;
};

class MockFrameOverlayCanvas : public SkCanvas {
 public:
  MOCK_METHOD2(onDrawRect, void(const SkRect&, const SkPaint&));
};

INSTANTIATE_PAINT_TEST_SUITE_P(FrameOverlayTest);

TEST_P(FrameOverlayTest, AcceleratedCompositing) {
  FrameOverlay* frame_overlay = CreateSolidYellowOverlay();
  frame_overlay->UpdatePrePaint();
  EXPECT_EQ(PropertyTreeState::Root(),
            frame_overlay->DefaultPropertyTreeState());

  // Ideally, we would get results from the compositor that showed that this
  // page overlay actually winds up getting drawn on top of the rest.
  // For now, we just check that we drew the right thing.
  MockFrameOverlayCanvas canvas;
  EXPECT_CALL(canvas,
              onDrawRect(SkRect::MakeWH(kViewportWidth, kViewportHeight),
                         Property(&SkPaint::getColor, SK_ColorYELLOW)));

  PaintRecordBuilder builder;
  frame_overlay->Paint(builder.Context());
  builder.EndRecording().Playback(&canvas);
  frame_overlay->Destroy();
}

TEST_P(FrameOverlayTest, DeviceEmulationScale) {
  DeviceEmulationParams params;
  params.scale = 1.5;
  params.view_size = gfx::Size(800, 600);
  GetWebView()->EnableDeviceEmulation(params);
  GetWebView()->MainFrameViewWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  FrameOverlay* frame_overlay = CreateSolidYellowOverlay();
  frame_overlay->UpdatePrePaint();
  auto* transform = GetWebView()
                        ->MainFrameImpl()
                        ->GetFrame()
                        ->GetPage()
                        ->GetVisualViewport()
                        .GetDeviceEmulationTransformNode();
  EXPECT_EQ(gfx::Transform::MakeScale(1.5), transform->Matrix());
  const auto& state = frame_overlay->DefaultPropertyTreeState();
  EXPECT_EQ(transform, &state.Transform());
  EXPECT_EQ(&ClipPaintPropertyNode::Root(), &state.Clip());
  EXPECT_EQ(&EffectPaintPropertyNode::Root(), &state.Effect());

  PaintController paint_controller;
  GraphicsContext context(paint_controller);
  frame_overlay->Paint(context);
  auto& paint_artifact = paint_controller.CommitNewDisplayItems();
  EXPECT_THAT(
      paint_artifact.GetDisplayItemList(),
      ElementsAre(IsSameId(frame_overlay->Id(), DisplayItem::kFrameOverlay)));
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600),
            paint_artifact.GetDisplayItemList()[0].VisualRect());
  EXPECT_THAT(
      paint_artifact.GetPaintChunks(),
      ElementsAre(IsPaintChunk(
          0, 1, PaintChunk::Id(frame_overlay->Id(), DisplayItem::kFrameOverlay),
          state, nullptr, gfx::Rect(0, 0, 800, 600))));
  frame_overlay->Destroy();
}

}  // namespace
}  // namespace blink
