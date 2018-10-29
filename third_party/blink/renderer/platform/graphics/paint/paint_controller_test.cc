// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_controller_test.h"

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

namespace blink {

// Tests using this class will be tested with under-invalidation-checking
// enabled and disabled.
class PaintControllerTest : public PaintTestConfigurations,
                            public PaintControllerTestBase {
};

INSTANTIATE_TEST_CASE_P(
    All,
    PaintControllerTest,
    testing::Values(0,
                    kBlinkGenPropertyTrees,
                    kSlimmingPaintV2,
                    kUnderInvalidationChecking,
                    kBlinkGenPropertyTrees | kUnderInvalidationChecking,
                    kSlimmingPaintV2 | kUnderInvalidationChecking));

TEST_P(PaintControllerTest, NestedRecorders) {
  GraphicsContext context(GetPaintController());
  FakeDisplayItemClient client("client", LayoutRect(100, 100, 200, 200));
  InitRootChunk();

  DrawRect(context, client, kBackgroundType, FloatRect(100, 100, 200, 200));
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 1,
                      TestDisplayItem(client, kBackgroundType));

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
}

TEST_P(PaintControllerTest, UpdateBasic) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 300, 300));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 200, 200));
  GraphicsContext context(GetPaintController());
  InitRootChunk();

  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 200, 200));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));

  EXPECT_EQ(0, NumCachedNewItems());

  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(first, kForegroundType));

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());

  InitRootChunk();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));

  EXPECT_EQ(2, NumCachedNewItems());
#if DCHECK_IS_ON()
  EXPECT_EQ(2, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(1, NumIndexedItems());
#endif

  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType));

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
}

TEST_P(PaintControllerTest, UpdateSwapOrder) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient unaffected("unaffected", LayoutRect(300, 300, 10, 10));
  GraphicsContext context(GetPaintController());
  InitRootChunk();

  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, second, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, unaffected, kBackgroundType, FloatRect(300, 300, 10, 10));
  DrawRect(context, unaffected, kForegroundType, FloatRect(300, 300, 10, 10));
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType),
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(second, kForegroundType),
                      TestDisplayItem(unaffected, kBackgroundType),
                      TestDisplayItem(unaffected, kForegroundType));

  InitRootChunk();
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, second, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, unaffected, kBackgroundType, FloatRect(300, 300, 10, 10));
  DrawRect(context, unaffected, kForegroundType, FloatRect(300, 300, 10, 10));

  EXPECT_EQ(6, NumCachedNewItems());
#if DCHECK_IS_ON()
  EXPECT_EQ(5, NumSequentialMatches());  // second, first foreground, unaffected
  EXPECT_EQ(1, NumOutOfOrderMatches());  // first
  EXPECT_EQ(2, NumIndexedItems());       // first
#endif

  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(second, kForegroundType),
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType),
                      TestDisplayItem(unaffected, kBackgroundType),
                      TestDisplayItem(unaffected, kForegroundType));

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
}

TEST_P(PaintControllerTest, UpdateSwapOrderWithInvalidation) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient unaffected("unaffected", LayoutRect(300, 300, 10, 10));
  GraphicsContext context(GetPaintController());
  InitRootChunk();

  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, second, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, unaffected, kBackgroundType, FloatRect(300, 300, 10, 10));
  DrawRect(context, unaffected, kForegroundType, FloatRect(300, 300, 10, 10));
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType),
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(second, kForegroundType),
                      TestDisplayItem(unaffected, kBackgroundType),
                      TestDisplayItem(unaffected, kForegroundType));

  InitRootChunk();
  first.Invalidate();
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, second, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, unaffected, kBackgroundType, FloatRect(300, 300, 10, 10));
  DrawRect(context, unaffected, kForegroundType, FloatRect(300, 300, 10, 10));

  EXPECT_EQ(4, NumCachedNewItems());
#if DCHECK_IS_ON()
  EXPECT_EQ(4, NumSequentialMatches());  // second, unaffected
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(2, NumIndexedItems());
#endif

  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(second, kForegroundType),
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType),
                      TestDisplayItem(unaffected, kBackgroundType),
                      TestDisplayItem(unaffected, kForegroundType));

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
}

TEST_P(PaintControllerTest, UpdateNewItemInMiddle) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient third("third", LayoutRect(125, 100, 200, 50));
  GraphicsContext context(GetPaintController());
  InitRootChunk();

  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(second, kBackgroundType));

  InitRootChunk();

  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, third, kBackgroundType, FloatRect(125, 100, 200, 50));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));

  EXPECT_EQ(2, NumCachedNewItems());
#if DCHECK_IS_ON()
  EXPECT_EQ(2, NumSequentialMatches());  // first, second
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(0, NumIndexedItems());
#endif

  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(third, kBackgroundType),
                      TestDisplayItem(second, kBackgroundType));

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
}

TEST_P(PaintControllerTest, UpdateInvalidationWithPhases) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient third("third", LayoutRect(300, 100, 50, 50));
  GraphicsContext context(GetPaintController());
  InitRootChunk();

  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, third, kBackgroundType, FloatRect(300, 100, 50, 50));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, third, kForegroundType, FloatRect(300, 100, 50, 50));
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(third, kBackgroundType),
                      TestDisplayItem(first, kForegroundType),
                      TestDisplayItem(second, kForegroundType),
                      TestDisplayItem(third, kForegroundType));

  InitRootChunk();

  second.Invalidate();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, third, kBackgroundType, FloatRect(300, 100, 50, 50));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, third, kForegroundType, FloatRect(300, 100, 50, 50));

  EXPECT_EQ(4, NumCachedNewItems());
#if DCHECK_IS_ON()
  EXPECT_EQ(4, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(2, NumIndexedItems());
#endif

  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(third, kBackgroundType),
                      TestDisplayItem(first, kForegroundType),
                      TestDisplayItem(second, kForegroundType),
                      TestDisplayItem(third, kForegroundType));

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
}

TEST_P(PaintControllerTest, IncrementalRasterInvalidation) {
  LayoutRect initial_rect(100, 100, 100, 100);
  std::unique_ptr<FakeDisplayItemClient> clients[6];
  for (auto& client : clients)
    client = std::make_unique<FakeDisplayItemClient>("", initial_rect);
  GraphicsContext context(GetPaintController());

  InitRootChunk();

  for (auto& client : clients)
    DrawRect(context, *client, kBackgroundType, FloatRect(initial_rect));
  CommitAndFinishCycle();

  InitRootChunk();
  clients[0]->SetVisualRect(LayoutRect(100, 100, 150, 100));
  clients[1]->SetVisualRect(LayoutRect(100, 100, 100, 150));
  clients[2]->SetVisualRect(LayoutRect(100, 100, 150, 80));
  clients[3]->SetVisualRect(LayoutRect(100, 100, 80, 150));
  clients[4]->SetVisualRect(LayoutRect(100, 100, 150, 150));
  clients[5]->SetVisualRect(LayoutRect(100, 100, 80, 80));
  for (auto& client : clients) {
    client->Invalidate(PaintInvalidationReason::kIncremental);
    DrawRect(context, *client, kBackgroundType,
             FloatRect(client->VisualRect()));
  }
  CommitAndFinishCycle();

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
}

