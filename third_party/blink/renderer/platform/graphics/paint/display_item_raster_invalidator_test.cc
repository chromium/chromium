// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/paint/display_item_raster_invalidator.h"

#include "base/functional/callback_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller_test.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/test_paint_artifact.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

using ::testing::UnorderedElementsAre;

class DisplayItemRasterInvalidatorTest : public PaintControllerTestBase,
                                         public PaintTestConfigurations,
                                         public RasterInvalidator::Callback {
 protected:
  DisplayItemRasterInvalidatorTest() = default;

  Vector<RasterInvalidationInfo> GetRasterInvalidations() {
    if (invalidator_->GetTracking()) {
      return invalidator_->GetTracking()->Invalidations();
    }
    return Vector<RasterInvalidationInfo>();
  }

  void InvalidateRect(const gfx::Rect&) override {}

  // In this file, DisplayItemRasterInvalidator is tested through
  // RasterInvalidator.
  Persistent<RasterInvalidator> invalidator_ =
      MakeGarbageCollected<RasterInvalidator>(*this);
};

class RasterInvalidationPaintController : public PaintControllerForTest {
  STACK_ALLOCATED();

 public:
  RasterInvalidationPaintController(
      PaintControllerPersistentData& persistent_data,
      RasterInvalidator& invalidator)
      : PaintControllerForTest(persistent_data), invalidator_(invalidator) {}
  ~RasterInvalidationPaintController() {
    ++sequence_number_;
    const auto& paint_artifact = CommitNewDisplayItems();
    invalidator_.Generate(PaintChunkSubset(paint_artifact),
                          // The layer bounds are big enough not to clip display
                          // item raster invalidation rects in the tests.
                          gfx::Vector2dF(), gfx::Size(20000, 20000),
                          PropertyTreeState::Root());
    for (auto& chunk : paint_artifact.GetPaintChunks()) {
      chunk.properties.ClearChangedToRoot(sequence_number_);
    }
  }

 private:
  RasterInvalidator& invalidator_;
  static int sequence_number_;
};

int RasterInvalidationPaintController::sequence_number_ = 1;

INSTANTIATE_PAINT_TEST_SUITE_P(DisplayItemRasterInvalidatorTest);

