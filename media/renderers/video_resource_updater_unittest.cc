// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/renderers/video_resource_updater.h"

#include <stddef.h>
#include <stdint.h>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/test/task_environment.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/test/fake_output_surface.h"
#include "components/viz/test/test_raster_interface.h"
#include "components/viz/test/test_shared_image_interface_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "skia/ext/skcolorspace_primaries.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace media {
namespace {

class UploadCounterRasterInterface : public viz::TestRasterInterface {
 public:
  void WritePixels(const gpu::Mailbox& dest_mailbox,
                   int dst_x_offset,
                   int dst_y_offset,
                   GLenum texture_target,
                   const SkPixmap& src_sk_pixmap) override {
    ++upload_count_;
    last_upload_ = reinterpret_cast<const uint8_t*>(src_sk_pixmap.addr());
  }

  int UploadCount() { return upload_count_; }
  void ResetUploadCount() { upload_count_ = 0; }
  const uint8_t* last_upload() const { return last_upload_; }

 private:
  int upload_count_;
  raw_ptr<const uint8_t, DanglingUntriaged> last_upload_ = nullptr;
};

class VideoResourceUpdaterTest : public testing::Test {
 protected:
  VideoResourceUpdaterTest() {
    auto raster = std::make_unique<UploadCounterRasterInterface>();

    raster_ = raster.get();

    context_provider_ =
        viz::TestContextProvider::CreateRaster(std::move(raster));
    context_provider_->BindToCurrentSequence();
  }

  // testing::Test implementation.
  void SetUp() override {
    testing::Test::SetUp();
    resource_provider_ = std::make_unique<viz::ClientResourceProvider>();
  }

  std::unique_ptr<VideoResourceUpdater> CreateUpdaterForHardware() {
    return std::make_unique<VideoResourceUpdater>(
        context_provider_.get(), resource_provider_.get(),
        /*shared_image_interface=*/nullptr,
        /*use_gpu_memory_buffer_resources=*/false,
        /*max_resource_size=*/10000);
  }

  std::unique_ptr<VideoResourceUpdater> CreateUpdaterForSoftware() {
    return std::make_unique<VideoResourceUpdater>(
        /*context_provider=*/nullptr, resource_provider_.get(),
        shared_image_interface_provider_.GetSharedImageInterface(),
        /*use_gpu_memory_buffer_resources=*/false, /*max_resource_size=*/10000);
  }

