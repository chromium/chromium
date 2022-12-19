// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_controller_test.h"

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_display_item_fragment.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/subsequence_recorder.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

using testing::ElementsAre;

namespace blink {

PaintControllerTestBase::DrawResult PaintControllerTestBase::Draw(
    GraphicsContext& context,
    const DisplayItemClient& client,
    DisplayItem::Type type,
    base::FunctionRef<void()> draw_function) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, client, type)) {
    return kCached;
  }

  auto& paint_controller = context.GetPaintController();
  auto* matching_cached_item =
      paint_controller.MatchingCachedItemToBeRepainted();
  PaintRecord old_record;
  if (matching_cached_item) {
    EXPECT_EQ(
        matching_cached_item->GetId(),
        DisplayItem::Id(client.Id(), type, paint_controller.CurrentFragment()));
    old_record = To<DrawingDisplayItem>(matching_cached_item)->GetPaintRecord();
  }

  bool would_be_cached =
      context.GetPaintController().IsCheckingUnderInvalidationForTesting();

  draw_function();

  if (would_be_cached) {
    DCHECK(!matching_cached_item);
    return kCached;
  }

  if (matching_cached_item) {
    // We should reused the cached paint record and paint into it.
    PaintRecord new_record =
        To<DrawingDisplayItem>(paint_controller.GetNewPaintArtifactShared()
                                   ->GetDisplayItemList()
                                   .back())
            .GetPaintRecord();
    EXPECT_NE(&old_record.GetFirstOp(), &new_record.GetFirstOp());
    EXPECT_EQ(old_record.bytes_used(), new_record.bytes_used());
    return kRepaintedCachedItem;
  }
  return kPaintedNew;
}

// Tests using this class will be tested with under-invalidation-checking
// enabled and disabled.
class PaintControllerTest : public PaintTestConfigurations,
                            public PaintControllerTestBase {
};

#define EXPECT_DEFAULT_ROOT_CHUNK(size)                               \
  EXPECT_THAT(GetPaintController().PaintChunks(),                     \
              ElementsAre(IsPaintChunk(0, size, DefaultRootChunkId(), \
                                       DefaultPaintChunkProperties())))

INSTANTIATE_TEST_SUITE_P(All,
                         PaintControllerTest,
                         testing::Values(0, kUnderInvalidationChecking));
TEST_P(PaintControllerTest, NestedRecorders) {
  GraphicsContext context(GetPaintController());
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>("client");
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    EXPECT_EQ(kPaintedNew, DrawRect(context, client, kBackgroundType,
                                    gfx::Rect(100, 100, 200, 200)));
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(client.Id(), kBackgroundType)));
  EXPECT_DEFAULT_ROOT_CHUNK(1);
}

TEST_P(PaintControllerTest, UpdateBasic) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  GraphicsContext context(GetPaintController());
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    EXPECT_EQ(kPaintedNew, DrawRect(context, first, kBackgroundType,
                                    gfx::Rect(100, 100, 300, 300)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, second, kBackgroundType,
                                    gfx::Rect(100, 100, 200, 200)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, first, kForegroundType,
                                    gfx::Rect(100, 100, 300, 300)));

    EXPECT_EQ(0u, NumCachedNewItems());
    EXPECT_EQ(0u, NumCachedNewSubsequences());
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(first.Id(), kBackgroundType),
                          IsSameId(second.Id(), kBackgroundType),
                          IsSameId(first.Id(), kForegroundType)));
  EXPECT_DEFAULT_ROOT_CHUNK(3);

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    EXPECT_EQ(kCached, DrawRect(context, first, kBackgroundType,
                                gfx::Rect(100, 100, 300, 300)));
    EXPECT_EQ(kCached, DrawRect(context, first, kForegroundType,
                                gfx::Rect(100, 100, 300, 300)));

    EXPECT_EQ(2u, NumCachedNewItems());
    EXPECT_EQ(0u, NumCachedNewSubsequences());
#if DCHECK_IS_ON()
    EXPECT_EQ(1u, NumIndexedItems());
    EXPECT_EQ(2u, NumSequentialMatches());
    EXPECT_EQ(0u, NumOutOfOrderMatches());
#endif
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(first.Id(), kBackgroundType),
                          IsSameId(first.Id(), kForegroundType)));
  EXPECT_DEFAULT_ROOT_CHUNK(2);
}

TEST_P(PaintControllerTest, UpdateSwapOrder) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  FakeDisplayItemClient& unaffected =
      *MakeGarbageCollected<FakeDisplayItemClient>("unaffected");
  GraphicsContext context(GetPaintController());
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    EXPECT_EQ(kPaintedNew, DrawRect(context, first, kBackgroundType,
                                    gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, first, kForegroundType,
                                    gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, second, kBackgroundType,
                                    gfx::Rect(100, 100, 50, 200)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, second, kForegroundType,
                                    gfx::Rect(100, 100, 50, 200)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, unaffected, kBackgroundType,
                                    gfx::Rect(300, 300, 10, 10)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, unaffected, kForegroundType,
                                    gfx::Rect(300, 300, 10, 10)));
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(first.Id(), kBackgroundType),
                          IsSameId(first.Id(), kForegroundType),
                          IsSameId(second.Id(), kBackgroundType),
                          IsSameId(second.Id(), kForegroundType),
                          IsSameId(unaffected.Id(), kBackgroundType),
                          IsSameId(unaffected.Id(), kForegroundType)));

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    EXPECT_EQ(kCached, DrawRect(context, second, kBackgroundType,
                                gfx::Rect(100, 100, 50, 200)));
    EXPECT_EQ(kCached, DrawRect(context, second, kForegroundType,
                                gfx::Rect(100, 100, 50, 200)));
    EXPECT_EQ(kCached, DrawRect(context, first, kBackgroundType,
                                gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kCached, DrawRect(context, first, kForegroundType,
                                gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kCached, DrawRect(context, unaffected, kBackgroundType,
                                gfx::Rect(300, 300, 10, 10)));
    EXPECT_EQ(kCached, DrawRect(context, unaffected, kForegroundType,
                                gfx::Rect(300, 300, 10, 10)));

    EXPECT_EQ(6u, NumCachedNewItems());
    EXPECT_EQ(0u, NumCachedNewSubsequences());
#if DCHECK_IS_ON()
    EXPECT_EQ(2u, NumIndexedItems());  // first
    EXPECT_EQ(5u,
              NumSequentialMatches());  // second, first foreground, unaffected
    EXPECT_EQ(1u, NumOutOfOrderMatches());  // first
#endif
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(second.Id(), kBackgroundType),
                          IsSameId(second.Id(), kForegroundType),
                          IsSameId(first.Id(), kBackgroundType),
                          IsSameId(first.Id(), kForegroundType),
                          IsSameId(unaffected.Id(), kBackgroundType),
                          IsSameId(unaffected.Id(), kForegroundType)));
  EXPECT_DEFAULT_ROOT_CHUNK(6);
}

TEST_P(PaintControllerTest, UpdateSwapOrderWithInvalidation) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  FakeDisplayItemClient& unaffected =
      *MakeGarbageCollected<FakeDisplayItemClient>("unaffected");
  GraphicsContext context(GetPaintController());
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    EXPECT_EQ(kPaintedNew, DrawRect(context, first, kBackgroundType,
                                    gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, first, kForegroundType,
                                    gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, second, kBackgroundType,
                                    gfx::Rect(100, 100, 50, 200)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, second, kForegroundType,
                                    gfx::Rect(100, 100, 50, 200)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, unaffected, kBackgroundType,
                                    gfx::Rect(300, 300, 10, 10)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, unaffected, kForegroundType,
                                    gfx::Rect(300, 300, 10, 10)));
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(first.Id(), kBackgroundType),
                          IsSameId(first.Id(), kForegroundType),
                          IsSameId(second.Id(), kBackgroundType),
                          IsSameId(second.Id(), kForegroundType),
                          IsSameId(unaffected.Id(), kBackgroundType),
                          IsSameId(unaffected.Id(), kForegroundType)));

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    first.Invalidate();
    EXPECT_EQ(kCached, DrawRect(context, second, kBackgroundType,
                                gfx::Rect(100, 100, 50, 200)));
    EXPECT_EQ(kCached, DrawRect(context, second, kForegroundType,
                                gfx::Rect(100, 100, 50, 200)));
    EXPECT_EQ(kRepaintedCachedItem, DrawRect(context, first, kBackgroundType,
                                             gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kRepaintedCachedItem, DrawRect(context, first, kForegroundType,
                                             gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kCached, DrawRect(context, unaffected, kBackgroundType,
                                gfx::Rect(300, 300, 10, 10)));
    EXPECT_EQ(kCached, DrawRect(context, unaffected, kForegroundType,
                                gfx::Rect(300, 300, 10, 10)));

    EXPECT_EQ(4u, NumCachedNewItems());
    EXPECT_EQ(0u, NumCachedNewSubsequences());
#if DCHECK_IS_ON()
    EXPECT_EQ(2u, NumIndexedItems());
    EXPECT_EQ(5u, NumSequentialMatches());
    EXPECT_EQ(1u, NumOutOfOrderMatches());
#endif
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(second.Id(), kBackgroundType),
                          IsSameId(second.Id(), kForegroundType),
                          IsSameId(first.Id(), kBackgroundType),
                          IsSameId(first.Id(), kForegroundType),
                          IsSameId(unaffected.Id(), kBackgroundType),
                          IsSameId(unaffected.Id(), kForegroundType)));
  EXPECT_DEFAULT_ROOT_CHUNK(6);
}

TEST_P(PaintControllerTest, UpdateNewItemInMiddle) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  FakeDisplayItemClient& third =
      *MakeGarbageCollected<FakeDisplayItemClient>("third");
  GraphicsContext context(GetPaintController());
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    EXPECT_EQ(kPaintedNew, DrawRect(context, first, kBackgroundType,
                                    gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, second, kBackgroundType,
                                    gfx::Rect(100, 100, 50, 200)));
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(first.Id(), kBackgroundType),
                          IsSameId(second.Id(), kBackgroundType)));

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    EXPECT_EQ(kCached, DrawRect(context, first, kBackgroundType,
                                gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, third, kBackgroundType,
                                    gfx::Rect(125, 100, 200, 50)));
    EXPECT_EQ(kCached, DrawRect(context, second, kBackgroundType,
                                gfx::Rect(100, 100, 50, 200)));

    EXPECT_EQ(2u, NumCachedNewItems());
    EXPECT_EQ(0u, NumCachedNewSubsequences());