TEST_P(PaintControllerTest, UpdateAddFirstOverlap) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 150, 150));
  FakeDisplayItemClient second("second", LayoutRect(200, 200, 50, 50));
  GraphicsContext context(GetPaintController());
  InitRootChunk();

  DrawRect(context, second, kBackgroundType, FloatRect(200, 200, 50, 50));
  DrawRect(context, second, kForegroundType, FloatRect(200, 200, 50, 50));
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(second, kForegroundType));

  InitRootChunk();

  first.Invalidate();
  second.Invalidate();
  second.SetVisualRect(LayoutRect(150, 250, 100, 100));
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 150, 150));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 150, 150));
  DrawRect(context, second, kBackgroundType, FloatRect(150, 250, 100, 100));
  DrawRect(context, second, kForegroundType, FloatRect(150, 250, 100, 100));
  EXPECT_EQ(0, NumCachedNewItems());
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType),
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(second, kForegroundType));

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());

  InitRootChunk();
  DrawRect(context, second, kBackgroundType, FloatRect(150, 250, 100, 100));
  DrawRect(context, second, kForegroundType, FloatRect(150, 250, 100, 100));

  EXPECT_EQ(2, NumCachedNewItems());
#if DCHECK_IS_ON()
  EXPECT_EQ(2, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(2, NumIndexedItems());
#endif

  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(second, kForegroundType));

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
}

TEST_P(PaintControllerTest, UpdateAddLastOverlap) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 150, 150));
  FakeDisplayItemClient second("second", LayoutRect(200, 200, 50, 50));
  GraphicsContext context(GetPaintController());
  InitRootChunk();

  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 150, 150));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 150, 150));
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType));

  InitRootChunk();

  first.Invalidate();
  first.SetVisualRect(LayoutRect(150, 150, 100, 100));
  second.Invalidate();
  DrawRect(context, first, kBackgroundType, FloatRect(150, 150, 100, 100));
  DrawRect(context, first, kForegroundType, FloatRect(150, 150, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(200, 200, 50, 50));
  DrawRect(context, second, kForegroundType, FloatRect(200, 200, 50, 50));
  EXPECT_EQ(0, NumCachedNewItems());
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType),
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(second, kForegroundType));

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());

  InitRootChunk();
  first.Invalidate();
  first.SetVisualRect(LayoutRect(100, 100, 150, 150));
  second.Invalidate();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 150, 150));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 150, 150));
  EXPECT_EQ(0, NumCachedNewItems());
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(first, kForegroundType));

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
}

TEST_P(PaintControllerTest, CachedDisplayItems) {
  FakeDisplayItemClient first("first");
  FakeDisplayItemClient second("second");
  GraphicsContext context(GetPaintController());
  InitRootChunk();

  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 150, 150));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 150, 150));
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(second, kBackgroundType));
  EXPECT_TRUE(ClientCacheIsValid(first));
  EXPECT_TRUE(ClientCacheIsValid(second));
  sk_sp<const PaintRecord> first_paint_record =
      static_cast<const DrawingDisplayItem&>(
          GetPaintController().GetDisplayItemList()[0])
          .GetPaintRecord();
  sk_sp<const PaintRecord> second_paint_record =
      static_cast<const DrawingDisplayItem&>(
          GetPaintController().GetDisplayItemList()[1])
          .GetPaintRecord();

  first.Invalidate();
  EXPECT_FALSE(ClientCacheIsValid(first));
  EXPECT_TRUE(ClientCacheIsValid(second));

  InitRootChunk();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 150, 150));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 150, 150));
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(second, kBackgroundType));
  // The first display item should be updated.
  EXPECT_NE(first_paint_record,
            static_cast<const DrawingDisplayItem&>(
                GetPaintController().GetDisplayItemList()[0])
                .GetPaintRecord());
  // The second display item should be cached.
  if (!RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    EXPECT_EQ(second_paint_record,
              static_cast<const DrawingDisplayItem&>(
                  GetPaintController().GetDisplayItemList()[1])
                  .GetPaintRecord());
  }
  EXPECT_TRUE(ClientCacheIsValid(first));
  EXPECT_TRUE(ClientCacheIsValid(second));

  InvalidateAll();
  EXPECT_FALSE(ClientCacheIsValid(first));
  EXPECT_FALSE(ClientCacheIsValid(second));
}

TEST_P(PaintControllerTest, UpdateSwapOrderWithChildren) {
  FakeDisplayItemClient container1("container1",
                                   LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2",
                                   LayoutRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", LayoutRect(100, 200, 50, 200));
  GraphicsContext context(GetPaintController());
  InitRootChunk();

  DrawRect(context, container1, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, content1, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, container1, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, container2, kBackgroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, content2, kForegroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, container2, kForegroundType, FloatRect(100, 200, 100, 100));
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 8,
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType),
                      TestDisplayItem(container1, kForegroundType),
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(container2, kForegroundType));

  InitRootChunk();

  // Simulate the situation when |container1| gets a z-index that is greater
  // than that of |container2|.
  DrawRect(context, container2, kBackgroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, content2, kForegroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, container2, kForegroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, container1, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, content1, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, container1, kForegroundType, FloatRect(100, 100, 100, 100));
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 8,
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(container2, kForegroundType),
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType),
                      TestDisplayItem(container1, kForegroundType));

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
}

TEST_P(PaintControllerTest, UpdateSwapOrderWithChildrenAndInvalidation) {
  FakeDisplayItemClient container1("container1",
                                   LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2",
                                   LayoutRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", LayoutRect(100, 200, 50, 200));
  GraphicsContext context(GetPaintController());
  InitRootChunk();

  DrawRect(context, container1, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, content1, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, container1, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, container2, kBackgroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, content2, kForegroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, container2, kForegroundType, FloatRect(100, 200, 100, 100));
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 8,
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType),
                      TestDisplayItem(container1, kForegroundType),
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(container2, kForegroundType));

  InitRootChunk();

  // Simulate the situation when |container1| gets a z-index that is greater
  // than that of |container2|, and |container1| is invalidated.
  container1.Invalidate();
  DrawRect(context, container2, kBackgroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, content2, kForegroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, container2, kForegroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, container1, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, content1, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, container1, kForegroundType, FloatRect(100, 100, 100, 100));
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 8,
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(container2, kForegroundType),
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType),
                      TestDisplayItem(container1, kForegroundType));

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
}