  // Note that the number of pixels needed for |size| must be less than or equal
  // to the number of pixels needed for size of 100x100.
  scoped_refptr<VideoFrame> CreateTestYUVVideoFrame(
      const gfx::Size& size = gfx::Size(10, 10)) {
    constexpr int kMaxDimension = 100;
    static std::array<uint8_t, kMaxDimension * kMaxDimension> y_data{};
    static std::array<uint8_t, kMaxDimension * kMaxDimension / 2> u_data{};
    static std::array<uint8_t, kMaxDimension * kMaxDimension / 2> v_data{};

    CHECK_LE(size.width() * size.height(), kMaxDimension * kMaxDimension);

    scoped_refptr<VideoFrame> video_frame =
        VideoFrame::WrapExternalYuvData(PIXEL_FORMAT_I422,   // format
                                        size,                // coded_size
                                        gfx::Rect(size),     // visible_rect
                                        size,                // natural_size
                                        size.width(),        // y_stride
                                        size.width() / 2,    // u_stride
                                        size.width() / 2,    // v_stride
                                        y_data,              // y_data
                                        u_data,              // u_data
                                        v_data,              // v_data
                                        base::TimeDelta());  // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  scoped_refptr<VideoFrame> CreateTestRGBVideoFrame(VideoPixelFormat format) {
    constexpr int kMaxDimension = 10;
    constexpr gfx::Size kSize = gfx::Size(kMaxDimension, kMaxDimension);
    static std::array<uint8_t, 4 * kMaxDimension * kMaxDimension> rgb_data{};
    scoped_refptr<VideoFrame> video_frame =
        VideoFrame::WrapExternalData(format,              // format
                                     kSize,               // coded_size
                                     gfx::Rect(kSize),    // visible_rect
                                     kSize,               // natural_size
                                     rgb_data,            // data,
                                     base::TimeDelta());  // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  scoped_refptr<VideoFrame> CreateTestRGBVideoFrameWithVisibleRect(
      VideoPixelFormat format) {
    constexpr int kMaxDimension = 5;
    constexpr gfx::Size kSize = gfx::Size(kMaxDimension, kMaxDimension);
    constexpr gfx::Rect kVisibleRect = gfx::Rect(2, 1, 3, 3);
#define PIX 0xFF, 0xFF, 0xFF, 0xFF
    static std::array<uint8_t, 4 * kMaxDimension * kMaxDimension> rgb_data{
        0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0, 0, 0, 0, 0, 0, 0, 0, 0,  //
        0, 0, 0, 0, 0, 0, 0, 0, PIX, PIX, PIX,                             //
        0, 0, 0, 0, 0, 0, 0, 0, PIX, PIX, PIX,                             //
        0, 0, 0, 0, 0, 0, 0, 0, PIX, PIX, PIX,                             //
        0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0, 0, 0, 0, 0, 0, 0, 0, 0,  //
    };
#undef PIX

    scoped_refptr<VideoFrame> video_frame =
        VideoFrame::WrapExternalData(format,               // format
                                     kSize,                // coded_size
                                     kVisibleRect,         // visible_rect
                                     kVisibleRect.size(),  // natural_size
                                     rgb_data,             // data,
                                     base::TimeDelta());   // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  scoped_refptr<VideoFrame> CreateTestY16VideoFrameWithVisibleRect() {
    constexpr int kMaxDimension = 5;
    constexpr gfx::Size kSize = gfx::Size(kMaxDimension, kMaxDimension);
    constexpr gfx::Rect kVisibleRect = gfx::Rect(2, 1, 3, 3);
    static std::array<uint8_t, 2 * kMaxDimension * kMaxDimension> y16_data = {
        0, 0, 0, 0, 0,    0,    0,    0,    0,    0,
        0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0, 0, 0, 0, 0,    0,    0,    0,    0,    0,
    };

    scoped_refptr<VideoFrame> video_frame =
        VideoFrame::WrapExternalData(PIXEL_FORMAT_Y16,
                                     kSize,                // coded_size
                                     kVisibleRect,         // visible_rect
                                     kVisibleRect.size(),  // natural_size
                                     y16_data,             // data,
                                     base::TimeDelta());   // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  scoped_refptr<VideoFrame> CreateTestHighBitFrame() {
    const int kDimension = 10;
    gfx::Size size(kDimension, kDimension);

    scoped_refptr<VideoFrame> video_frame(
        VideoFrame::CreateFrame(PIXEL_FORMAT_YUV420P10, size, gfx::Rect(size),
                                size, base::TimeDelta()));
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  scoped_refptr<VideoFrame> CreateNV12TestFrame() {
    const int kDimension = 10;
    gfx::Size size(kDimension, kDimension);

    scoped_refptr<VideoFrame> video_frame(VideoFrame::CreateFrame(
        PIXEL_FORMAT_NV12, size, gfx::Rect(size), size, base::TimeDelta()));
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  scoped_refptr<VideoFrame> CreateP010TestFrame() {
    const int kDimension = 10;
    gfx::Size size(kDimension, kDimension);

    scoped_refptr<VideoFrame> video_frame(VideoFrame::CreateFrame(
        PIXEL_FORMAT_P010LE, size, gfx::Rect(size), size, base::TimeDelta()));
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  void SetReleaseSyncToken(const gpu::SyncToken& sync_token) {
    release_sync_token_ = sync_token;
  }

  scoped_refptr<VideoFrame> CreateTestHardwareVideoFrame(
      viz::SharedImageFormat si_format,
      VideoPixelFormat format,
      const gfx::ColorSpace& color_space,
      unsigned target,
      bool needs_raster_access) {
    const int kDimension = 10;
    gfx::Size size(kDimension, kDimension);

    gpu::SharedImageMetadata metadata;
    metadata.format = si_format;
    metadata.size = size;
    metadata.color_space = color_space;
    metadata.surface_origin = kTopLeft_GrSurfaceOrigin;
    metadata.alpha_type = kOpaque_SkAlphaType;
    metadata.usage = needs_raster_access ? gpu::SHARED_IMAGE_USAGE_RASTER_READ
                                         : gpu::SharedImageUsageSet();
    scoped_refptr<gpu::ClientSharedImage> shared_image =
        gpu::ClientSharedImage::CreateForTesting(metadata, target);
    scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapSharedImage(
        format, shared_image, kMailboxSyncToken,
        base::BindOnce(&VideoResourceUpdaterTest::SetReleaseSyncToken,
                       base::Unretained(this)),
        size,                // coded_size
        gfx::Rect(size),     // visible_rect
        size,                // natural_size
        base::TimeDelta());  // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  scoped_refptr<VideoFrame> CreateTestRGBAHardwareVideoFrame() {
    return CreateTestHardwareVideoFrame(viz::SinglePlaneFormat::kRGBA_8888,
                                        PIXEL_FORMAT_ARGB, kSRGBColorSpace,
                                        GL_TEXTURE_2D,
                                        /*needs_raster_access=*/false);
  }

  scoped_refptr<VideoFrame> CreateTestStreamTextureHardwareVideoFrame(
      bool needs_copy) {
    scoped_refptr<VideoFrame> video_frame = CreateTestHardwareVideoFrame(
        viz::SinglePlaneFormat::kRGBA_8888, PIXEL_FORMAT_ARGB, kSRGBColorSpace,
        GL_TEXTURE_EXTERNAL_OES, /*needs_raster_access=*/needs_copy);
    video_frame->metadata().copy_required = needs_copy;
    return video_frame;
  }

#if BUILDFLAG(IS_WIN)
  scoped_refptr<VideoFrame> CreateTestDCompSurfaceVideoFrame() {
    scoped_refptr<VideoFrame> video_frame = CreateTestHardwareVideoFrame(
        viz::SinglePlaneFormat::kRGBA_8888, PIXEL_FORMAT_ARGB, kSRGBColorSpace,
        GL_TEXTURE_EXTERNAL_OES, /*needs_raster_access=*/false);
    video_frame->metadata().dcomp_surface = true;
    return video_frame;
  }
#endif

  size_t GetSharedImageCount() {
    return context_provider_->SharedImageInterface()->shared_image_count();
  }

  static const gpu::SyncToken kMailboxSyncToken;

  const gfx::ColorSpace kSRGBColorSpace = gfx::ColorSpace::CreateSRGB();

  // VideoResourceUpdater registers as a MemoryDumpProvider, which requires
  // a TaskRunner.
  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<UploadCounterRasterInterface, DanglingUntriaged> raster_;
  scoped_refptr<viz::TestContextProvider> context_provider_;
  std::unique_ptr<viz::ClientResourceProvider> resource_provider_;
  viz::TestSharedImageInterfaceProvider shared_image_interface_provider_;
  gpu::SyncToken release_sync_token_;
};

const gpu::SyncToken VideoResourceUpdaterTest::kMailboxSyncToken =
    gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                   gpu::CommandBufferId::FromUnsafeValue(0x123),
                   7);

TEST_F(VideoResourceUpdaterTest, SoftwareFrame) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  scoped_refptr<VideoFrame> video_frame = CreateTestYUVVideoFrame();

  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  // With multiplanar shared images, a TextureDrawQuad is created instead of
  // a YUVDrawQuad.
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);