#if DCHECK_IS_ON()
    EXPECT_EQ(0u, NumIndexedItems());
    EXPECT_EQ(2u, NumSequentialMatches());  // first, second
    EXPECT_EQ(0u, NumOutOfOrderMatches());
#endif
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(first.Id(), kBackgroundType),
                          IsSameId(third.Id(), kBackgroundType),
                          IsSameId(second.Id(), kBackgroundType)));
  EXPECT_DEFAULT_ROOT_CHUNK(3);
}

TEST_P(PaintControllerTest, UpdateInvalidationWithPhases) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  FakeDisplayItemClient& third =
      *MakeGarbageCollected<FakeDisplayItemClient>("third");
  GraphicsContext context(GetPaintController());
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 100, 100));
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, third, kBackgroundType, gfx::Rect(300, 100, 50, 50));
    DrawRect(context, first, kForegroundType, gfx::Rect(100, 100, 100, 100));
    DrawRect(context, second, kForegroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, third, kForegroundType, gfx::Rect(300, 100, 50, 50));
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(first.Id(), kBackgroundType),
                          IsSameId(second.Id(), kBackgroundType),
                          IsSameId(third.Id(), kBackgroundType),
                          IsSameId(first.Id(), kForegroundType),
                          IsSameId(second.Id(), kForegroundType),
                          IsSameId(third.Id(), kForegroundType)));

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    second.Invalidate();
    EXPECT_EQ(kCached, DrawRect(context, first, kBackgroundType,
                                gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kRepaintedCachedItem, DrawRect(context, second, kBackgroundType,
                                             gfx::Rect(100, 100, 50, 200)));
    EXPECT_EQ(kCached, DrawRect(context, third, kBackgroundType,
                                gfx::Rect(300, 100, 50, 50)));
    EXPECT_EQ(kCached, DrawRect(context, first, kForegroundType,
                                gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kRepaintedCachedItem, DrawRect(context, second, kForegroundType,
                                             gfx::Rect(100, 100, 50, 200)));
    EXPECT_EQ(kCached, DrawRect(context, third, kForegroundType,
                                gfx::Rect(300, 100, 50, 50)));

    EXPECT_EQ(4u, NumCachedNewItems());
    EXPECT_EQ(0u, NumCachedNewSubsequences());
#if DCHECK_IS_ON()
    EXPECT_EQ(0u, NumIndexedItems());
    EXPECT_EQ(6u, NumSequentialMatches());
    EXPECT_EQ(0u, NumOutOfOrderMatches());
#endif
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(first.Id(), kBackgroundType),
                          IsSameId(second.Id(), kBackgroundType),
                          IsSameId(third.Id(), kBackgroundType),
                          IsSameId(first.Id(), kForegroundType),
                          IsSameId(second.Id(), kForegroundType),
                          IsSameId(third.Id(), kForegroundType)));
  EXPECT_DEFAULT_ROOT_CHUNK(6);
}

TEST_P(PaintControllerTest, UpdateAddFirstOverlap) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  GraphicsContext context(GetPaintController());
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    DrawRect(context, second, kBackgroundType, gfx::Rect(200, 200, 50, 50));
    DrawRect(context, second, kForegroundType, gfx::Rect(200, 200, 50, 50));
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(second.Id(), kBackgroundType),
                          IsSameId(second.Id(), kForegroundType)));

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    first.Invalidate();
    second.Invalidate();
    EXPECT_EQ(kPaintedNew, DrawRect(context, first, kBackgroundType,
                                    gfx::Rect(100, 100, 150, 150)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, first, kForegroundType,
                                    gfx::Rect(100, 100, 150, 150)));
    EXPECT_EQ(kRepaintedCachedItem, DrawRect(context, second, kBackgroundType,
                                             gfx::Rect(150, 250, 100, 100)));
    EXPECT_EQ(kRepaintedCachedItem, DrawRect(context, second, kForegroundType,
                                             gfx::Rect(150, 250, 100, 100)));
    EXPECT_EQ(0u, NumCachedNewItems());
    EXPECT_EQ(0u, NumCachedNewSubsequences());
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(first.Id(), kBackgroundType),
                          IsSameId(first.Id(), kForegroundType),
                          IsSameId(second.Id(), kBackgroundType),
                          IsSameId(second.Id(), kForegroundType)));
  EXPECT_DEFAULT_ROOT_CHUNK(4);

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    EXPECT_EQ(kCached, DrawRect(context, second, kBackgroundType,
                                gfx::Rect(150, 250, 100, 100)));
    EXPECT_EQ(kCached, DrawRect(context, second, kForegroundType,
                                gfx::Rect(150, 250, 100, 100)));

    EXPECT_EQ(2u, NumCachedNewItems());
    EXPECT_EQ(0u, NumCachedNewSubsequences());
#if DCHECK_IS_ON()
    EXPECT_EQ(2u, NumIndexedItems());
    EXPECT_EQ(2u, NumSequentialMatches());
    EXPECT_EQ(0u, NumOutOfOrderMatches());
#endif
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(second.Id(), kBackgroundType),
                          IsSameId(second.Id(), kForegroundType)));
  EXPECT_DEFAULT_ROOT_CHUNK(2);
}

TEST_P(PaintControllerTest, UpdateAddLastOverlap) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  GraphicsContext context(GetPaintController());
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 150, 150));
    DrawRect(context, first, kForegroundType, gfx::Rect(100, 100, 150, 150));
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(first.Id(), kBackgroundType),
                          IsSameId(first.Id(), kForegroundType)));

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    first.Invalidate();
    second.Invalidate();
    EXPECT_EQ(kRepaintedCachedItem, DrawRect(context, first, kBackgroundType,
                                             gfx::Rect(150, 150, 100, 100)));
    EXPECT_EQ(kRepaintedCachedItem, DrawRect(context, first, kForegroundType,
                                             gfx::Rect(150, 150, 100, 100)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, second, kBackgroundType,
                                    gfx::Rect(200, 200, 50, 50)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, second, kForegroundType,
                                    gfx::Rect(200, 200, 50, 50)));
    EXPECT_EQ(0u, NumCachedNewItems());
    EXPECT_EQ(0u, NumCachedNewSubsequences());
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(first.Id(), kBackgroundType),
                          IsSameId(first.Id(), kForegroundType),
                          IsSameId(second.Id(), kBackgroundType),
                          IsSameId(second.Id(), kForegroundType)));
  EXPECT_DEFAULT_ROOT_CHUNK(4);

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    first.Invalidate();
    second.Invalidate();
    EXPECT_EQ(kRepaintedCachedItem, DrawRect(context, first, kBackgroundType,
                                             gfx::Rect(100, 100, 150, 150)));
    EXPECT_EQ(kRepaintedCachedItem, DrawRect(context, first, kForegroundType,
                                             gfx::Rect(100, 100, 150, 150)));
    EXPECT_EQ(0u, NumCachedNewItems());
    EXPECT_EQ(0u, NumCachedNewSubsequences());
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(first.Id(), kBackgroundType),
                          IsSameId(first.Id(), kForegroundType)));
  EXPECT_DEFAULT_ROOT_CHUNK(2);
}

TEST_P(PaintControllerTest, CachedDisplayItems) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  GraphicsContext context(GetPaintController());
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 150, 150));
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 150, 150));
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(first.Id(), kBackgroundType),
                          IsSameId(second.Id(), kBackgroundType)));
  EXPECT_TRUE(ClientCacheIsValid(first));
  EXPECT_TRUE(ClientCacheIsValid(second));

  first.Invalidate();
  EXPECT_FALSE(ClientCacheIsValid(first));
  EXPECT_TRUE(ClientCacheIsValid(second));

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    EXPECT_EQ(kRepaintedCachedItem, DrawRect(context, first, kBackgroundType,
                                             gfx::Rect(100, 100, 150, 150)));
    EXPECT_EQ(kCached, DrawRect(context, second, kBackgroundType,
                                gfx::Rect(100, 100, 150, 150)));
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(first.Id(), kBackgroundType),
                          IsSameId(second.Id(), kBackgroundType)));
  EXPECT_TRUE(ClientCacheIsValid(first));
  EXPECT_TRUE(ClientCacheIsValid(second));

  InvalidateAll();
  EXPECT_FALSE(ClientCacheIsValid(first));
  EXPECT_FALSE(ClientCacheIsValid(second));
}

TEST_P(PaintControllerTest, UpdateSwapOrderWithChildren) {
  FakeDisplayItemClient& container1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container1");
  FakeDisplayItemClient& content1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content1");
  FakeDisplayItemClient& container2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container2");
  FakeDisplayItemClient& content2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content2");
  GraphicsContext context(GetPaintController());
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    DrawRect(context, container1, kBackgroundType,
             gfx::Rect(100, 100, 100, 100));
    DrawRect(context, content1, kBackgroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, content1, kForegroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, container1, kForegroundType,
             gfx::Rect(100, 100, 100, 100));
    DrawRect(context, container2, kBackgroundType,
             gfx::Rect(100, 200, 100, 100));
    DrawRect(context, content2, kBackgroundType, gfx::Rect(100, 200, 50, 200));
    DrawRect(context, content2, kForegroundType, gfx::Rect(100, 200, 50, 200));
    DrawRect(context, container2, kForegroundType,
             gfx::Rect(100, 200, 100, 100));
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(container1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kForegroundType),
                          IsSameId(container1.Id(), kForegroundType),
                          IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kForegroundType),
                          IsSameId(container2.Id(), kForegroundType)));

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    // Simulate the situation when |container1| gets a z-index that is greater
    // than that of |container2|.
    EXPECT_EQ(kCached, DrawRect(context, container2, kBackgroundType,
                                gfx::Rect(100, 200, 100, 100)));
    EXPECT_EQ(kCached, DrawRect(context, content2, kBackgroundType,
                                gfx::Rect(100, 200, 50, 200)));
    EXPECT_EQ(kCached, DrawRect(context, content2, kForegroundType,
                                gfx::Rect(100, 200, 50, 200)));
    EXPECT_EQ(kCached, DrawRect(context, container2, kForegroundType,
                                gfx::Rect(100, 200, 100, 100)));
    EXPECT_EQ(kCached, DrawRect(context, container1, kBackgroundType,
                                gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kCached, DrawRect(context, content1, kBackgroundType,
                                gfx::Rect(100, 100, 50, 200)));
    EXPECT_EQ(kCached, DrawRect(context, content1, kForegroundType,
                                gfx::Rect(100, 100, 50, 200)));
    EXPECT_EQ(kCached, DrawRect(context, container1, kForegroundType,
                                gfx::Rect(100, 100, 100, 100)));
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kForegroundType),
                          IsSameId(container2.Id(), kForegroundType),
                          IsSameId(container1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kForegroundType),
                          IsSameId(container1.Id(), kForegroundType)));
  EXPECT_DEFAULT_ROOT_CHUNK(8);
}

