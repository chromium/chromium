// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_CONTROLLER_PAINT_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_CONTROLLER_PAINT_TEST_H_

#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller_test.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

namespace blink {

class PaintControllerPaintTestBase : public RenderingTest {
 public:
  PaintControllerPaintTestBase(LocalFrameClient* local_frame_client = nullptr)
      : RenderingTest(local_frame_client) {}

 protected:
  LayoutView& GetLayoutView() const { return *GetDocument().GetLayoutView(); }
  PaintController& RootPaintController() const {
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
      return *GetDocument().View()->GetPaintController();
    return GetLayoutView()
        .Layer()
        ->GraphicsLayerBacking()
        ->GetPaintController();
  }

  void SetUp() override {
    RenderingTest::SetUp();
    EnableCompositing();
  }

  bool PaintWithoutCommit(const IntRect* interest_rect = nullptr) {
    GetDocument().View()->Lifecycle().AdvanceTo(DocumentLifecycle::kInPaint);
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      if (GetLayoutView().Layer()->NeedsRepaint()) {
        GraphicsContext graphics_context(RootPaintController());
        GetDocument().View()->Paint(
            graphics_context, kGlobalPaintNormalPhase,
            interest_rect ? CullRect(*interest_rect)
                          : CullRect(LayoutRect::InfiniteIntRect()));
        return true;
      }
      GetDocument().View()->Lifecycle().AdvanceTo(
          DocumentLifecycle::kPaintClean);
      return false;
    }
    // Only root graphics layer is supported.
    if (!GetLayoutView().Layer()->GraphicsLayerBacking()->PaintWithoutCommit(
            interest_rect)) {
      GetDocument().View()->Lifecycle().AdvanceTo(
          DocumentLifecycle::kPaintClean);
      return false;
    }
    return true;
  }

  const DisplayItemClient& ViewScrollingBackgroundClient() {
    // TODO(wangxianzhu): SPv2 should use the same display item client.
    if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      return GetLayoutView()
          .GetScrollableArea()
          ->GetScrollingBackgroundDisplayItemClient();
    }
    return GetLayoutView();
  }

  void CommitAndFinishCycle() {
    // Only root graphics layer is supported.
    RootPaintController().CommitNewDisplayItems();
    RootPaintController().FinishCycle();
    GetDocument().View()->Lifecycle().AdvanceTo(DocumentLifecycle::kPaintClean);
  }

  void Paint(const IntRect* interest_rect = nullptr) {
    // Only root graphics layer is supported.
    if (PaintWithoutCommit(interest_rect))
      CommitAndFinishCycle();
  }

  bool DisplayItemListContains(const DisplayItemList& display_item_list,
                               DisplayItemClient& client,
                               DisplayItem::Type type) const {
    for (auto& item : display_item_list) {
      if (item.Client() == client && item.GetType() == type)
        return true;
    }
    return false;
  }

  int NumCachedNewItems() const {
    return RootPaintController().num_cached_new_items_;
  }

  const DisplayItemClient& CaretDisplayItemClientForTesting() const {
    return GetDocument()
        .GetFrame()
        ->Selection()
        .CaretDisplayItemClientForTesting();
  }

  void InvalidateAll(PaintController& paint_controller) {
    paint_controller.InvalidateAllForTesting();
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      DCHECK_EQ(&paint_controller, GetDocument().View()->GetPaintController());
      GetLayoutView().Layer()->SetNeedsRepaint();
    }
  }

  bool ClientCacheIsValid(const DisplayItemClient& client) {
    return RootPaintController().ClientCacheIsValid(client);
  }
};

class PaintControllerPaintTest : public PaintTestConfigurations,
                                 public PaintControllerPaintTestBase {
 public:
  PaintControllerPaintTest(LocalFrameClient* local_frame_client = nullptr)
      : PaintControllerPaintTestBase(local_frame_client) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_CONTROLLER_PAINT_TEST_H_
