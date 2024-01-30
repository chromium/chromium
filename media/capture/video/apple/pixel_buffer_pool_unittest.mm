// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/apple/pixel_buffer_pool.h"

#include "base/functional/bind.h"
#import "media/capture/video/apple/test/video_capture_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

// NV12, also known as 420v, also known as media::PIXEL_FORMAT_NV12.
constexpr OSType kPixelFormatNv12 =
    kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
// A common 4:3 resolution.
constexpr int kVgaWidth = 640;
constexpr int kVgaHeight = 480;

}  // namespace

TEST(PixelBufferPoolTest, CannotCreatePoolWithNonsenseArguments) {
  EXPECT_FALSE(PixelBufferPool::Create(0, -1, -1, 1));
}

TEST(PixelBufferPoolTest, CreatedBufferHasSpecifiedAttributes) {
  std::unique_ptr<PixelBufferPool> pool =
      PixelBufferPool::Create(kPixelFormatNv12, kVgaWidth, kVgaHeight, 1);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> buffer = pool->CreateBuffer();
  EXPECT_TRUE(CVPixelBufferGetPixelFormatType(buffer.get()) ==
              kPixelFormatNv12);
  EXPECT_EQ(CVPixelBufferGetWidth(buffer.get()),
            static_cast<size_t>(kVgaWidth));
  EXPECT_EQ(CVPixelBufferGetHeight(buffer.get()),
            static_cast<size_t>(kVgaHeight));
}

TEST(PixelBufferPoolTest, CreatedBufferHasIOSurface) {
  std::unique_ptr<PixelBufferPool> pool =
      PixelBufferPool::Create(kPixelFormatNv12, kVgaWidth, kVgaHeight, 1);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> buffer = pool->CreateBuffer();
  EXPECT_TRUE(CVPixelBufferGetIOSurface(buffer.get()));
}

TEST(PixelBufferPoolTest, CannotExceedMaxBuffersWhenHoldingOnToPixelBuffer) {
  constexpr size_t kPoolMaxBuffers = 2;
  std::unique_ptr<PixelBufferPool> pool = PixelBufferPool::Create(
      kPixelFormatNv12, kVgaWidth, kVgaHeight, kPoolMaxBuffers);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> first_buffer =
      pool->CreateBuffer();
  EXPECT_TRUE(first_buffer);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> second_buffer =
      pool->CreateBuffer();
  EXPECT_TRUE(second_buffer);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> third_buffer =
      pool->CreateBuffer();
  EXPECT_FALSE(third_buffer);
}

TEST(PixelBufferPoolTest, CannotExceedMaxBuffersWhenIOSurfaceIsInUse) {
  constexpr size_t kPoolMaxBuffers = 1;
  std::unique_ptr<PixelBufferPool> pool = PixelBufferPool::Create(
      kPixelFormatNv12, kVgaWidth, kVgaHeight, kPoolMaxBuffers);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> first_buffer =
      pool->CreateBuffer();
  EXPECT_TRUE(first_buffer);
  IOSurfaceRef io_surface = CVPixelBufferGetIOSurface(first_buffer.get());
  EXPECT_TRUE(io_surface);
  // Increment use count of raw ptr IOSurface reference while releasing the
  // pixel buffer's only reference.
  IOSurfaceIncrementUseCount(io_surface);
  first_buffer.reset();
  // The pixel buffer has not been returned to the pool.
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> second_buffer =
      pool->CreateBuffer();
  EXPECT_FALSE(second_buffer);
  // Cleanup.
  IOSurfaceDecrementUseCount(io_surface);
}

TEST(PixelBufferPoolTest, CanCreateBuffersIfMaxIsNull) {
  std::unique_ptr<PixelBufferPool> pool = PixelBufferPool::Create(
      kPixelFormatNv12, kVgaWidth, kVgaHeight, std::nullopt);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> first_buffer =
      pool->CreateBuffer();
  EXPECT_TRUE(first_buffer);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> second_buffer =
      pool->CreateBuffer();
  EXPECT_TRUE(second_buffer);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> third_buffer =
      pool->CreateBuffer();
  EXPECT_TRUE(third_buffer);
}

TEST(PixelBufferPoolTest, CanCreateBufferAfterPreviousBufferIsReleased) {
  constexpr size_t kPoolMaxBuffers = 1;
  std::unique_ptr<PixelBufferPool> pool = PixelBufferPool::Create(
      kPixelFormatNv12, kVgaWidth, kVgaHeight, kPoolMaxBuffers);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> buffer = pool->CreateBuffer();
  buffer.reset();
  buffer = pool->CreateBuffer();
  EXPECT_TRUE(buffer.get());
}

TEST(PixelBufferPoolTest, CanCreateBufferAfterPreviousIOSurfaceIsNoLongerUsed) {
  constexpr size_t kPoolMaxBuffers = 1;
  std::unique_ptr<PixelBufferPool> pool = PixelBufferPool::Create(
      kPixelFormatNv12, kVgaWidth, kVgaHeight, kPoolMaxBuffers);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> first_buffer =
      pool->CreateBuffer();
  EXPECT_TRUE(first_buffer);
  IOSurfaceRef io_surface = CVPixelBufferGetIOSurface(first_buffer.get());
  EXPECT_TRUE(io_surface);
  IOSurfaceIncrementUseCount(io_surface);
  first_buffer.reset();
  // Decrementing the use count when there are no pixel buffer references
  // returns it to the pool.
  IOSurfaceDecrementUseCount(io_surface);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> second_buffer =
      pool->CreateBuffer();
  EXPECT_TRUE(second_buffer);
}