  // Create the resource again, to test the path where the
  // resource are cached.
  resource = updater->CreateExternalResourceFromVideoFrame(video_frame);
  // With multiplanar shared images, a TextureDrawQuad is created instead of
  // a YUVDrawQuad.
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
}

TEST_F(VideoResourceUpdaterTest, SoftwareFrameNV12) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();

  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(CreateNV12TestFrame());
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);

  // Use a different frame for this test since frames with the same unique_id()
  // expect to use the same resource.
  scoped_refptr<VideoFrame> video_frame = CreateNV12TestFrame();
  resource = updater->CreateExternalResourceFromVideoFrame(video_frame);
  // With multiplanar shared images, a TextureDrawQuad is created instead of
  // a YUVDrawQuad.
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);

  resource = updater->CreateExternalResourceFromVideoFrame(video_frame);
  // With multiplanar shared images, a TextureDrawQuad is created instead of
  // a YUVDrawQuad.
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
}

// Ensure we end up with the right SharedImageFormat for each resource.
TEST_F(VideoResourceUpdaterTest, SoftwareFrameRGB) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  for (const auto& fmt : {PIXEL_FORMAT_XBGR, PIXEL_FORMAT_XRGB,
                          PIXEL_FORMAT_ABGR, PIXEL_FORMAT_ARGB}) {
    scoped_refptr<VideoFrame> video_frame = CreateTestRGBVideoFrame(fmt);
    VideoFrameExternalResource resource =
        updater->CreateExternalResourceFromVideoFrame(video_frame);
    EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
#if BUILDFLAG(IS_MAC)
    EXPECT_EQ(resource.resource.GetFormat(),
              viz::SinglePlaneFormat::kBGRA_8888);
#else
    EXPECT_EQ(resource.resource.GetSize(), video_frame->coded_size());

    if (fmt == PIXEL_FORMAT_XBGR) {
      EXPECT_EQ(resource.resource.GetFormat(),
                viz::SinglePlaneFormat::kRGBA_8888);
    } else if (fmt == PIXEL_FORMAT_XRGB) {
      EXPECT_EQ(resource.resource.GetFormat(),
                viz::SinglePlaneFormat::kBGRA_8888);

    } else if (fmt == PIXEL_FORMAT_ABGR) {
      EXPECT_EQ(resource.resource.GetFormat(),
                viz::SinglePlaneFormat::kRGBA_8888);

    } else if (fmt == PIXEL_FORMAT_ARGB) {
      EXPECT_EQ(resource.resource.GetFormat(),
                viz::SinglePlaneFormat::kBGRA_8888);
    }
#endif
  }
}

