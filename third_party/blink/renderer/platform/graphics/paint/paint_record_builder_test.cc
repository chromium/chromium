// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"

#include "third_party/blink/renderer/platform/graphics/paint/paint_controller_test.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_paint_canvas.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"

using testing::_;
using testing::ElementsAre;

namespace blink {

using PaintRecordBuilderTest = PaintControllerTestBase;

TEST_F(PaintRecordBuilderTest, TransientPaintController) {
  PaintRecordBuilder builder;
  auto& context = builder.Context();
  FakeDisplayItemClient client("client", IntRect(10, 10, 20, 20));
  DrawRect(context, client, kBackgroundType, FloatRect(10, 10, 20, 20));
  DrawRect(context, client, kForegroundType, FloatRect(15, 15, 10, 10));
  EXPECT_FALSE(ClientCacheIsValid(context.GetPaintController(), client));

  MockPaintCanvas canvas;
  PaintFlags flags;
  EXPECT_CALL(canvas, drawPicture(_)).Times(1);
  builder.EndRecording(canvas);

  EXPECT_THAT(context.GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&client, kBackgroundType),
                          IsSameId(&client, kForegroundType)));
  EXPECT_FALSE(ClientCacheIsValid(context.GetPaintController(), client));
}

TEST_F(PaintRecordBuilderTest, LastingPaintController) {
  InitRootChunk();

  PaintRecordBuilder builder(nullptr, nullptr, &GetPaintController());
  auto& context = builder.Context();
  EXPECT_EQ(&context.GetPaintController(), &GetPaintController());

  FakeDisplayItemClient client("client", IntRect(10, 10, 20, 20));
  DrawRect(context, client, kBackgroundType, FloatRect(10, 10, 20, 20));
  DrawRect(context, client, kForegroundType, FloatRect(15, 15, 10, 10));
  EXPECT_FALSE(ClientCacheIsValid(client));

  MockPaintCanvas canvas;
  PaintFlags flags;
  EXPECT_CALL(canvas, drawPicture(_)).Times(1);
  builder.EndRecording(canvas);
  EXPECT_TRUE(ClientCacheIsValid(client));

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&client, kBackgroundType),
                          IsSameId(&client, kForegroundType)));

  InitRootChunk();
  EXPECT_TRUE(DrawingRecorder::UseCachedDrawingIfPossible(context, client,
                                                          kBackgroundType));
  EXPECT_TRUE(DrawingRecorder::UseCachedDrawingIfPossible(context, client,
                                                          kForegroundType));
  EXPECT_CALL(canvas, drawPicture(_)).Times(1);
  builder.EndRecording(canvas);

  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&client, kBackgroundType),
                          IsSameId(&client, kForegroundType)));
  EXPECT_TRUE(ClientCacheIsValid(client));
}

TEST_F(PaintRecordBuilderTest, TransientAndAnotherPaintController) {
  GraphicsContext context(GetPaintController());

  InitRootChunk();
  FakeDisplayItemClient client("client", IntRect(10, 10, 20, 20));
  DrawRect(context, client, kBackgroundType, FloatRect(10, 10, 20, 20));
  DrawRect(context, client, kForegroundType, FloatRect(15, 15, 10, 10));
  CommitAndFinishCycle();
  EXPECT_THAT(GetPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&client, kBackgroundType),
                          IsSameId(&client, kForegroundType)));
  // EXPECT_TRUE(ClientCacheIsValid(client));

  PaintRecordBuilder builder;
  EXPECT_NE(&builder.Context().GetPaintController(), &GetPaintController());
  DrawRect(builder.Context(), client, kBackgroundType,
           FloatRect(10, 10, 20, 20));
  builder.EndRecording();

  // The transient PaintController in PaintRecordBuilder doesn't affect the
  // client's cache status in another PaintController.
  // EXPECT_TRUE(ClientCacheIsValid(client));
  // EXPECT_FALSE(
  //    ClientCacheIsValid(builder.Context().GetPaintController(), client));
}

}  // namespace blink
