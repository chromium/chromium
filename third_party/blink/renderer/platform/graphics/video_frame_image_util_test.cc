// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "media/base/video_frame.h"
#include "media/renderers/shared_image_video_frame_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"

namespace blink {

namespace {

constexpr auto kTestSize = gfx::Size(64, 64);

class ScopedFakeGpuContext {
 public:
  explicit ScopedFakeGpuContext(bool disable_imagebitmap) {
    SharedGpuContext::ResetForTesting();
    test_context_provider_ = viz::TestContextProvider::Create();

    if (disable_imagebitmap) {
      // Disable CanvasResourceProvider using GPU.
      auto& feature_info = test_context_provider_->GetWritableGpuFeatureInfo();
      feature_info.enabled_gpu_driver_bug_workarounds.push_back(
          DISABLE_IMAGEBITMAP_FROM_VIDEO_USING_GPU);
    }

    InitializeSharedGpuContext(test_context_provider_.get());
  }

  scoped_refptr<viz::ContextProvider> context_provider() const {
    return test_context_provider_;
  }

  viz::RasterContextProvider* raster_context_provider() const {
    return test_context_provider_.get();
  }

  ~ScopedFakeGpuContext() {
    task_environment_.RunUntilIdle();
    SharedGpuContext::ResetForTesting();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
};

}  // namespace

TEST(VideoFrameImageUtilTest, WillCreateAcceleratedImagesFromVideoFrame) {
  // I420A isn't a supported zero copy format.
  {
    auto alpha_frame = media::VideoFrame::CreateTransparentFrame(kTestSize);
    EXPECT_FALSE(WillCreateAcceleratedImagesFromVideoFrame(alpha_frame.get()));
  }

  // Software RGB frames aren't supported.
  {
    auto cpu_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                     media::VideoFrame::STORAGE_OWNED_MEMORY,
                                     media::PIXEL_FORMAT_XRGB);
    EXPECT_FALSE(WillCreateAcceleratedImagesFromVideoFrame(cpu_frame.get()));
  }

  // GpuMemoryBuffer frames aren't supported.
  {
    auto cpu_frame = CreateTestFrame(
        kTestSize, gfx::Rect(kTestSize), kTestSize,
        media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER, media::PIXEL_FORMAT_XRGB);
    EXPECT_FALSE(WillCreateAcceleratedImagesFromVideoFrame(cpu_frame.get()));
  }

  // Single mailbox shared images should be supported except on Android.
  {
    auto shared_image_frame = CreateTestFrame(
        kTestSize, gfx::Rect(kTestSize), kTestSize,
        media::VideoFrame::STORAGE_OPAQUE, media::PIXEL_FORMAT_XRGB);
    EXPECT_EQ(shared_image_frame->NumTextures(), 1u);
    EXPECT_TRUE(shared_image_frame->mailbox_holder(0).mailbox.IsSharedImage());
#if defined(OS_ANDROID)
    EXPECT_FALSE(
        WillCreateAcceleratedImagesFromVideoFrame(shared_image_frame.get()));
#else
    EXPECT_TRUE(
        WillCreateAcceleratedImagesFromVideoFrame(shared_image_frame.get()));
#endif
  }
}

// Android doesn't support zero copy images.
#if !defined(OS_ANDROID)
TEST(VideoFrameImageUtilTest, CreateImageFromVideoFrameZeroCopy) {
  ScopedFakeGpuContext fake_context(/*disable_imagebitmap=*/false);
  auto shared_image_frame = CreateTestFrame(
      kTestSize, gfx::Rect(kTestSize), kTestSize,
      media::VideoFrame::STORAGE_OPAQUE, media::PIXEL_FORMAT_XRGB);
  EXPECT_EQ(shared_image_frame->NumTextures(), 1u);
  EXPECT_TRUE(shared_image_frame->mailbox_holder(0).mailbox.IsSharedImage());

  auto image = CreateImageFromVideoFrame(shared_image_frame);
  ASSERT_TRUE(image->IsTextureBacked());
  EXPECT_EQ(memcmp(image->GetMailboxHolder().mailbox.name,
                   shared_image_frame->mailbox_holder(0).mailbox.name,
                   sizeof(gpu::Mailbox::Name)),
            0);
}
#endif

TEST(VideoFrameImageUtilTest, CreateImageFromVideoFrameSoftwareFrame) {
  base::test::SingleThreadTaskEnvironment task_environment_;
  auto cpu_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_OWNED_MEMORY,
                                   media::PIXEL_FORMAT_XRGB);
  auto image = CreateImageFromVideoFrame(cpu_frame);
  ASSERT_FALSE(image->IsTextureBacked());
  task_environment_.RunUntilIdle();
}

// TODO(crbug.com/1183572): Re-enable this test once we can map GpuMemoryBuffers
// again.
TEST(VideoFrameImageUtilTest,
     DISABLED_CreateImageFromVideoFrameGpuMemoryBufferFrame) {
  base::test::SingleThreadTaskEnvironment task_environment_;
  auto cpu_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
                                   media::PIXEL_FORMAT_NV12);
  auto image = CreateImageFromVideoFrame(cpu_frame);
  ASSERT_FALSE(image->IsTextureBacked());
  task_environment_.RunUntilIdle();
}

TEST(VideoFrameImageUtilTest, CreateImageFromVideoFrameTextureFrame) {
  base::test::SingleThreadTaskEnvironment task_environment_;
  auto cpu_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_OPAQUE,
                                   media::PIXEL_FORMAT_NV12);
  auto image = CreateImageFromVideoFrame(cpu_frame);

  // An unaccelerated image can't be created from a texture based VideoFrame
  // without a viz::RasterContextProvider.
  ASSERT_FALSE(image);
  task_environment_.RunUntilIdle();
}

