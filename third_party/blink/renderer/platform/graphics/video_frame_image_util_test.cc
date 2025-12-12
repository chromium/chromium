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
    SharedGpuContext::Reset();
  }

 private:
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  ScopedTestingPlatformSupport<AcceleratedCompositingTestPlatform>
      accelerated_compositing_scope_;
};

}  // namespace

class VideoFrameImageUtilTest
    : public ::testing::TestWithParam<std::tuple<bool, bool>> {
 protected:
  void SetUp() override {
    test_sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();

    if (gpu_compositing()) {
      fake_context_.emplace(disable_imagebitmap());
      ASSERT_TRUE(SharedGpuContext::IsGpuCompositingEnabled());
    }
  }

  bool gpu_compositing() { return std::get<0>(GetParam()); }

  bool disable_imagebitmap() { return std::get<1>(GetParam()); }

  bool expect_accelerated_images() {
    return gpu_compositing() && !disable_imagebitmap();
  }

  viz::RasterContextProvider* raster_context_provider() {
    return SharedGpuContext::ContextProviderWrapper()
               ? SharedGpuContext::ContextProviderWrapper()
                     ->ContextProvider()
                     .RasterContextProvider()
               : nullptr;
  }

  scoped_refptr<StaticBitmapImage> DoCreateImageFromVideoFrame(
      scoped_refptr<media::VideoFrame> frame,
      CanvasSnapshotProvider* snapshot_provider = nullptr,
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

    std::unique_ptr<CanvasSnapshotProvider> local_snapshot_provider;

    if (!snapshot_provider) {
      auto frame_color_space = frame->CompatRGBColorSpace();
      local_snapshot_provider = CreateSnapshotProviderForVideoFrame(
          dest_rect.size(), GetN32FormatForCanvas(), kPremul_SkAlphaType,
          frame_color_space, raster_context_provider());
      if (!local_snapshot_provider) {
        DLOG(ERROR) << "Failed to create CanvasResourceProvider.";
        return nullptr;
      }

      snapshot_provider = local_snapshot_provider.get();
      CHECK(snapshot_provider);
    }
    return CreateImageFromVideoFrame(std::move(frame), snapshot_provider,
                                     video_renderer, prefer_tagged_orientation);
  }

  scoped_refptr<gpu::TestSharedImageInterface> test_sii_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::optional<ScopedFakeGpuContext> fake_context_;
};

INSTANTIATE_TEST_SUITE_P(,
                         VideoFrameImageUtilTest,
                         ::testing::Values(std::make_tuple(false, false),
                                           std::make_tuple(true, true),
                                           std::make_tuple(true, false)));

TEST_P(VideoFrameImageUtilTest, VideoTransformationToFromImageOrientation) {
  for (int i = static_cast<int>(ImageOrientationEnum::kMinValue);
       i <= static_cast<int>(ImageOrientationEnum::kMaxValue); ++i) {
    auto blink_orientation = static_cast<ImageOrientationEnum>(i);
    auto media_transform =
        ImageOrientationToVideoTransformation(blink_orientation);
    EXPECT_EQ(blink_orientation,
              VideoTransformationToImageOrientation(media_transform));
  }
}

TEST_P(VideoFrameImageUtilTest, CreateImageFromVideoFrameOrientation) {
  auto frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                               media::VideoFrame::STORAGE_OWNED_MEMORY,
                               media::PIXEL_FORMAT_XRGB, base::TimeDelta(),
                               test_sii_.get());

  constexpr auto kTestTransform =
      media::VideoTransformation(media::VIDEO_ROTATION_90, /*mirrored=*/true);
  constexpr auto kTestOrientation = ImageOrientationEnum::kOriginLeftTop;

  frame->metadata().transformation = kTestTransform;

  // We expect applying transform during copy if `prefer_tagged_orientation` is
  // false.
  auto image = DoCreateImageFromVideoFrame(frame, nullptr, nullptr,
                                           /*prefer_tagged_orientation=*/false);
  EXPECT_EQ(image->Orientation(), ImageOrientationEnum::kDefault);

  // We expect doing copy without transform applied and result image be tagged
  // with correct orientation.
  image = DoCreateImageFromVideoFrame(frame, nullptr, nullptr,
                                      /*prefer_tagged_orientation=*/true);

  // TODO(crbug.com/40172676): Accelerated images are not tagged correctly.
  if (expect_accelerated_images()) {
    EXPECT_EQ(image->Orientation(), ImageOrientationEnum::kDefault);
  } else {
    EXPECT_EQ(image->Orientation(), kTestOrientation);
  }
}

