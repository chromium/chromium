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
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
      return *GetDocument().View()->GetPaintController();
    return GetLayoutView()
        .Layer()
        ->GraphicsLayerBacking()
        ->GetPaintController();
  }

  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

  bool PaintWithoutCommit(
      const base::Optional<IntRect>& interest_rect = base::nullopt) {
    GetDocument().View()->Lifecycle().AdvanceTo(DocumentLifecycle::kInPaint);
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      if (GetLayoutView().Layer()->SelfOrDescendantNeedsRepaint()) {
        GraphicsContext graphics_context(RootPaintController());
        GetDocument().View()->Paint(
            graphics_context, kGlobalPaintNormalPhase,
            interest_rect ? CullRect(*interest_rect) : CullRect::Infinite());
        return true;
      }
      GetDocument().View()->Lifecycle().AdvanceTo(
          DocumentLifecycle::kPaintClean);
      return false;
    }
    // Only root graphics layer is supported.
    if (!GetLayoutView()
             .Layer()
             ->GraphicsLayerBacking()
             ->PaintWithoutCommitForTesting(interest_rect)) {
      GetDocument().View()->Lifecycle().AdvanceTo(
          DocumentLifecycle::kPaintClean);
      return false;
    }
    return true;
  }

  const DisplayItemClient& ViewScrollingBackgroundClient() {
    return GetLayoutView()
        .GetScrollableArea()
        ->GetScrollingBackgroundDisplayItemClient();
  }

  void CommitAndFinishCycle() {
    // Only root graphics layer is supported.
    RootPaintController().CommitNewDisplayItems();
    RootPaintController().FinishCycle();
    GetDocument().View()->Lifecycle().AdvanceTo(DocumentLifecycle::kPaintClean);
  }

  void Paint(const base::Optional<IntRect>& interest_rect = base::nullopt) {
    // Only root graphics layer is supported.
    if (PaintWithoutCommit(interest_rect))
      CommitAndFinishCycle();
  }

  int NumCachedNewItems() const {
    return RootPaintController().num_cached_new_items_;
  }

  void InvalidateAll(PaintController& paint_controller) {
    paint_controller.InvalidateAllForTesting();
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      DCHECK_EQ(&paint_controller, GetDocument().View()->GetPaintController());
      GetLayoutView().Layer()->SetNeedsRepaint();
    }
  }

  bool ClientCacheIsValid(const DisplayItemClient& client) {
    return RootPaintController().ClientCacheIsValid(client);
  }

  using SubsequenceMarkers = PaintController::SubsequenceMarkers;
  SubsequenceMarkers* GetSubsequenceMarkers(const DisplayItemClient& client) {
    return RootPaintController().GetSubsequenceMarkers(client);
  }
};

class PaintControllerPaintTest : public PaintTestConfigurations,
                                 public PaintControllerPaintTestBase {
 public:
  PaintControllerPaintTest(LocalFrameClient* local_frame_client = nullptr)
      : PaintControllerPaintTestBase(local_frame_client) {}
};

// Shorter names for frequently used display item types in core/ tests.
const DisplayItem::Type kNonScrollingBackgroundChunkType =
    DisplayItem::PaintPhaseToDrawingType(PaintPhase::kSelfBlockBackgroundOnly);
const DisplayItem::Type kScrollingBackgroundChunkType =
    DisplayItem::PaintPhaseToClipType(PaintPhase::kSelfBlockBackgroundOnly);
const DisplayItem::Type kClippedContentsBackgroundChunkType =
    DisplayItem::PaintPhaseToClipType(
        PaintPhase::kDescendantBlockBackgroundsOnly);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_CONTROLLER_PAINT_TEST_H_