TEST_P(PaintControllerTest, CachedSubsequenceForcePaintChunk) {
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled())
    return;

  GraphicsContext context(GetPaintController());

  FakeDisplayItemClient root("root");
  auto root_properties = DefaultPaintChunkProperties();
  PaintChunk::Id root_id(root, DisplayItem::kCaret);
  GetPaintController().UpdateCurrentPaintChunkProperties(root_id,
                                                         root_properties);
  DrawRect(context, root, kBackgroundType, FloatRect(100, 100, 100, 100));

  FakeDisplayItemClient container("container");
  {
    auto container_properties = DefaultPaintChunkProperties();
    PaintChunk::Id container_id(container, DisplayItem::kCaret);

    SubsequenceRecorder r(context, container);
    GetPaintController().UpdateCurrentPaintChunkProperties(
        container_id, container_properties);
    DrawRect(context, container, kBackgroundType,
             FloatRect(100, 100, 100, 100));
    DrawRect(context, container, kForegroundType,
             FloatRect(100, 100, 100, 100));
  }

  DrawRect(context, root, kForegroundType, FloatRect(100, 100, 100, 100));

  CommitAndFinishCycle();

  // Even though the paint properties match, |container| should receive its
  // own PaintChunk because it created a subsequence.
  EXPECT_EQ(3u, GetPaintController().GetPaintArtifact().PaintChunks().size());
  EXPECT_EQ(root,
            GetPaintController().GetPaintArtifact().PaintChunks()[0].id.client);
  EXPECT_EQ(container,
            GetPaintController().GetPaintArtifact().PaintChunks()[1].id.client);
  EXPECT_EQ(root,
            GetPaintController().GetPaintArtifact().PaintChunks()[2].id.client);

  GetPaintController().UpdateCurrentPaintChunkProperties(root_id,
                                                         root_properties);
  DrawRect(context, root, kBackgroundType, FloatRect(100, 100, 100, 100));
  EXPECT_TRUE(GetPaintController().UseCachedSubsequenceIfPossible(container));
  DrawRect(context, root, kForegroundType, FloatRect(100, 100, 100, 100));
  CommitAndFinishCycle();

  // |container| should still receive its own PaintChunk because it is a cached
  // subsequence.
  EXPECT_EQ(3u, GetPaintController().GetPaintArtifact().PaintChunks().size());
  EXPECT_EQ(root,
            GetPaintController().GetPaintArtifact().PaintChunks()[0].id.client);
  EXPECT_EQ(container,
            GetPaintController().GetPaintArtifact().PaintChunks()[1].id.client);
  EXPECT_EQ(root,
            GetPaintController().GetPaintArtifact().PaintChunks()[2].id.client);
}

TEST_P(PaintControllerTest, CachedSubsequenceSwapOrder) {
  FakeDisplayItemClient container1("container1",
                                   LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2",
                                   LayoutRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", LayoutRect(100, 200, 50, 200));
  GraphicsContext context(GetPaintController());

  auto container1_effect = CreateOpacityEffect(e0(), 0.5);
  auto container1_properties = DefaultPaintChunkProperties();
  container1_properties.SetEffect(container1_effect.get());

  auto container2_effect = CreateOpacityEffect(e0(), 0.5);
  auto container2_properties = DefaultPaintChunkProperties();
  container2_properties.SetEffect(container2_effect.get());

  {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        PaintChunk::Id(container1, kBackgroundType), container1_properties);

    SubsequenceRecorder r(context, container1);
    DrawRect(context, container1, kBackgroundType,
             FloatRect(100, 100, 100, 100));
    DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
    DrawRect(context, content1, kForegroundType, FloatRect(100, 100, 50, 200));
    DrawRect(context, container1, kForegroundType,
             FloatRect(100, 100, 100, 100));
  }
  {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        PaintChunk::Id(container2, kBackgroundType), container2_properties);

    SubsequenceRecorder r(context, container2);
    DrawRect(context, container2, kBackgroundType,
             FloatRect(100, 200, 100, 100));
    DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
    DrawRect(context, content2, kForegroundType, FloatRect(100, 200, 50, 200));
    DrawRect(context, container2, kForegroundType,
             FloatRect(100, 200, 100, 100));
  }
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 8,
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType),
                      TestDisplayItem(container1, kForegroundType),

                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(container2, kForegroundType));

  auto* markers = GetSubsequenceMarkers(container1);
  CHECK(markers);
  EXPECT_EQ(0u, markers->start);
  EXPECT_EQ(4u, markers->end);

  markers = GetSubsequenceMarkers(container2);
  CHECK(markers);
  EXPECT_EQ(4u, markers->start);
  EXPECT_EQ(8u, markers->end);

  EXPECT_EQ(2u, GetPaintController().PaintChunks().size());
  EXPECT_EQ(PaintChunk::Id(container1, kBackgroundType),
            GetPaintController().PaintChunks()[0].id);
  EXPECT_EQ(PaintChunk::Id(container2, kBackgroundType),
            GetPaintController().PaintChunks()[1].id);

  // Simulate the situation when |container1| gets a z-index that is greater
  // than that of |container2|.
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    // When under-invalidation-checking is enabled,
    // useCachedSubsequenceIfPossible is forced off, and the client is expected
    // to create the same painting as in the previous paint.
    EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container2));
    {
      PaintChunk::Id id(container2, kBackgroundType);
      GetPaintController().UpdateCurrentPaintChunkProperties(
          id, container2_properties);

      SubsequenceRecorder r(context, container2);
      DrawRect(context, container2, kBackgroundType,
               FloatRect(100, 200, 100, 100));
      DrawRect(context, content2, kBackgroundType,
               FloatRect(100, 200, 50, 200));
      DrawRect(context, content2, kForegroundType,
               FloatRect(100, 200, 50, 200));
      DrawRect(context, container2, kForegroundType,
               FloatRect(100, 200, 100, 100));
    }
    EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container1));
    {
      PaintChunk::Id id(container1, kBackgroundType);
      GetPaintController().UpdateCurrentPaintChunkProperties(
          id, container1_properties);

      SubsequenceRecorder r(context, container1);
      DrawRect(context, container1, kBackgroundType,
               FloatRect(100, 100, 100, 100));
      DrawRect(context, content1, kBackgroundType,
               FloatRect(100, 100, 50, 200));
      DrawRect(context, content1, kForegroundType,
               FloatRect(100, 100, 50, 200));
      DrawRect(context, container1, kForegroundType,
               FloatRect(100, 100, 100, 100));
    }
  } else {
    EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container2));
    EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container1));
  }

  EXPECT_EQ(8, NumCachedNewItems());
#if DCHECK_IS_ON()
  EXPECT_EQ(0, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(0, NumIndexedItems());
#endif

  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 8,
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(container2, kForegroundType),
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType),
                      TestDisplayItem(container1, kForegroundType));

  markers = GetSubsequenceMarkers(container2);
  CHECK(markers);
  EXPECT_EQ(0u, markers->start);
  EXPECT_EQ(4u, markers->end);

  markers = GetSubsequenceMarkers(container1);
  CHECK(markers);
  EXPECT_EQ(4u, markers->start);
  EXPECT_EQ(8u, markers->end);

  EXPECT_EQ(2u, GetPaintController().PaintChunks().size());
  EXPECT_EQ(PaintChunk::Id(container2, kBackgroundType),
            GetPaintController().PaintChunks()[0].id);
  EXPECT_EQ(PaintChunk::Id(container1, kBackgroundType),
            GetPaintController().PaintChunks()[1].id);
}

