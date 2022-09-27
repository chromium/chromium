// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"

#include "cc/paint/skottie_wrapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller_test.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_paint_canvas.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"

using testing::_;
using testing::ElementsAre;

namespace blink {

using PaintRecordBuilderTest = PaintControllerTestBase;

TEST_F(PaintRecordBuilderTest, TransientPaintController) {
  auto* builder = MakeGarbageCollected<PaintRecordBuilder>();
  auto& context = builder->Context();
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>("client");
  DrawRect(context, client, kBackgroundType, gfx::Rect(10, 10, 20, 20));
  DrawRect(context, client, kForegroundType, gfx::Rect(15, 15, 10, 10));
  EXPECT_FALSE(ClientCacheIsValid(context.GetPaintController(), client));

  MockPaintCanvas canvas;
  cc::PaintFlags flags;
  EXPECT_CALL(canvas, drawPicture(_)).Times(1);
  builder->EndRecording(canvas);

  EXPECT_THAT(context.GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(client.Id(), kBackgroundType),
                          IsSameId(client.Id(), kForegroundType)));
  EXPECT_FALSE(ClientCacheIsValid(context.GetPaintController(), client));
}

TEST_F(PaintRecordBuilderTest, LastingPaintController) {
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>("client");
  auto* builder =
      MakeGarbageCollected<PaintRecordBuilder>(GetPaintController());
  auto& context = builder->Context();
  MockPaintCanvas canvas;
  cc::PaintFlags flags;
  {
    PaintControllerCycleScopeForTest cycle_scope(GetPaintController());
    InitRootChunk();

    EXPECT_EQ(&context.GetPaintController(), &GetPaintController());

    DrawRect(context, client, kBackgroundType, gfx::Rect(10, 10, 20, 20));
    DrawRect(context, client, kForegroundType, gfx::Rect(15, 15, 10, 10));
    EXPECT_FALSE(ClientCacheIsValid(client));

    EXPECT_CALL(canvas, drawPicture(_)).Times(1);
    builder->EndRecording(canvas);
  }
  EXPECT_TRUE(ClientCacheIsValid(client));

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(client.Id(), kBackgroundType),
                          IsSameId(client.Id(), kForegroundType)));

  {
    PaintControllerCycleScopeForTest cycle_scope(GetPaintController());
    InitRootChunk();
    EXPECT_TRUE(DrawingRecorder::UseCachedDrawingIfPossible(context, client,
                                                            kBackgroundType));
    EXPECT_TRUE(DrawingRecorder::UseCachedDrawingIfPossible(context, client,
                                                            kForegroundType));
    EXPECT_CALL(canvas, drawPicture(_)).Times(1);
    builder->EndRecording(canvas);
  }

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(client.Id(), kBackgroundType),
                          IsSameId(client.Id(), kForegroundType)));
  EXPECT_TRUE(ClientCacheIsValid(client));
}

TEST_F(PaintRecordBuilderTest, TransientAndAnotherPaintController) {
  GraphicsContext context(GetPaintController());
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>("client");
  auto* builder = MakeGarbageCollected<PaintRecordBuilder>();
  {
    PaintControllerCycleScopeForTest cycle_scope(GetPaintController());
    InitRootChunk();
    DrawRect(context, client, kBackgroundType, gfx::Rect(10, 10, 20, 20));
    DrawRect(context, client, kForegroundType, gfx::Rect(15, 15, 10, 10));
    GetPaintController().CommitNewDisplayItems();
  }
  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(client.Id(), kBackgroundType),
                          IsSameId(client.Id(), kForegroundType)));
  EXPECT_TRUE(ClientCacheIsValid(client));

  {
    PaintControllerCycleScopeForTest cycle_scope(GetPaintController());
    EXPECT_NE(&builder->Context().GetPaintController(), &GetPaintController());
    DrawRect(builder->Context(), client, kBackgroundType,
             gfx::Rect(10, 10, 20, 20));
    builder->EndRecording();
  }

  // The transient PaintController in PaintRecordBuilder doesn't affect the
  // client's cache status in another PaintController.
  EXPECT_TRUE(ClientCacheIsValid(client));
  EXPECT_FALSE(
      ClientCacheIsValid(builder->Context().GetPaintController(), client));
}

}  // namespace blink