TEST_P(PaintControllerTest, UpdateSwapOrderWithChildrenAndInvalidation) {
  FakeDisplayItemClient& content1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content1");
  FakeDisplayItemClient& container1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container1");
  FakeDisplayItemClient& content2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content2");
  FakeDisplayItemClient& container2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container2");
  GraphicsContext context(GetPaintController());
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    DrawRect(context, container1, kBackgroundType,
             gfx::Rect(100, 100, 100, 100));
    DrawRect(context, content1, kBackgroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, content1, kForegroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, container1, kForegroundType,
             gfx::Rect(100, 100, 100, 100));
    DrawRect(context, container2, kBackgroundType,
             gfx::Rect(100, 200, 100, 100));
    DrawRect(context, content2, kBackgroundType, gfx::Rect(100, 200, 50, 200));
    DrawRect(context, content2, kForegroundType, gfx::Rect(100, 200, 50, 200));
    DrawRect(context, container2, kForegroundType,
             gfx::Rect(100, 200, 100, 100));
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(container1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kForegroundType),
                          IsSameId(container1.Id(), kForegroundType),
                          IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kForegroundType),
                          IsSameId(container2.Id(), kForegroundType)));

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    // Simulate the situation when |container1| gets a z-index that is greater
    // than that of |container2|, and |container1| is invalidated.
    container1.Invalidate();
    EXPECT_EQ(kCached, DrawRect(context, container2, kBackgroundType,
                                gfx::Rect(100, 200, 100, 100)));
    EXPECT_EQ(kCached, DrawRect(context, content2, kBackgroundType,
                                gfx::Rect(100, 200, 50, 200)));
    EXPECT_EQ(kCached, DrawRect(context, content2, kForegroundType,
                                gfx::Rect(100, 200, 50, 200)));
    EXPECT_EQ(kCached, DrawRect(context, container2, kForegroundType,
                                gfx::Rect(100, 200, 100, 100)));
    EXPECT_EQ(kRepaintedCachedItem,
              DrawRect(context, container1, kBackgroundType,
                       gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kCached, DrawRect(context, content1, kBackgroundType,
                                gfx::Rect(100, 100, 50, 200)));
    EXPECT_EQ(kCached, DrawRect(context, content1, kForegroundType,
                                gfx::Rect(100, 100, 50, 200)));
    EXPECT_EQ(kRepaintedCachedItem,
              DrawRect(context, container1, kForegroundType,
                       gfx::Rect(100, 100, 100, 100)));
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kForegroundType),
                          IsSameId(container2.Id(), kForegroundType),
                          IsSameId(container1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kForegroundType),
                          IsSameId(container1.Id(), kForegroundType)));
  EXPECT_DEFAULT_ROOT_CHUNK(8);
}

TEST_P(PaintControllerTest, CachedSubsequenceForcePaintChunk) {
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled())
    return;

  FakeDisplayItemClient& root =
      *MakeGarbageCollected<FakeDisplayItemClient>("root");
  auto root_properties = DefaultPaintChunkProperties();
  PaintChunk::Id root_id(root.Id(), DisplayItem::kCaret);
  FakeDisplayItemClient& container =
      *MakeGarbageCollected<FakeDisplayItemClient>("container");
  auto container_properties = DefaultPaintChunkProperties();
  PaintChunk::Id container_id(container.Id(), DisplayItem::kCaret);
  GraphicsContext context(GetPaintController());

  {
    CommitCycleScope cycle_scope(GetPaintController());

    GetPaintController().UpdateCurrentPaintChunkProperties(root_id, root,
                                                           root_properties);
    DrawRect(context, root, kBackgroundType, gfx::Rect(100, 100, 100, 100));

    {
      SubsequenceRecorder r(context, container);
      GetPaintController().UpdateCurrentPaintChunkProperties(
          container_id, container, container_properties);
      DrawRect(context, container, kBackgroundType,
               gfx::Rect(100, 100, 100, 100));
      DrawRect(context, container, kForegroundType,
               gfx::Rect(100, 100, 100, 100));
    }

    DrawRect(context, root, kForegroundType, gfx::Rect(100, 100, 100, 100));
  }

  // Even though the paint properties match, |container| should receive its
  // own PaintChunk because it created a subsequence.
  EXPECT_THAT(
      GetPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(0, 1, root_id, root_properties),
                  IsPaintChunk(1, 3, container_id, container_properties),
                  IsPaintChunk(3, 4, PaintChunk::Id(root.Id(), kForegroundType),
                               root_properties)));

  {
    CommitCycleScope cycle_scope(GetPaintController());
    GetPaintController().UpdateCurrentPaintChunkProperties(root_id, root,
                                                           root_properties);
    EXPECT_EQ(kCached, DrawRect(context, root, kBackgroundType,
                                gfx::Rect(100, 100, 100, 100)));
    EXPECT_TRUE(GetPaintController().UseCachedSubsequenceIfPossible(container));
    EXPECT_EQ(kCached, DrawRect(context, root, kForegroundType,
                                gfx::Rect(100, 100, 100, 100)));
  }

  // |container| should still receive its own PaintChunk because it is a cached
  // subsequence.
  EXPECT_THAT(
      GetPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(0, 1, root_id, root_properties),
                  IsPaintChunk(1, 3, container_id, container_properties),
                  IsPaintChunk(3, 4, PaintChunk::Id(root.Id(), kForegroundType),
                               container_properties)));
}

TEST_P(PaintControllerTest, CachedSubsequenceSwapOrder) {
  FakeDisplayItemClient& container1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container1");
  FakeDisplayItemClient& content1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content1");
  FakeDisplayItemClient& container2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container2");
  FakeDisplayItemClient& content2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content2");
  GraphicsContext context(GetPaintController());

  PaintChunk::Id container1_id(container1.Id(), kBackgroundType);
  auto container1_effect = CreateOpacityEffect(e0(), 0.5);
  auto container1_properties = DefaultPaintChunkProperties();
  container1_properties.SetEffect(*container1_effect);

  PaintChunk::Id container2_id(container2.Id(), kBackgroundType);
  auto container2_effect = CreateOpacityEffect(e0(), 0.5);
  auto container2_properties = DefaultPaintChunkProperties();
  container2_properties.SetEffect(*container2_effect);

  {
    CommitCycleScope cycle_scope(GetPaintController());

    {
      GetPaintController().UpdateCurrentPaintChunkProperties(
          container1_id, container1, container1_properties);

      SubsequenceRecorder r(context, container1);
      DrawRect(context, container1, kBackgroundType,
               gfx::Rect(100, 100, 100, 100));
      DrawRect(context, content1, kBackgroundType,
               gfx::Rect(100, 100, 50, 200));
      DrawRect(context, content1, kForegroundType,
               gfx::Rect(100, 100, 50, 200));
      DrawRect(context, container1, kForegroundType,
               gfx::Rect(100, 100, 100, 100));
    }
    {
      GetPaintController().UpdateCurrentPaintChunkProperties(
          container2_id, container2, container2_properties);

      SubsequenceRecorder r(context, container2);
      DrawRect(context, container2, kBackgroundType,
               gfx::Rect(100, 200, 100, 100));
      DrawRect(context, content2, kBackgroundType,
               gfx::Rect(100, 200, 50, 200));
      DrawRect(context, content2, kForegroundType,
               gfx::Rect(100, 200, 50, 200));
      DrawRect(context, container2, kForegroundType,
               gfx::Rect(100, 200, 100, 100));
    }
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(container1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kForegroundType),
                          IsSameId(container1.Id(), kForegroundType),

                          IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kForegroundType),
                          IsSameId(container2.Id(), kForegroundType)));

  EXPECT_SUBSEQUENCE(container1, 0, 1);
  EXPECT_NO_SUBSEQUENCE(content1);
  EXPECT_SUBSEQUENCE(container2, 1, 2);
  EXPECT_NO_SUBSEQUENCE(content2);

  EXPECT_THAT(
      GetPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(0, 4, container1_id, container1_properties),
                  IsPaintChunk(4, 8, container2_id, container2_properties)));

  {
    CommitCycleScope cycle_scope(GetPaintController());
    // Simulate the situation when |container1| gets a z-index that is greater
    // than that of |container2|.
    if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
      // When under-invalidation-checking is enabled,
      // UseCachedSubsequenceIfPossible is forced off, and the client is
      // expected to create the same painting as in the previous paint.
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container2));
      {
        GetPaintController().UpdateCurrentPaintChunkProperties(
            container2_id, container2, container2_properties);

        SubsequenceRecorder r(context, container2);
        EXPECT_EQ(kCached, DrawRect(context, container2, kBackgroundType,
                                    gfx::Rect(100, 200, 100, 100)));
        EXPECT_EQ(kCached, DrawRect(context, content2, kBackgroundType,
                                    gfx::Rect(100, 200, 50, 200)));
        EXPECT_EQ(kCached, DrawRect(context, content2, kForegroundType,
                                    gfx::Rect(100, 200, 50, 200)));
        EXPECT_EQ(kCached, DrawRect(context, container2, kForegroundType,
                                    gfx::Rect(100, 200, 100, 100)));
      }
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container1));
      {
        GetPaintController().UpdateCurrentPaintChunkProperties(
            container1_id, container1, container1_properties);

        SubsequenceRecorder r(context, container1);
        EXPECT_EQ(kCached, DrawRect(context, container1, kBackgroundType,
                                    gfx::Rect(100, 100, 100, 100)));
        EXPECT_EQ(kCached, DrawRect(context, content1, kBackgroundType,
                                    gfx::Rect(100, 100, 50, 200)));
        EXPECT_EQ(kCached, DrawRect(context, content1, kForegroundType,
                                    gfx::Rect(100, 100, 50, 200)));
        EXPECT_EQ(kCached, DrawRect(context, container1, kForegroundType,
                                    gfx::Rect(100, 100, 100, 100)));
      }
    } else {
      EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container2));
      EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container1));
    }

    EXPECT_EQ(8u, NumCachedNewItems());
    EXPECT_EQ(2u, NumCachedNewSubsequences());
#if DCHECK_IS_ON()
    EXPECT_EQ(0u, NumIndexedItems());
    EXPECT_EQ(0u, NumSequentialMatches());
    EXPECT_EQ(0u, NumOutOfOrderMatches());