// Ensure the visible data is where it's supposed to be.
TEST_F(VideoResourceUpdaterTest, SoftwareFrameRGBNonOrigin) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  for (const auto& fmt : {PIXEL_FORMAT_XBGR, PIXEL_FORMAT_XRGB,
                          PIXEL_FORMAT_ABGR, PIXEL_FORMAT_ARGB}) {
    auto video_frame = CreateTestRGBVideoFrameWithVisibleRect(fmt);
    VideoFrameExternalResource resource =
        updater->CreateExternalResourceFromVideoFrame(video_frame);
    EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
    EXPECT_EQ(resource.resource.GetSize(), video_frame->coded_size());

    auto rect = video_frame->visible_rect();

    const auto bytes_per_row = video_frame->row_bytes(VideoFrame::Plane::kARGB);
    const auto bytes_per_element =
        VideoFrame::BytesPerElement(fmt, VideoFrame::Plane::kARGB);
    auto* dest_pixels = raster_->last_upload() + rect.y() * bytes_per_row +
                        rect.x() * bytes_per_element;
    auto* src_pixels = video_frame->visible_data(VideoFrame::Plane::kARGB);

    // Pixels are 0xFFFFFFFF, so channel reordering doesn't matter.
    for (int y = 0; y < rect.height(); ++y) {
      for (int x = 0; x < rect.width() * bytes_per_element; ++x) {
        const auto pos = y * bytes_per_row + x;
        ASSERT_EQ(src_pixels[pos], dest_pixels[pos]);
      }
    }
  }
}

// Ensure the visible data is where it's supposed to be.
TEST_F(VideoResourceUpdaterTest, SoftwareFrameY16NonOrigin) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();

  auto video_frame = CreateTestY16VideoFrameWithVisibleRect();
  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
  EXPECT_EQ(resource.resource.GetSize(), video_frame->coded_size());

  auto rect = video_frame->visible_rect();

  // Just used for sizing information, channel order doesn't matter.
  constexpr auto kOutputFormat = PIXEL_FORMAT_ARGB;

  const auto bytes_per_row =
      VideoFrame::RowBytes(VideoFrame::Plane::kARGB, kOutputFormat,
                           video_frame->coded_size().width());
  const auto bytes_per_element =
      VideoFrame::BytesPerElement(kOutputFormat, VideoFrame::Plane::kARGB);
  auto* dest_pixels = raster_->last_upload() + rect.y() * bytes_per_row +
                      rect.x() * bytes_per_element;

  // Pixels are 0xFFFFFFFF, so channel reordering doesn't matter.
  for (int y = 0; y < rect.height(); ++y) {
    for (int x = 0; x < rect.width() * bytes_per_element; ++x) {
      const auto pos = y * bytes_per_row + x;
      ASSERT_EQ(0xFF, dest_pixels[pos]);
    }
  }
}

TEST_F(VideoResourceUpdaterTest, SoftwareFrameRGBAF16) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();

  const gfx::Size coded_size(16, 16);
  auto video_frame = VideoFrame::CreateFrame(PIXEL_FORMAT_RGBAF16, coded_size,
                                             gfx::Rect(coded_size), coded_size,
                                             base::TimeDelta());
  CHECK(video_frame);

  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
  EXPECT_EQ(resource.resource.GetSize(), video_frame->coded_size());
}

TEST_F(VideoResourceUpdaterTest, HighBitFrameNoF16) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  scoped_refptr<VideoFrame> video_frame = CreateTestHighBitFrame();

  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  // With multiplanar shared images, a TextureDrawQuad is created instead of
  // a YUVDrawQuad.
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);

  // Create the resource again, to test the path where the
  // resource are cached.
  resource = updater->CreateExternalResourceFromVideoFrame(video_frame);
  // With multiplanar shared images, a TextureDrawQuad is created instead of
  // a YUVDrawQuad.
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
}

class VideoResourceUpdaterTestWithF16 : public VideoResourceUpdaterTest {
 public:
  VideoResourceUpdaterTestWithF16() : VideoResourceUpdaterTest() {
  }
};

TEST_F(VideoResourceUpdaterTestWithF16, HighBitFrame) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  scoped_refptr<VideoFrame> video_frame = CreateTestHighBitFrame();

  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  // With multiplanar shared images, a TextureDrawQuad is created instead of
  // a YUVDrawQuad.
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);

  // Create the resource again, to test the path where the
  // resource are cached.
  VideoFrameExternalResource resource2 =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  // With multiplanar shared images, a TextureDrawQuad is created instead of
  // a YUVDrawQuad.
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
}

class VideoResourceUpdaterTestWithR16 : public VideoResourceUpdaterTest {
 public:
  VideoResourceUpdaterTestWithR16() : VideoResourceUpdaterTest() {
    auto* sii = context_provider_->SharedImageInterface();
    auto shared_image_caps = sii->GetCapabilities();
    shared_image_caps.supports_r16_shared_images = true;
    sii->SetCapabilities(shared_image_caps);
  }
};