TEST_P(PaintControllerTest, CachedSubsequenceAndDisplayItemsSwapOrder) {
  FakeDisplayItemClient root("root", LayoutRect(0, 0, 300, 300));
  FakeDisplayItemClient content1("content1", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2",
                                   LayoutRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", LayoutRect(100, 200, 50, 200));
  GraphicsContext context(GetPaintController());

  InitRootChunk();

  DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  {
    SubsequenceRecorder r(context, container2);
    DrawRect(context, container2, kBackgroundType,
             FloatRect(100, 200, 100, 100));
    DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
    DrawRect(context, content2, kForegroundType, FloatRect(100, 200, 50, 200));
    DrawRect(context, container2, kForegroundType,
             FloatRect(100, 200, 100, 100));
  }
  DrawRect(context, content1, kForegroundType, FloatRect(100, 100, 50, 200));
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(container2, kForegroundType),
                      TestDisplayItem(content1, kForegroundType));

  auto* markers = GetSubsequenceMarkers(container2);
  CHECK(markers);
  EXPECT_EQ(1u, markers->start);
  EXPECT_EQ(5u, markers->end);

  // Simulate the situation when |container2| gets a z-index that is smaller
  // than that of |content1|.
  InitRootChunk();
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    // When under-invalidation-checking is enabled,
    // useCachedSubsequenceIfPossible is forced off, and the client is expected
    // to create the same painting as in the previous paint.
    EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container2));
    {
      SubsequenceRecorder r(context, container2);
      DrawRect(context, container2, kBackgroundType,
               FloatRect(100, 200, 100, 100));
      DrawRect(context, content2, kBackgroundType,
               FloatRect(100, 200, 50, 200));
      DrawRect(context, content2, kForegroundType,
               FloatRect(100, 200, 50, 200));
      DrawRect(context, container2, kForegroundType,
               FloatRect(100, 200, 100, 100));
    }
    DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
    DrawRect(context, content1, kForegroundType, FloatRect(100, 100, 50, 200));
  } else {
    EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container2));
    EXPECT_TRUE(DrawingRecorder::UseCachedDrawingIfPossible(context, content1,
                                                            kBackgroundType));
    EXPECT_TRUE(DrawingRecorder::UseCachedDrawingIfPossible(context, content1,
                                                            kForegroundType));
  }

  EXPECT_EQ(6, NumCachedNewItems());
#if DCHECK_IS_ON()
  EXPECT_EQ(2, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(0, NumIndexedItems());
#endif

  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(container2, kForegroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType));

  markers = GetSubsequenceMarkers(container2);
  CHECK(markers);
  EXPECT_EQ(0u, markers->start);
  EXPECT_EQ(4u, markers->end);
}

TEST_P(PaintControllerTest, CachedSubsequenceContainingFragments) {
  GraphicsContext context(GetPaintController());
  FakeDisplayItemClient root("root");
  constexpr size_t kFragmentCount = 3;
  FakeDisplayItemClient container("container");

  // The first paint.
  auto paint_container = [this, &context, &container]() {
    SubsequenceRecorder r(context, container);
    for (size_t i = 0; i < kFragmentCount; ++i) {
      ScopedDisplayItemFragment scoped_fragment(context, i);
      ScopedPaintChunkProperties content_chunk_properties(
          GetPaintController(), DefaultPaintChunkProperties(), container,
          kBackgroundType);
      DrawRect(context, container, kBackgroundType,
               FloatRect(100, 100, 100, 100));
    }
  };
  {
    ScopedPaintChunkProperties root_chunk_properties(
        GetPaintController(), DefaultPaintChunkProperties(), root,
        kBackgroundType);
    DrawRect(context, root, kBackgroundType, FloatRect(100, 100, 100, 100));
    paint_container();
    DrawRect(context, root, kForegroundType, FloatRect(100, 100, 100, 100));
  }
  CommitAndFinishCycle();

  auto check_paint_results = [this, &root, &container]() {
    const auto& chunks = GetPaintController().PaintChunks();
    EXPECT_EQ(chunks.size(), 3u);
    EXPECT_EQ(chunks[0], PaintChunk(0, 1, PaintChunk::Id(root, kBackgroundType),
                                    DefaultPaintChunkProperties()));
    // One chunk for all of the fragments because they have the
    // same properties.
    EXPECT_EQ(chunks[1],
              PaintChunk(1, 4, PaintChunk::Id(container, kBackgroundType),
                         DefaultPaintChunkProperties()));
    EXPECT_EQ(chunks[2], PaintChunk(4, 5, PaintChunk::Id(root, kForegroundType),
                                    DefaultPaintChunkProperties()));
  };
  // Check results of the first paint.
  check_paint_results();

  // The second paint.
  {
    ScopedPaintChunkProperties root_chunk_properties(
        GetPaintController(), DefaultPaintChunkProperties(), root,
        kBackgroundType);
    DrawRect(context, root, kBackgroundType, FloatRect(100, 100, 100, 100));

    if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
      EXPECT_FALSE(
          GetPaintController().UseCachedSubsequenceIfPossible(container));
      paint_container();
    } else {
      EXPECT_TRUE(
          GetPaintController().UseCachedSubsequenceIfPossible(container));
    }
    DrawRect(context, root, kForegroundType, FloatRect(100, 100, 100, 100));
  }
  CommitAndFinishCycle();

  // The second paint should produce the exactly same results.
  check_paint_results();
}

TEST_P(PaintControllerTest, UpdateSwapOrderCrossingChunks) {
  FakeDisplayItemClient container1("container1",
                                   LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2",
                                   LayoutRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", LayoutRect(100, 200, 50, 200));
  GraphicsContext context(GetPaintController());

  auto container1_effect = CreateOpacityEffect(e0(), 0.5);
  auto container1_properties = DefaultPaintChunkProperties();
  container1_properties.SetEffect(container1_effect.get());

  auto container2_effect = CreateOpacityEffect(e0(), 0.5);
  auto container2_properties = DefaultPaintChunkProperties();
  container2_properties.SetEffect(container2_effect.get());

  GetPaintController().UpdateCurrentPaintChunkProperties(
      PaintChunk::Id(container1, kBackgroundType), container1_properties);
  DrawRect(context, container1, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  GetPaintController().UpdateCurrentPaintChunkProperties(
      PaintChunk::Id(container2, kBackgroundType), container2_properties);
  DrawRect(context, container2, kBackgroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType));

  EXPECT_EQ(2u, GetPaintController().PaintChunks().size());
  EXPECT_EQ(PaintChunk::Id(container1, kBackgroundType),
            GetPaintController().PaintChunks()[0].id);
  EXPECT_EQ(PaintChunk::Id(container2, kBackgroundType),
            GetPaintController().PaintChunks()[1].id);

  // Move content2 into container1, without invalidation.
  GetPaintController().UpdateCurrentPaintChunkProperties(
      PaintChunk::Id(container1, kBackgroundType), container1_properties);
  DrawRect(context, container1, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
  GetPaintController().UpdateCurrentPaintChunkProperties(
      PaintChunk::Id(container2, kBackgroundType), container2_properties);
  DrawRect(context, container2, kBackgroundType, FloatRect(100, 200, 100, 100));

  EXPECT_EQ(4, NumCachedNewItems());
#if DCHECK_IS_ON()
  EXPECT_EQ(3, NumSequentialMatches());
  EXPECT_EQ(1, NumOutOfOrderMatches());
  EXPECT_EQ(1, NumIndexedItems());
#endif

  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType),
                      TestDisplayItem(container2, kBackgroundType));

  EXPECT_EQ(2u, GetPaintController().PaintChunks().size());
  EXPECT_EQ(PaintChunk::Id(container1, kBackgroundType),
            GetPaintController().PaintChunks()[0].id);
  EXPECT_EQ(PaintChunk::Id(container2, kBackgroundType),
            GetPaintController().PaintChunks()[1].id);
}