#endif
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kForegroundType),
                          IsSameId(container2.Id(), kForegroundType),
                          IsSameId(container1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kForegroundType),
                          IsSameId(container1.Id(), kForegroundType)));

  EXPECT_SUBSEQUENCE(container1, 1, 2);
  EXPECT_NO_SUBSEQUENCE(content1);
  EXPECT_SUBSEQUENCE(container2, 0, 1);
  EXPECT_NO_SUBSEQUENCE(content2);

  EXPECT_THAT(
      GetPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(0, 4, container2_id, container2_properties),
                  IsPaintChunk(4, 8, container1_id, container1_properties)));
}

TEST_P(PaintControllerTest, CachedSubsequenceAndDisplayItemsSwapOrder) {
  FakeDisplayItemClient& content1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content1");
  FakeDisplayItemClient& container2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container2");
  FakeDisplayItemClient& content2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content2");
  GraphicsContext context(GetPaintController());

  PaintChunk::Id content1_id(content1.Id(), kBackgroundType);
  PaintChunk::Id container2_id(container2.Id(), kBackgroundType);
  PaintChunk::Id content2_id(content2.Id(), kBackgroundType);

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    DrawRect(context, content1, kBackgroundType, gfx::Rect(100, 100, 50, 200));
    {
      SubsequenceRecorder r(context, container2);
      DrawRect(context, container2, kBackgroundType,
               gfx::Rect(100, 200, 100, 100));
      DrawRect(context, content2, kBackgroundType,
               gfx::Rect(100, 200, 50, 200));
      DrawRect(context, content2, kForegroundType,
               gfx::Rect(100, 200, 50, 200));
      DrawRect(context, container2, kForegroundType,
               gfx::Rect(100, 200, 100, 100));
    }
    DrawRect(context, content1, kForegroundType, gfx::Rect(100, 100, 50, 200));
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(content1.Id(), kBackgroundType),
                          IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kForegroundType),
                          IsSameId(container2.Id(), kForegroundType),
                          IsSameId(content1.Id(), kForegroundType)));

  EXPECT_NO_SUBSEQUENCE(content1);
  EXPECT_SUBSEQUENCE(container2, 1, 2);
  EXPECT_NO_SUBSEQUENCE(content2);

  EXPECT_THAT(
      GetPaintController().PaintChunks(),
      ElementsAre(
          IsPaintChunk(0, 1, DefaultRootChunkId(),
                       DefaultPaintChunkProperties()),
          IsPaintChunk(1, 5, PaintChunk::Id(container2.Id(), kBackgroundType),
                       DefaultPaintChunkProperties()),
          IsPaintChunk(5, 6, PaintChunk::Id(content1.Id(), kForegroundType),
                       DefaultPaintChunkProperties())));

  // Simulate the situation when |container2| gets a z-index that is smaller
  // than that of |content1|.
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
      // When under-invalidation-checking is enabled,
      // UseCachedSubsequenceIfPossible is forced off, and the client is
      // expected to create the same painting as in the previous paint.
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container2));
      {
        SubsequenceRecorder r(context, container2);
        EXPECT_EQ(kCached, DrawRect(context, container2, kBackgroundType,
                                    gfx::Rect(100, 200, 100, 100)));
        EXPECT_EQ(kCached, DrawRect(context, content2, kBackgroundType,
                                    gfx::Rect(100, 200, 50, 200)));
        EXPECT_EQ(kCached, DrawRect(context, content2, kForegroundType,
                                    gfx::Rect(100, 200, 50, 200)));
        EXPECT_EQ(kCached, DrawRect(context, container2, kForegroundType,
                                    gfx::Rect(100, 200, 100, 100)));
      }
      EXPECT_EQ(kCached, DrawRect(context, content1, kBackgroundType,
                                  gfx::Rect(100, 100, 50, 200)));
      EXPECT_EQ(kCached, DrawRect(context, content1, kForegroundType,
                                  gfx::Rect(100, 100, 50, 200)));
    } else {
      EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container2));
      EXPECT_TRUE(DrawingRecorder::UseCachedDrawingIfPossible(context, content1,
                                                              kBackgroundType));
      EXPECT_TRUE(DrawingRecorder::UseCachedDrawingIfPossible(context, content1,
                                                              kForegroundType));
    }

    EXPECT_EQ(6u, NumCachedNewItems());
    EXPECT_EQ(1u, NumCachedNewSubsequences());
#if DCHECK_IS_ON()
    EXPECT_EQ(0u, NumIndexedItems());
    EXPECT_EQ(2u, NumSequentialMatches());
    EXPECT_EQ(0u, NumOutOfOrderMatches());
#endif
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kForegroundType),
                          IsSameId(container2.Id(), kForegroundType),
                          IsSameId(content1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kForegroundType)));

  EXPECT_NO_SUBSEQUENCE(content1);
  EXPECT_SUBSEQUENCE(container2, 0, 1);
  EXPECT_NO_SUBSEQUENCE(content2);

  EXPECT_THAT(
      GetPaintController().PaintChunks(),
      ElementsAre(
          IsPaintChunk(0, 4, PaintChunk::Id(container2.Id(), kBackgroundType),
                       DefaultPaintChunkProperties()),
          IsPaintChunk(4, 6, PaintChunk::Id(content1.Id(), kBackgroundType),
                       DefaultPaintChunkProperties())));
}

TEST_P(PaintControllerTest, DisplayItemSwapOrderBeforeCachedSubsequence) {
  FakeDisplayItemClient& content1a =
      *MakeGarbageCollected<FakeDisplayItemClient>("content1a");
  FakeDisplayItemClient& content1b =
      *MakeGarbageCollected<FakeDisplayItemClient>("content1b");
  FakeDisplayItemClient& container2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container2");
  FakeDisplayItemClient& content3 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content3");
  GraphicsContext context(GetPaintController());

  PaintChunk::Id content1a_id(content1a.Id(), kBackgroundType);
  PaintChunk::Id content1b_id(content1b.Id(), kBackgroundType);
  PaintChunk::Id container2_id(container2.Id(), kBackgroundType);
  PaintChunk::Id content3_id(content3.Id(), kBackgroundType);
  gfx::Rect rect(100, 100, 50, 200);

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    DrawRect(context, content1a, kBackgroundType, rect);
    DrawRect(context, content1b, kBackgroundType, rect);
    {
      SubsequenceRecorder r(context, container2);
      DrawRect(context, container2, kBackgroundType, rect);
    }
    DrawRect(context, content3, kBackgroundType, rect);
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(content1a.Id(), kBackgroundType),
                          IsSameId(content1b.Id(), kBackgroundType),
                          IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content3.Id(), kBackgroundType)));

  // New paint order:
  // Subsequence(container1): container1, content1b(cached), content1a(cached).
  // Subsequence(container2): cached
  // Subsequence(contaienr3): container3, content3
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
      EXPECT_FALSE(DrawingRecorder::UseCachedDrawingIfPossible(
          context, content1b, kBackgroundType));
      EXPECT_EQ(kCached, DrawRect(context, content1b, kBackgroundType, rect));
      EXPECT_FALSE(DrawingRecorder::UseCachedDrawingIfPossible(
          context, content1a, kBackgroundType));
      EXPECT_EQ(kCached, DrawRect(context, content1a, kBackgroundType, rect));
      {
        EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
            context, container2));
        SubsequenceRecorder r(context, container2);
        EXPECT_EQ(kCached,
                  DrawRect(context, container2, kBackgroundType, rect));
      }
      EXPECT_FALSE(DrawingRecorder::UseCachedDrawingIfPossible(
          context, content3, kBackgroundType));
      EXPECT_EQ(kCached, DrawRect(context, content3, kBackgroundType, rect));
    } else {
      EXPECT_TRUE(DrawingRecorder::UseCachedDrawingIfPossible(
          context, content1b, kBackgroundType));
      EXPECT_TRUE(DrawingRecorder::UseCachedDrawingIfPossible(
          context, content1a, kBackgroundType));
      EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container2));
      EXPECT_TRUE(DrawingRecorder::UseCachedDrawingIfPossible(context, content3,
                                                              kBackgroundType));
    }

    EXPECT_EQ(4u, NumCachedNewItems());
    EXPECT_EQ(1u, NumCachedNewSubsequences());
#if DCHECK_IS_ON()
    EXPECT_EQ(1u, NumIndexedItems());
    EXPECT_EQ(2u, NumSequentialMatches());
    EXPECT_EQ(1u, NumOutOfOrderMatches());
#endif
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(content1b.Id(), kBackgroundType),
                          IsSameId(content1a.Id(), kBackgroundType),
                          IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content3.Id(), kBackgroundType)));
}

TEST_P(PaintControllerTest, CachedSubsequenceContainingFragments) {
  GraphicsContext context(GetPaintController());
  FakeDisplayItemClient& root =
      *MakeGarbageCollected<FakeDisplayItemClient>("root");
  constexpr wtf_size_t kFragmentCount = 3;
  FakeDisplayItemClient& container =
      *MakeGarbageCollected<FakeDisplayItemClient>("container");

  auto paint_container = [this, &context, &container]() {
    SubsequenceRecorder r(context, container);
    for (wtf_size_t i = 0; i < kFragmentCount; ++i) {
      ScopedDisplayItemFragment scoped_fragment(context, i);
      ScopedPaintChunkProperties content_chunk_properties(
          GetPaintController(), DefaultPaintChunkProperties(), container,
          kBackgroundType);
      DrawRect(context, container, kBackgroundType,
               gfx::Rect(100, 100, 100, 100));
    }
  };

  // The first paint.
  {
    CommitCycleScope cycle_scope(GetPaintController());
    ScopedPaintChunkProperties root_chunk_properties(
        GetPaintController(), DefaultPaintChunkProperties(), root,
        kBackgroundType);
    DrawRect(context, root, kBackgroundType, gfx::Rect(100, 100, 100, 100));
    paint_container();
    DrawRect(context, root, kForegroundType, gfx::Rect(100, 100, 100, 100));
  }

  auto check_paint_results = [this, &root, &container]() {
    EXPECT_THAT(
        GetPaintController().PaintChunks(),
        ElementsAre(
            IsPaintChunk(0, 1, PaintChunk::Id(root.Id(), kBackgroundType),
                         DefaultPaintChunkProperties()),
            // One chunk for all of the fragments because they have the
            // same properties.
            IsPaintChunk(1, 4, PaintChunk::Id(container.Id(), kBackgroundType),
                         DefaultPaintChunkProperties()),
            IsPaintChunk(4, 5, PaintChunk::Id(root.Id(), kForegroundType),
                         DefaultPaintChunkProperties())));
  };
  // Check results of the first paint.
  check_paint_results();

  // The second paint.
  {
    CommitCycleScope cycle_scope(GetPaintController());
    ScopedPaintChunkProperties root_chunk_properties(
        GetPaintController(), DefaultPaintChunkProperties(), root,
        kBackgroundType);
    EXPECT_EQ(kCached, DrawRect(context, root, kBackgroundType,
                                gfx::Rect(100, 100, 100, 100)));

    if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
      EXPECT_FALSE(
          GetPaintController().UseCachedSubsequenceIfPossible(container));
      paint_container();
    } else {
      EXPECT_TRUE(
          GetPaintController().UseCachedSubsequenceIfPossible(container));
    }
    EXPECT_EQ(kCached, DrawRect(context, root, kForegroundType,
                                gfx::Rect(100, 100, 100, 100)));
  }

  // The second paint should produce the exactly same results.
  check_paint_results();
}

