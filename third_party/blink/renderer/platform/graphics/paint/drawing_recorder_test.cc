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
  GraphicsContext context(GetPaintController());
  {
    PaintControllerCycleScopeForTest cycle_scope(GetPaintController());
    InitRootChunk();
    DrawNothing(context, client, kForegroundType);
    GetPaintController().CommitNewDisplayItems();
  }
  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(client.Id(), kForegroundType)));
  EXPECT_TRUE(
      To<DrawingDisplayItem>(GetPaintController().GetDisplayItemList()[0])
          .GetPaintRecord()
          .empty());
}

TEST_F(DrawingRecorderTest, Rect) {
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  GraphicsContext context(GetPaintController());
  {
    PaintControllerCycleScopeForTest cycle_scope(GetPaintController());
    InitRootChunk();
    DrawRect(context, client, kForegroundType, kBounds);
    GetPaintController().CommitNewDisplayItems();
  }
  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(client.Id(), kForegroundType)));
}

TEST_F(DrawingRecorderTest, Cached) {
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  GraphicsContext context(GetPaintController());
  {
    PaintControllerCycleScopeForTest cycle_scope(GetPaintController());
    InitRootChunk();
    DrawNothing(context, client, kBackgroundType);
    DrawRect(context, client, kForegroundType, kBounds);
    GetPaintController().CommitNewDisplayItems();
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(client.Id(), kBackgroundType),
                          IsSameId(client.Id(), kForegroundType)));

  {
    PaintControllerCycleScopeForTest cycle_scope(GetPaintController());
    InitRootChunk();
    DrawNothing(context, client, kBackgroundType);
    DrawRect(context, client, kForegroundType, kBounds);

    EXPECT_EQ(2u, NumCachedNewItems());

    GetPaintController().CommitNewDisplayItems();
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(client.Id(), kBackgroundType),
                          IsSameId(client.Id(), kForegroundType)));
}

}  // namespace
}  // namespace blink
