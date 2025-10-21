// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_raster_interface.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "media/base/video_frame.h"
#include "media/renderers/shared_image_video_frame_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"
#include "third_party/skia/include/gpu/ganesh/GrDriverBugWorkarounds.h"

namespace blink {

namespace {

constexpr auto kTestSize = gfx::Size(64, 64);
constexpr auto kTestFormat = viz::SinglePlaneFormat::kRGBA_8888;
constexpr auto kTestAlphaType = kPremul_SkAlphaType;
constexpr auto kTestColorSpace = gfx::ColorSpace::CreateSRGB();

class AcceleratedCompositingTestPlatform
    : public blink::TestingPlatformSupport {
 public:
  bool IsGpuCompositingDisabled() const override { return false; }
};

class ScopedFakeGpuContext {
 public:
  explicit ScopedFakeGpuContext(bool disable_imagebitmap) {
    SharedGpuContext::Reset();
    test_context_provider_ = viz::TestContextProvider::CreateRaster();

    if (disable_imagebitmap) {
      // Disable CanvasResourceProvider using GPU.
      auto& feature_info = test_context_provider_->GetWritableGpuFeatureInfo();
      feature_info.enabled_gpu_driver_bug_workarounds.push_back(
          DISABLE_IMAGEBITMAP_FROM_VIDEO_USING_GPU);
    }

    InitializeSharedGpuContextRaster(test_context_provider_.get());
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

}  // namespace

class VideoFrameImageUtilTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();
    test_sii_->UseTestGMBInSharedImageCreationWithBufferUsage();
  }

  // TODO(crbug.com/1186864): Remove |expect_broken_tagging| when fixed.
  void TestOrientation(scoped_refptr<media::VideoFrame> frame,
                       bool expect_broken_tagging = false) {
    constexpr auto kTestTransform =
        media::VideoTransformation(media::VIDEO_ROTATION_90, /*mirrored=*/true);
    constexpr auto kTestOrientation = ImageOrientationEnum::kOriginLeftTop;

    frame->metadata().transformation = kTestTransform;
    auto image =
        DoCreateImageFromVideoFrame(frame, nullptr, nullptr,
                                    /*prefer_tagged_orientation=*/true);
    if (expect_broken_tagging) {
      EXPECT_EQ(image->Orientation(), ImageOrientationEnum::kDefault);
    } else {
      EXPECT_EQ(image->Orientation(), kTestOrientation);
    }

    image = DoCreateImageFromVideoFrame(frame, nullptr, nullptr,
                                        /*prefer_tagged_orientation=*/false);
    EXPECT_EQ(image->Orientation(), ImageOrientationEnum::kDefault);
  }

  scoped_refptr<StaticBitmapImage> DoCreateImageFromVideoFrame(
      scoped_refptr<media::VideoFrame> frame,
      CanvasResourceProvider* resource_provider = nullptr,
      media::PaintCanvasVideoRenderer* video_renderer = nullptr,
      bool prefer_tagged_orientation = true) {
    const auto transform =
        frame->metadata().transformation.value_or(media::kNoTransformation);
    // Since we're copying, the destination is always aligned with the origin.
    const auto& visible_rect = frame->visible_rect();
    auto dest_rect =
        gfx::Rect(0, 0, visible_rect.width(), visible_rect.height());
    if (transform.rotation == media::VIDEO_ROTATION_90 ||
        transform.rotation == media::VIDEO_ROTATION_270) {
      dest_rect.Transpose();
    }

    std::unique_ptr<CanvasResourceProvider> local_resource_provider;

    if (!resource_provider) {
      auto frame_color_space = frame->CompatRGBColorSpace();
      local_resource_provider = CreateResourceProviderForVideoFrame(
          dest_rect.size(), GetN32FormatForCanvas(), kPremul_SkAlphaType,
          frame_color_space,
          SharedGpuContext::ContextProviderWrapper()
              ? SharedGpuContext::ContextProviderWrapper()
                    ->ContextProvider()
                    .RasterContextProvider()
              : nullptr);
      if (!local_resource_provider) {
        DLOG(ERROR) << "Failed to create CanvasResourceProvider.";
        return nullptr;
      }

      resource_provider = local_resource_provider.get();
      CHECK(resource_provider);
    }
    return CreateImageFromVideoFrame(std::move(frame), resource_provider,
                                     video_renderer, prefer_tagged_orientation);
  }