TEST_P(PaintControllerTest, UpdateSwapOrderCrossingChunks) {
  FakeDisplayItemClient& container1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container1");
  FakeDisplayItemClient& content1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content1");
  FakeDisplayItemClient& container2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container2");
  FakeDisplayItemClient& content2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content2");
  GraphicsContext context(GetPaintController());

  PaintChunk::Id container1_id(container1.Id(), kBackgroundType);
  auto container1_effect = CreateOpacityEffect(e0(), 0.5);
  auto container1_properties = DefaultPaintChunkProperties();
  container1_properties.SetEffect(*container1_effect);

  PaintChunk::Id container2_id(container2.Id(), kBackgroundType);
  auto container2_effect = CreateOpacityEffect(e0(), 0.5);
  auto container2_properties = DefaultPaintChunkProperties();
  container2_properties.SetEffect(*container2_effect);

  {
    CommitCycleScope cycle_scope(GetPaintController());
    GetPaintController().UpdateCurrentPaintChunkProperties(
        container1_id, container1, container1_properties);
    DrawRect(context, container1, kBackgroundType,
             gfx::Rect(100, 100, 100, 100));
    DrawRect(context, content1, kBackgroundType, gfx::Rect(100, 100, 50, 200));
    GetPaintController().UpdateCurrentPaintChunkProperties(
        container2_id, container2, container2_properties);
    DrawRect(context, container2, kBackgroundType,
             gfx::Rect(100, 200, 100, 100));
    DrawRect(context, content2, kBackgroundType, gfx::Rect(100, 200, 50, 200));
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(container1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kBackgroundType),
                          IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kBackgroundType)));

  EXPECT_THAT(
      GetPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(0, 2, container1_id, container1_properties),
                  IsPaintChunk(2, 4, container2_id, container2_properties)));

  // Move content2 into container1, without invalidation.
  {
    CommitCycleScope cycle_scope(GetPaintController());
    GetPaintController().UpdateCurrentPaintChunkProperties(
        container1_id, container1, container1_properties);
    EXPECT_EQ(kCached, DrawRect(context, container1, kBackgroundType,
                                gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kCached, DrawRect(context, content1, kBackgroundType,
                                gfx::Rect(100, 100, 50, 200)));
    EXPECT_EQ(kCached, DrawRect(context, content2, kBackgroundType,
                                gfx::Rect(100, 200, 50, 200)));
    GetPaintController().UpdateCurrentPaintChunkProperties(
        container2_id, container2, container2_properties);
    EXPECT_EQ(kCached, DrawRect(context, container2, kBackgroundType,
                                gfx::Rect(100, 200, 100, 100)));

    EXPECT_EQ(4u, NumCachedNewItems());
    EXPECT_EQ(0u, NumCachedNewSubsequences());
#if DCHECK_IS_ON()
    EXPECT_EQ(1u, NumIndexedItems());
    EXPECT_EQ(3u, NumSequentialMatches());
    EXPECT_EQ(1u, NumOutOfOrderMatches());
#endif
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(container1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kBackgroundType),
                          IsSameId(container2.Id(), kBackgroundType)));

  EXPECT_THAT(
      GetPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(0, 3, container1_id, container1_properties),
                  IsPaintChunk(3, 4, container2_id, container2_properties)));
}

TEST_P(PaintControllerTest, OutOfOrderNoCrash) {
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>("client");
  GraphicsContext context(GetPaintController());

  const DisplayItem::Type kType1 = DisplayItem::kDrawingFirst;
  const DisplayItem::Type kType2 =
      static_cast<DisplayItem::Type>(DisplayItem::kDrawingFirst + 1);
  const DisplayItem::Type kType3 =
      static_cast<DisplayItem::Type>(DisplayItem::kDrawingFirst + 2);
  const DisplayItem::Type kType4 =
      static_cast<DisplayItem::Type>(DisplayItem::kDrawingFirst + 3);

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    DrawRect(context, client, kType1, gfx::Rect(100, 100, 100, 100));
    DrawRect(context, client, kType2, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, client, kType3, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, client, kType4, gfx::Rect(100, 100, 100, 100));
  }

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    EXPECT_EQ(kCached,
              DrawRect(context, client, kType2, gfx::Rect(100, 100, 50, 200)));
    EXPECT_EQ(kCached,
              DrawRect(context, client, kType3, gfx::Rect(100, 100, 50, 200)));
    EXPECT_EQ(kCached,
              DrawRect(context, client, kType1, gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kCached,
              DrawRect(context, client, kType4, gfx::Rect(100, 100, 100, 100)));
  }
}

TEST_P(PaintControllerTest, CachedNestedSubsequenceUpdate) {
  FakeDisplayItemClient& container1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container1");
  FakeDisplayItemClient& content1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content1");
  FakeDisplayItemClient& container2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container2");
  FakeDisplayItemClient& content2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content2");
  GraphicsContext context(GetPaintController());

  PaintChunk::Id container1_background_id(container1.Id(), kBackgroundType);
  auto container1_effect = CreateOpacityEffect(e0(), 0.5);
  auto container1_background_properties = DefaultPaintChunkProperties();
  container1_background_properties.SetEffect(*container1_effect);
  PaintChunk::Id container1_foreground_id(container1.Id(), kForegroundType);
  auto container1_foreground_properties = DefaultPaintChunkProperties();
  container1_foreground_properties.SetEffect(*container1_effect);

  PaintChunk::Id content1_id(content1.Id(), kBackgroundType);
  auto content1_effect = CreateOpacityEffect(e0(), 0.6);
  auto content1_properties = DefaultPaintChunkProperties();
  content1_properties.SetEffect(*content1_effect);

  PaintChunk::Id container2_background_id(container2.Id(), kBackgroundType);
  auto container2_effect = CreateOpacityEffect(e0(), 0.7);
  auto container2_background_properties = DefaultPaintChunkProperties();
  container2_background_properties.SetEffect(*container2_effect);

  PaintChunk::Id content2_id(content2.Id(), kBackgroundType);
  auto content2_effect = CreateOpacityEffect(e0(), 0.8);
  auto content2_properties = DefaultPaintChunkProperties();
  content2_properties.SetEffect(*content2_effect);

  {
    CommitCycleScope cycle_scope(GetPaintController());
    {
      SubsequenceRecorder r(context, container1);
      GetPaintController().UpdateCurrentPaintChunkProperties(
          container1_background_id, container1,
          container1_background_properties);
      DrawRect(context, container1, kBackgroundType,
               gfx::Rect(100, 100, 100, 100));

      {
        SubsequenceRecorder inner_r(context, content1);
        GetPaintController().UpdateCurrentPaintChunkProperties(
            content1_id, content1, content1_properties);
        DrawRect(context, content1, kBackgroundType,
                 gfx::Rect(100, 100, 50, 200));
        DrawRect(context, content1, kForegroundType,
                 gfx::Rect(100, 100, 50, 200));
      }
      GetPaintController().UpdateCurrentPaintChunkProperties(
          container1_foreground_id, container1,
          container1_foreground_properties);
      DrawRect(context, container1, kForegroundType,
               gfx::Rect(100, 100, 100, 100));
    }
    {
      SubsequenceRecorder r(context, container2);
      GetPaintController().UpdateCurrentPaintChunkProperties(
          container2_background_id, container2,
          container2_background_properties);
      DrawRect(context, container2, kBackgroundType,
               gfx::Rect(100, 200, 100, 100));
      {
        SubsequenceRecorder inner_r(context, content2);
        GetPaintController().UpdateCurrentPaintChunkProperties(
            content2_id, content2, content2_properties);
        DrawRect(context, content2, kBackgroundType,
                 gfx::Rect(100, 200, 50, 200));
      }
    }
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(container1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kForegroundType),
                          IsSameId(container1.Id(), kForegroundType),
                          IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kBackgroundType)));

  EXPECT_SUBSEQUENCE(container1, 0, 3);
  EXPECT_SUBSEQUENCE(content1, 1, 2);
  EXPECT_SUBSEQUENCE(container2, 3, 5);
  EXPECT_SUBSEQUENCE(content2, 4, 5);

  EXPECT_THAT(
      GetPaintController().PaintChunks(),
      ElementsAre(IsPaintChunk(0, 1, container1_background_id,
                               container1_background_properties),
                  IsPaintChunk(1, 3, content1_id, content1_properties),
                  IsPaintChunk(3, 4, container1_foreground_id,
                               container1_foreground_properties),
                  IsPaintChunk(4, 5, container2_background_id,
                               container2_background_properties),
                  IsPaintChunk(5, 6, content2_id, content2_properties)));

  {
    CommitCycleScope cycle_scope(GetPaintController());
    // Invalidate container1 but not content1.
    container1.Invalidate();
    // Container2 itself now becomes empty (but still has the 'content2' child),
    // and chooses not to output subsequence info.
    container2.Invalidate();
    content2.Invalidate();
    EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container2));
    EXPECT_FALSE(
        SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, content2));
    // Content2 now outputs foreground only.
    {
      SubsequenceRecorder r(context, content2);
      GetPaintController().UpdateCurrentPaintChunkProperties(
          content2_id, content2, content2_properties);
      EXPECT_EQ(kPaintedNew, DrawRect(context, content2, kForegroundType,
                                      gfx::Rect(100, 200, 50, 200)));
    }
    // Repaint container1 with foreground only.
    {
      SubsequenceRecorder r(context, container1);
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container1));
      // Use cached subsequence of content1.
      if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
        // When under-invalidation-checking is enabled,
        // UseCachedSubsequenceIfPossible is forced off, and the client is
        // expected to create the same painting as in the previous paint.
        EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
            context, content1));
        SubsequenceRecorder inner_r(context, content1);
        GetPaintController().UpdateCurrentPaintChunkProperties(
            content1_id, content1, content1_properties);
        EXPECT_EQ(kCached, DrawRect(context, content1, kBackgroundType,
                                    gfx::Rect(100, 100, 50, 200)));
        EXPECT_EQ(kCached, DrawRect(context, content1, kForegroundType,
                                    gfx::Rect(100, 100, 50, 200)));
      } else {
        EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
            context, content1));
      }
      GetPaintController().UpdateCurrentPaintChunkProperties(
          container1_foreground_id, container1,
          container1_foreground_properties);
      EXPECT_EQ(kRepaintedCachedItem,
                DrawRect(context, container1, kForegroundType,
                         gfx::Rect(100, 100, 100, 100)));
    }

    EXPECT_EQ(2u, NumCachedNewItems());
    EXPECT_EQ(1u, NumCachedNewSubsequences());
