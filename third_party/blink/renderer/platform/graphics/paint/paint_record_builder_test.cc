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
  PaintRecordBuilder builder;
  auto& context = builder.Context();
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>("client");
  DrawRect(context, client, kBackgroundType, gfx::Rect(10, 10, 20, 20));
  DrawRect(context, client, kForegroundType, gfx::Rect(15, 15, 10, 10));
  EXPECT_FALSE(ClientCacheIsValid(context.GetPaintController(), client));

  EXPECT_THAT(
      GetNewPaintArtifact(context.GetPaintController()).GetDisplayItemList(),
      ElementsAre(IsSameId(client.Id(), kBackgroundType),
                  IsSameId(client.Id(), kForegroundType)));

  MockPaintCanvas canvas;
  cc::PaintFlags flags;
  EXPECT_CALL(canvas, drawPicture(_)).Times(1);
  builder.EndRecording(canvas);
}

TEST_F(PaintRecordBuilderTest, TransientAndAnotherPaintController) {
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>("client");
  PaintRecordBuilder builder;
  {
    AutoCommitPaintController paint_controller(GetPersistentData());
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, client, kBackgroundType, gfx::Rect(10, 10, 20, 20));
    DrawRect(context, client, kForegroundType, gfx::Rect(15, 15, 10, 10));
  }
  EXPECT_THAT(GetPersistentData().GetDisplayItemList(),
              ElementsAre(IsSameId(client.Id(), kBackgroundType),
                          IsSameId(client.Id(), kForegroundType)));
  EXPECT_TRUE(ClientCacheIsValid(client));

  {
    AutoCommitPaintController paint_controller(GetPersistentData());
    EXPECT_TRUE(ClientCacheIsValid(client));
    GraphicsContext context(paint_controller);
    EXPECT_NE(&builder.Context().GetPaintController(), &paint_controller);
    DrawRect(builder.Context(), client, kBackgroundType,
             gfx::Rect(10, 10, 20, 20));
    builder.EndRecording();
  }

  // The transient PaintController in PaintRecordBuilder doesn't affect the
  // client's cache status in the persistent data.
  EXPECT_TRUE(GetPersistentData().ClientCacheIsValid(client));
  EXPECT_FALSE(
      ClientCacheIsValid(builder.Context().GetPaintController(), client));
}

}  // namespace blink