TEST_P(PaintControllerTest, OutOfOrderNoCrash) {
  FakeDisplayItemClient client("client");
  GraphicsContext context(GetPaintController());

  const DisplayItem::Type kType1 = DisplayItem::kDrawingFirst;
  const DisplayItem::Type kType2 =
      static_cast<DisplayItem::Type>(DisplayItem::kDrawingFirst + 1);
  const DisplayItem::Type kType3 =
      static_cast<DisplayItem::Type>(DisplayItem::kDrawingFirst + 2);
  const DisplayItem::Type kType4 =
      static_cast<DisplayItem::Type>(DisplayItem::kDrawingFirst + 3);

  InitRootChunk();
  DrawRect(context, client, kType1, FloatRect(100, 100, 100, 100));
  DrawRect(context, client, kType2, FloatRect(100, 100, 50, 200));
  DrawRect(context, client, kType3, FloatRect(100, 100, 50, 200));
  DrawRect(context, client, kType4, FloatRect(100, 100, 100, 100));

  CommitAndFinishCycle();

  InitRootChunk();
  DrawRect(context, client, kType2, FloatRect(100, 100, 50, 200));
  DrawRect(context, client, kType3, FloatRect(100, 100, 50, 200));
  DrawRect(context, client, kType1, FloatRect(100, 100, 100, 100));
  DrawRect(context, client, kType4, FloatRect(100, 100, 100, 100));

  CommitAndFinishCycle();
}

TEST_P(PaintControllerTest, CachedNestedSubsequenceUpdate) {
  FakeDisplayItemClient container1("container1",
                                   LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", LayoutRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2",
                                   LayoutRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", LayoutRect(100, 200, 50, 200));
  GraphicsContext context(GetPaintController());

  auto container1_effect = CreateOpacityEffect(e0(), 0.5);
  auto container1_background_properties = DefaultPaintChunkProperties();
  container1_background_properties.SetEffect(container1_effect.get());
  auto container1_foreground_properties = DefaultPaintChunkProperties();
  container1_foreground_properties.SetEffect(container1_effect.get());

  auto content1_effect = CreateOpacityEffect(e0(), 0.6);
  auto content1_properties = DefaultPaintChunkProperties();
  content1_properties.SetEffect(content1_effect.get());

  auto container2_effect = CreateOpacityEffect(e0(), 0.7);
  auto container2_background_properties = DefaultPaintChunkProperties();
  container2_background_properties.SetEffect(container2_effect.get());

  auto content2_effect = CreateOpacityEffect(e0(), 0.8);
  auto content2_properties = DefaultPaintChunkProperties();
  content2_properties.SetEffect(content2_effect.get());

  {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        PaintChunk::Id(container1, kBackgroundType),
        container1_background_properties);
    SubsequenceRecorder r(context, container1);
    DrawRect(context, container1, kBackgroundType,
             FloatRect(100, 100, 100, 100));

    {
      GetPaintController().UpdateCurrentPaintChunkProperties(
          PaintChunk::Id(content1, kBackgroundType), content1_properties);
      SubsequenceRecorder r(context, content1);
      DrawRect(context, content1, kBackgroundType,
               FloatRect(100, 100, 50, 200));
      DrawRect(context, content1, kForegroundType,
               FloatRect(100, 100, 50, 200));
    }
    GetPaintController().UpdateCurrentPaintChunkProperties(
        PaintChunk::Id(container1, kForegroundType),
        container1_foreground_properties);
    DrawRect(context, container1, kForegroundType,
             FloatRect(100, 100, 100, 100));
  }
  {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        PaintChunk::Id(container2, kBackgroundType),
        container2_background_properties);
    SubsequenceRecorder r(context, container2);
    DrawRect(context, container2, kBackgroundType,
             FloatRect(100, 200, 100, 100));
    {
      GetPaintController().UpdateCurrentPaintChunkProperties(
          PaintChunk::Id(content2, kBackgroundType), content2_properties);
      SubsequenceRecorder r(context, content2);
      DrawRect(context, content2, kBackgroundType,
               FloatRect(100, 200, 50, 200));
    }
  }
  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 6,
                      TestDisplayItem(container1, kBackgroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType),
                      TestDisplayItem(container1, kForegroundType),
                      TestDisplayItem(container2, kBackgroundType),
                      TestDisplayItem(content2, kBackgroundType));

  auto* markers = GetSubsequenceMarkers(container1);
  CHECK(markers);
  EXPECT_EQ(0u, markers->start);
  EXPECT_EQ(4u, markers->end);

  markers = GetSubsequenceMarkers(content1);
  CHECK(markers);
  EXPECT_EQ(1u, markers->start);
  EXPECT_EQ(3u, markers->end);

  markers = GetSubsequenceMarkers(container2);
  CHECK(markers);
  EXPECT_EQ(4u, markers->start);
  EXPECT_EQ(6u, markers->end);

  markers = GetSubsequenceMarkers(content2);
  CHECK(markers);
  EXPECT_EQ(5u, markers->start);
  EXPECT_EQ(6u, markers->end);

  EXPECT_EQ(5u, GetPaintController().PaintChunks().size());
  EXPECT_EQ(PaintChunk::Id(container1, kBackgroundType),
            GetPaintController().PaintChunks()[0].id);
  EXPECT_EQ(PaintChunk::Id(content1, kBackgroundType),
            GetPaintController().PaintChunks()[1].id);
  EXPECT_EQ(PaintChunk::Id(container1, kForegroundType),
            GetPaintController().PaintChunks()[2].id);
  EXPECT_EQ(PaintChunk::Id(container2, kBackgroundType),
            GetPaintController().PaintChunks()[3].id);
  EXPECT_EQ(PaintChunk::Id(content2, kBackgroundType),
            GetPaintController().PaintChunks()[4].id);

  // Invalidate container1 but not content1.
  container1.Invalidate();

  // Container2 itself now becomes empty (but still has the 'content2' child),
  // and chooses not to output subsequence info.

  container2.Invalidate();
  content2.Invalidate();
  EXPECT_FALSE(
      SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, container2));
  EXPECT_FALSE(
      SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, content2));
  // Content2 now outputs foreground only.
  {
    GetPaintController().UpdateCurrentPaintChunkProperties(
        PaintChunk::Id(content2, kForegroundType), content2_properties);
    SubsequenceRecorder r(context, content2);
    DrawRect(context, content2, kForegroundType, FloatRect(100, 200, 50, 200));
  }
  // Repaint container1 with foreground only.
  {
    EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container1));
    SubsequenceRecorder r(context, container1);
    // Use cached subsequence of content1.
    if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
      // When under-invalidation-checking is enabled,
      // useCachedSubsequenceIfPossible is forced off, and the client is
      // expected to create the same painting as in the previous paint.
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, content1));
        GetPaintController().UpdateCurrentPaintChunkProperties(
            PaintChunk::Id(content1, kBackgroundType), content1_properties);
      SubsequenceRecorder r(context, content1);
      DrawRect(context, content1, kBackgroundType,
               FloatRect(100, 100, 50, 200));
      DrawRect(context, content1, kForegroundType,
               FloatRect(100, 100, 50, 200));
    } else {
      EXPECT_TRUE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, content1));
    }
    GetPaintController().UpdateCurrentPaintChunkProperties(
        PaintChunk::Id(container1, kForegroundType),
        container1_foreground_properties);
    DrawRect(context, container1, kForegroundType,
             FloatRect(100, 100, 100, 100));
  }

  EXPECT_EQ(2, NumCachedNewItems());
