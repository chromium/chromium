// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CONTROLLER_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CONTROLLER_TEST_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"

namespace blink {

class GraphicsContext;

class PaintControllerTestBase : public testing::Test {
 public:
  static void DrawNothing(GraphicsContext& context,
                          const DisplayItemClient& client,
                          DisplayItem::Type type) {
    if (DrawingRecorder::UseCachedDrawingIfPossible(context, client, type))
      return;
    DrawingRecorder recorder(context, client, type);
  }

  template <typename Rect>
  static void DrawRect(GraphicsContext& context,
                       const DisplayItemClient& client,
                       DisplayItem::Type type,
                       const Rect& bounds) {
    if (DrawingRecorder::UseCachedDrawingIfPossible(context, client, type))
      return;
    DrawingRecorder recorder(context, client, type);
    context.DrawRect(RoundedIntRect(FloatRect(bounds)));
  }

 protected:
  PaintControllerTestBase()
      : root_paint_property_client_("root"),
        root_paint_chunk_id_(root_paint_property_client_,
                             DisplayItem::kUninitializedType),
        paint_controller_(std::make_unique<PaintController>()) {}

  void InitRootChunk() { InitRootChunk(GetPaintController()); }
  void InitRootChunk(PaintController& paint_controller) {
    paint_controller.UpdateCurrentPaintChunkProperties(
        root_paint_chunk_id_, DefaultPaintChunkProperties());
  }
  const PaintChunk::Id DefaultRootChunkId() const {
    return root_paint_chunk_id_;
  }

  PaintController& GetPaintController() { return *paint_controller_; }

  size_t NumCachedNewItems() const {
    return paint_controller_->num_cached_new_items_;
  }
  size_t NumCachedNewSubsequences() const {
    return paint_controller_->num_cached_new_subsequences_;
  }
#if DCHECK_IS_ON()
  size_t NumIndexedItems() const {
    return paint_controller_->num_indexed_items_;
  }
  size_t NumSequentialMatches() const {
    return paint_controller_->num_sequential_matches_;
  }
  size_t NumOutOfOrderMatches() const {
    return paint_controller_->num_out_of_order_matches_;
  }
#endif

  void InvalidateAll() { paint_controller_->InvalidateAllForTesting(); }

  void CommitAndFinishCycle() {
    paint_controller_->CommitNewDisplayItems();
    paint_controller_->FinishCycle();
  }

  using SubsequenceMarkers = PaintController::SubsequenceMarkers;
  SubsequenceMarkers* GetSubsequenceMarkers(const DisplayItemClient& client) {
    return paint_controller_->GetSubsequenceMarkers(client);
  }

  static bool ClientCacheIsValid(const PaintController& paint_controller,
                                 const DisplayItemClient& client) {
    return paint_controller.ClientCacheIsValid(client);
  }

  bool ClientCacheIsValid(const DisplayItemClient& client) const {
    return ClientCacheIsValid(*paint_controller_, client);
  }

 private:
  FakeDisplayItemClient root_paint_property_client_;
  PaintChunk::Id root_paint_chunk_id_;
  std::unique_ptr<PaintController> paint_controller_;
};

// Matcher for checking display item list. Sample usage:
// EXPECT_THAT(paint_controller.GetDisplayItemList(),
//             ElementsAre(IsSameId(client1, kBackgroundType),
//                         IsSameId(client2, kForegroundType)));
MATCHER_P(IsSameId, id, "") {
  return arg.GetId() == id;
}
MATCHER_P2(IsSameId, client, type, "") {
  return arg.GetId() == DisplayItem::Id(*client, type);
}

// Matcher for checking paint chunks. Sample usage:
// EXPACT_THAT(paint_controller.PaintChunks(),
//             ELementsAre(IsPaintChunk(0, 1, id1, properties1),
//                         IsPaintChunk(1, 3, id2, properties2)));
inline bool CheckChunk(const PaintChunk& chunk,
                       size_t begin,
                       size_t end,
                       const PaintChunk::Id& id,
                       const PropertyTreeState& properties,
                       const HitTestData* hit_test_data = nullptr) {
  return chunk.begin_index == begin && chunk.end_index == end &&
         chunk.id == id && chunk.properties == properties &&
         ((!chunk.hit_test_data && !hit_test_data) ||
          (chunk.hit_test_data && hit_test_data &&
           *chunk.hit_test_data == *hit_test_data));
}
MATCHER_P4(IsPaintChunk, begin, end, id, properties, "") {
  return CheckChunk(arg, begin, end, id, properties);
}
MATCHER_P5(IsPaintChunk, begin, end, id, properties, hit_test_data, "") {
  return CheckChunk(arg, begin, end, id, properties, &hit_test_data);
}

// Shorter names for frequently used display item types in tests.
const DisplayItem::Type kBackgroundType = DisplayItem::kBoxDecorationBackground;
const DisplayItem::Type kForegroundType =
    static_cast<DisplayItem::Type>(DisplayItem::kDrawingPaintPhaseFirst + 5);
const DisplayItem::Type kDocumentBackgroundType =
    DisplayItem::kDocumentBackground;
const DisplayItem::Type kScrollHitTestType = DisplayItem::kScrollHitTest;
const DisplayItem::Type kClipType = DisplayItem::kClipPaintPhaseFirst;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CONTROLLER_TEST_H_