#if DCHECK_IS_ON()
    EXPECT_EQ(6u, NumIndexedItems());
    EXPECT_EQ(0u, NumSequentialMatches());
    EXPECT_EQ(1u, NumOutOfOrderMatches());
#endif
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(content2.Id(), kForegroundType),
                          IsSameId(content1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kForegroundType),
                          IsSameId(container1.Id(), kForegroundType)));

  EXPECT_NO_SUBSEQUENCE(container2);
  EXPECT_SUBSEQUENCE(content2, 0, 1);
  EXPECT_SUBSEQUENCE(container1, 1, 3);
  EXPECT_SUBSEQUENCE(content1, 1, 2);

  EXPECT_THAT(GetPaintController().PaintChunks(),
              ElementsAre(IsPaintChunk(0, 1, content2_id, content2_properties),
                          IsPaintChunk(1, 3, content1_id, content1_properties),
                          IsPaintChunk(3, 4, container1_foreground_id,
                                       container1_foreground_properties)));
}

TEST_P(PaintControllerTest, CachedNestedSubsequenceKeepingDescendants) {
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled())
    return;

  FakeDisplayItemClient& root =
      *MakeGarbageCollected<FakeDisplayItemClient>("root");
  auto properties = DefaultPaintChunkProperties();
  GraphicsContext context(GetPaintController());
  PaintChunk::Id root_id(root.Id(), DisplayItem::kLayerChunk);
  FakeDisplayItemClient& container1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container1");
  PaintChunk::Id container1_bg_id(container1.Id(), kBackgroundType);
  PaintChunk::Id container1_fg_id(container1.Id(), kForegroundType);
  FakeDisplayItemClient& content1a =
      *MakeGarbageCollected<FakeDisplayItemClient>("content1a");
  PaintChunk::Id content1a_id(content1a.Id(), kBackgroundType);
  FakeDisplayItemClient& content1b =
      *MakeGarbageCollected<FakeDisplayItemClient>("content1b");
  PaintChunk::Id content1b_id(content1b.Id(), kForegroundType);
  FakeDisplayItemClient& container2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container2");
  PaintChunk::Id container2_id(container2.Id(), kBackgroundType);
  FakeDisplayItemClient& content2a =
      *MakeGarbageCollected<FakeDisplayItemClient>("content2a");
  PaintChunk::Id content2a_id(content2a.Id(), kBackgroundType);
  FakeDisplayItemClient& content2b =
      *MakeGarbageCollected<FakeDisplayItemClient>("content2b");
  PaintChunk::Id content2b_id(content2b.Id(), kForegroundType);
  {
    CommitCycleScope cycle_scope(GetPaintController());
    GetPaintController().UpdateCurrentPaintChunkProperties(root_id, root,
                                                           properties);

    {
      SubsequenceRecorder r(context, container1);
      DrawRect(context, container1, kBackgroundType,
               gfx::Rect(100, 100, 100, 100));
      {
        SubsequenceRecorder inner_r(context, content1a);
        DrawRect(context, content1a, kBackgroundType,
                 gfx::Rect(100, 100, 50, 200));
      }
      {
        SubsequenceRecorder inner_r(context, content1b);
        DrawRect(context, content1b, kForegroundType,
                 gfx::Rect(100, 100, 50, 200));
      }
      DrawRect(context, container1, kForegroundType,
               gfx::Rect(100, 100, 100, 100));
    }
    {
      SubsequenceRecorder r(context, container2);
      DrawRect(context, container2, kBackgroundType,
               gfx::Rect(100, 200, 100, 100));
      {
        SubsequenceRecorder inner_r(context, content2a);
        DrawRect(context, content2a, kBackgroundType,
                 gfx::Rect(100, 200, 50, 200));
      }
      {
        SubsequenceRecorder inner_r(context, content2b);
        DrawRect(context, content2b, kForegroundType,
                 gfx::Rect(100, 200, 50, 200));
      }
    }

    EXPECT_EQ(0u, NumCachedNewItems());
    EXPECT_EQ(0u, NumCachedNewSubsequences());
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(container1.Id(), kBackgroundType),
                          IsSameId(content1a.Id(), kBackgroundType),
                          IsSameId(content1b.Id(), kForegroundType),
                          IsSameId(container1.Id(), kForegroundType),
                          IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content2a.Id(), kBackgroundType),
                          IsSameId(content2b.Id(), kForegroundType)));

  EXPECT_SUBSEQUENCE(container1, 0, 4);
  EXPECT_SUBSEQUENCE(content1a, 1, 2);
  EXPECT_SUBSEQUENCE(content1b, 2, 3);
  EXPECT_SUBSEQUENCE(container2, 4, 7);
  EXPECT_SUBSEQUENCE(content2a, 5, 6);
  EXPECT_SUBSEQUENCE(content2b, 6, 7);

  EXPECT_THAT(GetPaintController().PaintChunks(),
              ElementsAre(IsPaintChunk(0, 1, container1_bg_id, properties),
                          IsPaintChunk(1, 2, content1a_id, properties),
                          IsPaintChunk(2, 3, content1b_id, properties),
                          IsPaintChunk(3, 4, container1_fg_id, properties),
                          IsPaintChunk(4, 5, container2_id, properties),
                          IsPaintChunk(5, 6, content2a_id, properties),
                          IsPaintChunk(6, 7, content2b_id, properties)));

  // Nothing invalidated. Should keep all subsequences.
  {
    CommitCycleScope cycle_scope(GetPaintController());
    EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container1));
    EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container2));

    EXPECT_EQ(7u, NumCachedNewItems());
    EXPECT_EQ(6u, NumCachedNewSubsequences());
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(container1.Id(), kBackgroundType),
                          IsSameId(content1a.Id(), kBackgroundType),
                          IsSameId(content1b.Id(), kForegroundType),
                          IsSameId(container1.Id(), kForegroundType),
                          IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content2a.Id(), kBackgroundType),
                          IsSameId(content2b.Id(), kForegroundType)));

  EXPECT_SUBSEQUENCE(container1, 0, 4);
  EXPECT_SUBSEQUENCE(content1a, 1, 2);
  EXPECT_SUBSEQUENCE(content1b, 2, 3);
  EXPECT_SUBSEQUENCE(container2, 4, 7);
  EXPECT_SUBSEQUENCE(content2a, 5, 6);
  EXPECT_SUBSEQUENCE(content2b, 6, 7);

  EXPECT_THAT(GetPaintController().PaintChunks(),
              ElementsAre(IsPaintChunk(0, 1, container1_bg_id, properties),
                          IsPaintChunk(1, 2, content1a_id, properties),
                          IsPaintChunk(2, 3, content1b_id, properties),
                          IsPaintChunk(3, 4, container1_fg_id, properties),
                          IsPaintChunk(4, 5, container2_id, properties),
                          IsPaintChunk(5, 6, content2a_id, properties),
                          IsPaintChunk(6, 7, content2b_id, properties)));

  // Swap order of the subsequences of container1 and container2.
  // Nothing invalidated. Should keep all subsequences.
  {
    CommitCycleScope cycle_scope(GetPaintController());
    EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container2));
    EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container1));

    EXPECT_EQ(7u, NumCachedNewItems());
    EXPECT_EQ(6u, NumCachedNewSubsequences());
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content2a.Id(), kBackgroundType),
                          IsSameId(content2b.Id(), kForegroundType),
                          IsSameId(container1.Id(), kBackgroundType),
                          IsSameId(content1a.Id(), kBackgroundType),
                          IsSameId(content1b.Id(), kForegroundType),
                          IsSameId(container1.Id(), kForegroundType)));

  EXPECT_SUBSEQUENCE(container2, 0, 3);
  EXPECT_SUBSEQUENCE(content2a, 1, 2);
  EXPECT_SUBSEQUENCE(content2b, 2, 3);
  EXPECT_SUBSEQUENCE(container1, 3, 7);
  EXPECT_SUBSEQUENCE(content1a, 4, 5);
  EXPECT_SUBSEQUENCE(content1b, 5, 6);

  EXPECT_THAT(GetPaintController().PaintChunks(),
              ElementsAre(IsPaintChunk(0, 1, container2_id, properties),
                          IsPaintChunk(1, 2, content2a_id, properties),
                          IsPaintChunk(2, 3, content2b_id, properties),
                          IsPaintChunk(3, 4, container1_bg_id, properties),
                          IsPaintChunk(4, 5, content1a_id, properties),
                          IsPaintChunk(5, 6, content1b_id, properties),
                          IsPaintChunk(6, 7, container1_fg_id, properties)));
}

TEST_P(PaintControllerTest, SkipCache) {
  FakeDisplayItemClient& multicol =
      *MakeGarbageCollected<FakeDisplayItemClient>("multicol");
  FakeDisplayItemClient& content =
      *MakeGarbageCollected<FakeDisplayItemClient>("content");
  GraphicsContext context(GetPaintController());
  gfx::Rect rect1(100, 100, 50, 50);
  gfx::Rect rect2(150, 100, 50, 50);
  gfx::Rect rect3(200, 100, 50, 50);

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

    DrawRect(context, multicol, kBackgroundType, gfx::Rect(100, 200, 100, 100));

    GetPaintController().BeginSkippingCache();
    DrawRect(context, content, kForegroundType, rect1);
    DrawRect(context, content, kForegroundType, rect2);
    GetPaintController().EndSkippingCache();
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(multicol.Id(), kBackgroundType),
                          IsSameId(content.Id(), kForegroundType),
                          IsSameId(content.Id(), kForegroundType)));
  EXPECT_DEFAULT_ROOT_CHUNK(3);

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    // Draw again with nothing invalidated.
    EXPECT_TRUE(ClientCacheIsValid(multicol));
    EXPECT_EQ(kCached, DrawRect(context, multicol, kBackgroundType,
                                gfx::Rect(100, 200, 100, 100)));

    GetPaintController().BeginSkippingCache();
    EXPECT_EQ(kPaintedNew, DrawRect(context, content, kForegroundType, rect1));
    EXPECT_EQ(kPaintedNew, DrawRect(context, content, kForegroundType, rect2));
    GetPaintController().EndSkippingCache();

    EXPECT_EQ(1u, NumCachedNewItems());
    EXPECT_EQ(0u, NumCachedNewSubsequences());
