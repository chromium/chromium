// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"

#include "base/functional/callback_helpers.h"
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
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"
#include "third_party/skia/include/gpu/ganesh/GrDriverBugWorkarounds.h"

namespace blink {

namespace {

constexpr auto kTestSize = gfx::Size(64, 64);
const auto kTestInfo = SkImageInfo::MakeN32Premul(64, 64);

class AcceleratedCompositingTestPlatform
    : public blink::TestingPlatformSupport {
 public:
  bool IsGpuCompositingDisabled() const override { return false; }
};

class ScopedFakeGpuContext {
 public:
  explicit ScopedFakeGpuContext(bool disable_imagebitmap) {
    SharedGpuContext::Reset();
    test_context_provider_ = viz::TestContextProvider::Create();

    if (disable_imagebitmap) {
      // Disable CanvasResourceProvider using GPU.
      auto& feature_info = test_context_provider_->GetWritableGpuFeatureInfo();
      feature_info.enabled_gpu_driver_bug_workarounds.push_back(
          DISABLE_IMAGEBITMAP_FROM_VIDEO_USING_GPU);
    }

    InitializeSharedGpuContextGLES2(test_context_provider_.get());
  }

  scoped_refptr<viz::ContextProvider> context_provider() const {
    return test_context_provider_;
  }

  viz::RasterContextProvider* raster_context_provider() const {
    return test_context_provider_.get();
  }

  ~ScopedFakeGpuContext() {
    task_environment_.RunUntilIdle();
    SharedGpuContext::Reset();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  ScopedTestingPlatformSupport<AcceleratedCompositingTestPlatform>
      accelerated_compositing_scope_;
};

// TODO(crbug.com/1186864): Remove |expect_broken_tagging| when fixed.
void TestOrientation(scoped_refptr<media::VideoFrame> frame,
                     bool expect_broken_tagging = false) {
  constexpr auto kTestTransform =
      media::VideoTransformation(media::VIDEO_ROTATION_90, /*mirrored=*/true);
  constexpr auto kTestOrientation = ImageOrientationEnum::kOriginLeftTop;

  frame->metadata().transformation = kTestTransform;
  auto image =
      CreateImageFromVideoFrame(frame, true, nullptr, nullptr, gfx::Rect(),
                                /*prefer_tagged_orientation=*/true);
  if (expect_broken_tagging) {
    EXPECT_EQ(image->CurrentFrameOrientation(), ImageOrientationEnum::kDefault);
  } else {
    EXPECT_EQ(image->CurrentFrameOrientation(), kTestOrientation);
  }

  image = CreateImageFromVideoFrame(frame, true, nullptr, nullptr, gfx::Rect(),
                                    /*prefer_tagged_orientation=*/false);
  EXPECT_EQ(image->CurrentFrameOrientation(), ImageOrientationEnum::kDefault);
}

}  // namespace

TEST(VideoFrameImageUtilTest, VideoTransformationToFromImageOrientation) {
  for (int i = static_cast<int>(ImageOrientationEnum::kMinValue);
       i <= static_cast<int>(ImageOrientationEnum::kMaxValue); ++i) {
    auto blink_orientation = static_cast<ImageOrientationEnum>(i);
    auto media_transform =
        ImageOrientationToVideoTransformation(blink_orientation);
    EXPECT_EQ(blink_orientation,
              VideoTransformationToImageOrientation(media_transform));
  }
}

TEST(VideoFrameImageUtilTest, WillCreateAcceleratedImagesFromVideoFrame) {
  // I420A isn't a supported zero copy format.
  {
    auto alpha_frame = media::VideoFrame::CreateTransparentFrame(kTestSize);
    EXPECT_FALSE(WillCreateAcceleratedImagesFromVideoFrame(alpha_frame.get()));
  }

  // Software RGB frames aren't supported.
  {
    auto cpu_frame =
        CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                        media::VideoFrame::STORAGE_OWNED_MEMORY,
                        media::PIXEL_FORMAT_XRGB, base::TimeDelta());
    EXPECT_FALSE(WillCreateAcceleratedImagesFromVideoFrame(cpu_frame.get()));
  }

