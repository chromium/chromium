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
      return GetDocument().View()->GetPaintControllerForTesting();
    return GetLayoutView()
        .Layer()
        ->GraphicsLayerBacking()
        ->GetPaintController();
  }

  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

  const DisplayItemClient& ViewScrollingBackgroundClient() {
    return GetLayoutView()
        .GetScrollableArea()
        ->GetScrollingBackgroundDisplayItemClient();
  }

  void UpdateAllLifecyclePhasesExceptPaint() {
    GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
        DocumentUpdateReason::kTest);
  }

  void PaintContents(const IntRect& interest_rect) {
    GetDocument().View()->PaintContentsForTest(CullRect(interest_rect));
  }

  void InvalidateAll() {
    RootPaintController().InvalidateAllForTesting();
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
      GetLayoutView().Layer()->SetNeedsRepaint();
  }

  bool ClientCacheIsValid(const DisplayItemClient& client) {
    return RootPaintController().ClientCacheIsValid(client);
  }

  using SubsequenceMarkers = PaintController::SubsequenceMarkers;
  SubsequenceMarkers* GetSubsequenceMarkers(const DisplayItemClient& client) {
    return RootPaintController().GetSubsequenceMarkers(client);
  }

  static bool IsNotContentType(DisplayItem::Type type) {
    return type == DisplayItem::kFrameOverlay ||
           type == DisplayItem::kForeignLayerLinkHighlight ||
           type == DisplayItem::kForeignLayerViewportScroll ||
           type == DisplayItem::kForeignLayerViewportScrollbar;
  }

  // Excludes display items for LayoutView non-scrolling background, visual
  // viewport, overlays, etc. Includes LayoutView scrolling background.
  DisplayItemRange ContentDisplayItems() {
    const auto& display_item_list = RootPaintController().GetDisplayItemList();
    wtf_size_t begin_index = 0;
    wtf_size_t end_index = display_item_list.size();
    while (begin_index < end_index &&
           &display_item_list[begin_index].Client() == &GetLayoutView())
      begin_index++;
    while (end_index > begin_index &&
           IsNotContentType(display_item_list[end_index - 1].GetType()))
      end_index--;
    return display_item_list.ItemsInRange(begin_index, end_index);
  }

  // Excludes paint chunks for LayoutView non-scrolling background and scroll
  // hit test, visual viewport, overlays, etc. Includes LayoutView scrolling
  // background.
  PaintChunkSubset ContentPaintChunks() {
    const auto& chunks = RootPaintController().PaintChunks();
    wtf_size_t begin_index = 0;
    wtf_size_t end_index = chunks.size();
    while (begin_index < end_index) {
      const auto& client = chunks[begin_index].id.client;
      if (&client != &GetLayoutView() && &client != GetLayoutView().Layer())
        break;
      begin_index++;
    }
    while (end_index > begin_index &&
           IsNotContentType(chunks[end_index - 1].id.type))
      end_index--;
    return PaintChunkSubset(RootPaintController().GetPaintArtifactShared(),
                            begin_index, end_index);
  }

  class CachedItemAndSubsequenceCounter {
   public:
    CachedItemAndSubsequenceCounter()
        : reset_uma_reporting_(&PaintController::disable_uma_reporting_, true) {
      Reset();
    }
    void Reset() {
      old_num_cached_items_ = PaintController::sum_num_cached_items_;
      old_num_cached_subsequences_ =
          PaintController::sum_num_cached_subsequences_;
    }
    size_t NumNewCachedItems() const {
      return PaintController::sum_num_cached_items_ - old_num_cached_items_;
    }
    size_t NumNewCachedSubsequences() const {
      return PaintController::sum_num_cached_subsequences_ -
             old_num_cached_subsequences_;
    }

   private:
    base::AutoReset<bool> reset_uma_reporting_;
    size_t old_num_cached_items_;
    size_t old_num_cached_subsequences_;
  };
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

#define VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM \
  IsSameId(&ViewScrollingBackgroundClient(), DisplayItem::kDocumentBackground)

// Checks for view scrolling background chunk in common case that there is only
// one display item in the chunk and no hit test rects.
#define VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON                   \
  IsPaintChunk(0, 1,                                             \
               PaintChunk::Id(ViewScrollingBackgroundClient(),   \
                              DisplayItem::kDocumentBackground), \
               GetLayoutView().FirstFragment().ContentsProperties())

// This version also checks the following additional parameters:
//   wtf_size_t display_item_count,
//   const HitTestData* hit_test_data,
//   (optional) const IntRect& bounds
#define VIEW_SCROLLING_BACKGROUND_CHUNK(display_item_count, ...)     \
  IsPaintChunk(0, display_item_count,                                \
               PaintChunk::Id(ViewScrollingBackgroundClient(),       \
                              DisplayItem::kDocumentBackground),     \
               GetLayoutView().FirstFragment().ContentsProperties(), \
               __VA_ARGS__)

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_CONTROLLER_PAINT_TEST_H_