#if DCHECK_IS_ON()
  EXPECT_EQ(0, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(0, NumIndexedItems());
#endif

  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(content2, kForegroundType),
                      TestDisplayItem(content1, kBackgroundType),
                      TestDisplayItem(content1, kForegroundType),
                      TestDisplayItem(container1, kForegroundType));

  markers = GetSubsequenceMarkers(content2);
  CHECK(markers);
  EXPECT_EQ(0u, markers->start);
  EXPECT_EQ(1u, markers->end);

  markers = GetSubsequenceMarkers(container1);
  CHECK(markers);
  EXPECT_EQ(1u, markers->start);
  EXPECT_EQ(4u, markers->end);

  markers = GetSubsequenceMarkers(content1);
  CHECK(markers);
  EXPECT_EQ(1u, markers->start);
  EXPECT_EQ(3u, markers->end);

  EXPECT_EQ(3u, GetPaintController().PaintChunks().size());
  EXPECT_EQ(PaintChunk::Id(content2, kForegroundType),
            GetPaintController().PaintChunks()[0].id);
  EXPECT_EQ(PaintChunk::Id(content1, kBackgroundType),
            GetPaintController().PaintChunks()[1].id);
  EXPECT_EQ(PaintChunk::Id(container1, kForegroundType),
            GetPaintController().PaintChunks()[2].id);
}

TEST_P(PaintControllerTest, SkipCache) {
  FakeDisplayItemClient multicol("multicol", LayoutRect(100, 100, 200, 200));
  FakeDisplayItemClient content("content", LayoutRect(100, 100, 100, 100));
  GraphicsContext context(GetPaintController());
  InitRootChunk();

  FloatRect rect1(100, 100, 50, 50);
  FloatRect rect2(150, 100, 50, 50);
  FloatRect rect3(200, 100, 50, 50);

  DrawRect(context, multicol, kBackgroundType, FloatRect(100, 200, 100, 100));

  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundType, rect1);
  DrawRect(context, content, kForegroundType, rect2);
  GetPaintController().EndSkippingCache();

  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(multicol, kBackgroundType),
                      TestDisplayItem(content, kForegroundType),
                      TestDisplayItem(content, kForegroundType));
  sk_sp<const PaintRecord> record1 =
      static_cast<const DrawingDisplayItem&>(
          GetPaintController().GetDisplayItemList()[1])
          .GetPaintRecord();
  sk_sp<const PaintRecord> record2 =
      static_cast<const DrawingDisplayItem&>(
          GetPaintController().GetDisplayItemList()[2])
          .GetPaintRecord();
  EXPECT_NE(record1, record2);

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());

  InitRootChunk();
  // Draw again with nothing invalidated.
  EXPECT_TRUE(ClientCacheIsValid(multicol));
  DrawRect(context, multicol, kBackgroundType, FloatRect(100, 200, 100, 100));

  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundType, rect1);
  DrawRect(context, content, kForegroundType, rect2);
  GetPaintController().EndSkippingCache();

  EXPECT_EQ(1, NumCachedNewItems());
#if DCHECK_IS_ON()
  EXPECT_EQ(1, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(0, NumIndexedItems());
#endif

  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(multicol, kBackgroundType),
                      TestDisplayItem(content, kForegroundType),
                      TestDisplayItem(content, kForegroundType));
  EXPECT_NE(record1, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().GetDisplayItemList()[1])
                         .GetPaintRecord());
  EXPECT_NE(record2, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().GetDisplayItemList()[2])
                         .GetPaintRecord());

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());

  InitRootChunk();
  // Now the multicol becomes 3 columns and repaints.
  multicol.Invalidate();
  DrawRect(context, multicol, kBackgroundType, FloatRect(100, 100, 100, 100));

  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundType, rect1);
  DrawRect(context, content, kForegroundType, rect2);
  DrawRect(context, content, kForegroundType, rect3);
  GetPaintController().EndSkippingCache();

  // We should repaint everything on invalidation of the scope container.
  EXPECT_DISPLAY_LIST(GetPaintController().NewDisplayItemList(), 4,
                      TestDisplayItem(multicol, kBackgroundType),
                      TestDisplayItem(content, kForegroundType),
                      TestDisplayItem(content, kForegroundType),
                      TestDisplayItem(content, kForegroundType));
  EXPECT_NE(record1, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().NewDisplayItemList()[1])
                         .GetPaintRecord());
  EXPECT_NE(record2, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().NewDisplayItemList()[2])
                         .GetPaintRecord());

  CommitAndFinishCycle();

  EXPECT_EQ(1u, GetPaintController().PaintChunks().size());
}

TEST_P(PaintControllerTest, PartialSkipCache) {
  FakeDisplayItemClient content("content");
  GraphicsContext context(GetPaintController());

  FloatRect rect1(100, 100, 50, 50);
  FloatRect rect2(150, 100, 50, 50);
  FloatRect rect3(200, 100, 50, 50);

  InitRootChunk();
  DrawRect(context, content, kBackgroundType, rect1);
  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundType, rect2);
  GetPaintController().EndSkippingCache();
  DrawRect(context, content, kForegroundType, rect3);

  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(content, kBackgroundType),
                      TestDisplayItem(content, kForegroundType),
                      TestDisplayItem(content, kForegroundType));
  sk_sp<const PaintRecord> record0 =
      static_cast<const DrawingDisplayItem&>(
          GetPaintController().GetDisplayItemList()[0])
          .GetPaintRecord();
  sk_sp<const PaintRecord> record1 =
      static_cast<const DrawingDisplayItem&>(
          GetPaintController().GetDisplayItemList()[1])
          .GetPaintRecord();
  sk_sp<const PaintRecord> record2 =
      static_cast<const DrawingDisplayItem&>(
          GetPaintController().GetDisplayItemList()[2])
          .GetPaintRecord();
  EXPECT_NE(record1, record2);

  // Content's cache is invalid because it has display items skipped cache.
  EXPECT_FALSE(ClientCacheIsValid(content));
  EXPECT_EQ(PaintInvalidationReason::kUncacheable,
            content.GetPaintInvalidationReason());

  InitRootChunk();
  // Draw again with nothing invalidated.
  DrawRect(context, content, kBackgroundType, rect1);
  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundType, rect2);
  GetPaintController().EndSkippingCache();
  DrawRect(context, content, kForegroundType, rect3);

  EXPECT_EQ(0, NumCachedNewItems());
#if DCHECK_IS_ON()
  EXPECT_EQ(0, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  EXPECT_EQ(0, NumIndexedItems());
#endif

  CommitAndFinishCycle();

  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 3,
                      TestDisplayItem(content, kBackgroundType),
                      TestDisplayItem(content, kForegroundType),
                      TestDisplayItem(content, kForegroundType));
  EXPECT_NE(record0, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().GetDisplayItemList()[0])
                         .GetPaintRecord());
  EXPECT_NE(record1, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().GetDisplayItemList()[1])
                         .GetPaintRecord());
  EXPECT_NE(record2, static_cast<const DrawingDisplayItem&>(
                         GetPaintController().GetDisplayItemList()[2])
                         .GetPaintRecord());
}


TEST_P(PaintControllerTest, SmallPaintControllerHasOnePaintChunk) {
  FakeDisplayItemClient client("test client");

  InitRootChunk();
  GraphicsContext context(GetPaintController());
  DrawRect(context, client, kBackgroundType, FloatRect(0, 0, 100, 100));

  CommitAndFinishCycle();
  const auto& paint_chunks = GetPaintController().PaintChunks();
  ASSERT_EQ(1u, paint_chunks.size());
  EXPECT_EQ(0u, paint_chunks[0].begin_index);
  EXPECT_EQ(1u, paint_chunks[0].end_index);
}