  scoped_refptr<gpu::TestSharedImageInterface> test_sii_;
};

TEST_F(VideoFrameImageUtilTest, VideoTransformationToFromImageOrientation) {
  for (int i = static_cast<int>(ImageOrientationEnum::kMinValue);
       i <= static_cast<int>(ImageOrientationEnum::kMaxValue); ++i) {
    auto blink_orientation = static_cast<ImageOrientationEnum>(i);
    auto media_transform =
        ImageOrientationToVideoTransformation(blink_orientation);
    EXPECT_EQ(blink_orientation,
              VideoTransformationToImageOrientation(media_transform));
  }
}

TEST_F(VideoFrameImageUtilTest, WillCreateAcceleratedImagesFromVideoFrame) {
  for (bool gpu_compositing : {false, true}) {
    std::optional<ScopedFakeGpuContext> fake_context;

    if (gpu_compositing) {
      fake_context.emplace(/*disable_imagebitmap=*/false);
    }
    // I420A frame.
    {
      auto alpha_frame = media::VideoFrame::CreateTransparentFrame(kTestSize);
      EXPECT_EQ(WillCreateAcceleratedImagesFromVideoFrame(alpha_frame.get()),
                gpu_compositing);
    }

    // Software RGB frame.
    {
      auto cpu_frame = CreateTestFrame(
          kTestSize, gfx::Rect(kTestSize), kTestSize,
          media::VideoFrame::STORAGE_OWNED_MEMORY, media::PIXEL_FORMAT_XRGB,
          base::TimeDelta(), test_sii_.get());
      EXPECT_EQ(WillCreateAcceleratedImagesFromVideoFrame(cpu_frame.get()),
                gpu_compositing);
    }

    // GpuMemoryBuffer frame.
    {
      auto cpu_frame = CreateTestFrame(
          kTestSize, gfx::Rect(kTestSize), kTestSize,
          media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
          media::PIXEL_FORMAT_XRGB, base::TimeDelta(), test_sii_.get());
      EXPECT_EQ(WillCreateAcceleratedImagesFromVideoFrame(cpu_frame.get()),
                gpu_compositing);
    }

    // shared images frame.
    {
      auto shared_image_frame = CreateTestFrame(
          kTestSize, gfx::Rect(kTestSize), kTestSize,
          media::VideoFrame::STORAGE_OPAQUE, media::PIXEL_FORMAT_XRGB,
          base::TimeDelta(), test_sii_.get());
      EXPECT_TRUE(shared_image_frame->HasSharedImage());
      EXPECT_EQ(
          WillCreateAcceleratedImagesFromVideoFrame(shared_image_frame.get()),
          gpu_compositing);
    }
  }
}

TEST_F(VideoFrameImageUtilTest, CreateImageFromVideoFrameSoftwareFrame) {
  base::test::SingleThreadTaskEnvironment task_environment_;
  auto cpu_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_OWNED_MEMORY,
                                   media::PIXEL_FORMAT_XRGB, base::TimeDelta(),
                                   test_sii_.get());
  auto image = DoCreateImageFromVideoFrame(cpu_frame);
  EXPECT_FALSE(image->IsTextureBacked());

  TestOrientation(cpu_frame);
  task_environment_.RunUntilIdle();
}

TEST_F(VideoFrameImageUtilTest, CreateImageFromVideoFrameGpuMemoryBufferFrame) {
  base::test::SingleThreadTaskEnvironment task_environment_;
  auto cpu_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
                                   media::PIXEL_FORMAT_NV12, base::TimeDelta(),
                                   test_sii_.get());
  auto image = DoCreateImageFromVideoFrame(cpu_frame);
  ASSERT_FALSE(image->IsTextureBacked());
  task_environment_.RunUntilIdle();
}

TEST_F(VideoFrameImageUtilTest, CreateImageFromVideoFrameTextureFrame) {
  base::test::SingleThreadTaskEnvironment task_environment_;
  auto cpu_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_OPAQUE,
                                   media::PIXEL_FORMAT_NV12, base::TimeDelta(),
                                   test_sii_.get());
  auto image = DoCreateImageFromVideoFrame(cpu_frame);

  // An unaccelerated image can't be created from a texture based VideoFrame
  // without a viz::RasterContextProvider.
  ASSERT_FALSE(image);
  task_environment_.RunUntilIdle();
}

TEST_F(VideoFrameImageUtilTest,
       CreateAcceleratedImageFromVideoFrameBasicSoftwareFrame) {
  ScopedFakeGpuContext fake_context(/*disable_imagebitmap=*/false);
  auto cpu_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_OWNED_MEMORY,
                                   media::PIXEL_FORMAT_XRGB, base::TimeDelta(),
                                   test_sii_.get());
  auto image = DoCreateImageFromVideoFrame(cpu_frame);
  ASSERT_TRUE(image->IsTextureBacked());
}