TEST_F(VideoResourceUpdaterTestWithR16, HighBitFrame) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  scoped_refptr<VideoFrame> video_frame = CreateTestHighBitFrame();

  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  // With multiplanar shared images, a TextureDrawQuad is created instead of
  // a YUVDrawQuad.
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);

  // Create the resource again, to test the path where the
  // resource are cached.
  VideoFrameExternalResource resource2 =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  // With multiplanar shared images, a TextureDrawQuad is created instead of
  // a YUVDrawQuad.
  EXPECT_EQ(VideoFrameResourceType::RGB, resource2.type);
}

TEST_F(VideoResourceUpdaterTest, NV12FrameSoftwareCompositor) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForSoftware();
  scoped_refptr<VideoFrame> video_frame = CreateNV12TestFrame();

  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resource.type);
}

TEST_F(VideoResourceUpdaterTest, P010FrameSoftwareCompositor) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForSoftware();
  scoped_refptr<VideoFrame> video_frame = CreateP010TestFrame();

  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resource.type);
}

TEST_F(VideoResourceUpdaterTest, HighBitFrameSoftwareCompositor) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForSoftware();
  scoped_refptr<VideoFrame> video_frame = CreateTestHighBitFrame();

  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resource.type);
}

TEST_F(VideoResourceUpdaterTest, ReuseResource) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  scoped_refptr<VideoFrame> video_frame = CreateTestYUVVideoFrame();
  video_frame->set_timestamp(base::Seconds(1234));

  // Allocate the resource for a YUV video frame.
  raster_->ResetUploadCount();
  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  // With multiplanar shared images, a TextureDrawQuad is created instead of
  // a YUVDrawQuad, we have a single resource and callback and use raster
  // interface for uploads.
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
  // EXPECT_TRUE(resource.resource);
  EXPECT_TRUE(resource.release_callback);
  EXPECT_EQ(0, raster_->UploadCount());

  // Simulate the ResourceProvider releasing the resource back to the video
  // updater.
  std::move(resource.release_callback).Run(gpu::SyncToken(), false);

  // Allocate resource for the same frame.
  raster_->ResetUploadCount();
  resource = updater->CreateExternalResourceFromVideoFrame(video_frame);
  // With multiplanar shared images, a TextureDrawQuad is created instead of
  // a YUVDrawQuad, we have a single resource and callback and use raster
  // interface for uploads.
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
  EXPECT_TRUE(resource.release_callback);
  // The data should be reused so expect no texture uploads.
  EXPECT_EQ(0, raster_->UploadCount());
}

TEST_F(VideoResourceUpdaterTest, ReuseResourceNV12) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  scoped_refptr<VideoFrame> video_frame = CreateNV12TestFrame();
  video_frame->set_timestamp(base::Seconds(1234));
  raster_->set_texture_rg(true);

  // Allocate the resource for a YUV video frame.
  raster_->ResetUploadCount();
  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  // With multiplanar shared images, a TextureDrawQuad is created instead of
  // a YUVDrawQuad, we have a single resource and callback and use raster
  // interface for uploads.
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
  EXPECT_TRUE(resource.release_callback);
  EXPECT_EQ(0, raster_->UploadCount());

  // Simulate the ResourceProvider releasing the resource back to the video
  // updater.
  std::move(resource.release_callback).Run(gpu::SyncToken(), false);

  // Allocate resource for the same frame.
  raster_->ResetUploadCount();
  resource = updater->CreateExternalResourceFromVideoFrame(video_frame);
  // With multiplanar shared images, a TextureDrawQuad is created instead of
  // a YUVDrawQuad, we have a single resource and callback and use raster
  // interface for uploads.
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
  EXPECT_TRUE(resource.release_callback);
  // The data should be reused so expect no texture uploads.
  EXPECT_EQ(0, raster_->UploadCount());
}

TEST_F(VideoResourceUpdaterTest, ReuseResourceNoDelete) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  scoped_refptr<VideoFrame> video_frame = CreateTestYUVVideoFrame();
  video_frame->set_timestamp(base::Seconds(1234));

  // Allocate the resource for a YUV video frame.
  raster_->ResetUploadCount();
  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  // With multiplanar shared images, a TextureDrawQuad is created instead of
  // a YUVDrawQuad, we have a single resource and callback and use raster
  // interface for uploads.
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
  EXPECT_TRUE(resource.release_callback);
  EXPECT_EQ(0, raster_->UploadCount());

  // Allocate resource for the same frame.
  raster_->ResetUploadCount();
  resource = updater->CreateExternalResourceFromVideoFrame(video_frame);
  // With multiplanar shared images, a TextureDrawQuad is created instead of
  // a YUVDrawQuad, we have a single resource and callback and use raster
  // interface for uploads.
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
  EXPECT_TRUE(resource.release_callback);
  // The data should be reused so expect no texture uploads.
  EXPECT_EQ(0, raster_->UploadCount());
}

TEST_F(VideoResourceUpdaterTest, SoftwareFrameSoftwareCompositor) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForSoftware();
  scoped_refptr<VideoFrame> video_frame = CreateTestYUVVideoFrame();

  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resource.type);
}

