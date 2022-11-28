// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CONTROLLER_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CONTROLLER_TEST_H_

#include <utility>

#include "base/dcheck_is_on.h"
#include "base/functional/function_ref.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/hit_test_data.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"

namespace blink {

class GraphicsContext;

class PaintControllerCycleScopeForTest : public PaintControllerCycleScope {
 public:
  explicit PaintControllerCycleScopeForTest(PaintController& controller)
      : PaintControllerCycleScope(controller, /*record_debug_info*/ true) {}
};

class CommitCycleScope : public PaintControllerCycleScopeForTest {
 public:
  using PaintControllerCycleScopeForTest::PaintControllerCycleScopeForTest;
  ~CommitCycleScope() {
    for (auto* controller : controllers_)
      controller->CommitNewDisplayItems();
  }
};

class PaintControllerTestBase : public testing::Test {
 public:
  enum DrawResult {
    kCached,
    kPaintedNew,
    kRepaintedCachedItem,
  };

  static DrawResult Draw(GraphicsContext& context,
                         const DisplayItemClient& client,
                         DisplayItem::Type type,
                         base::FunctionRef<void()> draw_function);

  static DrawResult DrawNothing(GraphicsContext& context,
                                const DisplayItemClient& client,
                                DisplayItem::Type type) {
    return Draw(context, client, type, [&]() {
      DrawingRecorder recorder(context, client, type, gfx::Rect());
    });
  }

  static DrawResult DrawRect(GraphicsContext& context,
                             const DisplayItemClient& client,
                             DisplayItem::Type type,
                             const gfx::Rect& bounds) {
    return Draw(context, client, type, [&]() {
      DrawingRecorder recorder(context, client, type, bounds);
      context.DrawRect(bounds, AutoDarkMode::Disabled());
    });
  }

 protected:
  PaintControllerTestBase()
      : root_paint_property_client_(
            MakeGarbageCollected<FakeDisplayItemClient>("root")),
        root_paint_chunk_id_(root_paint_property_client_->Id(),
                             DisplayItem::kUninitializedType),
        paint_controller_(std::make_unique<PaintController>()) {}

  void SetUp() override {
    testing::FLAGS_gtest_death_test_style = "threadsafe";
  }

  void InitRootChunk() { InitRootChunk(GetPaintController()); }
  void InitRootChunk(PaintController& paint_controller) {
    paint_controller.UpdateCurrentPaintChunkProperties(
        root_paint_chunk_id_, *root_paint_property_client_,
        DefaultPaintChunkProperties());
    paint_controller.RecordDebugInfo(*root_paint_property_client_);
  }
  const PaintChunk::Id DefaultRootChunkId() const {
    return root_paint_chunk_id_;
  }

  PaintController& GetPaintController() { return *paint_controller_; }

  wtf_size_t NumCachedNewItems() const {
    return paint_controller_->num_cached_new_items_;
  }
  wtf_size_t NumCachedNewSubsequences() const {
    return paint_controller_->num_cached_new_subsequences_;
  }
#if DCHECK_IS_ON()
  wtf_size_t NumIndexedItems() const {
    return paint_controller_->num_indexed_items_;
  }
  wtf_size_t NumSequentialMatches() const {
    return paint_controller_->num_sequential_matches_;
  }
  wtf_size_t NumOutOfOrderMatches() const {
    return paint_controller_->num_out_of_order_matches_;
  }
#endif

  void InvalidateAll() { paint_controller_->InvalidateAllForTesting(); }

  using SubsequenceMarkers = PaintController::SubsequenceMarkers;
  const SubsequenceMarkers* GetSubsequenceMarkers(
      const DisplayItemClient& client) {
    return paint_controller_->GetSubsequenceMarkers(client.Id());
  }

  static bool ClientCacheIsValid(const PaintController& paint_controller,
                                 const DisplayItemClient& client) {
    return paint_controller.ClientCacheIsValid(client);
  }