  // GpuMemoryBuffer frames aren't supported.
  {
    auto cpu_frame =
        CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                        media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
                        media::PIXEL_FORMAT_XRGB, base::TimeDelta());
    EXPECT_FALSE(WillCreateAcceleratedImagesFromVideoFrame(cpu_frame.get()));
  }

  // Single mailbox shared images should be supported on most platforms.
  {
    auto shared_image_frame =
        CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                        media::VideoFrame::STORAGE_OPAQUE,
                        media::PIXEL_FORMAT_XRGB, base::TimeDelta());
    EXPECT_TRUE(shared_image_frame->HasSharedImage());
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)
    EXPECT_FALSE(
        WillCreateAcceleratedImagesFromVideoFrame(shared_image_frame.get()));
#else
    EXPECT_TRUE(
        WillCreateAcceleratedImagesFromVideoFrame(shared_image_frame.get()));
#endif
  }
}

// Some platforms don't support zero copy images.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)
TEST(VideoFrameImageUtilTest, CreateImageFromVideoFrameZeroCopy) {
  ScopedFakeGpuContext fake_context(/*disable_imagebitmap=*/false);
  auto shared_image_frame =
      CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                      media::VideoFrame::STORAGE_OPAQUE,
                      media::PIXEL_FORMAT_XRGB, base::TimeDelta());
  EXPECT_TRUE(shared_image_frame->HasSharedImage());

  auto image = CreateImageFromVideoFrame(shared_image_frame);
  ASSERT_TRUE(image->IsTextureBacked());
  EXPECT_EQ(memcmp(image->GetMailboxHolder().mailbox.name,
                   shared_image_frame->shared_image()->mailbox().name,
                   sizeof(gpu::Mailbox::Name)),
            0);
}
#endif

TEST(VideoFrameImageUtilTest, CreateImageFromVideoFrameSoftwareFrame) {
  base::test::SingleThreadTaskEnvironment task_environment_;
  auto cpu_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_OWNED_MEMORY,
                                   media::PIXEL_FORMAT_XRGB, base::TimeDelta());
  auto image = CreateImageFromVideoFrame(cpu_frame);
  EXPECT_FALSE(image->IsTextureBacked());

  TestOrientation(cpu_frame);
  task_environment_.RunUntilIdle();
}

TEST(VideoFrameImageUtilTest, CreateImageFromVideoFrameGpuMemoryBufferFrame) {
  base::test::SingleThreadTaskEnvironment task_environment_;
  auto cpu_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
                                   media::PIXEL_FORMAT_NV12, base::TimeDelta());
  auto image = CreateImageFromVideoFrame(cpu_frame);
  ASSERT_FALSE(image->IsTextureBacked());
  task_environment_.RunUntilIdle();
}

TEST(VideoFrameImageUtilTest, CreateImageFromVideoFrameTextureFrame) {
  base::test::SingleThreadTaskEnvironment task_environment_;
  auto cpu_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_OPAQUE,
                                   media::PIXEL_FORMAT_NV12, base::TimeDelta());
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
                                   media::PIXEL_FORMAT_XRGB, base::TimeDelta());
  auto image = CreateImageFromVideoFrame(cpu_frame);
  ASSERT_TRUE(image->IsTextureBacked());
}

TEST(VideoFrameImageUtilTest, CreateAcceleratedImageFromGpuMemoryBufferFrame) {
  ScopedFakeGpuContext fake_context(/*disable_imagebitmap=*/false);
  auto gmb_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
                                   media::PIXEL_FORMAT_NV12, base::TimeDelta());
  auto image = CreateImageFromVideoFrame(gmb_frame);
  ASSERT_TRUE(image->IsTextureBacked());
  TestOrientation(gmb_frame, /*expect_broken_tagging=*/true);
}