#if DCHECK_IS_ON()
    EXPECT_EQ(0u, NumIndexedItems());
    EXPECT_EQ(1u, NumSequentialMatches());
    EXPECT_EQ(0u, NumOutOfOrderMatches());
#endif
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(multicol.Id(), kBackgroundType),
                          IsSameId(content.Id(), kForegroundType),
                          IsSameId(content.Id(), kForegroundType)));
  EXPECT_DEFAULT_ROOT_CHUNK(3);

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    // Now the multicol becomes 3 columns and repaints.
    multicol.Invalidate();
    EXPECT_EQ(kRepaintedCachedItem, DrawRect(context, multicol, kBackgroundType,
                                             gfx::Rect(100, 100, 100, 100)));

    GetPaintController().BeginSkippingCache();
    EXPECT_EQ(kPaintedNew, DrawRect(context, content, kForegroundType, rect1));
    EXPECT_EQ(kPaintedNew, DrawRect(context, content, kForegroundType, rect2));
    EXPECT_EQ(kPaintedNew, DrawRect(context, content, kForegroundType, rect3));
    GetPaintController().EndSkippingCache();

    // We should repaint everything on invalidation of the scope container.
    const auto& display_item_list =
        GetPaintController().GetNewPaintArtifactShared()->GetDisplayItemList();
    EXPECT_THAT(display_item_list,
                ElementsAre(IsSameId(multicol.Id(), kBackgroundType),
                            IsSameId(content.Id(), kForegroundType),
                            IsSameId(content.Id(), kForegroundType),
                            IsSameId(content.Id(), kForegroundType)));
  }
  EXPECT_DEFAULT_ROOT_CHUNK(4);
}

TEST_P(PaintControllerTest, PartialSkipCache) {
  FakeDisplayItemClient& content =
      *MakeGarbageCollected<FakeDisplayItemClient>("content");
  GraphicsContext context(GetPaintController());

  gfx::Rect rect1(100, 100, 50, 50);
  gfx::Rect rect2(150, 100, 50, 50);
  gfx::Rect rect3(200, 100, 50, 50);

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    DrawRect(context, content, kBackgroundType, rect1);
    GetPaintController().BeginSkippingCache();
    DrawRect(context, content, kForegroundType, rect2);
    GetPaintController().EndSkippingCache();
    DrawRect(context, content, kForegroundType, rect3);
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(content.Id(), kBackgroundType),
                          IsSameId(content.Id(), kForegroundType),
                          IsSameId(content.Id(), kForegroundType)));

  // Content's cache is invalid because it has display items skipped cache.
  EXPECT_FALSE(ClientCacheIsValid(content));
  EXPECT_EQ(PaintInvalidationReason::kUncacheable,
            content.GetPaintInvalidationReason());

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    // Draw again with nothing invalidated.
    EXPECT_EQ(kPaintedNew, DrawRect(context, content, kBackgroundType, rect1));
    GetPaintController().BeginSkippingCache();
    EXPECT_EQ(kPaintedNew, DrawRect(context, content, kForegroundType, rect2));
    GetPaintController().EndSkippingCache();
    EXPECT_EQ(kPaintedNew, DrawRect(context, content, kForegroundType, rect3));

    EXPECT_EQ(0u, NumCachedNewItems());
    EXPECT_EQ(0u, NumCachedNewSubsequences());
#if DCHECK_IS_ON()
    EXPECT_EQ(0u, NumIndexedItems());
    EXPECT_EQ(0u, NumSequentialMatches());
    EXPECT_EQ(0u, NumOutOfOrderMatches());
#endif
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(content.Id(), kBackgroundType),
                          IsSameId(content.Id(), kForegroundType),
                          IsSameId(content.Id(), kForegroundType)));
}

TEST_P(PaintControllerTest, SkipCacheDuplicatedItemAndChunkIds) {
  FakeDisplayItemClient& chunk_client =
      *MakeGarbageCollected<FakeDisplayItemClient>("chunk client");
  FakeDisplayItemClient& item_client =
      *MakeGarbageCollected<FakeDisplayItemClient>("item client");
  auto properties = DefaultPaintChunkProperties();
  PaintChunk::Id chunk_id(chunk_client.Id(), DisplayItem::kLayerChunk);
  auto& paint_controller = GetPaintController();

  {
    CommitCycleScope cycle_scope(paint_controller);
    GraphicsContext context(paint_controller);
    paint_controller.BeginSkippingCache();
    paint_controller.SetWillForceNewChunk(true);
    paint_controller.UpdateCurrentPaintChunkProperties(chunk_id, chunk_client,
                                                       properties);
    DrawRect(context, item_client, kBackgroundType, gfx::Rect(0, 0, 100, 100));
    paint_controller.SetWillForceNewChunk(true);
    paint_controller.UpdateCurrentPaintChunkProperties(chunk_id, chunk_client,
                                                       properties);
    DrawRect(context, item_client, kBackgroundType, gfx::Rect(0, 0, 100, 100));
    paint_controller.EndSkippingCache();
  }

  EXPECT_THAT(paint_controller.GetDisplayItemList(),
              ElementsAre(IsSameId(item_client.Id(), kBackgroundType),
                          IsSameId(item_client.Id(), kBackgroundType)));
  EXPECT_FALSE(paint_controller.GetDisplayItemList()[0].IsCacheable());
  EXPECT_FALSE(paint_controller.GetDisplayItemList()[1].IsCacheable());

  EXPECT_THAT(GetPaintController().PaintChunks(),
              ElementsAre(IsPaintChunk(0, 1, chunk_id, properties),
                          IsPaintChunk(1, 2, chunk_id, properties)));
  EXPECT_FALSE(paint_controller.PaintChunks()[0].is_cacheable);
  EXPECT_FALSE(paint_controller.PaintChunks()[1].is_cacheable);
}

TEST_P(PaintControllerTest, SmallPaintControllerHasOnePaintChunk) {
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>("test client");

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    GraphicsContext context(GetPaintController());
    DrawRect(context, client, kBackgroundType, gfx::Rect(0, 0, 100, 100));
  }
  EXPECT_THAT(GetPaintController().PaintChunks(),
              ElementsAre(IsPaintChunk(0, 1)));
}
void DrawPath(GraphicsContext& context,
              DisplayItemClient& client,
              DisplayItem::Type type,
              unsigned count) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, client, type))
    return;

  DrawingRecorder recorder(context, client, type, gfx::Rect(0, 0, 100, 100));
  SkPathBuilder builder;
  builder.moveTo(0, 0);
  builder.lineTo(0, 100);
  builder.lineTo(50, 50);
  builder.lineTo(100, 100);
  builder.lineTo(100, 0);
  builder.close();
  SkPath path = builder.detach();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  for (unsigned i = 0; i < count; i++)
    context.DrawPath(path, flags, AutoDarkMode::Disabled());
}

TEST_P(PaintControllerTest, BeginAndEndFrame) {
  class FakeFrame {};

  // PaintController should have one null frame in the stack since beginning.
  GetPaintController().SetFirstPainted();
  FrameFirstPaint result = GetPaintController().EndFrame(nullptr);
  EXPECT_TRUE(result.first_painted);
  EXPECT_FALSE(result.text_painted);
  EXPECT_FALSE(result.image_painted);
  // Readd the null frame.
  GetPaintController().BeginFrame(nullptr);

  std::unique_ptr<FakeFrame> frame1(new FakeFrame);
  GetPaintController().BeginFrame(frame1.get());
  GetPaintController().SetFirstPainted();
  GetPaintController().SetTextPainted();
  GetPaintController().SetImagePainted();

  result = GetPaintController().EndFrame(frame1.get());
  EXPECT_TRUE(result.first_painted);
  EXPECT_TRUE(result.text_painted);
  EXPECT_TRUE(result.image_painted);

  std::unique_ptr<FakeFrame> frame2(new FakeFrame);
  GetPaintController().BeginFrame(frame2.get());
  GetPaintController().SetFirstPainted();

  std::unique_ptr<FakeFrame> frame3(new FakeFrame);
  GetPaintController().BeginFrame(frame3.get());
  GetPaintController().SetTextPainted();
  GetPaintController().SetImagePainted();

  result = GetPaintController().EndFrame(frame3.get());
  EXPECT_FALSE(result.first_painted);
  EXPECT_TRUE(result.text_painted);
  EXPECT_TRUE(result.image_painted);

  result = GetPaintController().EndFrame(frame2.get());
  EXPECT_TRUE(result.first_painted);
  EXPECT_FALSE(result.text_painted);
  EXPECT_FALSE(result.image_painted);
}