  bool ClientCacheIsValid(const DisplayItemClient& client) const {
    return ClientCacheIsValid(*paint_controller_, client);
  }

 private:
  Persistent<FakeDisplayItemClient> root_paint_property_client_;
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
MATCHER_P2(IsSameId, client_id, type, "") {
  return arg.GetId() == DisplayItem::Id(client_id, type);
}
MATCHER_P3(IsSameId, client_id, type, fragment, "") {
  return arg.GetId() == DisplayItem::Id(client_id, type, fragment);
}

// Matcher for checking paint chunks. Sample usage:
// EXPACT_THAT(paint_controller.PaintChunks(),
//             ELementsAre(IsPaintChunk(0, 1, id1, properties1),
//                         IsPaintChunk(1, 3, id2, properties2)));
inline bool CheckChunk(const PaintChunk& chunk,
                       wtf_size_t begin,
                       wtf_size_t end) {
  return chunk.begin_index == begin && chunk.end_index == end;
}
inline bool CheckChunk(const PaintChunk& chunk,
                       wtf_size_t begin,
                       wtf_size_t end,
                       const PaintChunk::Id& id,
                       const PropertyTreeStateOrAlias& properties,
                       const HitTestData* hit_test_data = nullptr,
                       const gfx::Rect* bounds = nullptr) {
  return chunk.begin_index == begin && chunk.end_index == end &&
         chunk.id == id && chunk.properties == properties &&
         ((!chunk.hit_test_data && !hit_test_data) ||
          (chunk.hit_test_data && hit_test_data &&
           *chunk.hit_test_data == *hit_test_data)) &&
         (!bounds || chunk.bounds == *bounds);
}
MATCHER_P2(IsPaintChunk, begin, end, "") {
  return CheckChunk(arg, begin, end);
}
MATCHER_P4(IsPaintChunk, begin, end, id, properties, "") {
  return CheckChunk(arg, begin, end, id, properties);
}
MATCHER_P5(IsPaintChunk, begin, end, id, properties, hit_test_data, "") {
  return CheckChunk(arg, begin, end, id, properties, hit_test_data);
}
MATCHER_P6(IsPaintChunk,
           begin,
           end,
           id,
           properties,
           hit_test_data,
           bounds,
           "") {
  return CheckChunk(arg, begin, end, id, properties, hit_test_data, &bounds);
}

// Shorter names for frequently used display item types in tests.
const DisplayItem::Type kBackgroundType = DisplayItem::kBoxDecorationBackground;
const DisplayItem::Type kForegroundType =
    static_cast<DisplayItem::Type>(DisplayItem::kDrawingPaintPhaseFirst + 5);
const DisplayItem::Type kClipType = DisplayItem::kClipPaintPhaseFirst;

#define EXPECT_SUBSEQUENCE(client, expected_start_chunk_index,     \
                           expected_end_chunk_index)               \
  do {                                                             \
    auto* subsequence = GetSubsequenceMarkers(client);             \
    ASSERT_NE(nullptr, subsequence);                               \
    EXPECT_EQ(static_cast<wtf_size_t>(expected_start_chunk_index), \
              subsequence->start_chunk_index);                     \
    EXPECT_EQ(static_cast<wtf_size_t>(expected_end_chunk_index),   \
              subsequence->end_chunk_index);                       \
  } while (false)

#define EXPECT_SUBSEQUENCE_FROM_CHUNK(client, start_chunk_iterator, \
                                      chunk_count)                  \
  EXPECT_SUBSEQUENCE(                                               \
      client, (start_chunk_iterator).IndexInPaintArtifact(),        \
      (start_chunk_iterator).IndexInPaintArtifact() + chunk_count)

#define EXPECT_NO_SUBSEQUENCE(client) \
  EXPECT_EQ(nullptr, GetSubsequenceMarkers(client))

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CONTROLLER_TEST_H_