TEST_P(DisplayItemRasterInvalidatorTest, FullInvalidationWithoutLayoutChange) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 300, 300));
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 200, 200));
    DrawRect(context, first, kForegroundType, gfx::Rect(100, 150, 300, 300));
  }

  first.Invalidate();
  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 300, 300));
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 200, 200));
    DrawRect(context, first, kForegroundType, gfx::Rect(100, 150, 300, 300));
  }

  EXPECT_THAT(GetRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  first.Id(), "first", gfx::Rect(100, 100, 300, 350),
                  PaintInvalidationReason::kLayout}));
  invalidator_->SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, FullInvalidationWithGeometryChange) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 300, 300));
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 200, 200));
    DrawRect(context, first, kForegroundType, gfx::Rect(100, 150, 300, 300));
  }

  first.Invalidate();
  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, first, kBackgroundType, gfx::Rect(200, 100, 300, 300));
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 200, 200));
    DrawRect(context, first, kForegroundType, gfx::Rect(200, 150, 300, 300));
  }

  EXPECT_THAT(GetRasterInvalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{first.Id(), "first",
                                         gfx::Rect(100, 100, 300, 350),
                                         PaintInvalidationReason::kLayout},
                  RasterInvalidationInfo{first.Id(), "first",
                                         gfx::Rect(200, 100, 300, 350),
                                         PaintInvalidationReason::kLayout}));
  invalidator_->SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, RemoveItemInMiddle) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 300, 300));
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 200, 200));
    DrawRect(context, first, kForegroundType, gfx::Rect(100, 100, 300, 300));
  }

  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 300, 300));
    DrawRect(context, first, kForegroundType, gfx::Rect(100, 100, 300, 300));
  }

  EXPECT_THAT(GetRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  second.Id(), "second", gfx::Rect(100, 100, 200, 200),
                  PaintInvalidationReason::kDisappeared}));
  invalidator_->SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, SwapOrder) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  FakeDisplayItemClient& unaffected =
      *MakeGarbageCollected<FakeDisplayItemClient>("unaffected");
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 100, 100));
    DrawRect(context, first, kForegroundType, gfx::Rect(100, 100, 100, 100));
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, second, kForegroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, unaffected, kBackgroundType, gfx::Rect(300, 300, 10, 10));
    DrawRect(context, unaffected, kForegroundType, gfx::Rect(300, 300, 10, 10));
  }

  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, second, kForegroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 100, 100));
    DrawRect(context, first, kForegroundType, gfx::Rect(100, 100, 100, 100));
    DrawRect(context, unaffected, kBackgroundType, gfx::Rect(300, 300, 10, 10));
    DrawRect(context, unaffected, kForegroundType, gfx::Rect(300, 300, 10, 10));
  }

  EXPECT_THAT(GetRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  first.Id(), "first", gfx::Rect(100, 100, 100, 100),
                  PaintInvalidationReason::kReordered}));
  invalidator_->SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, SwapOrderAndInvalidateFirst) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  FakeDisplayItemClient& unaffected =
      *MakeGarbageCollected<FakeDisplayItemClient>("unaffected");
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 100, 100));
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, unaffected, kBackgroundType, gfx::Rect(300, 300, 10, 10));
  }

  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    first.Invalidate(PaintInvalidationReason::kOutline);
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 100, 100));
    DrawRect(context, unaffected, kBackgroundType, gfx::Rect(300, 300, 10, 10));
  }

  EXPECT_THAT(GetRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  first.Id(), "first", gfx::Rect(100, 100, 100, 100),
                  PaintInvalidationReason::kOutline}));
  invalidator_->SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, SwapOrderAndInvalidateSecond) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  FakeDisplayItemClient& unaffected =
      *MakeGarbageCollected<FakeDisplayItemClient>("unaffected");
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 100, 100));
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, unaffected, kBackgroundType, gfx::Rect(300, 300, 10, 10));
  }

  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    second.Invalidate(PaintInvalidationReason::kOutline);
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 100, 100));
    DrawRect(context, unaffected, kBackgroundType, gfx::Rect(300, 300, 10, 10));
  }

  EXPECT_THAT(GetRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  second.Id(), "second", gfx::Rect(100, 100, 50, 200),
                  PaintInvalidationReason::kOutline}));
  invalidator_->SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, SwapOrderWithIncrementalInvalidation) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  FakeDisplayItemClient& unaffected =
      *MakeGarbageCollected<FakeDisplayItemClient>("unaffected");
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 100, 100));
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, unaffected, kBackgroundType, gfx::Rect(300, 300, 10, 10));
  }

  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    first.Invalidate(PaintInvalidationReason::kIncremental);
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 100, 100));
    DrawRect(context, unaffected, kBackgroundType, gfx::Rect(300, 300, 10, 10));
  }

  // Incremental invalidation is not applicable when the item is reordered.
  EXPECT_THAT(GetRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  first.Id(), "first", gfx::Rect(100, 100, 100, 100),
                  PaintInvalidationReason::kReordered}));
  invalidator_->SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, NewItemInMiddle) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  FakeDisplayItemClient& third =
      *MakeGarbageCollected<FakeDisplayItemClient>("third");
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 100, 100));
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 50, 200));
  }

  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 100, 100));
    DrawRect(context, third, kBackgroundType, gfx::Rect(125, 100, 200, 50));
    DrawRect(context, second, kBackgroundType, gfx::Rect(100, 100, 50, 200));
  }

  EXPECT_THAT(GetRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  third.Id(), "third", gfx::Rect(125, 100, 200, 50),
                  PaintInvalidationReason::kAppeared}));
  invalidator_->SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, Incremental) {
  gfx::Rect initial_rect(100, 100, 100, 100);
  Persistent<FakeDisplayItemClient> clients[6];
  for (size_t i = 0; i < std::size(clients); i++) {
    clients[i] =
        MakeGarbageCollected<FakeDisplayItemClient>(String::Format("%zu", i));
  }
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);

    for (auto& client : clients)
      DrawRect(context, *client, kBackgroundType, gfx::Rect(initial_rect));
  }

  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    gfx::Rect visual_rects[] = {
        gfx::Rect(100, 100, 150, 100), gfx::Rect(100, 100, 100, 150),
        gfx::Rect(100, 100, 150, 80),  gfx::Rect(100, 100, 80, 150),
        gfx::Rect(100, 100, 150, 150), gfx::Rect(100, 100, 80, 80)};
    for (size_t i = 0; i < std::size(clients); i++) {
      clients[i]->Invalidate(PaintInvalidationReason::kIncremental);
      DrawRect(context, *clients[i], kBackgroundType,
               gfx::Rect(visual_rects[i]));
    }
  }

  EXPECT_THAT(GetRasterInvalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{clients[0]->Id(), "0",
                                         gfx::Rect(200, 100, 50, 100),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{clients[1]->Id(), "1",
                                         gfx::Rect(100, 200, 100, 50),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{clients[2]->Id(), "2",
                                         gfx::Rect(200, 100, 50, 80),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{clients[2]->Id(), "2",
                                         gfx::Rect(100, 180, 100, 20),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{clients[3]->Id(), "3",
                                         gfx::Rect(180, 100, 20, 100),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{clients[3]->Id(), "3",
                                         gfx::Rect(100, 200, 80, 50),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{clients[4]->Id(), "4",
                                         gfx::Rect(200, 100, 50, 150),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{clients[4]->Id(), "4",
                                         gfx::Rect(100, 200, 150, 50),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{clients[5]->Id(), "5",
                                         gfx::Rect(180, 100, 20, 100),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{
                      clients[5]->Id(), "5", gfx::Rect(100, 180, 100, 20),
                      PaintInvalidationReason::kIncremental}));
  invalidator_->SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, AddRemoveFirstAndInvalidateSecond) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, second, kBackgroundType, gfx::Rect(200, 200, 50, 50));
    DrawRect(context, second, kForegroundType, gfx::Rect(200, 200, 50, 50));
  }

  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    first.Invalidate();
    second.Invalidate();
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 150, 150));
    DrawRect(context, first, kForegroundType, gfx::Rect(100, 100, 150, 150));
    DrawRect(context, second, kBackgroundType, gfx::Rect(150, 250, 100, 100));
    DrawRect(context, second, kForegroundType, gfx::Rect(150, 250, 100, 100));
    EXPECT_EQ(0u, NumCachedNewItems(paint_controller));
  }

  EXPECT_THAT(GetRasterInvalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{first.Id(), "first",
                                         gfx::Rect(100, 100, 150, 150),
                                         PaintInvalidationReason::kAppeared},
                  RasterInvalidationInfo{second.Id(), "second",
                                         gfx::Rect(200, 200, 50, 50),
                                         PaintInvalidationReason::kLayout},
                  RasterInvalidationInfo{second.Id(), "second",
                                         gfx::Rect(150, 250, 100, 100),
                                         PaintInvalidationReason::kLayout}));
  invalidator_->SetTracksRasterInvalidations(false);

  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, second, kBackgroundType, gfx::Rect(150, 250, 100, 100));
    DrawRect(context, second, kForegroundType, gfx::Rect(150, 250, 100, 100));
  }

  EXPECT_THAT(GetRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  first.Id(), "first", gfx::Rect(100, 100, 150, 150),
                  PaintInvalidationReason::kDisappeared}));
  invalidator_->SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, InvalidateFirstAndAddRemoveSecond) {
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  FakeDisplayItemClient& second =
      *MakeGarbageCollected<FakeDisplayItemClient>("second");
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 150, 150));
    DrawRect(context, first, kForegroundType, gfx::Rect(100, 100, 150, 150));
  }

  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    first.Invalidate();
    second.Invalidate();
    DrawRect(context, first, kBackgroundType, gfx::Rect(150, 150, 100, 100));
    DrawRect(context, first, kForegroundType, gfx::Rect(150, 150, 100, 100));
    DrawRect(context, second, kBackgroundType, gfx::Rect(200, 200, 50, 50));
    DrawRect(context, second, kForegroundType, gfx::Rect(200, 200, 50, 50));
  }

  EXPECT_THAT(GetRasterInvalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{first.Id(), "first",
                                         gfx::Rect(100, 100, 150, 150),
                                         PaintInvalidationReason::kLayout},
                  RasterInvalidationInfo{second.Id(), "second",
                                         gfx::Rect(200, 200, 50, 50),
                                         PaintInvalidationReason::kAppeared}));
  invalidator_->SetTracksRasterInvalidations(false);

  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    first.Invalidate();
    second.Invalidate();
    DrawRect(context, first, kBackgroundType, gfx::Rect(100, 100, 150, 150));
    DrawRect(context, first, kForegroundType, gfx::Rect(100, 100, 150, 150));
  }

  EXPECT_THAT(GetRasterInvalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{first.Id(), "first",
                                         gfx::Rect(100, 100, 150, 150),
                                         PaintInvalidationReason::kLayout},
                  RasterInvalidationInfo{
                      second.Id(), "second", gfx::Rect(200, 200, 50, 50),
                      PaintInvalidationReason::kDisappeared}));
  invalidator_->SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, SwapOrderWithChildren) {
  FakeDisplayItemClient& container1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container1");
  FakeDisplayItemClient& content1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content1");
  FakeDisplayItemClient& container2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container2");
  FakeDisplayItemClient& content2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content2");
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
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

  // Simulate the situation when |container1| gets a z-index that is greater
  // than that of |container2|.
  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, container2, kBackgroundType,
             gfx::Rect(100, 200, 100, 100));
    DrawRect(context, content2, kBackgroundType, gfx::Rect(100, 200, 50, 200));
    DrawRect(context, content2, kForegroundType, gfx::Rect(100, 200, 50, 200));
    DrawRect(context, container2, kForegroundType,
             gfx::Rect(100, 200, 100, 100));
    DrawRect(context, container1, kBackgroundType,
             gfx::Rect(100, 100, 100, 100));
    DrawRect(context, content1, kBackgroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, content1, kForegroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, container1, kForegroundType,
             gfx::Rect(100, 100, 100, 100));
  }

  EXPECT_THAT(GetRasterInvalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{container1.Id(), "container1",
                                         gfx::Rect(100, 100, 100, 100),
                                         PaintInvalidationReason::kReordered},
                  RasterInvalidationInfo{content1.Id(), "content1",
                                         gfx::Rect(100, 100, 50, 200),
                                         PaintInvalidationReason::kReordered}));
  invalidator_->SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, SwapOrderWithChildrenAndInvalidation) {
  FakeDisplayItemClient& container1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container1");
  FakeDisplayItemClient& content1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content1");
  FakeDisplayItemClient& container2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container2");
  FakeDisplayItemClient& content2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content2");
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
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

  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    // Simulate the situation when |container1| gets a z-index that is greater
    // than that of |container2|, and |container1| is invalidated.
    container2.Invalidate();
    DrawRect(context, container2, kBackgroundType,
             gfx::Rect(100, 200, 100, 100));
    DrawRect(context, content2, kBackgroundType, gfx::Rect(100, 200, 50, 200));
    DrawRect(context, content2, kForegroundType, gfx::Rect(100, 200, 50, 200));
    DrawRect(context, container2, kForegroundType,
             gfx::Rect(100, 200, 100, 100));
    DrawRect(context, container1, kBackgroundType,
             gfx::Rect(100, 100, 100, 100));
    DrawRect(context, content1, kBackgroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, content1, kForegroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, container1, kForegroundType,
             gfx::Rect(100, 100, 100, 100));
  }

  EXPECT_THAT(GetRasterInvalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{container1.Id(), "container1",
                                         gfx::Rect(100, 100, 100, 100),
                                         PaintInvalidationReason::kReordered},
                  RasterInvalidationInfo{content1.Id(), "content1",
                                         gfx::Rect(100, 100, 50, 200),
                                         PaintInvalidationReason::kReordered},
                  RasterInvalidationInfo{container2.Id(), "container2",
                                         gfx::Rect(100, 200, 100, 100),
                                         PaintInvalidationReason::kLayout}));
  invalidator_->SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, SwapOrderCrossingChunks) {
  FakeDisplayItemClient& container1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container1");
  FakeDisplayItemClient& content1 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content1");
  FakeDisplayItemClient& container2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("container2");
  FakeDisplayItemClient& content2 =
      *MakeGarbageCollected<FakeDisplayItemClient>("content2");

  auto* container1_effect = CreateOpacityEffect(e0(), 0.5);
  auto container1_properties = DefaultPaintChunkProperties();
  container1_properties.SetEffect(*container1_effect);

  auto* container2_effect = CreateOpacityEffect(e0(), 0.5);
  auto container2_properties = DefaultPaintChunkProperties();
  container2_properties.SetEffect(*container2_effect);

  PaintChunk::Id container1_id(container1.Id(), kBackgroundType);
  PaintChunk::Id container2_id(container2.Id(), kBackgroundType);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    paint_controller.UpdateCurrentPaintChunkProperties(
        container1_id, container1, container1_properties);
    DrawRect(context, container1, kBackgroundType,
             gfx::Rect(100, 100, 100, 100));
    DrawRect(context, content1, kBackgroundType, gfx::Rect(100, 100, 50, 200));
    paint_controller.UpdateCurrentPaintChunkProperties(
        container2_id, container2, container2_properties);
    DrawRect(context, container2, kBackgroundType,
             gfx::Rect(100, 200, 100, 100));
    DrawRect(context, content2, kBackgroundType, gfx::Rect(100, 200, 50, 200));
  }

  // Move content2 into container1, without invalidation.
  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    paint_controller.UpdateCurrentPaintChunkProperties(
        container1_id, container1, container1_properties);
    DrawRect(context, container1, kBackgroundType,
             gfx::Rect(100, 100, 100, 100));
    DrawRect(context, content1, kBackgroundType, gfx::Rect(100, 100, 50, 200));
    DrawRect(context, content2, kBackgroundType, gfx::Rect(100, 200, 50, 200));
    paint_controller.UpdateCurrentPaintChunkProperties(
        container2_id, container2, container2_properties);
    DrawRect(context, container2, kBackgroundType,
             gfx::Rect(100, 200, 100, 100));
  }

  EXPECT_THAT(GetRasterInvalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{content2.Id(), "content2",
                                         gfx::Rect(100, 200, 50, 200),
                                         PaintInvalidationReason::kDisappeared},
                  RasterInvalidationInfo{content2.Id(), "content2",
                                         gfx::Rect(100, 200, 50, 200),
                                         PaintInvalidationReason::kAppeared}));
  invalidator_->SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, SkipCache) {
  FakeDisplayItemClient& multicol =
      *MakeGarbageCollected<FakeDisplayItemClient>("multicol");
  FakeDisplayItemClient& content =
      *MakeGarbageCollected<FakeDisplayItemClient>("content");
  gfx::Rect rect1(100, 100, 50, 50);
  gfx::Rect rect2(150, 100, 50, 50);
  gfx::Rect rect3(200, 100, 50, 50);

  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, multicol, kBackgroundType, gfx::Rect(100, 200, 100, 100));
    paint_controller.BeginSkippingCache();
    DrawRect(context, content, kForegroundType, rect1);
    DrawRect(context, content, kForegroundType, rect2);
    paint_controller.EndSkippingCache();
  }

  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    // Draw again with nothing invalidated.
    EXPECT_TRUE(ClientCacheIsValid(multicol));
    DrawRect(context, multicol, kBackgroundType, gfx::Rect(100, 200, 100, 100));

    paint_controller.BeginSkippingCache();
    DrawRect(context, content, kForegroundType, rect1);
    DrawRect(context, content, kForegroundType, rect2);
    paint_controller.EndSkippingCache();
  }

  EXPECT_THAT(GetRasterInvalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  content.Id(), "content", UnionRects(rect1, rect2),
                  PaintInvalidationReason::kUncacheable}));
  invalidator_->SetTracksRasterInvalidations(false);

  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    // Now the multicol becomes 3 columns and repaints.
    multicol.Invalidate();
    DrawRect(context, multicol, kBackgroundType, gfx::Rect(100, 100, 100, 100));

    paint_controller.BeginSkippingCache();
    DrawRect(context, content, kForegroundType, rect1);
    DrawRect(context, content, kForegroundType, rect2);
    DrawRect(context, content, kForegroundType, rect3);
    paint_controller.EndSkippingCache();
  }

  EXPECT_THAT(
      GetRasterInvalidations(),
      UnorderedElementsAre(
          RasterInvalidationInfo{multicol.Id(), "multicol",
                                 gfx::Rect(100, 200, 100, 100),
                                 PaintInvalidationReason::kLayout},
          RasterInvalidationInfo{multicol.Id(), "multicol",
                                 gfx::Rect(100, 100, 100, 100),
                                 PaintInvalidationReason::kLayout},
          RasterInvalidationInfo{content.Id(), "content",
                                 UnionRects(rect1, UnionRects(rect2, rect3)),
                                 PaintInvalidationReason::kUncacheable}));
  invalidator_->SetTracksRasterInvalidations(false);
}