TEST_F(VideoResourceUpdaterTest, SoftwareFrameRGBSoftwareCompositor) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForSoftware();
  for (const auto& fmt : {PIXEL_FORMAT_XBGR, PIXEL_FORMAT_XRGB,
                          PIXEL_FORMAT_ABGR, PIXEL_FORMAT_ARGB}) {
    scoped_refptr<VideoFrame> video_frame = CreateTestRGBVideoFrame(fmt);
    VideoFrameExternalResource resource =
        updater->CreateExternalResourceFromVideoFrame(video_frame);
    EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resource.type);
    EXPECT_EQ(resource.resource.GetFormat(),
              viz::SinglePlaneFormat::kBGRA_8888);
  }
}

TEST_F(VideoResourceUpdaterTest, ReuseResourceoftwareCompositor) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForSoftware();
  scoped_refptr<VideoFrame> video_frame = CreateTestYUVVideoFrame();
  video_frame->set_timestamp(base::Seconds(1234));

  // Allocate the resource for a software video frame.
  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resource.type);
  EXPECT_TRUE(resource.release_callback);

  // Simulate the ResourceProvider releasing the resource back to the video
  // updater.
  std::move(resource.release_callback).Run(gpu::SyncToken(), false);

  // Allocate resource for the same frame.
  resource = updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resource.type);
  EXPECT_TRUE(resource.release_callback);
}

TEST_F(VideoResourceUpdaterTest, ReuseResourceNoDeleteSoftwareCompositor) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForSoftware();
  scoped_refptr<VideoFrame> video_frame = CreateTestYUVVideoFrame();
  video_frame->set_timestamp(base::Seconds(1234));

  // Allocate the resource for a software video frame.
  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resource.type);
  EXPECT_TRUE(resource.release_callback);

  // Allocate resource for the same frame.
  resource = updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resource.type);
  EXPECT_TRUE(resource.release_callback);
}

TEST_F(VideoResourceUpdaterTest, ChangeResourceizeSoftwareCompositor) {
  constexpr gfx::Size kSize1(10, 10);
  constexpr gfx::Size kSize2(20, 20);

  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForSoftware();

  // Allocate the resource for a software video frame.
  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(
          CreateTestYUVVideoFrame(kSize1));

  // Simulate the ResourceProvider releasing the resource back to the video
  // updater.
  std::move(resource.release_callback).Run(gpu::SyncToken(), false);

  // Allocate resource for the next frame with a different size.
  resource = updater->CreateExternalResourceFromVideoFrame(
      CreateTestYUVVideoFrame(kSize2));
}

TEST_F(VideoResourceUpdaterTest, CreateForHardwarePlanes_SharedImageFormat) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  scoped_refptr<VideoFrame> video_frame = CreateTestHardwareVideoFrame(
      viz::MultiPlaneFormat::kI420, PIXEL_FORMAT_I420, kSRGBColorSpace,
      GL_TEXTURE_RECTANGLE_ARB,
      /*needs_raster_access=*/false);
  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
  EXPECT_TRUE(resource.release_callback);
  EXPECT_EQ(viz::MultiPlaneFormat::kI420, resource.resource.GetFormat());
  EXPECT_EQ(resource.resource.synchronization_type,
            viz::TransferableResource::SynchronizationType::kSyncToken);

  video_frame = CreateTestHardwareVideoFrame(viz::MultiPlaneFormat::kI420,
                                             PIXEL_FORMAT_I420, kSRGBColorSpace,
                                             GL_TEXTURE_RECTANGLE_ARB,
                                             /*needs_raster_access=*/false);
  video_frame->metadata().read_lock_fences_enabled = true;

  resource = updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_EQ(
      resource.resource.synchronization_type,
      viz::TransferableResource::SynchronizationType::kGpuCommandsCompleted);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(VideoResourceUpdaterTest,
       CreateForHardwarePlanes_StreamTexture_CopyToNewTexture) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  EXPECT_EQ(0u, GetSharedImageCount());
  scoped_refptr<VideoFrame> video_frame =
      CreateTestStreamTextureHardwareVideoFrame(/*needs_copy=*/false);

  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
  EXPECT_TRUE(resource.release_callback);
  EXPECT_EQ((GLenum)GL_TEXTURE_EXTERNAL_OES,
            resource.resource.texture_target());
  EXPECT_EQ(0u, GetSharedImageCount());

  // A copied stream texture should return an RGBA resource in a new
  // GL_TEXTURE_2D texture.
  video_frame = CreateTestStreamTextureHardwareVideoFrame(/*needs_copy=*/true);
  resource = updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resource.type);
  EXPECT_TRUE(resource.release_callback);
  EXPECT_EQ((GLenum)GL_TEXTURE_2D, resource.resource.texture_target());
  EXPECT_EQ(1u, GetSharedImageCount());
}
#endif