TEST(PixelBufferPoolTest,
     SimplyReferencingAnIOSurfaceDoesNotPreventItReturningToThePool) {
  constexpr size_t kPoolMaxBuffers = 1;
  std::unique_ptr<PixelBufferPool> pool = PixelBufferPool::Create(
      kPixelFormatNv12, kVgaWidth, kVgaHeight, kPoolMaxBuffers);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> first_buffer =
      pool->CreateBuffer();
  EXPECT_TRUE(first_buffer);
  base::apple::ScopedCFTypeRef<IOSurfaceRef> first_buffer_io_surface(
      CVPixelBufferGetIOSurface(first_buffer.get()),
      base::scoped_policy::RETAIN);
  EXPECT_TRUE(first_buffer_io_surface);
  // Releasing the first buffer returns it to the pool, despite the IOSurface
  // still being referenced by |first_buffer_io_surface|.
  first_buffer.reset();

  base::apple::ScopedCFTypeRef<CVPixelBufferRef> second_buffer =
      pool->CreateBuffer();
  EXPECT_TRUE(second_buffer);
  base::apple::ScopedCFTypeRef<IOSurfaceRef> second_buffer_io_surface(
      CVPixelBufferGetIOSurface(second_buffer.get()),
      base::scoped_policy::RETAIN);
  EXPECT_TRUE(second_buffer_io_surface);

  // Because this is a recycled pixel buffer, the IOSurface is also recycled.
  EXPECT_EQ(IOSurfaceGetID(first_buffer_io_surface.get()),
            IOSurfaceGetID(second_buffer_io_surface.get()));
}

TEST(PixelBufferPoolTest, RecreatePoolAndObserveRecycledIOSurfaceID) {
  constexpr size_t kPoolMaxBuffers = 1;
  std::unique_ptr<PixelBufferPool> pool = PixelBufferPool::Create(
      kPixelFormatNv12, kVgaWidth, kVgaHeight, kPoolMaxBuffers);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> first_buffer =
      pool->CreateBuffer();
  EXPECT_TRUE(first_buffer);
  IOSurfaceID first_buffer_id =
      IOSurfaceGetID(CVPixelBufferGetIOSurface(first_buffer.get()));

  // Free references and recreate the pool. There is nothing preventing the
  // IOSurfaceID from being recycled, even by a different CVPixelBufferPool with
  // a different resolution!
  first_buffer.reset();
  pool = PixelBufferPool::Create(kPixelFormatNv12, kVgaWidth / 2,
                                 kVgaHeight / 2, kPoolMaxBuffers);

  base::apple::ScopedCFTypeRef<CVPixelBufferRef> second_buffer =
      pool->CreateBuffer();
  EXPECT_TRUE(second_buffer);
  IOSurfaceID second_buffer_id =
      IOSurfaceGetID(CVPixelBufferGetIOSurface(second_buffer.get()));

  // The new pool is allowed to recycle the old IOSurface ID.
  //
  // This test documents "foot gun" behavior that is not documented by Apple
  // anywhere. If the test starts failing, it may be because this behavior is
  // specific to version or hardware. In such cases, feel free to disable the
  // test.
  EXPECT_EQ(first_buffer_id, second_buffer_id);
}

TEST(PixelBufferPoolTest, BuffersCanOutliveThePool) {
  std::unique_ptr<PixelBufferPool> pool =
      PixelBufferPool::Create(kPixelFormatNv12, kVgaWidth, kVgaHeight, 1);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> buffer = pool->CreateBuffer();
  pool.reset();
  EXPECT_TRUE(CVPixelBufferGetPixelFormatType(buffer.get()) ==
              kPixelFormatNv12);
  EXPECT_EQ(CVPixelBufferGetWidth(buffer.get()),
            static_cast<size_t>(kVgaWidth));
  EXPECT_EQ(CVPixelBufferGetHeight(buffer.get()),
            static_cast<size_t>(kVgaHeight));
  EXPECT_TRUE(CVPixelBufferGetIOSurface(buffer.get()));
}

TEST(PixelBufferPoolTest, CanFlushWhileBufferIsInUse) {
  std::unique_ptr<PixelBufferPool> pool = PixelBufferPool::Create(
      kPixelFormatNv12, kVgaWidth, kVgaHeight, std::nullopt);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> retained_buffer =
      pool->CreateBuffer();
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> released_buffer =
      pool->CreateBuffer();
  released_buffer.reset();
  // We expect the memory of |released_buffer| to be freed now, but there is no
  // way to assert this in a unittest.
  pool->Flush();
  // We expect |retained_buffer| is still usable. Inspecting its properties.
  EXPECT_TRUE(CVPixelBufferGetPixelFormatType(retained_buffer.get()) ==
              kPixelFormatNv12);
  EXPECT_EQ(CVPixelBufferGetWidth(retained_buffer.get()),
            static_cast<size_t>(kVgaWidth));
  EXPECT_EQ(CVPixelBufferGetHeight(retained_buffer.get()),
            static_cast<size_t>(kVgaHeight));
  EXPECT_TRUE(CVPixelBufferGetIOSurface(retained_buffer.get()));
}

}  // namespace media