TEST_P(DisplayItemRasterInvalidatorTest, PartialSkipCache) {
  FakeDisplayItemClient& content =
      *MakeGarbageCollected<FakeDisplayItemClient>("content");

  gfx::Rect rect1(100, 100, 50, 50);
  gfx::Rect rect2(150, 100, 50, 50);
  gfx::Rect rect3(200, 100, 50, 50);

  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, content, kBackgroundType, rect1);
    paint_controller.BeginSkippingCache();
    DrawRect(context, content, kForegroundType, rect2);
    paint_controller.EndSkippingCache();
    DrawRect(context, content, kForegroundType, rect3);
  }

  invalidator_->SetTracksRasterInvalidations(true);
  {
    RasterInvalidationPaintController paint_controller(GetPersistentData(),
                                                       *invalidator_);
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    // Draw again with nothing invalidated.
    DrawRect(context, content, kBackgroundType, rect1);
    paint_controller.BeginSkippingCache();
    DrawRect(context, content, kForegroundType, rect2);
    paint_controller.EndSkippingCache();
    DrawRect(context, content, kForegroundType, rect3);
  }

  EXPECT_THAT(
      GetRasterInvalidations(),
      UnorderedElementsAre(RasterInvalidationInfo{
          content.Id(), "content", UnionRects(rect1, UnionRects(rect2, rect3)),
          PaintInvalidationReason::kUncacheable}));
  invalidator_->SetTracksRasterInvalidations(false);
}

}  // namespace blink