TEST_F(VideoResourceUpdaterTest, CreateForHardwarePlanes_TextureQuad) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  EXPECT_EQ(0u, GetSharedImageCount());
  scoped_refptr<VideoFrame> video_frame =
      CreateTestStreamTextureHardwareVideoFrame(/*needs_copy=*/false);

  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_TRUE(resource.release_callback);
  EXPECT_EQ((GLenum)GL_TEXTURE_EXTERNAL_OES,
            resource.resource.texture_target());
  EXPECT_EQ(0u, GetSharedImageCount());
}

#if BUILDFLAG(IS_WIN)
// Check that a video frame marked as containing a DComp surface turns into a
// texture draw quad that is required for overlay.
TEST_F(VideoResourceUpdaterTest, CreateForHardwarePlanes_DCompSurface) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  EXPECT_EQ(0u, GetSharedImageCount());
  scoped_refptr<VideoFrame> video_frame = CreateTestDCompSurfaceVideoFrame();

  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_TRUE(resource.release_callback);
  EXPECT_EQ((GLenum)GL_TEXTURE_EXTERNAL_OES,
            resource.resource.texture_target());
  EXPECT_EQ(0u, GetSharedImageCount());

  updater->ObtainFrameResource(video_frame);

  std::unique_ptr<viz::CompositorRenderPass> pass =
      viz::CompositorRenderPass::Create();
  pass->SetNew(/*pass_id=*/viz::CompositorRenderPassId{1},
               /*output_rect=*/gfx::Rect(video_frame->coded_size()),
               /*damage_rect=*/gfx::Rect(),
               /*transform_to_root_target=*/gfx::Transform());
  updater->AppendQuad(
      /*render_pass=*/pass.get(), video_frame,
      /*transform=*/gfx::Transform(),
      /*quad_rect=*/gfx::Rect(video_frame->coded_size()),
      /*visible_quad_rect=*/gfx::Rect(video_frame->coded_size()),
      gfx::MaskFilterInfo(), /*clip_rect=*/std::nullopt,
      /*context_opaque=*/true, /*draw_opacity=*/1.0,
      /*sorting_context_id=*/0);

  EXPECT_EQ(1u, pass->quad_list.size());

  const viz::TextureDrawQuad* quad =
      pass->quad_list.ElementAt(0)->DynamicCast<viz::TextureDrawQuad>();
  EXPECT_NE(nullptr, quad);
  EXPECT_EQ(gfx::ProtectedVideoType::kHardwareProtected,
            quad->protected_video_type);
  EXPECT_EQ(viz::OverlayPriority::kRequired, quad->overlay_priority_hint);

  updater->ReleaseFrameResource();
}
#endif

// Passthrough the sync token returned by the compositor if we don't have an
// existing release sync token.
TEST_F(VideoResourceUpdaterTest, PassReleaseSyncToken) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();

  const gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                                  gpu::CommandBufferId::FromUnsafeValue(0x123),
                                  123);

  {
    scoped_refptr<VideoFrame> video_frame = CreateTestRGBAHardwareVideoFrame();

    VideoFrameExternalResource resource =
        updater->CreateExternalResourceFromVideoFrame(video_frame);

    EXPECT_TRUE(resource.release_callback);
    std::move(resource.release_callback).Run(sync_token, false);
  }

  EXPECT_EQ(release_sync_token_, sync_token);
}

// Generate new sync token because video frame has an existing sync token.
TEST_F(VideoResourceUpdaterTest, GenerateReleaseSyncToken) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();

  const gpu::SyncToken sync_token1(gpu::CommandBufferNamespace::GPU_IO,
                                   gpu::CommandBufferId::FromUnsafeValue(0x123),
                                   123);

  const gpu::SyncToken sync_token2(gpu::CommandBufferNamespace::GPU_IO,
                                   gpu::CommandBufferId::FromUnsafeValue(0x234),
                                   234);

  {
    scoped_refptr<VideoFrame> video_frame = CreateTestRGBAHardwareVideoFrame();

    VideoFrameExternalResource resource1 =
        updater->CreateExternalResourceFromVideoFrame(video_frame);
    EXPECT_TRUE(resource1.release_callback);
    std::move(resource1.release_callback).Run(sync_token1, false);

    VideoFrameExternalResource resource2 =
        updater->CreateExternalResourceFromVideoFrame(video_frame);
    EXPECT_TRUE(resource2.release_callback);
    std::move(resource2.release_callback).Run(sync_token2, false);
  }

  EXPECT_TRUE(release_sync_token_.HasData());
  EXPECT_NE(release_sync_token_, sync_token1);
  EXPECT_EQ(release_sync_token_, sync_token2);
}