TEST_P(VideoFrameImageUtilTest, WillCreateAcceleratedImagesFromVideoFrame) {
  EXPECT_EQ(WillCreateAcceleratedImagesFromVideoFrame(),
            expect_accelerated_images());
}

TEST_P(VideoFrameImageUtilTest, CreateImageFromVideoFrameSoftwareFrame) {
  auto cpu_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                   media::VideoFrame::STORAGE_OWNED_MEMORY,
                                   media::PIXEL_FORMAT_XRGB, base::TimeDelta(),
                                   test_sii_.get());
  auto image = DoCreateImageFromVideoFrame(cpu_frame);
  EXPECT_EQ(image->IsTextureBacked(), expect_accelerated_images());
}

TEST_P(VideoFrameImageUtilTest, CreateImageFromVideoFrameGpuMemoryBufferFrame) {
  auto cpu_frame = CreateTestFrame(
      kTestSize, gfx::Rect(kTestSize), kTestSize,
      media::VideoFrame::STORAGE_MAPPABLE_SHARED_IMAGE,
      media::PIXEL_FORMAT_NV12, base::TimeDelta(), test_sii_.get());
  auto image = DoCreateImageFromVideoFrame(cpu_frame);
  EXPECT_EQ(image->IsTextureBacked(), expect_accelerated_images());
}

TEST_P(VideoFrameImageUtilTest, CreateImageFromVideoFrameTextureFrame) {
  scoped_refptr<media::VideoFrame> texture_frame;

  // If we have context provider, use its SII.
  if (raster_context_provider()) {
    texture_frame = media::CreateSharedImageRGBAFrame(
        raster_context_provider(), kTestSize, gfx::Rect(kTestSize),
        base::DoNothing());
  } else {
    // If not, use test shared image interface.
    texture_frame = CreateTestFrame(kTestSize, gfx::Rect(kTestSize), kTestSize,
                                    media::VideoFrame::STORAGE_OPAQUE,
                                    media::PIXEL_FORMAT_NV12, base::TimeDelta(),
                                    test_sii_.get());
  }

  auto image = DoCreateImageFromVideoFrame(texture_frame);

  if (gpu_compositing()) {
    EXPECT_EQ(image->IsTextureBacked(), expect_accelerated_images());
  } else {
    // An unaccelerated image can't be created from a texture based VideoFrame
    // without a viz::RasterContextProvider.
    ASSERT_FALSE(image);
  }
}

TEST_P(VideoFrameImageUtilTest, FlushedAcceleratedImage) {
  // Only matters for accelerated case.
  if (!expect_accelerated_images()) {
    GTEST_SKIP();
  }

  auto texture_frame = media::CreateSharedImageRGBAFrame(
      raster_context_provider(), kTestSize, gfx::Rect(kTestSize),
      base::DoNothing());

  auto provider = CreateSnapshotProviderForVideoFrame(
      kTestSize, kTestFormat, kTestAlphaType, kTestColorSpace,
      raster_context_provider());
  ASSERT_TRUE(provider);
  EXPECT_TRUE(provider->IsAccelerated());

  auto image = DoCreateImageFromVideoFrame(texture_frame, provider.get());
  EXPECT_TRUE(image->IsTextureBacked());

  image = DoCreateImageFromVideoFrame(texture_frame, provider.get());
  EXPECT_TRUE(image->IsTextureBacked());
}

TEST_P(VideoFrameImageUtilTest, CreateSnapshotProviderForVideoFrame) {
  auto provider = CreateSnapshotProviderForVideoFrame(
      kTestSize, kTestFormat, kTestAlphaType, kTestColorSpace,
      raster_context_provider());
  ASSERT_TRUE(provider);
  EXPECT_EQ(provider->IsAccelerated(), expect_accelerated_images());
}

}  // namespace blink
