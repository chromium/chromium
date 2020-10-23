// Copyright 2015 The Chromium Authors. All rights reserved.
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
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
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
                         const IntSize& size) const override {
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            graphics_context, frame_overlay, DisplayItem::kFrameOverlay))
      return;
    FloatRect rect(0, 0, size.Width(), size.Height());
    DrawingRecorder recorder(graphics_context, frame_overlay,
                             DisplayItem::kFrameOverlay,
                             IntRect(IntPoint(), size));
    graphics_context.FillRect(rect, color_);
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

  std::unique_ptr<FrameOverlay> CreateSolidYellowOverlay() {
    return std::make_unique<FrameOverlay>(
        GetWebView()->MainFrameImpl()->GetFrame(),
        std::make_unique<SolidColorOverlay>(SK_ColorYELLOW));
  }

  template <typename OverlayType>
  void RunFrameOverlayTestWithAcceleratedCompositing();

 private:
  frame_test_helpers::WebViewHelper helper_;
};

class MockFrameOverlayCanvas : public SkCanvas {
 public:
  MOCK_METHOD2(onDrawRect, void(const SkRect&, const SkPaint&));
};

INSTANTIATE_PAINT_TEST_SUITE_P(FrameOverlayTest);

TEST_P(FrameOverlayTest, AcceleratedCompositing) {
  std::unique_ptr<FrameOverlay> frame_overlay = CreateSolidYellowOverlay();
  frame_overlay->UpdatePrePaint();
  EXPECT_EQ(PropertyTreeState::Root(),
            frame_overlay->DefaultPropertyTreeState());

  // Ideally, we would get results from the compositor that showed that this
  // page overlay actually winds up getting drawn on top of the rest.
  // For now, we just check that the GraphicsLayer will draw the right thing.
  MockFrameOverlayCanvas canvas;
  EXPECT_CALL(canvas,
              onDrawRect(SkRect::MakeWH(kViewportWidth, kViewportHeight),
                         Property(&SkPaint::getColor, SK_ColorYELLOW)));

  PaintRecordBuilder builder;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    frame_overlay->Paint(builder.Context());
  } else {
    auto* graphics_layer = frame_overlay->GetGraphicsLayer();
    EXPECT_FALSE(graphics_layer->IsHitTestable());
    EXPECT_EQ(PropertyTreeState::Root(),
              graphics_layer->GetPropertyTreeState());
    Vector<PreCompositedLayerInfo> pre_composited_layers;
    graphics_layer->PaintRecursively(builder.Context(), pre_composited_layers);
    ASSERT_EQ(1u, pre_composited_layers.size());
  }
  builder.EndRecording()->Playback(&canvas);
}

TEST_P(FrameOverlayTest, DeviceEmulationScale) {
  DeviceEmulationParams params;
  params.scale = 1.5;
  params.view_size = gfx::Size(800, 600);
  GetWebView()->EnableDeviceEmulation(params);
  GetWebView()->MainFrameViewWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  auto* frame_view = GetWebView()->MainFrameImpl()->GetFrameView();
  std::unique_ptr<FrameOverlay> frame_overlay = CreateSolidYellowOverlay();
  frame_overlay->UpdatePrePaint();
  auto* transform = frame_view->GetPage()
                        ->GetVisualViewport()
                        .GetDeviceEmulationTransformNode();
  EXPECT_EQ(TransformationMatrix().Scale(1.5), transform->Matrix());
  const auto& state = frame_overlay->DefaultPropertyTreeState();
  EXPECT_EQ(transform, &state.Transform());
  EXPECT_EQ(&ClipPaintPropertyNode::Root(), &state.Clip());
  EXPECT_EQ(&EffectPaintPropertyNode::Root(), &state.Effect());

  PaintController paint_controller(PaintController::kTransient);
  GraphicsContext context(paint_controller);
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    frame_overlay->Paint(context);
  } else {
    auto* graphics_layer = frame_overlay->GetGraphicsLayer();
    EXPECT_FALSE(graphics_layer->IsHitTestable());
    EXPECT_EQ(state, graphics_layer->GetPropertyTreeState());
    Vector<PreCompositedLayerInfo> pre_composited_layers;
    EXPECT_TRUE(
        graphics_layer->PaintRecursively(context, pre_composited_layers));
    ASSERT_EQ(1u, pre_composited_layers.size());
    EXPECT_EQ(graphics_layer, pre_composited_layers[0].graphics_layer);
  }
  paint_controller.CommitNewDisplayItems();

  EXPECT_THAT(
      paint_controller.GetDisplayItemList(),
      ElementsAre(IsSameId(frame_overlay.get(), DisplayItem::kFrameOverlay)));
  EXPECT_EQ(IntRect(0, 0, 800, 600),
            paint_controller.GetDisplayItemList()[0].VisualRect());
  EXPECT_THAT(
      paint_controller.PaintChunks(),
      ElementsAre(IsPaintChunk(
          0, 1, PaintChunk::Id(*frame_overlay, DisplayItem::kFrameOverlay),
          state, nullptr, IntRect(0, 0, 800, 600))));
}

TEST_P(FrameOverlayTest, LayerOrder) {
  // This test doesn't apply in CompositeAfterPaint.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  auto frame_overlay1 = CreateSolidYellowOverlay();
  auto frame_overlay2 = CreateSolidYellowOverlay();
  frame_overlay1->UpdatePrePaint();
  frame_overlay2->UpdatePrePaint();

  auto* parent_layer = GetWebView()
                           ->MainFrameImpl()
                           ->GetFrameView()
                           ->GetLayoutView()
                           ->Compositor()
                           ->PaintRootGraphicsLayer();
  ASSERT_EQ(3u, parent_layer->Children().size());
  EXPECT_EQ(parent_layer, frame_overlay1->GetGraphicsLayer()->Parent());
  EXPECT_EQ(parent_layer->Children()[1], frame_overlay1->GetGraphicsLayer());
  EXPECT_EQ(parent_layer, frame_overlay2->GetGraphicsLayer()->Parent());
  EXPECT_EQ(parent_layer->Children()[2], frame_overlay2->GetGraphicsLayer());

  auto extra_layer = std::make_unique<GraphicsLayer>(parent_layer->Client());
  parent_layer->AddChild(extra_layer.get());

  frame_overlay1->UpdatePrePaint();
  frame_overlay2->UpdatePrePaint();
  ASSERT_EQ(4u, parent_layer->Children().size());
  EXPECT_EQ(parent_layer, frame_overlay1->GetGraphicsLayer()->Parent());
  EXPECT_EQ(parent_layer->Children()[2], frame_overlay1->GetGraphicsLayer());
  EXPECT_EQ(parent_layer, frame_overlay2->GetGraphicsLayer()->Parent());
  EXPECT_EQ(parent_layer->Children()[3], frame_overlay2->GetGraphicsLayer());
}

}  // namespace
}  // namespace blink
