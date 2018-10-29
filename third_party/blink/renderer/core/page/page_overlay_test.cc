// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/page_overlay.h"

#include <memory>

#include "base/time/time.h"
#include "cc/paint/paint_canvas.h"
#include "cc/trees/layer_tree_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"

using testing::_;
using testing::AtLeast;
using testing::Property;

namespace blink {
namespace {

static const int kViewportWidth = 800;
static const int kViewportHeight = 600;

// These unit tests cover both PageOverlay and PageOverlayList.

// PageOverlay that paints a solid color.
class SolidColorOverlay : public PageOverlay::Delegate {
 public:
  SolidColorOverlay(Color color) : color_(color) {}

  void PaintPageOverlay(const PageOverlay& page_overlay,
                        GraphicsContext& graphics_context,
                        const IntSize& size) const override {
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            graphics_context, page_overlay, DisplayItem::kPageOverlay))
      return;
    FloatRect rect(0, 0, size.Width(), size.Height());
    DrawingRecorder recorder(graphics_context, page_overlay,
                             DisplayItem::kPageOverlay);
    graphics_context.FillRect(rect, color_);
  }

 private:
  Color color_;
};

class PageOverlayTest : public testing::Test {
 protected:
  PageOverlayTest() {
    helper_.Initialize(nullptr /* web_frame_client */,
                       nullptr /* web_view_client */,
                       nullptr /* web_widget_client */);
    GetWebView()->Resize(WebSize(kViewportWidth, kViewportHeight));
    GetWebView()->UpdateAllLifecyclePhases();
  }

  WebViewImpl* GetWebView() const { return helper_.GetWebView(); }

  std::unique_ptr<PageOverlay> CreateSolidYellowOverlay() {
    return PageOverlay::Create(
        GetWebView()->MainFrameImpl()->GetFrame(),
        std::make_unique<SolidColorOverlay>(SK_ColorYELLOW));
  }

  void SetViewportSize(const WebSize& size) {
    content::LayerTreeView* layer_tree_view = helper_.GetLayerTreeView();
    layer_tree_view->SetViewportSizeAndScale(
        static_cast<gfx::Size>(size), /*device_scale_factor=*/1.f,
        layer_tree_view->layer_tree_host()->local_surface_id_from_parent(),
        layer_tree_view->layer_tree_host()
            ->local_surface_id_allocation_time_from_parent());
  }

  template <typename OverlayType>
  void RunPageOverlayTestWithAcceleratedCompositing();

 private:
  frame_test_helpers::WebViewHelper helper_;
};

template <bool (*getter)(), void (*setter)(bool)>
class RuntimeFeatureChange {
 public:
  RuntimeFeatureChange(bool new_value) : old_value_(getter()) {
    setter(new_value);
  }
  ~RuntimeFeatureChange() { setter(old_value_); }

 private:
  bool old_value_;
};

class MockPageOverlayCanvas : public SkCanvas {
 public:
  MockPageOverlayCanvas(int width, int height) : SkCanvas(width, height) {}
  MOCK_METHOD2(onDrawRect, void(const SkRect&, const SkPaint&));
};

TEST_F(PageOverlayTest, PageOverlay_AcceleratedCompositing) {
  SetViewportSize(WebSize(kViewportWidth, kViewportHeight));

  std::unique_ptr<PageOverlay> page_overlay = CreateSolidYellowOverlay();
  page_overlay->Update();
  GetWebView()->UpdateAllLifecyclePhases();

  GraphicsLayer* graphics_layer = page_overlay->GetGraphicsLayer();
  WebRect rect(0, 0, kViewportWidth, kViewportHeight);

  IntRect int_rect = rect;
  graphics_layer->Paint(&int_rect);

  // Ideally, we would get results from the compositor that showed that this
  // page overlay actually winds up getting drawn on top of the rest.
  // For now, we just check that the GraphicsLayer will draw the right thing.
  MockPageOverlayCanvas canvas(kViewportWidth, kViewportHeight);
  EXPECT_CALL(canvas, onDrawRect(_, _)).Times(AtLeast(0));
  EXPECT_CALL(canvas,
              onDrawRect(SkRect::MakeWH(kViewportWidth, kViewportHeight),
                         Property(&SkPaint::getColor, SK_ColorYELLOW)));
  canvas.drawPicture(
      ToSkPicture(graphics_layer->CapturePaintRecord(), int_rect));
}

TEST_F(PageOverlayTest, PageOverlay_VisualRect) {
  std::unique_ptr<PageOverlay> page_overlay = CreateSolidYellowOverlay();
  page_overlay->Update();
  GetWebView()->UpdateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(0, 0, kViewportWidth, kViewportHeight),
            page_overlay->VisualRect());
}

}  // namespace
}  // namespace blink
