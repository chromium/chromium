// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller_test.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"

using testing::ElementsAre;

namespace blink {

using DrawingRecorderTest = PaintControllerTestBase;

namespace {

const gfx::Rect kBounds(1, 2, 3, 4);

TEST_F(DrawingRecorderTest, Nothing) {
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  {
    AutoCommitPaintController paint_controller(GetPersistentData());
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawNothing(context, client, kForegroundType);
  }
  EXPECT_THAT(GetPersistentData().GetDisplayItemList(),
              ElementsAre(IsSameId(client.Id(), kForegroundType)));
  EXPECT_TRUE(
      To<DrawingDisplayItem>(GetPersistentData().GetDisplayItemList()[0])
          .GetPaintRecord()
          .empty());
}

TEST_F(DrawingRecorderTest, Rect) {
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  {
    AutoCommitPaintController paint_controller(GetPersistentData());
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, client, kForegroundType, kBounds);
  }
  EXPECT_THAT(GetPersistentData().GetDisplayItemList(),
              ElementsAre(IsSameId(client.Id(), kForegroundType)));
}

TEST_F(DrawingRecorderTest, Cached) {
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  {
    AutoCommitPaintController paint_controller(GetPersistentData());
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawNothing(context, client, kBackgroundType);
    DrawRect(context, client, kForegroundType, kBounds);
  }

  EXPECT_THAT(GetPersistentData().GetDisplayItemList(),
              ElementsAre(IsSameId(client.Id(), kBackgroundType),
                          IsSameId(client.Id(), kForegroundType)));

  {
    AutoCommitPaintController paint_controller(GetPersistentData());
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawNothing(context, client, kBackgroundType);
    DrawRect(context, client, kForegroundType, kBounds);

    EXPECT_EQ(2u, NumCachedNewItems(paint_controller));
  }

  EXPECT_THAT(GetPersistentData().GetDisplayItemList(),
              ElementsAre(IsSameId(client.Id(), kBackgroundType),
                          IsSameId(client.Id(), kForegroundType)));
}

}  // namespace
}  // namespace blink