TEST(VideoFrameImageUtilTest,
     CreateAcceleratedImageFromVideoFrameBasicSoftwareFrame) {
  ScopedFakeGpuContext fake_context(/*disable_imagebitmap=*/false);
  auto cpu_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_OWNED_MEMORY,
                                   media::PIXEL_FORMAT_XRGB);
  auto image = CreateImageFromVideoFrame(cpu_frame);
  ASSERT_TRUE(image->IsTextureBacked());
}

TEST(VideoFrameImageUtilTest, CreateAcceleratedImageFromGpuMemoryBufferFrame) {
  ScopedFakeGpuContext fake_context(/*disable_imagebitmap=*/false);
  auto gmb_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
                                   media::PIXEL_FORMAT_NV12);
  auto image = CreateImageFromVideoFrame(gmb_frame);
  ASSERT_TRUE(image->IsTextureBacked());
}

TEST(VideoFrameImageUtilTest, CreateAcceleratedImageFromTextureFrame) {
  ScopedFakeGpuContext fake_context(/*disable_imagebitmap=*/false);

  auto texture_frame = media::CreateSharedImageRGBAFrame(
      fake_context.context_provider(), kTestSize, gfx::Rect(kTestSize),
      base::DoNothing::Once());
  auto image = CreateImageFromVideoFrame(texture_frame,
                                         /*allow_zero_copy_images=*/false);
  ASSERT_TRUE(image->IsTextureBacked());
}

TEST(VideoFrameImageUtilTest, SoftwareCreateResourceProviderForVideoFrame) {
  // Creating a provider with a null viz::RasterContextProvider should result in
  // a non-accelerated provider being created.
  auto provider =
      CreateResourceProviderForVideoFrame(IntSize(kTestSize), nullptr);
  ASSERT_TRUE(provider);
  EXPECT_FALSE(provider->IsAccelerated());
}

TEST(VideoFrameImageUtilTest, AcceleratedCreateResourceProviderForVideoFrame) {
  ScopedFakeGpuContext fake_context(/*disable_imagebitmap=*/false);
  ASSERT_TRUE(SharedGpuContext::IsGpuCompositingEnabled());

  auto* raster_context_provider = fake_context.raster_context_provider();
  ASSERT_TRUE(raster_context_provider);

  // Creating a provider with a null viz::RasterContextProvider should result in
  // a non-accelerated provider being created.
  {
    auto provider =
        CreateResourceProviderForVideoFrame(IntSize(kTestSize), nullptr);
    ASSERT_TRUE(provider);
    EXPECT_FALSE(provider->IsAccelerated());
  }

  // Creating a provider with a real raster context provider should result in
  // an accelerated provider being created.
  {
    auto provider = CreateResourceProviderForVideoFrame(
        IntSize(kTestSize), raster_context_provider);
    ASSERT_TRUE(provider);
    EXPECT_TRUE(provider->IsAccelerated());
  }
}

TEST(VideoFrameImageUtilTest, WorkaroundCreateResourceProviderForVideoFrame) {
  ScopedFakeGpuContext fake_context(/*disable_imagebitmap=*/true);
  ASSERT_TRUE(SharedGpuContext::IsGpuCompositingEnabled());

  auto* raster_context_provider = fake_context.raster_context_provider();
  ASSERT_TRUE(raster_context_provider);

  // Creating a provider with a real raster context provider should result in
  // an unaccelerated provider being created due to the workaround.
  {
    auto provider = CreateResourceProviderForVideoFrame(
        IntSize(kTestSize), raster_context_provider);
    ASSERT_TRUE(provider);
    EXPECT_FALSE(provider->IsAccelerated());
  }
}

TEST(VideoFrameImageUtilTest, DestRectWithoutCanvasResourceProvider) {
  base::test::SingleThreadTaskEnvironment task_environment_;
  auto cpu_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_OWNED_MEMORY,
                                   media::PIXEL_FORMAT_XRGB);

  // A CanvasResourceProvider must be provided with a custom destination rect.
  auto image = CreateImageFromVideoFrame(cpu_frame, true, nullptr, nullptr,
                                         gfx::Rect(0, 0, 10, 10));
  ASSERT_FALSE(image);
  task_environment_.RunUntilIdle();
}

TEST(VideoFrameImageUtilTest, CanvasResourceProviderTooSmallForDestRect) {
  base::test::SingleThreadTaskEnvironment task_environment_;
  auto cpu_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_OWNED_MEMORY,
                                   media::PIXEL_FORMAT_XRGB);

  auto provider =
      CreateResourceProviderForVideoFrame(IntSize(gfx::Size(16, 16)), nullptr);
  ASSERT_TRUE(provider);
  EXPECT_FALSE(provider->IsAccelerated());

  auto image = CreateImageFromVideoFrame(cpu_frame, true, provider.get(),
                                         nullptr, gfx::Rect(kTestSize));
  ASSERT_FALSE(image);
  task_environment_.RunUntilIdle();
}

TEST(VideoFrameImageUtilTest, CanvasResourceProviderDestRect) {
  base::test::SingleThreadTaskEnvironment task_environment_;
  auto cpu_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_OWNED_MEMORY,
                                   media::PIXEL_FORMAT_XRGB);

  auto provider = CreateResourceProviderForVideoFrame(
      IntSize(gfx::Size(128, 128)), nullptr);
  ASSERT_TRUE(provider);
  EXPECT_FALSE(provider->IsAccelerated());

  auto image = CreateImageFromVideoFrame(cpu_frame, true, provider.get(),
                                         nullptr, gfx::Rect(16, 16, 64, 64));
  ASSERT_TRUE(image);
  task_environment_.RunUntilIdle();
}

}  // namespace blink
