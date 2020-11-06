// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/pixel_buffer_pool_mac.h"

#include "base/bind.h"
#include "base/mac/scoped_nsobject.h"
#import "media/capture/video/mac/test/video_capture_test_utils_mac.h"
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
  base::ScopedCFTypeRef<CVPixelBufferRef> buffer = pool->CreateBuffer();
  EXPECT_TRUE(CVPixelBufferGetPixelFormatType(buffer) == kPixelFormatNv12);
  EXPECT_EQ(CVPixelBufferGetWidth(buffer), static_cast<size_t>(kVgaWidth));
  EXPECT_EQ(CVPixelBufferGetHeight(buffer), static_cast<size_t>(kVgaHeight));
}

TEST(PixelBufferPoolTest, CreatedBufferHasIOSurface) {
  std::unique_ptr<PixelBufferPool> pool =
      PixelBufferPool::Create(kPixelFormatNv12, kVgaWidth, kVgaHeight, 1);
  base::ScopedCFTypeRef<CVPixelBufferRef> buffer = pool->CreateBuffer();
  EXPECT_TRUE(CVPixelBufferGetIOSurface(buffer));
}

TEST(PixelBufferPoolTest, CannotExceedMaxBuffers) {
  std::unique_ptr<PixelBufferPool> pool =
      PixelBufferPool::Create(kPixelFormatNv12, kVgaWidth, kVgaHeight, 2);
  base::ScopedCFTypeRef<CVPixelBufferRef> first_buffer = pool->CreateBuffer();
  EXPECT_TRUE(first_buffer);
  base::ScopedCFTypeRef<CVPixelBufferRef> second_buffer = pool->CreateBuffer();
  EXPECT_TRUE(second_buffer);
  base::ScopedCFTypeRef<CVPixelBufferRef> third_buffer = pool->CreateBuffer();
  EXPECT_FALSE(third_buffer);
}

TEST(PixelBufferPoolTest, CanCreateBuffersIfMaxIsNull) {
  std::unique_ptr<PixelBufferPool> pool = PixelBufferPool::Create(
      kPixelFormatNv12, kVgaWidth, kVgaHeight, base::nullopt);
  base::ScopedCFTypeRef<CVPixelBufferRef> first_buffer = pool->CreateBuffer();
  EXPECT_TRUE(first_buffer);
  base::ScopedCFTypeRef<CVPixelBufferRef> second_buffer = pool->CreateBuffer();
  EXPECT_TRUE(second_buffer);
  base::ScopedCFTypeRef<CVPixelBufferRef> third_buffer = pool->CreateBuffer();
  EXPECT_TRUE(third_buffer);
}

TEST(PixelBufferPoolTest, CanCreateBufferAfterPreviousBufferIsReleased) {
  std::unique_ptr<PixelBufferPool> pool =
      PixelBufferPool::Create(kPixelFormatNv12, kVgaWidth, kVgaHeight, 1);
  base::ScopedCFTypeRef<CVPixelBufferRef> buffer = pool->CreateBuffer();
  buffer.reset();
  buffer = pool->CreateBuffer();
  EXPECT_TRUE(buffer);
}

TEST(PixelBufferPoolTest, BuffersCanOutliveThePool) {
  std::unique_ptr<PixelBufferPool> pool =
      PixelBufferPool::Create(kPixelFormatNv12, kVgaWidth, kVgaHeight, 1);
  base::ScopedCFTypeRef<CVPixelBufferRef> buffer = pool->CreateBuffer();
  pool.reset();
  EXPECT_TRUE(CVPixelBufferGetPixelFormatType(buffer) == kPixelFormatNv12);
  EXPECT_EQ(CVPixelBufferGetWidth(buffer), static_cast<size_t>(kVgaWidth));
  EXPECT_EQ(CVPixelBufferGetHeight(buffer), static_cast<size_t>(kVgaHeight));
  EXPECT_TRUE(CVPixelBufferGetIOSurface(buffer));
}

TEST(PixelBufferPoolTest, CanFlushWhileBufferIsInUse) {
  std::unique_ptr<PixelBufferPool> pool = PixelBufferPool::Create(
      kPixelFormatNv12, kVgaWidth, kVgaHeight, base::nullopt);
  base::ScopedCFTypeRef<CVPixelBufferRef> retained_buffer =
      pool->CreateBuffer();
  base::ScopedCFTypeRef<CVPixelBufferRef> released_buffer =
      pool->CreateBuffer();
  released_buffer.reset();
  // We expect the memory of |released_buffer| to be freed now, but there is no
  // way to assert this in a unittest.
  pool->Flush();
  // We expect |retained_buffer| is still usable. Inspecting its properties.
  EXPECT_TRUE(CVPixelBufferGetPixelFormatType(retained_buffer) ==
              kPixelFormatNv12);
  EXPECT_EQ(CVPixelBufferGetWidth(retained_buffer),
            static_cast<size_t>(kVgaWidth));
  EXPECT_EQ(CVPixelBufferGetHeight(retained_buffer),
            static_cast<size_t>(kVgaHeight));
  EXPECT_TRUE(CVPixelBufferGetIOSurface(retained_buffer));
}

}  // namespace media
