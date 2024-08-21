// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/subsequence_recorder.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

using testing::ElementsAre;

namespace blink {

// Death tests don't work properly on Android.
#if defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)

class PaintControllerUnderInvalidationTest
    : private ScopedPaintUnderInvalidationCheckingForTest,
      public PaintControllerTestBase {
 public:
  PaintControllerUnderInvalidationTest()
      : ScopedPaintUnderInvalidationCheckingForTest(true) {}
};

TEST_F(PaintControllerUnderInvalidationTest, ChangeDrawing) {
  auto test = [&]() {
    FakeDisplayItemClient& first =
        *MakeGarbageCollected<FakeDisplayItemClient>("first");
    {
      AutoCommitPaintController paint_controller(GetPersistentData());
      GraphicsContext context(paint_controller);
      InitRootChunk(paint_controller);
      DrawRect(context, first, kBackgroundType, gfx::Rect(1, 1, 1, 1));
      DrawRect(context, first, kForegroundType, gfx::Rect(1, 1, 3, 3));
    }

    {
      AutoCommitPaintController paint_controller(GetPersistentData());
      GraphicsContext context(paint_controller);
      InitRootChunk(paint_controller);
      DrawRect(context, first, kBackgroundType, gfx::Rect(2, 2, 3, 3));
      DrawRect(context, first, kForegroundType, gfx::Rect(1, 1, 3, 3));
    }
  };

  EXPECT_DEATH(test(),
               "Under-invalidation: display item changed\n"
#if DCHECK_IS_ON()
               ".*New display item:.*2,2 3x3.*\n"
               ".*Old display item:.*1,1 1x1"
#endif
  );
}

TEST_F(PaintControllerUnderInvalidationTest, MoreDrawing) {
  // We don't detect under-invalidation in this case, and PaintController can
  // also handle the case gracefully.
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");

  {
    AutoCommitPaintController paint_controller(GetPersistentData());
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, first, kBackgroundType, gfx::Rect(1, 1, 1, 1));
  }

  {
    AutoCommitPaintController paint_controller(GetPersistentData());
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, first, kBackgroundType, gfx::Rect(1, 1, 1, 1));
    DrawRect(context, first, kForegroundType, gfx::Rect(1, 1, 3, 3));
  }
}

TEST_F(PaintControllerUnderInvalidationTest, LessDrawing) {
  // We don't detect under-invalidation in this case, and PaintController can
  // also handle the case gracefully.
  FakeDisplayItemClient& first =
      *MakeGarbageCollected<FakeDisplayItemClient>("first");
  {
    AutoCommitPaintController paint_controller(GetPersistentData());
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, first, kBackgroundType, gfx::Rect(1, 1, 1, 1));
    DrawRect(context, first, kForegroundType, gfx::Rect(1, 1, 3, 3));
  }

  {
    AutoCommitPaintController paint_controller(GetPersistentData());
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    DrawRect(context, first, kBackgroundType, gfx::Rect(1, 1, 1, 1));
  }
}

TEST_F(PaintControllerUnderInvalidationTest, ChangeDrawingInSubsequence) {
  auto test = [&]() {
    FakeDisplayItemClient& first =
        *MakeGarbageCollected<FakeDisplayItemClient>("first");
    {
      AutoCommitPaintController paint_controller(GetPersistentData());
      GraphicsContext context(paint_controller);
      InitRootChunk(paint_controller);
      {
        SubsequenceRecorder r(context, first);
        DrawRect(context, first, kBackgroundType, gfx::Rect(1, 1, 1, 1));
        DrawRect(context, first, kForegroundType, gfx::Rect(1, 1, 3, 3));
      }
    }

    {
      AutoCommitPaintController paint_controller(GetPersistentData());
      GraphicsContext context(paint_controller);
      InitRootChunk(paint_controller);
      {
        EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
            context, first));
        SubsequenceRecorder r(context, first);
        DrawRect(context, first, kBackgroundType, gfx::Rect(2, 2, 1, 1));
        DrawRect(context, first, kForegroundType, gfx::Rect(1, 1, 3, 3));
      }
    }
  };

  EXPECT_DEATH(test(),
               "In cached subsequence for .*first.*\n"
               ".*Under-invalidation: display item changed\n"
#if DCHECK_IS_ON()
               ".*New display item:.*2,2 1x1.*\n"
               ".*Old display item:.*1,1 1x1"
#endif
  );
}

TEST_F(PaintControllerUnderInvalidationTest, MoreDrawingInSubsequence) {
  auto test = [&]() {
    FakeDisplayItemClient& first =
        *MakeGarbageCollected<FakeDisplayItemClient>("first");
    {
      AutoCommitPaintController paint_controller(GetPersistentData());
      GraphicsContext context(paint_controller);
      InitRootChunk(paint_controller);
      {
        SubsequenceRecorder r(context, first);
        DrawRect(context, first, kBackgroundType, gfx::Rect(1, 1, 1, 1));
      }
    }

    {
      AutoCommitPaintController paint_controller(GetPersistentData());
      GraphicsContext context(paint_controller);
      InitRootChunk(paint_controller);
      {
        EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
            context, first));
        SubsequenceRecorder r(context, first);
        DrawRect(context, first, kBackgroundType, gfx::Rect(1, 1, 1, 1));
        DrawRect(context, first, kForegroundType, gfx::Rect(1, 1, 3, 3));
      }
    }
  };

  EXPECT_DEATH(test(),
               "In cached subsequence for .*first.*\n"
               ".*Under-invalidation: extra display item\n"
#if DCHECK_IS_ON()
               ".*New display item:.*1,1 3x3"
#endif
  );
}