TEST_F(VideoFrameImageUtilTest,
       CreateAcceleratedImageFromGpuMemoryBufferFrame) {
  ScopedFakeGpuContext fake_context(/*disable_imagebitmap=*/false);
  auto gmb_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
                                   media::PIXEL_FORMAT_NV12, base::TimeDelta(),
                                   test_sii_.get());
  auto image = DoCreateImageFromVideoFrame(gmb_frame);
  ASSERT_TRUE(image->IsTextureBacked());
  TestOrientation(gmb_frame, /*expect_broken_tagging=*/true);
}

TEST_F(VideoFrameImageUtilTest, CreateAcceleratedImageFromTextureFrame) {
  ScopedFakeGpuContext fake_context(/*disable_imagebitmap=*/false);

  auto texture_frame = media::CreateSharedImageRGBAFrame(
      fake_context.raster_context_provider(), kTestSize, gfx::Rect(kTestSize),
      base::DoNothing());
  auto image = DoCreateImageFromVideoFrame(texture_frame);
  ASSERT_TRUE(image->IsTextureBacked());
  TestOrientation(texture_frame, /*expect_broken_tagging=*/true);
}

TEST_F(VideoFrameImageUtilTest, FlushedAcceleratedImage) {
  ScopedFakeGpuContext fake_context(/*disable_imagebitmap=*/false);
  auto* raster_context_provider = fake_context.raster_context_provider();
  ASSERT_TRUE(raster_context_provider);

  auto texture_frame = media::CreateSharedImageRGBAFrame(
      raster_context_provider, kTestSize, gfx::Rect(kTestSize),
      base::DoNothing());

  auto provider = CreateResourceProviderForVideoFrame(
      kTestSize, kTestFormat, kTestAlphaType, kTestColorSpace,
      raster_context_provider);
  ASSERT_TRUE(provider);
  EXPECT_TRUE(provider->IsAccelerated());

  auto image = DoCreateImageFromVideoFrame(texture_frame, provider.get());
  EXPECT_TRUE(image->IsTextureBacked());

  image = DoCreateImageFromVideoFrame(texture_frame, provider.get());
  EXPECT_TRUE(image->IsTextureBacked());

  ASSERT_FALSE(provider->Recorder().HasRecordedDrawOps());
}

TEST_F(VideoFrameImageUtilTest, SoftwareCreateResourceProviderForVideoFrame) {
  // Creating a provider with a null viz::RasterContextProvider should result in
  // a non-accelerated provider being created.
  auto provider = CreateResourceProviderForVideoFrame(
      kTestSize, kTestFormat, kTestAlphaType, kTestColorSpace, nullptr);
  ASSERT_TRUE(provider);
  EXPECT_FALSE(provider->IsAccelerated());
}

TEST_F(VideoFrameImageUtilTest,
       AcceleratedCreateResourceProviderForVideoFrame) {
  ScopedFakeGpuContext fake_context(/*disable_imagebitmap=*/false);
  ASSERT_TRUE(SharedGpuContext::IsGpuCompositingEnabled());

  auto* raster_context_provider = fake_context.raster_context_provider();
  ASSERT_TRUE(raster_context_provider);

  // Creating a provider with a null viz::RasterContextProvider should result in
  // a non-accelerated provider being created.
  {
    auto provider = CreateResourceProviderForVideoFrame(
        kTestSize, kTestFormat, kTestAlphaType, kTestColorSpace, nullptr);
    ASSERT_TRUE(provider);
    EXPECT_FALSE(provider->IsAccelerated());
  }

  // Creating a provider with a real raster context provider should result in
  // an accelerated provider being created.
  {
    auto provider = CreateResourceProviderForVideoFrame(
        kTestSize, kTestFormat, kTestAlphaType, kTestColorSpace,
        raster_context_provider);
    ASSERT_TRUE(provider);
    EXPECT_TRUE(provider->IsAccelerated());
  }
}

TEST_F(VideoFrameImageUtilTest, WorkaroundCreateResourceProviderForVideoFrame) {
  ScopedFakeGpuContext fake_context(/*disable_imagebitmap=*/true);
  ASSERT_TRUE(SharedGpuContext::IsGpuCompositingEnabled());

  auto* raster_context_provider = fake_context.raster_context_provider();
  ASSERT_TRUE(raster_context_provider);

  // Creating a provider with a real raster context provider should result in
  // an unaccelerated provider being created due to the workaround.
  {
    auto provider = CreateResourceProviderForVideoFrame(
        kTestSize, kTestFormat, kTestAlphaType, kTestColorSpace,
        raster_context_provider);
    ASSERT_TRUE(provider);
    EXPECT_FALSE(provider->IsAccelerated());
  }
}

}  // namespace blink
