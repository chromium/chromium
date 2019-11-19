// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/display_item_raster_invalidator.h"

#include "base/bind_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller_test.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/test_paint_artifact.h"

namespace blink {

using ::testing::UnorderedElementsAre;

class DisplayItemRasterInvalidatorTest : public PaintControllerTestBase,
                                         public PaintTestConfigurations {
 protected:
  DisplayItemRasterInvalidatorTest() : invalidator_(base::DoNothing()) {}

  Vector<RasterInvalidationInfo> GenerateRasterInvalidations() {
    GetPaintController().CommitNewDisplayItems();
    invalidator_.Generate(
        GetPaintController().GetPaintArtifactShared(),
        // The layer rect is big enough not to clip display item raster
        // invalidation rects.
        IntRect(0, 0, 20000, 20000), PropertyTreeState::Root());
    GetPaintController().FinishCycle();
    GetPaintController().ClearPropertyTreeChangedStateTo(
        PropertyTreeState::Root());

    if (invalidator_.GetTracking())
      return invalidator_.GetTracking()->Invalidations();
    return Vector<RasterInvalidationInfo>();
  }

  // In this file, DisplayItemRasterInvalidator is tested through
  // RasterInvalidator.
  RasterInvalidator invalidator_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(DisplayItemRasterInvalidatorTest);

TEST_P(DisplayItemRasterInvalidatorTest, RemoveItemInMiddle) {
  FakeDisplayItemClient first("first", IntRect(100, 100, 300, 300));
  FakeDisplayItemClient second("second", IntRect(100, 100, 200, 200));
  GraphicsContext context(GetPaintController());

  InitRootChunk();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 200, 200));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));
  GenerateRasterInvalidations();

  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 300, 300));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 300, 300));

  EXPECT_THAT(GenerateRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  &second, "second", IntRect(100, 100, 200, 200),
                  PaintInvalidationReason::kDisappeared}));
  invalidator_.SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, SwapOrder) {
  FakeDisplayItemClient first("first", IntRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", IntRect(100, 100, 50, 200));
  FakeDisplayItemClient unaffected("unaffected", IntRect(300, 300, 10, 10));
  GraphicsContext context(GetPaintController());

  InitRootChunk();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, second, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, unaffected, kBackgroundType, FloatRect(300, 300, 10, 10));
  DrawRect(context, unaffected, kForegroundType, FloatRect(300, 300, 10, 10));
  GenerateRasterInvalidations();

  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, second, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, unaffected, kBackgroundType, FloatRect(300, 300, 10, 10));
  DrawRect(context, unaffected, kForegroundType, FloatRect(300, 300, 10, 10));

  EXPECT_THAT(GenerateRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  &first, "first", IntRect(100, 100, 100, 100),
                  PaintInvalidationReason::kReordered}));
  invalidator_.SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, SwapOrderAndInvalidateFirst) {
  FakeDisplayItemClient first("first", IntRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", IntRect(100, 100, 50, 200));
  FakeDisplayItemClient unaffected("unaffected", IntRect(300, 300, 10, 10));
  GraphicsContext context(GetPaintController());

  InitRootChunk();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, unaffected, kBackgroundType, FloatRect(300, 300, 10, 10));
  GenerateRasterInvalidations();

  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  first.Invalidate(PaintInvalidationReason::kOutline);
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, unaffected, kBackgroundType, FloatRect(300, 300, 10, 10));

  EXPECT_THAT(GenerateRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  &first, "first", IntRect(100, 100, 100, 100),
                  PaintInvalidationReason::kOutline}));
  invalidator_.SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, SwapOrderAndInvalidateSecond) {
  FakeDisplayItemClient first("first", IntRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", IntRect(100, 100, 50, 200));
  FakeDisplayItemClient unaffected("unaffected", IntRect(300, 300, 10, 10));
  GraphicsContext context(GetPaintController());

  InitRootChunk();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, unaffected, kBackgroundType, FloatRect(300, 300, 10, 10));
  GenerateRasterInvalidations();

  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  second.Invalidate(PaintInvalidationReason::kOutline);
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, unaffected, kBackgroundType, FloatRect(300, 300, 10, 10));

  EXPECT_THAT(GenerateRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  &second, "second", IntRect(100, 100, 50, 200),
                  PaintInvalidationReason::kOutline}));
  invalidator_.SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, SwapOrderWithIncrementalInvalidation) {
  FakeDisplayItemClient first("first", IntRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", IntRect(100, 100, 50, 200));
  FakeDisplayItemClient unaffected("unaffected", IntRect(300, 300, 10, 10));
  GraphicsContext context(GetPaintController());

  InitRootChunk();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, unaffected, kBackgroundType, FloatRect(300, 300, 10, 10));
  GenerateRasterInvalidations();

  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  first.SetVisualRect(IntRect(100, 100, 200, 100));
  first.Invalidate(PaintInvalidationReason::kIncremental);
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, unaffected, kBackgroundType, FloatRect(300, 300, 10, 10));

  // Incremental invalidation is not applicable when the item is reordered.
  EXPECT_THAT(GenerateRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  &first, "first", IntRect(100, 100, 200, 100),
                  PaintInvalidationReason::kReordered}));
  invalidator_.SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, NewItemInMiddle) {
  FakeDisplayItemClient first("first", IntRect(100, 100, 100, 100));
  FakeDisplayItemClient second("second", IntRect(100, 100, 50, 200));
  FakeDisplayItemClient third("third", IntRect(125, 100, 200, 50));
  GraphicsContext context(GetPaintController());

  InitRootChunk();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));
  GenerateRasterInvalidations();

  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, third, kBackgroundType, FloatRect(125, 100, 200, 50));
  DrawRect(context, second, kBackgroundType, FloatRect(100, 100, 50, 200));

  EXPECT_THAT(GenerateRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  &third, "third", IntRect(125, 100, 200, 50),
                  PaintInvalidationReason::kAppeared}));
  invalidator_.SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, Incremental) {
  IntRect initial_rect(100, 100, 100, 100);
  std::unique_ptr<FakeDisplayItemClient> clients[6];
  for (size_t i = 0; i < base::size(clients); i++) {
    clients[i] = std::make_unique<FakeDisplayItemClient>(
        String::Format("%zu", i), initial_rect);
  }
  GraphicsContext context(GetPaintController());

  InitRootChunk();

  for (auto& client : clients)
    DrawRect(context, *client, kBackgroundType, FloatRect(initial_rect));
  GenerateRasterInvalidations();

  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  clients[0]->SetVisualRect(IntRect(100, 100, 150, 100));
  clients[1]->SetVisualRect(IntRect(100, 100, 100, 150));
  clients[2]->SetVisualRect(IntRect(100, 100, 150, 80));
  clients[3]->SetVisualRect(IntRect(100, 100, 80, 150));
  clients[4]->SetVisualRect(IntRect(100, 100, 150, 150));
  clients[5]->SetVisualRect(IntRect(100, 100, 80, 80));
  for (auto& client : clients) {
    client->Invalidate(PaintInvalidationReason::kIncremental);
    DrawRect(context, *client, kBackgroundType,
             FloatRect(client->VisualRect()));
  }

  EXPECT_THAT(GenerateRasterInvalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{clients[0].get(), "0",
                                         IntRect(200, 100, 50, 100),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{clients[1].get(), "1",
                                         IntRect(100, 200, 100, 50),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{clients[2].get(), "2",
                                         IntRect(200, 100, 50, 80),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{clients[2].get(), "2",
                                         IntRect(100, 180, 100, 20),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{clients[3].get(), "3",
                                         IntRect(180, 100, 20, 100),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{clients[3].get(), "3",
                                         IntRect(100, 200, 80, 50),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{clients[4].get(), "4",
                                         IntRect(200, 100, 50, 150),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{clients[4].get(), "4",
                                         IntRect(100, 200, 150, 50),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{clients[5].get(), "5",
                                         IntRect(180, 100, 20, 100),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{
                      clients[5].get(), "5", IntRect(100, 180, 100, 20),
                      PaintInvalidationReason::kIncremental}));
  invalidator_.SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, AddRemoveFirstAndInvalidateSecond) {
  FakeDisplayItemClient chunk("chunk");
  FakeDisplayItemClient first("first", IntRect(100, 100, 150, 150));
  FakeDisplayItemClient second("second", IntRect(200, 200, 50, 50));
  GraphicsContext context(GetPaintController());

  InitRootChunk();
  DrawRect(context, second, kBackgroundType, FloatRect(200, 200, 50, 50));
  DrawRect(context, second, kForegroundType, FloatRect(200, 200, 50, 50));
  GenerateRasterInvalidations();

  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  first.Invalidate();
  second.Invalidate();
  second.SetVisualRect(IntRect(150, 250, 100, 100));
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 150, 150));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 150, 150));
  DrawRect(context, second, kBackgroundType, FloatRect(150, 250, 100, 100));
  DrawRect(context, second, kForegroundType, FloatRect(150, 250, 100, 100));
  EXPECT_EQ(0u, NumCachedNewItems());

  EXPECT_THAT(
      GenerateRasterInvalidations(),
      UnorderedElementsAre(
          RasterInvalidationInfo{&first, "first", IntRect(100, 100, 150, 150),
                                 PaintInvalidationReason::kAppeared},
          RasterInvalidationInfo{&second, "second", IntRect(200, 200, 50, 50),
                                 PaintInvalidationReason::kFull},
          RasterInvalidationInfo{&second, "second", IntRect(150, 250, 100, 100),
                                 PaintInvalidationReason::kFull}));
  invalidator_.SetTracksRasterInvalidations(false);

  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  DrawRect(context, second, kBackgroundType, FloatRect(150, 250, 100, 100));
  DrawRect(context, second, kForegroundType, FloatRect(150, 250, 100, 100));

  EXPECT_THAT(GenerateRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  &first, "first", IntRect(100, 100, 150, 150),
                  PaintInvalidationReason::kDisappeared}));
  invalidator_.SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, InvalidateFirstAndAddRemoveSecond) {
  FakeDisplayItemClient first("first", IntRect(100, 100, 150, 150));
  FakeDisplayItemClient second("second", IntRect(200, 200, 50, 50));
  GraphicsContext context(GetPaintController());

  InitRootChunk();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 150, 150));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 150, 150));
  GenerateRasterInvalidations();

  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  first.Invalidate();
  first.SetVisualRect(IntRect(150, 150, 100, 100));
  second.Invalidate();
  DrawRect(context, first, kBackgroundType, FloatRect(150, 150, 100, 100));
  DrawRect(context, first, kForegroundType, FloatRect(150, 150, 100, 100));
  DrawRect(context, second, kBackgroundType, FloatRect(200, 200, 50, 50));
  DrawRect(context, second, kForegroundType, FloatRect(200, 200, 50, 50));

  EXPECT_THAT(
      GenerateRasterInvalidations(),
      UnorderedElementsAre(
          RasterInvalidationInfo{&first, "first", IntRect(100, 100, 150, 150),
                                 PaintInvalidationReason::kFull},
          RasterInvalidationInfo{&second, "second", IntRect(200, 200, 50, 50),
                                 PaintInvalidationReason::kAppeared}));
  invalidator_.SetTracksRasterInvalidations(false);

  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  first.Invalidate();
  first.SetVisualRect(IntRect(100, 100, 150, 150));
  second.Invalidate();
  DrawRect(context, first, kBackgroundType, FloatRect(100, 100, 150, 150));
  DrawRect(context, first, kForegroundType, FloatRect(100, 100, 150, 150));

  EXPECT_THAT(
      GenerateRasterInvalidations(),
      UnorderedElementsAre(
          RasterInvalidationInfo{&first, "first", IntRect(100, 100, 150, 150),
                                 PaintInvalidationReason::kFull},
          RasterInvalidationInfo{&second, "second", IntRect(200, 200, 50, 50),
                                 PaintInvalidationReason::kDisappeared}));
  invalidator_.SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, SwapOrderWithChildren) {
  FakeDisplayItemClient container1("container1", IntRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", IntRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2", IntRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", IntRect(100, 200, 50, 200));
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
  GenerateRasterInvalidations();

  // Simulate the situation when |container1| gets a z-index that is greater
  // than that of |container2|.
  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  DrawRect(context, container2, kBackgroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, content2, kForegroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, container2, kForegroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, container1, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, content1, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, container1, kForegroundType, FloatRect(100, 100, 100, 100));

  EXPECT_THAT(GenerateRasterInvalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{&container1, "container1",
                                         IntRect(100, 100, 100, 100),
                                         PaintInvalidationReason::kReordered},
                  RasterInvalidationInfo{&content1, "content1",
                                         IntRect(100, 100, 50, 200),
                                         PaintInvalidationReason::kReordered}));
  invalidator_.SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, SwapOrderWithChildrenAndInvalidation) {
  FakeDisplayItemClient container1("container1", IntRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", IntRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2", IntRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", IntRect(100, 200, 50, 200));
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
  GenerateRasterInvalidations();

  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  // Simulate the situation when |container1| gets a z-index that is greater
  // than that of |container2|, and |container1| is invalidated.
  container2.Invalidate();
  DrawRect(context, container2, kBackgroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, content2, kForegroundType, FloatRect(100, 200, 50, 200));
  DrawRect(context, container2, kForegroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, container1, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, content1, kForegroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, container1, kForegroundType, FloatRect(100, 100, 100, 100));

  EXPECT_THAT(GenerateRasterInvalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{&container1, "container1",
                                         IntRect(100, 100, 100, 100),
                                         PaintInvalidationReason::kReordered},
                  RasterInvalidationInfo{&content1, "content1",
                                         IntRect(100, 100, 50, 200),
                                         PaintInvalidationReason::kReordered},
                  RasterInvalidationInfo{&container2, "container2",
                                         IntRect(100, 200, 100, 100),
                                         PaintInvalidationReason::kFull}));
  invalidator_.SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, SwapOrderCrossingChunks) {
  FakeDisplayItemClient container1("container1", IntRect(100, 100, 100, 100));
  FakeDisplayItemClient content1("content1", IntRect(100, 100, 50, 200));
  FakeDisplayItemClient container2("container2", IntRect(100, 200, 100, 100));
  FakeDisplayItemClient content2("content2", IntRect(100, 200, 50, 200));
  GraphicsContext context(GetPaintController());

  auto container1_effect = CreateOpacityEffect(e0(), 0.5);
  auto container1_properties = DefaultPaintChunkProperties();
  container1_properties.SetEffect(*container1_effect);

  auto container2_effect = CreateOpacityEffect(e0(), 0.5);
  auto container2_properties = DefaultPaintChunkProperties();
  container2_properties.SetEffect(*container2_effect);

  GetPaintController().UpdateCurrentPaintChunkProperties(
      PaintChunk::Id(container1, kBackgroundType), container1_properties);
  DrawRect(context, container1, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  GetPaintController().UpdateCurrentPaintChunkProperties(
      PaintChunk::Id(container2, kBackgroundType), container2_properties);
  DrawRect(context, container2, kBackgroundType, FloatRect(100, 200, 100, 100));
  DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
  GenerateRasterInvalidations();

  // Move content2 into container1, without invalidation.
  invalidator_.SetTracksRasterInvalidations(true);
  GetPaintController().UpdateCurrentPaintChunkProperties(
      PaintChunk::Id(container1, kBackgroundType), container1_properties);
  DrawRect(context, container1, kBackgroundType, FloatRect(100, 100, 100, 100));
  DrawRect(context, content1, kBackgroundType, FloatRect(100, 100, 50, 200));
  DrawRect(context, content2, kBackgroundType, FloatRect(100, 200, 50, 200));
  GetPaintController().UpdateCurrentPaintChunkProperties(
      PaintChunk::Id(container2, kBackgroundType), container2_properties);
  DrawRect(context, container2, kBackgroundType, FloatRect(100, 200, 100, 100));

  EXPECT_THAT(GenerateRasterInvalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{&content2, "content2",
                                         IntRect(100, 200, 50, 200),
                                         PaintInvalidationReason::kDisappeared},
                  RasterInvalidationInfo{&content2, "content2",
                                         IntRect(100, 200, 50, 200),
                                         PaintInvalidationReason::kAppeared}));
  invalidator_.SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, SkipCache) {
  FakeDisplayItemClient multicol("multicol", IntRect(100, 100, 200, 200));
  FakeDisplayItemClient content("content", IntRect(100, 100, 100, 100));
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
  GenerateRasterInvalidations();

  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  // Draw again with nothing invalidated.
  EXPECT_TRUE(ClientCacheIsValid(multicol));
  DrawRect(context, multicol, kBackgroundType, FloatRect(100, 200, 100, 100));

  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundType, rect1);
  DrawRect(context, content, kForegroundType, rect2);
  GetPaintController().EndSkippingCache();

  EXPECT_THAT(GenerateRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  &content, "content", IntRect(100, 100, 100, 100),
                  PaintInvalidationReason::kUncacheable}));
  invalidator_.SetTracksRasterInvalidations(false);

  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  // Now the multicol becomes 3 columns and repaints.
  multicol.Invalidate();
  DrawRect(context, multicol, kBackgroundType, FloatRect(100, 100, 100, 100));

  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundType, rect1);
  DrawRect(context, content, kForegroundType, rect2);
  DrawRect(context, content, kForegroundType, rect3);
  GetPaintController().EndSkippingCache();

  EXPECT_THAT(GenerateRasterInvalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{&multicol, "multicol",
                                         IntRect(100, 100, 200, 200),
                                         PaintInvalidationReason::kFull},
                  RasterInvalidationInfo{
                      &content, "content", IntRect(100, 100, 100, 100),
                      PaintInvalidationReason::kUncacheable}));
  invalidator_.SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, PartialSkipCache) {
  FakeDisplayItemClient content("content", IntRect(100, 100, 250, 250));
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
  GenerateRasterInvalidations();

  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  // Draw again with nothing invalidated.
  DrawRect(context, content, kBackgroundType, rect1);
  GetPaintController().BeginSkippingCache();
  DrawRect(context, content, kForegroundType, rect2);
  GetPaintController().EndSkippingCache();
  DrawRect(context, content, kForegroundType, rect3);

  EXPECT_THAT(GenerateRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  &content, "content", IntRect(100, 100, 250, 250),
                  PaintInvalidationReason::kUncacheable}));
  invalidator_.SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, Partial) {
  FakeDisplayItemClient client("client", IntRect(100, 100, 300, 300));
  GraphicsContext context(GetPaintController());

  InitRootChunk();
  DrawRect(context, client, kBackgroundType, FloatRect(100, 100, 300, 300));
  GenerateRasterInvalidations();

  // Test partial rect invalidation without other invalidations.
  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  client.SetPartialInvalidationVisualRect(IntRect(150, 160, 170, 180));
  DrawRect(context, client, kBackgroundType, FloatRect(100, 100, 300, 300));

  // Partial invalidation.
  EXPECT_THAT(GenerateRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  &client, "client", IntRect(150, 160, 170, 180),
                  PaintInvalidationReason::kRectangle}));
  EXPECT_EQ(IntRect(), client.PartialInvalidationVisualRect());
  invalidator_.SetTracksRasterInvalidations(false);

  // Test partial rect invalidation with full invalidation.
  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  client.SetPartialInvalidationVisualRect(IntRect(150, 160, 170, 180));
  client.Invalidate();
  DrawRect(context, client, kBackgroundType, FloatRect(100, 100, 300, 300));

  // Partial invalidation is shadowed by full invalidation.
  EXPECT_THAT(GenerateRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  &client, "client", IntRect(100, 100, 300, 300),
                  PaintInvalidationReason::kFull}));
  EXPECT_EQ(IntRect(), client.PartialInvalidationVisualRect());
  invalidator_.SetTracksRasterInvalidations(false);

  // Test partial rect invalidation with incremental invalidation.
  invalidator_.SetTracksRasterInvalidations(true);
  InitRootChunk();
  client.SetPartialInvalidationVisualRect(IntRect(150, 160, 170, 180));
  client.SetVisualRect(IntRect(100, 100, 300, 400));
  DrawRect(context, client, kBackgroundType, FloatRect(100, 100, 300, 400));

  // Both partial invalidation and incremental invalidation.
  EXPECT_THAT(
      GenerateRasterInvalidations(),
      UnorderedElementsAre(
          RasterInvalidationInfo{&client, "client", IntRect(150, 160, 170, 180),
                                 PaintInvalidationReason::kRectangle},
          RasterInvalidationInfo{&client, "client", IntRect(100, 400, 300, 100),
                                 PaintInvalidationReason::kIncremental}));
  invalidator_.SetTracksRasterInvalidations(false);
}

}  // namespace blink