TEST_F(PaintControllerUnderInvalidationTest, LessDrawingInSubsequence) {
  auto test = [&]() {
    FakeDisplayItemClient& first =
        *MakeGarbageCollected<FakeDisplayItemClient>("first");
    {
      AutoCommitPaintController paint_controller(GetPersistentData());
      GraphicsContext context(paint_controller);
      InitRootChunk(paint_controller);
      {
        SubsequenceRecorder r(context, first);
        DrawRect(context, first, kBackgroundType, gfx::Rect(1, 1, 3, 3));
        DrawRect(context, first, kForegroundType, gfx::Rect(1, 1, 3, 3));
      }
    }

    {
      AutoCommitPaintController paint_controller(GetPersistentData());
      GraphicsContext context(paint_controller);
      InitRootChunk(paint_controller);
      {
        EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
            context, first));
        SubsequenceRecorder r(context, first);
        DrawRect(context, first, kBackgroundType, gfx::Rect(1, 1, 3, 3));
      }
    }
  };

  EXPECT_DEATH(test(),
               "In cached subsequence for .*first.*\n"
               ".*Under-invalidation: chunk changed");
}

TEST_F(PaintControllerUnderInvalidationTest, InvalidationInSubsequence) {
  // We allow invalidated display item clients as long as they would produce the
  // same display items. The cases of changed display items are tested by other
  // test cases.
  FakeDisplayItemClient& container =
      *MakeGarbageCollected<FakeDisplayItemClient>("container");
  FakeDisplayItemClient& content =
      *MakeGarbageCollected<FakeDisplayItemClient>("content");
  {
    AutoCommitPaintController paint_controller(GetPersistentData());
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    {
      SubsequenceRecorder r(context, container);
      DrawRect(context, content, kBackgroundType, gfx::Rect(1, 1, 3, 3));
    }
  }

  content.Invalidate();
  {
    AutoCommitPaintController paint_controller(GetPersistentData());
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    // Leave container not invalidated.
    {
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container));
      SubsequenceRecorder r(context, container);
      DrawRect(context, content, kBackgroundType, gfx::Rect(1, 1, 3, 3));
    }
  }
}

TEST_F(PaintControllerUnderInvalidationTest, SubsequenceBecomesEmpty) {
  auto test = [&]() {
    FakeDisplayItemClient& target =
        *MakeGarbageCollected<FakeDisplayItemClient>("target");
    {
      AutoCommitPaintController paint_controller(GetPersistentData());
      GraphicsContext context(paint_controller);
      InitRootChunk(paint_controller);
      {
        SubsequenceRecorder r(context, target);
        DrawRect(context, target, kBackgroundType, gfx::Rect(1, 1, 3, 3));
      }
    }

    {
      AutoCommitPaintController paint_controller(GetPersistentData());
      GraphicsContext context(paint_controller);
      InitRootChunk(paint_controller);
      {
        EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
            context, target));
        SubsequenceRecorder r(context, target);
      }
    }
  };

  EXPECT_DEATH(test(),
               "In cached subsequence for .*target.*\n"
               ".*Under-invalidation: new subsequence wrong length");
}

TEST_F(PaintControllerUnderInvalidationTest, SkipCacheInSubsequence) {
  FakeDisplayItemClient& container =
      *MakeGarbageCollected<FakeDisplayItemClient>("container");
  FakeDisplayItemClient& content =
      *MakeGarbageCollected<FakeDisplayItemClient>("content");
  {
    AutoCommitPaintController paint_controller(GetPersistentData());
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    {
      SubsequenceRecorder r(context, container);
      {
        DisplayItemCacheSkipper cache_skipper(context);
        DrawRect(context, content, kBackgroundType, gfx::Rect(1, 1, 3, 3));
      }
      DrawRect(context, content, kForegroundType, gfx::Rect(2, 2, 4, 4));
    }
  }

  {
    AutoCommitPaintController paint_controller(GetPersistentData());
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    {
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container));
      SubsequenceRecorder r(context, container);
      {
        DisplayItemCacheSkipper cache_skipper(context);
        DrawRect(context, content, kBackgroundType, gfx::Rect(2, 2, 4, 4));
      }
      DrawRect(context, content, kForegroundType, gfx::Rect(2, 2, 4, 4));
    }
  }
}

TEST_F(PaintControllerUnderInvalidationTest,
       EmptySubsequenceInCachedSubsequence) {
  FakeDisplayItemClient& container =
      *MakeGarbageCollected<FakeDisplayItemClient>("container");
  FakeDisplayItemClient& content =
      *MakeGarbageCollected<FakeDisplayItemClient>("content");
  {
    AutoCommitPaintController paint_controller(GetPersistentData());
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    {
      SubsequenceRecorder r(context, container);
      DrawRect(context, container, kBackgroundType, gfx::Rect(1, 1, 3, 3));
      { SubsequenceRecorder r1(context, content); }
      DrawRect(context, container, kForegroundType, gfx::Rect(1, 1, 3, 3));
    }
  }

  {
    AutoCommitPaintController paint_controller(GetPersistentData());
    GraphicsContext context(paint_controller);
    InitRootChunk(paint_controller);
    {
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, container));
      SubsequenceRecorder r(context, container);
      DrawRect(context, container, kBackgroundType, gfx::Rect(1, 1, 3, 3));
      EXPECT_FALSE(SubsequenceRecorder::UseCachedSubsequenceIfPossible(
          context, content));
      { SubsequenceRecorder r1(context, content); }
      DrawRect(context, container, kForegroundType, gfx::Rect(1, 1, 3, 3));
    }
  }
}

#endif  // defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)

}  // namespace blink