TEST(VideoFrameImageUtilTest, CreateAcceleratedImageFromTextureFrame) {
  ScopedFakeGpuContext fake_context(/*disable_imagebitmap=*/false);

  auto texture_frame = media::CreateSharedImageRGBAFrame(
      fake_context.raster_context_provider(), kTestSize, gfx::Rect(kTestSize),
      base::DoNothing());
  auto image = CreateImageFromVideoFrame(texture_frame,
                                         /*allow_zero_copy_images=*/false);
  ASSERT_TRUE(image->IsTextureBacked());
  TestOrientation(texture_frame, /*expect_broken_tagging=*/true);
}

TEST(VideoFrameImageUtilTest, FlushedAcceleratedImage) {
  ScopedFakeGpuContext fake_context(/*disable_imagebitmap=*/false);
  auto* raster_context_provider = fake_context.raster_context_provider();
  ASSERT_TRUE(raster_context_provider);

  auto texture_frame = media::CreateSharedImageRGBAFrame(
      raster_context_provider, kTestSize, gfx::Rect(kTestSize),
      base::DoNothing());

  auto provider =
      CreateResourceProviderForVideoFrame(kTestInfo, raster_context_provider);
  ASSERT_TRUE(provider);
  EXPECT_TRUE(provider->IsAccelerated());

  auto image = CreateImageFromVideoFrame(texture_frame,
                                         /*allow_zero_copy_images=*/false,
                                         provider.get());
  EXPECT_TRUE(image->IsTextureBacked());

  image = CreateImageFromVideoFrame(texture_frame,
                                    /*allow_zero_copy_images=*/false,
                                    provider.get());
  EXPECT_TRUE(image->IsTextureBacked());

  ASSERT_FALSE(provider->Recorder().HasRecordedDrawOps());
}

TEST(VideoFrameImageUtilTest, SoftwareCreateResourceProviderForVideoFrame) {
  // Creating a provider with a null viz::RasterContextProvider should result in
  // a non-accelerated provider being created.
  auto provider = CreateResourceProviderForVideoFrame(kTestInfo, nullptr);
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
    auto provider = CreateResourceProviderForVideoFrame(kTestInfo, nullptr);
    ASSERT_TRUE(provider);
    EXPECT_FALSE(provider->IsAccelerated());
  }

  // Creating a provider with a real raster context provider should result in
  // an accelerated provider being created.
  {
    auto provider =
        CreateResourceProviderForVideoFrame(kTestInfo, raster_context_provider);
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
    auto provider =
        CreateResourceProviderForVideoFrame(kTestInfo, raster_context_provider);
    ASSERT_TRUE(provider);
    EXPECT_FALSE(provider->IsAccelerated());
  }
}

TEST(VideoFrameImageUtilTest, DestRectWithoutCanvasResourceProvider) {
  base::test::SingleThreadTaskEnvironment task_environment_;
  auto cpu_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_OWNED_MEMORY,
                                   media::PIXEL_FORMAT_XRGB, base::TimeDelta());

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
                                   media::PIXEL_FORMAT_XRGB, base::TimeDelta());

  auto provider = CreateResourceProviderForVideoFrame(
      SkImageInfo::MakeN32Premul(16, 16), nullptr);
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
                                   media::PIXEL_FORMAT_XRGB, base::TimeDelta());

  auto provider = CreateResourceProviderForVideoFrame(
      SkImageInfo::MakeN32Premul(128, 128), nullptr);
  ASSERT_TRUE(provider);
  EXPECT_FALSE(provider->IsAccelerated());

  auto image = CreateImageFromVideoFrame(cpu_frame, true, provider.get(),
                                         nullptr, gfx::Rect(16, 16, 64, 64));
  ASSERT_TRUE(image);
  task_environment_.RunUntilIdle();
}

}  // namespace blink