TEST_P(PaintControllerTest, InsertValidItemInFront) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  FakeDisplayItemClient& third =
      *MakeGarbageCollected<FakeDisplayItemClient>("third");
  FakeDisplayItemClient& fourth =
      *MakeGarbageCollected<FakeDisplayItemClient>("fourth");
  GraphicsContext context(GetPaintController());

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 300, 300));
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 200, 200));
    DrawRect(context, third, kBackgroundType, gfx::Rect(100, 100, 100, 100));
    DrawRect(context, fourth, kBackgroundType, gfx::Rect(100, 100, 50, 50));

    EXPECT_EQ(0u, NumCachedNewItems());
    EXPECT_EQ(0u, NumCachedNewSubsequences());
  }
  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(first.Id(), kBackgroundType),
                          IsSameId(second.Id(), kBackgroundType),
                          IsSameId(third.Id(), kBackgroundType),
                          IsSameId(fourth.Id(), kBackgroundType)));
  EXPECT_TRUE(first.IsValid());
  EXPECT_TRUE(second.IsValid());
  EXPECT_TRUE(third.IsValid());
  EXPECT_TRUE(fourth.IsValid());

  // Simulate that a composited scrolling element is scrolled down, and "first"
  // and "second" are scrolled out of the interest rect.
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    EXPECT_EQ(kCached, DrawRect(context, third, kBackgroundType,
                                gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kCached, DrawRect(context, fourth, kBackgroundType,
                                gfx::Rect(100, 100, 50, 50)));

    EXPECT_EQ(2u, NumCachedNewItems());
    EXPECT_EQ(0u, NumCachedNewSubsequences());
#if DCHECK_IS_ON()
    // We indexed "first" and "second" when finding the cached item for "third".
    EXPECT_EQ(2u, NumIndexedItems());
    EXPECT_EQ(2u, NumSequentialMatches());
    EXPECT_EQ(0u, NumOutOfOrderMatches());
#endif
  }
  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(third.Id(), kBackgroundType),
                          IsSameId(fourth.Id(), kBackgroundType)));
  EXPECT_TRUE(first.IsValid());
  EXPECT_TRUE(second.IsValid());
  EXPECT_TRUE(third.IsValid());
  EXPECT_TRUE(fourth.IsValid());

  // Simulate "first" and "second" are scrolled back into the interest rect.
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    EXPECT_EQ(kPaintedNew, DrawRect(context, first, kBackgroundType,
                                    gfx::Rect(100, 100, 300, 300)));
    EXPECT_EQ(kPaintedNew, DrawRect(context, second, kBackgroundType,
                                    gfx::Rect(100, 100, 200, 200)));
    EXPECT_EQ(kCached, DrawRect(context, third, kBackgroundType,
                                gfx::Rect(100, 100, 100, 100)));
    EXPECT_EQ(kCached, DrawRect(context, fourth, kBackgroundType,
                                gfx::Rect(100, 100, 50, 50)));

    EXPECT_EQ(2u, NumCachedNewItems());
    EXPECT_EQ(0u, NumCachedNewSubsequences());
#if DCHECK_IS_ON()
    // We indexed "third" and "fourth" when finding the cached item for "first".
    EXPECT_EQ(2u, NumIndexedItems());
    EXPECT_EQ(2u, NumSequentialMatches());
    EXPECT_EQ(0u, NumOutOfOrderMatches());
#endif
  }
  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(first.Id(), kBackgroundType),
                          IsSameId(second.Id(), kBackgroundType),
                          IsSameId(third.Id(), kBackgroundType),
                          IsSameId(fourth.Id(), kBackgroundType)));
  EXPECT_TRUE(first.IsValid());
  EXPECT_TRUE(second.IsValid());
  EXPECT_TRUE(third.IsValid());
  EXPECT_TRUE(fourth.IsValid());
}

TEST_P(PaintControllerTest, TransientPaintControllerIncompleteCycle) {
  auto paint_controller =
      std::make_unique<PaintController>(PaintController::kTransient);
  GraphicsContext context(*paint_controller);
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>("client");
  InitRootChunk(*paint_controller);
  DrawRect(context, client, kBackgroundType, gfx::Rect(100, 100, 50, 50));
  // The client of a transient paint controller can abort without
  // CommintNewDisplayItems() and FinishCycle(). This should not crash.
  paint_controller = nullptr;
}

TEST_P(PaintControllerTest, AllowDuplicatedIdForTransientPaintController) {
  auto paint_controller =
      std::make_unique<PaintController>(PaintController::kTransient);
  GraphicsContext context(*paint_controller);
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>("client");
  {
    CommitCycleScope cycle_scope(*paint_controller);
    InitRootChunk(*paint_controller);
    {
      paint_controller->SetWillForceNewChunk(true);
      ScopedPaintChunkProperties p(*paint_controller,
                                   DefaultPaintChunkProperties(), client,
                                   kBackgroundType);
      DrawRect(context, client, kBackgroundType, gfx::Rect(100, 100, 50, 50));
    }
    {
      paint_controller->SetWillForceNewChunk(true);
      ScopedPaintChunkProperties p(*paint_controller,
                                   DefaultPaintChunkProperties(), client,
                                   kBackgroundType);
      DrawRect(context, client, kBackgroundType, gfx::Rect(100, 100, 50, 50));
    }
  }
  EXPECT_EQ(2u, paint_controller->GetDisplayItemList().size());
  EXPECT_EQ(2u, paint_controller->PaintChunks().size());
}

TEST_P(PaintControllerTest, AllowDuplicatedIdForUncacheableItem) {
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled())
    return;

  gfx::Rect r(100, 100, 300, 300);
  FakeDisplayItemClient& cacheable =
      *MakeGarbageCollected<FakeDisplayItemClient>("cacheable");
  FakeDisplayItemClient& uncacheable =
      *MakeGarbageCollected<FakeDisplayItemClient>("uncacheable");
  GraphicsContext context(GetPaintController());

  uncacheable.Invalidate(PaintInvalidationReason::kUncacheable);
  EXPECT_TRUE(cacheable.IsCacheable());
  EXPECT_FALSE(uncacheable.IsCacheable());

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    {
      SubsequenceRecorder recorder(context, cacheable);
      DrawRect(context, cacheable, kBackgroundType, gfx::Rect(r));
      DrawRect(context, uncacheable, kBackgroundType, gfx::Rect(r));
      // This should not trigger the duplicated id assert.
      DrawRect(context, uncacheable, kBackgroundType, gfx::Rect(r));
    }
  }
  EXPECT_TRUE(GetPaintController().GetDisplayItemList()[0].IsCacheable());
  EXPECT_FALSE(GetPaintController().GetDisplayItemList()[1].IsCacheable());
  EXPECT_FALSE(GetPaintController().GetDisplayItemList()[2].IsCacheable());
  EXPECT_TRUE(cacheable.IsCacheable());
  EXPECT_FALSE(uncacheable.IsCacheable());

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    EXPECT_TRUE(GetPaintController().UseCachedSubsequenceIfPossible(cacheable));
  }
  EXPECT_TRUE(GetPaintController().GetDisplayItemList()[0].IsCacheable());
  EXPECT_FALSE(GetPaintController().GetDisplayItemList()[1].IsCacheable());
  EXPECT_FALSE(GetPaintController().GetDisplayItemList()[2].IsCacheable());
  EXPECT_TRUE(cacheable.IsCacheable());
  EXPECT_FALSE(uncacheable.IsCacheable());
}

TEST_P(PaintControllerTest, RecordRegionCaptureDataValidData) {
  static const auto kCropId = RegionCaptureCropId(base::Token::CreateRandom());
  static const gfx::Rect kBounds(1, 2, 640, 480);

  GraphicsContext context(GetPaintController());
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>("client");
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    GetPaintController().RecordRegionCaptureData(client, kCropId, kBounds);

    DrawRect(context, client, kBackgroundType, gfx::Rect(100, 100, 200, 200));
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(client.Id(), kBackgroundType)));
  EXPECT_DEFAULT_ROOT_CHUNK(1);
  const Vector<PaintChunk>& chunks = GetPaintController().PaintChunks();
  EXPECT_EQ(1u, chunks.size());
  EXPECT_EQ(kBounds, chunks[0].region_capture_data->find(kCropId)->second);
}

// Death tests don't work properly on Android.
#if defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)

TEST_P(PaintControllerTest, RecordRegionCaptureDataEmptyToken) {
  static const auto kCropId = RegionCaptureCropId(base::Token{});
  static const gfx::Rect kBounds(1, 2, 640, 480);

  GraphicsContext context(GetPaintController());
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>("client");
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();

#if DCHECK_IS_ON()
    EXPECT_DEATH(
        GetPaintController().RecordRegionCaptureData(client, kCropId, kBounds),
        "Check failed: !crop_id->is_zero");
  }
#else
    // If DCHECKs are not enabled, we should just record the data as-is.
    GetPaintController().RecordRegionCaptureData(client, kCropId, kBounds);
    DrawRect(context, client, kBackgroundType, gfx::Rect(100, 100, 200, 200));
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(client.Id(), kBackgroundType)));
  EXPECT_DEFAULT_ROOT_CHUNK(1);
  const Vector<PaintChunk>& chunks = GetPaintController().PaintChunks();
  EXPECT_EQ(1u, chunks.size());
  EXPECT_EQ(kBounds, chunks[0].region_capture_data->find(kCropId)->second);
#endif
}

TEST_P(PaintControllerTest, DuplicatedSubsequences) {
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>("test");
  PaintController& controller = GetPaintController();
  GraphicsContext context(controller);

  auto paint_duplicated_subsequences = [&]() {
    {
      CommitCycleScope cycle_scope(controller);
      InitRootChunk();
      {
        SubsequenceRecorder r(context, client);
        DrawRect(context, client, kBackgroundType,
                 gfx::Rect(100, 100, 100, 100));
      }
      {
        SubsequenceRecorder r(context, client);
        DrawRect(context, client, kForegroundType,
                 gfx::Rect(100, 100, 100, 100));
      }
    }
  };

#if DCHECK_IS_ON()
  EXPECT_DEATH(paint_duplicated_subsequences(),
               "Multiple subsequences for client: \"test\"");
#else
  // The following is for non-DCHECK path. No security CHECK should trigger.
  {
    CommitCycleScope cycle_scope(GetPaintController());
    paint_duplicated_subsequences();
    // Paint again.
    InitRootChunk();
    if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
      EXPECT_FALSE(GetPaintController().UseCachedSubsequenceIfPossible(client));
      SubsequenceRecorder r(context, client);
      DrawRect(context, client, kBackgroundType, gfx::Rect(100, 100, 100, 100));
    } else {
      EXPECT_TRUE(GetPaintController().UseCachedSubsequenceIfPossible(client));
    }
    {
      // Should not use the cached duplicated subsequence.
      EXPECT_FALSE(GetPaintController().UseCachedSubsequenceIfPossible(client));
      SubsequenceRecorder r(context, client);
      DrawRect(context, client, kForegroundType, gfx::Rect(100, 100, 100, 100));
    }
  }
#endif
}

TEST_P(PaintControllerTest, DeletedClientInUnderInvalidatedSubsequence) {
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled())
    return;

  FakeDisplayItemClient& container =
      *MakeGarbageCollected<FakeDisplayItemClient>("container");
  auto* content = MakeGarbageCollected<FakeDisplayItemClient>("content");
  GraphicsContext context(GetPaintController());

  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    {
      SubsequenceRecorder r(context, container);
      DrawRect(context, *content, kBackgroundType,
               gfx::Rect(100, 100, 300, 300));
    }
  }

  content = nullptr;
  {
    CommitCycleScope cycle_scope(GetPaintController());
    InitRootChunk();
    // Leave container not invalidated; this should not crash.
    EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(context,
                                                                    container));
  }
}

#endif  // defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)

}  // namespace blink