void DrawPath(GraphicsContext& context,
              DisplayItemClient& client,
              DisplayItem::Type type,
              unsigned count) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, client, type))
    return;

  DrawingRecorder recorder(context, client, type);
  SkPath path;
  path.moveTo(0, 0);
  path.lineTo(0, 100);
  path.lineTo(50, 50);
  path.lineTo(100, 100);
  path.lineTo(100, 0);
  path.close();
  PaintFlags flags;
  flags.setAntiAlias(true);
  for (unsigned i = 0; i < count; i++)
    context.DrawPath(path, flags);
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

TEST_P(PaintControllerTest, InvalidateAll) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  EXPECT_TRUE(GetPaintController().CacheIsAllInvalid());
  CommitAndFinishCycle();
  EXPECT_TRUE(GetPaintController().GetPaintArtifact().IsEmpty());
  EXPECT_FALSE(GetPaintController().CacheIsAllInvalid());

  InvalidateAll();
  EXPECT_TRUE(GetPaintController().CacheIsAllInvalid());
  CommitAndFinishCycle();
  EXPECT_TRUE(GetPaintController().GetPaintArtifact().IsEmpty());
  EXPECT_FALSE(GetPaintController().CacheIsAllInvalid());

  FakeDisplayItemClient client("client", LayoutRect(1, 2, 3, 4));
  GraphicsContext context(GetPaintController());

  InitRootChunk();
  DrawRect(context, client, kBackgroundType, FloatRect(1, 2, 3, 4));
  CommitAndFinishCycle();
  EXPECT_FALSE(GetPaintController().GetPaintArtifact().IsEmpty());
  EXPECT_FALSE(GetPaintController().CacheIsAllInvalid());

  InvalidateAll();
  EXPECT_TRUE(GetPaintController().GetPaintArtifact().IsEmpty());
  EXPECT_TRUE(GetPaintController().CacheIsAllInvalid());
}

TEST_P(PaintControllerTest, InsertValidItemInFront) {
  FakeDisplayItemClient first("first", LayoutRect(100, 100, 300, 300));
  FakeDisplayItemClient second("second", LayoutRect(100, 100, 200, 200));
  FakeDisplayItemClient third("third", LayoutRect(100, 100, 100, 100));
  FakeDisplayItemClient fourth("fourth", LayoutRect(100, 100, 50, 50));
  GraphicsContext context(GetPaintController());

  InitRootChunk();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 200, 200));
  DrawRect(context, third, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, fourth, kBackgroundType, FloatRect(100, 100, 50, 50));

  EXPECT_EQ(0, NumCachedNewItems());
  CommitAndFinishCycle();
  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(third, kBackgroundType),
                      TestDisplayItem(fourth, kBackgroundType));
  EXPECT_TRUE(first.IsValid());
  EXPECT_TRUE(second.IsValid());
  EXPECT_TRUE(third.IsValid());
  EXPECT_TRUE(fourth.IsValid());

  // Simulate that a composited scrolling element is scrolled down, and "first"
  // and "second" are scrolled out of the interest rect.
  InitRootChunk();
  DrawRect(context, third, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, fourth, kBackgroundType, FloatRect(100, 100, 50, 50));

  EXPECT_EQ(2, NumCachedNewItems());
#if DCHECK_IS_ON()
  EXPECT_EQ(2, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  // We indexed "first" and "second" when finding the cached item for "third".
  EXPECT_EQ(2, NumIndexedItems());
#endif

  CommitAndFinishCycle();
  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(third, kBackgroundType),
                      TestDisplayItem(fourth, kBackgroundType));
  EXPECT_TRUE(first.IsValid());
  EXPECT_TRUE(second.IsValid());
  EXPECT_TRUE(third.IsValid());
  EXPECT_TRUE(fourth.IsValid());

  // Simulate "first" and "second" are scrolled back into the interest rect.
  InitRootChunk();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 200, 200));
  DrawRect(context, third, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, fourth, kBackgroundType, FloatRect(100, 100, 50, 50));

  EXPECT_EQ(2, NumCachedNewItems());
#if DCHECK_IS_ON()
  EXPECT_EQ(2, NumSequentialMatches());
  EXPECT_EQ(0, NumOutOfOrderMatches());
  // We indexed "third" and "fourth" when finding the cached item for "first".
  EXPECT_EQ(2, NumIndexedItems());
#endif

  CommitAndFinishCycle();
  EXPECT_DISPLAY_LIST(GetPaintController().GetDisplayItemList(), 4,
                      TestDisplayItem(first, kBackgroundType),
                      TestDisplayItem(second, kBackgroundType),
                      TestDisplayItem(third, kBackgroundType),
                      TestDisplayItem(fourth, kBackgroundType));
  EXPECT_TRUE(first.IsValid());
  EXPECT_TRUE(second.IsValid());
  EXPECT_TRUE(third.IsValid());
  EXPECT_TRUE(fourth.IsValid());
}

// Death tests don't work properly on Android.
#if defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)

TEST_P(PaintControllerTest, DuplicatedSubsequences) {
  FakeDisplayItemClient client("test", LayoutRect(100, 100, 100, 100));
  GraphicsContext context(GetPaintController());

  auto paint_duplicated_subsequences = [&]() {
    InitRootChunk();
    {
      SubsequenceRecorder r(context, client);
      DrawRect(context, client, kBackgroundType, FloatRect(100, 100, 100, 100));
    }
    {
      SubsequenceRecorder r(context, client);
      DrawRect(context, client, kForegroundType, FloatRect(100, 100, 100, 100));
    }
    CommitAndFinishCycle();
  };

#if DCHECK_IS_ON()
  EXPECT_DEATH(paint_duplicated_subsequences(),
               "Multiple subsequences for client: \"test\"");
  return;
#endif

  // The following is for non-DCHECK path. No security CHECK should trigger.
  paint_duplicated_subsequences();
  // Paint again.
  InitRootChunk();
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    EXPECT_FALSE(GetPaintController().UseCachedSubsequenceIfPossible(client));
    SubsequenceRecorder r(context, client);
    DrawRect(context, client, kBackgroundType, FloatRect(100, 100, 100, 100));
  } else {
    EXPECT_TRUE(GetPaintController().UseCachedSubsequenceIfPossible(client));
  }
  {
    // Should not use the cached duplicated subsequence.
    EXPECT_FALSE(GetPaintController().UseCachedSubsequenceIfPossible(client));
    SubsequenceRecorder r(context, client);
    DrawRect(context, client, kForegroundType, FloatRect(100, 100, 100, 100));
  }
  CommitAndFinishCycle();
}