// Pass mailbox sync token as is if no GL operations are performed before frame
// resource are handed off to the compositor.
TEST_F(VideoResourceUpdaterTest, PassMailboxSyncToken) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();

  scoped_refptr<VideoFrame> video_frame = CreateTestRGBAHardwareVideoFrame();

  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);

  EXPECT_TRUE(resource.resource.sync_token().HasData());
  EXPECT_EQ(resource.resource.sync_token(), kMailboxSyncToken);
}

// Generate new sync token for compositor when copying the texture.
TEST_F(VideoResourceUpdaterTest, GenerateSyncTokenOnTextureCopy) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();

  scoped_refptr<VideoFrame> video_frame =
      CreateTestStreamTextureHardwareVideoFrame(/*needs_copy=*/true);

  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);

  EXPECT_TRUE(resource.resource.sync_token().HasData());
  EXPECT_NE(resource.resource.sync_token(), kMailboxSyncToken);
}

// NV12 VideoFrames backed by a single native texture can be sampled out
// by GL as RGB. To use them as HW overlays we need to know the format
// of the underlying buffer, that is YUV_420_BIPLANAR.
TEST_F(VideoResourceUpdaterTest, CreateForHardwarePlanes_SingleNV12) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  EXPECT_EQ(0u, GetSharedImageCount());
  scoped_refptr<VideoFrame> video_frame = CreateTestHardwareVideoFrame(
      viz::MultiPlaneFormat::kNV12, PIXEL_FORMAT_NV12, kSRGBColorSpace,
      GL_TEXTURE_EXTERNAL_OES,
      /*needs_raster_access=*/false);
  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
  EXPECT_EQ((GLenum)GL_TEXTURE_EXTERNAL_OES,
            resource.resource.texture_target());
  EXPECT_EQ(viz::MultiPlaneFormat::kNV12, resource.resource.GetFormat());
  EXPECT_EQ(0u, GetSharedImageCount());
}

TEST_F(VideoResourceUpdaterTest,
       CreateForHardwarePlanes_DualNV12_SharedImageFormat) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  EXPECT_EQ(0u, GetSharedImageCount());
  scoped_refptr<VideoFrame> video_frame = CreateTestHardwareVideoFrame(
      viz::MultiPlaneFormat::kNV12, PIXEL_FORMAT_NV12, kSRGBColorSpace,
      GL_TEXTURE_RECTANGLE_ARB,
      /*needs_raster_access=*/false);
  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  // Setting to kSharedImageFormat, resource type should bo RGB.
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
  EXPECT_EQ((GLenum)GL_TEXTURE_RECTANGLE_ARB,
            resource.resource.texture_target());
  EXPECT_EQ(viz::MultiPlaneFormat::kNV12, resource.resource.GetFormat());
  EXPECT_EQ(0u, GetSharedImageCount());

  video_frame = CreateTestHardwareVideoFrame(viz::MultiPlaneFormat::kNV12,
                                             PIXEL_FORMAT_NV12, kSRGBColorSpace,
                                             GL_TEXTURE_EXTERNAL_OES,
                                             /*needs_raster_access=*/false);

  resource = updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
  EXPECT_TRUE(resource.release_callback);
  EXPECT_EQ((GLenum)GL_TEXTURE_EXTERNAL_OES,
            resource.resource.texture_target());
  // |updater| doesn't set |buffer_format| in this case.
  EXPECT_EQ(viz::MultiPlaneFormat::kNV12, resource.resource.GetFormat());
  EXPECT_EQ(0u, GetSharedImageCount());
}

TEST_F(VideoResourceUpdaterTest, CreateForHardwarePlanes_SingleP010HDR) {
  constexpr auto kHDR10ColorSpace = gfx::ColorSpace::CreateHDR10();
  gfx::HDRMetadata hdr_metadata{};
  hdr_metadata.smpte_st_2086 =
      gfx::HdrMetadataSmpteSt2086(SkNamedPrimariesExt::kP3,
                                  /*luminance_max=*/1000,
                                  /*luminance_min=*/0);
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  EXPECT_EQ(0u, GetSharedImageCount());
  scoped_refptr<VideoFrame> video_frame = CreateTestHardwareVideoFrame(
      viz::MultiPlaneFormat::kP010, PIXEL_FORMAT_P010LE, kHDR10ColorSpace,
      GL_TEXTURE_EXTERNAL_OES, /*needs_raster_access=*/false);
  video_frame->set_color_space(kHDR10ColorSpace);
  video_frame->set_hdr_metadata(hdr_metadata);

  VideoFrameExternalResource resource =
      updater->CreateExternalResourceFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGB, resource.type);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_EXTERNAL_OES),
            resource.resource.texture_target());
  EXPECT_EQ(viz::MultiPlaneFormat::kP010, resource.resource.GetFormat());
  EXPECT_EQ(kHDR10ColorSpace, resource.resource.GetColorSpace());
  EXPECT_EQ(hdr_metadata, resource.resource.hdr_metadata);
  EXPECT_EQ(0u, GetSharedImageCount());
}

}  // namespace
}  // namespace media