class PaintControllerUnderInvalidationTest
    : public PaintControllerTestBase,
      private ScopedPaintUnderInvalidationCheckingForTest {
 public:
  PaintControllerUnderInvalidationTest()
      : PaintControllerTestBase(),
        ScopedPaintUnderInvalidationCheckingForTest(true) {}

 protected:
  void SetUp() override {
    testing::FLAGS_gtest_death_test_style = "threadsafe";
  }

  void TestChangeDrawing() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());

    InitRootChunk();
    first.SetVisualRect(LayoutRect(100, 100, 300, 300));
    DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
    DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));
    CommitAndFinishCycle();

    InitRootChunk();
    first.SetVisualRect(LayoutRect(200, 200, 300, 300));
    DrawRect(context, first, kBackgroundType, FloatRect(200, 200, 300, 300));
    DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));
    CommitAndFinishCycle();
  }

  void TestMoreDrawing() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());

    InitRootChunk();
    DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
    CommitAndFinishCycle();

    InitRootChunk();
    DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
    DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));
    CommitAndFinishCycle();
  }

  void TestLessDrawing() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());

    InitRootChunk();
    DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
    DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));
    CommitAndFinishCycle();

    InitRootChunk();
    DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
    CommitAndFinishCycle();
  }

  void TestChangeDrawingInSubsequence() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());
    InitRootChunk();
    {
      SubsequenceRecorder r(context, first);
      first.SetVisualRect(LayoutRect(100, 100, 300, 300));
      DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
      DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));
    }
    CommitAndFinishCycle();

    InitRootChunk();
    {
      EXPECT_FALSE(
          SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, first));
      SubsequenceRecorder r(context, first);
      first.SetVisualRect(LayoutRect(200, 200, 300, 300));
      DrawRect(context, first, kBackgroundType, FloatRect(200, 200, 300, 300));
      DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));
    }
    CommitAndFinishCycle();
  }

  void TestMoreDrawingInSubsequence() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());

    InitRootChunk();
    {
      SubsequenceRecorder r(context, first);
      DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
    }
    CommitAndFinishCycle();

    InitRootChunk();
    {
      EXPECT_FALSE(
          SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, first));
      SubsequenceRecorder r(context, first);
      DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
      DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));
    }
    CommitAndFinishCycle();
  }

  void TestLessDrawingInSubsequence() {
    FakeDisplayItemClient first("first");
    GraphicsContext context(GetPaintController());

    InitRootChunk();
    {
      SubsequenceRecorder r(context, first);
      DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
      DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));
    }
    CommitAndFinishCycle();

    InitRootChunk();
    {
      EXPECT_FALSE(
          SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, first));
      SubsequenceRecorder r(context, first);
      DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
    }
    CommitAndFinishCycle();
  }

  void TestInvalidationInSubsequence() {
    FakeDisplayItemClient container("container");
    FakeDisplayItemClient content("content");
    GraphicsContext context(GetPaintController());

    InitRootChunk();
    {
      SubsequenceRecorder r(context, container);
      DrawRect(context, content, kBackgroundType,
               FloatRect(100, 100, 300, 300));
    }
    CommitAndFinishCycle();

    content.Invalidate();
    InitRootChunk();
    // Leave container not invalidated.
    {
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container));
      SubsequenceRecorder r(context, container);
      DrawRect(context, content, kBackgroundType,
               FloatRect(100, 100, 300, 300));
    }
    CommitAndFinishCycle();
  }

  void TestSubsequenceBecomesEmpty() {
    FakeDisplayItemClient target("target");
    GraphicsContext context(GetPaintController());

    InitRootChunk();
    {
      SubsequenceRecorder r(context, target);
      DrawRect(context, target, kBackgroundType, FloatRect(100, 100, 300, 300));
    }
    CommitAndFinishCycle();

    InitRootChunk();
    {
      EXPECT_FALSE(
          SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, target));
      SubsequenceRecorder r(context, target);
    }
    CommitAndFinishCycle();
  }
};

TEST_F(PaintControllerUnderInvalidationTest, ChangeDrawing) {
  EXPECT_DEATH(TestChangeDrawing(), "under-invalidation: display item changed");
}

TEST_F(PaintControllerUnderInvalidationTest, MoreDrawing) {
  // We don't detect under-invalidation in this case, and PaintController can
  // also handle the case gracefully.
  TestMoreDrawing();
}

TEST_F(PaintControllerUnderInvalidationTest, LessDrawing) {
  // We don't detect under-invalidation in this case, and PaintController can
  // also handle the case gracefully.
  TestLessDrawing();
}

TEST_F(PaintControllerUnderInvalidationTest, ChangeDrawingInSubsequence) {
  EXPECT_DEATH(TestChangeDrawingInSubsequence(),
               "In cached subsequence for first.*"
               "under-invalidation: display item changed");
}

TEST_F(PaintControllerUnderInvalidationTest, MoreDrawingInSubsequence) {
  // TODO(wangxianzhu): Detect more drawings at the end of a subsequence.
  TestMoreDrawingInSubsequence();
}

TEST_F(PaintControllerUnderInvalidationTest, LessDrawingInSubsequence) {
  EXPECT_DEATH(TestLessDrawingInSubsequence(),
               "In cached subsequence for first.*"
               "under-invalidation: new subsequence wrong length");
}

TEST_F(PaintControllerUnderInvalidationTest, InvalidationInSubsequence) {
  // We allow invalidated display item clients as long as they would produce the
  // same display items. The cases of changed display items are tested by other
  // test cases.
  TestInvalidationInSubsequence();
}

TEST_F(PaintControllerUnderInvalidationTest, SubsequenceBecomesEmpty) {
  EXPECT_DEATH(TestSubsequenceBecomesEmpty(),
               "In cached subsequence for target.*"
               "under-invalidation: new subsequence wrong length");
}

TEST_F(PaintControllerUnderInvalidationTest, SkipCacheInSubsequence) {
  FakeDisplayItemClient container("container");
  FakeDisplayItemClient content("content");
  GraphicsContext context(GetPaintController());

  InitRootChunk();
  {
    SubsequenceRecorder r(context, container);
    {
      DisplayItemCacheSkipper cache_skipper(context);
      DrawRect(context, content, kBackgroundType,
               FloatRect(100, 100, 300, 300));
    }
    DrawRect(context, content, kForegroundType, FloatRect(200, 200, 400, 400));
  }
  CommitAndFinishCycle();

  InitRootChunk();
  {
    EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container));
    SubsequenceRecorder r(context, container);
    {
      DisplayItemCacheSkipper cache_skipper(context);
      DrawRect(context, content, kBackgroundType,
               FloatRect(200, 200, 400, 400));
    }
    DrawRect(context, content, kForegroundType, FloatRect(200, 200, 400, 400));
  }
  CommitAndFinishCycle();
}

TEST_F(PaintControllerUnderInvalidationTest,
       EmptySubsequenceInCachedSubsequence) {
  FakeDisplayItemClient container("container");
  FakeDisplayItemClient content("content");
  GraphicsContext context(GetPaintController());

  InitRootChunk();
  {
    SubsequenceRecorder r(context, container);
    DrawRect(context, container, kBackgroundType,
             FloatRect(100, 100, 300, 300));
    { SubsequenceRecorder r1(context, content); }
    DrawRect(context, container, kForegroundType,
             FloatRect(100, 100, 300, 300));
  }
  CommitAndFinishCycle();

  InitRootChunk();
  {
    EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
        context, container));
    SubsequenceRecorder r(context, container);
    DrawRect(context, container, kBackgroundType,
             FloatRect(100, 100, 300, 300));
    EXPECT_FALSE(
        SubsequenceRecorder::UseCachedSubsequenceIfPossible(context, content));
    { SubsequenceRecorder r1(context, content); }
    DrawRect(context, container, kForegroundType,
             FloatRect(100, 100, 300, 300));
  }
  CommitAndFinishCycle();
}

#endif  // defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)

}  // namespace blink
