// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_resource_updater.h"

#include <stddef.h>
#include <stdint.h>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/test/task_environment.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/client/shared_bitmap_reporter.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/test/fake_output_surface.h"
#include "components/viz/test/test_gles2_interface.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/video_frame.h"
#include "skia/ext/skcolorspace_primaries.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

class FakeSharedBitmapReporter : public viz::SharedBitmapReporter {
 public:
  FakeSharedBitmapReporter() = default;
  ~FakeSharedBitmapReporter() override = default;

  // viz::SharedBitmapReporter implementation.
  void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                               const viz::SharedBitmapId& id) override {
    DCHECK_EQ(shared_bitmaps_.count(id), 0u);
    shared_bitmaps_.insert(id);
  }
  void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override {
    DCHECK_EQ(shared_bitmaps_.count(id), 1u);
    shared_bitmaps_.erase(id);
  }

  const base::flat_set<viz::SharedBitmapId> shared_bitmaps() const {
    return shared_bitmaps_;
  }

 private:
  base::flat_set<viz::SharedBitmapId> shared_bitmaps_;
};

class UploadCounterGLES2Interface : public viz::TestGLES2Interface {
 public:
  void TexSubImage2D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     const void* pixels) override {
    ++upload_count_;
    last_upload_ = reinterpret_cast<const uint8_t*>(pixels);
  }

  int UploadCount() { return upload_count_; }
  void ResetUploadCount() { upload_count_ = 0; }
  const uint8_t* last_upload() const { return last_upload_; }

 private:
  int upload_count_;
  const uint8_t* last_upload_ = nullptr;
};

class VideoResourceUpdaterTest : public testing::Test {
 protected:
  VideoResourceUpdaterTest() {
    auto gl = std::make_unique<UploadCounterGLES2Interface>();

    gl_ = gl.get();

    context_provider_ = viz::TestContextProvider::Create(std::move(gl));
    context_provider_->BindToCurrentSequence();
  }

  // testing::Test implementation.
  void SetUp() override {
    testing::Test::SetUp();
    resource_provider_ = std::make_unique<viz::ClientResourceProvider>();
  }

  std::unique_ptr<VideoResourceUpdater> CreateUpdaterForHardware(
      bool use_stream_video_draw_quad = false) {
    return std::make_unique<VideoResourceUpdater>(
        context_provider_.get(), nullptr, resource_provider_.get(),
        use_stream_video_draw_quad, /*use_gpu_memory_buffer_resources=*/false,
        /*max_resource_size=*/10000);
  }

  std::unique_ptr<VideoResourceUpdater> CreateUpdaterForSoftware() {
    return std::make_unique<VideoResourceUpdater>(
        /*context_provider=*/nullptr, &shared_bitmap_reporter_,
        resource_provider_.get(), /*use_stream_video_draw_quad=*/false,
        /*use_gpu_memory_buffer_resources=*/false, /*max_resource_size=*/10000);
  }

  // Note that the number of pixels needed for |size| must be less than or equal
  // to the number of pixels needed for size of 100x100.
  scoped_refptr<VideoFrame> CreateTestYUVVideoFrame(
      const gfx::Size& size = gfx::Size(10, 10)) {
    constexpr int kMaxDimension = 100;
    static uint8_t y_data[kMaxDimension * kMaxDimension] = {0};
    static uint8_t u_data[kMaxDimension * kMaxDimension / 2] = {0};
    static uint8_t v_data[kMaxDimension * kMaxDimension / 2] = {0};

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

  scoped_refptr<VideoFrame> CreateWonkyTestYUVVideoFrame() {
    const int kDimension = 10;
    const int kYWidth = kDimension + 5;
    const int kUWidth = (kYWidth + 1) / 2 + 200;
    const int kVWidth = (kYWidth + 1) / 2 + 1;
    static uint8_t y_data[kYWidth * kDimension] = {0};
    static uint8_t u_data[kUWidth * kDimension] = {0};
    static uint8_t v_data[kVWidth * kDimension] = {0};

    scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapExternalYuvData(
        PIXEL_FORMAT_I422,                        // format
        gfx::Size(kYWidth, kDimension),           // coded_size
        gfx::Rect(2, 0, kDimension, kDimension),  // visible_rect
        gfx::Size(kDimension, kDimension),        // natural_size
        -kYWidth,                                 // y_stride (negative)
        kUWidth,                                  // u_stride
        kVWidth,                                  // v_stride
        y_data + kYWidth * (kDimension - 1),      // y_data
        u_data,                                   // u_data
        v_data,                                   // v_data
        base::TimeDelta());                       // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  scoped_refptr<VideoFrame> CreateTestRGBVideoFrame(VideoPixelFormat format) {
    constexpr int kMaxDimension = 10;
    constexpr gfx::Size kSize = gfx::Size(kMaxDimension, kMaxDimension);
    static uint32_t rgb_data[kMaxDimension * kMaxDimension] = {0};
    scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapExternalData(
        format,                                // format
        kSize,                                 // coded_size
        gfx::Rect(kSize),                      // visible_rect
        kSize,                                 // natural_size
        reinterpret_cast<uint8_t*>(rgb_data),  // data,
        sizeof(rgb_data),                      // data_size
        base::TimeDelta());                    // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  scoped_refptr<VideoFrame> CreateTestRGBVideoFrameWithVisibleRect(
      VideoPixelFormat format) {
    constexpr int kMaxDimension = 5;
    constexpr gfx::Size kSize = gfx::Size(kMaxDimension, kMaxDimension);
    constexpr gfx::Rect kVisibleRect = gfx::Rect(2, 1, 3, 3);
    constexpr uint32_t kPix = 0xFFFFFFFF;
    static uint32_t rgb_data[kMaxDimension * kMaxDimension] = {
        0x00, 0x00, 0x00, 0x00, 0x00,  //
        0x00, 0x00, kPix, kPix, kPix,  //
        0x00, 0x00, kPix, kPix, kPix,  //
        0x00, 0x00, kPix, kPix, kPix,  //
        0x00, 0x00, 0x00, 0x00, 0x00,  //
    };

    scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapExternalData(
        format,                                // format
        kSize,                                 // coded_size
        kVisibleRect,                          // visible_rect
        kVisibleRect.size(),                   // natural_size
        reinterpret_cast<uint8_t*>(rgb_data),  // data,
        sizeof(rgb_data),                      // data_size
        base::TimeDelta());                    // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  scoped_refptr<VideoFrame> CreateTestY16VideoFrameWithVisibleRect() {
    constexpr int kMaxDimension = 5;
    constexpr gfx::Size kSize = gfx::Size(kMaxDimension, kMaxDimension);
    constexpr gfx::Rect kVisibleRect = gfx::Rect(2, 1, 3, 3);
    constexpr uint16_t kPix = 0xFFFF;
    static uint16_t y16_data[kMaxDimension * kMaxDimension] = {
        0x00, 0x00, 0x00, 0x00, 0x00,  //
        0x00, 0x00, kPix, kPix, kPix,  //
        0x00, 0x00, kPix, kPix, kPix,  //
        0x00, 0x00, kPix, kPix, kPix,  //
        0x00, 0x00, 0x00, 0x00, 0x00,  //
    };

    scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapExternalData(
        PIXEL_FORMAT_Y16,
        kSize,                                 // coded_size
        kVisibleRect,                          // visible_rect
        kVisibleRect.size(),                   // natural_size
        reinterpret_cast<uint8_t*>(y16_data),  // data,
        sizeof(y16_data),                      // data_size
        base::TimeDelta());                    // timestamp
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

  scoped_refptr<VideoFrame> CreateP016TestFrame() {
    const int kDimension = 10;
    gfx::Size size(kDimension, kDimension);

    scoped_refptr<VideoFrame> video_frame(VideoFrame::CreateFrame(
        PIXEL_FORMAT_P016LE, size, gfx::Rect(size), size, base::TimeDelta()));
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  void SetReleaseSyncToken(const gpu::SyncToken& sync_token) {
    release_sync_token_ = sync_token;
  }

  scoped_refptr<VideoFrame> CreateTestHardwareVideoFrame(
      VideoPixelFormat format,
      unsigned target) {
    const int kDimension = 10;
    gfx::Size size(kDimension, kDimension);

    auto mailbox = gpu::Mailbox::GenerateForSharedImage();

    gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes] = {
        gpu::MailboxHolder(mailbox, kMailboxSyncToken, target)};
    scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapNativeTextures(
        format, mailbox_holders,
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
    return CreateTestHardwareVideoFrame(PIXEL_FORMAT_ARGB, GL_TEXTURE_2D);
  }

  scoped_refptr<VideoFrame> CreateTestStreamTextureHardwareVideoFrame(
      bool needs_copy) {
    scoped_refptr<VideoFrame> video_frame = CreateTestHardwareVideoFrame(
        PIXEL_FORMAT_ARGB, GL_TEXTURE_EXTERNAL_OES);
    video_frame->metadata().copy_required = needs_copy;
    return video_frame;
  }

#if BUILDFLAG(IS_WIN)
  scoped_refptr<VideoFrame> CreateTestDCompSurfaceVideoFrame() {
    scoped_refptr<VideoFrame> video_frame = CreateTestHardwareVideoFrame(
        PIXEL_FORMAT_ARGB, GL_TEXTURE_EXTERNAL_OES);
    video_frame->metadata().dcomp_surface = true;
    return video_frame;
  }
#endif

  scoped_refptr<VideoFrame> CreateTestYuvHardwareVideoFrame(
      VideoPixelFormat format,
      size_t num_textures,
      unsigned target) {
    const int kDimension = 10;
    gfx::Size size(kDimension, kDimension);

    gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
    for (size_t i = 0; i < num_textures; ++i) {
      gpu::Mailbox mailbox;
      mailbox.name[0] = 50 + 1;
      mailbox_holders[i] =
          gpu::MailboxHolder(mailbox, kMailboxSyncToken, target);
    }
    scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapNativeTextures(
        format, mailbox_holders,
        base::BindOnce(&VideoResourceUpdaterTest::SetReleaseSyncToken,
                       base::Unretained(this)),
        size,                // coded_size
        gfx::Rect(size),     // visible_rect
        size,                // natural_size
        base::TimeDelta());  // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  size_t GetSharedImageCount() {
    return context_provider_->SharedImageInterface()->shared_image_count();
  }

  static const gpu::SyncToken kMailboxSyncToken;

  // VideoResourceUpdater registers as a MemoryDumpProvider, which requires
  // a TaskRunner.
  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<UploadCounterGLES2Interface, DanglingUntriaged> gl_;
  scoped_refptr<viz::TestContextProvider> context_provider_;
  FakeSharedBitmapReporter shared_bitmap_reporter_;
  std::unique_ptr<viz::ClientResourceProvider> resource_provider_;
  gpu::SyncToken release_sync_token_;
};

const gpu::SyncToken VideoResourceUpdaterTest::kMailboxSyncToken =
    gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                   gpu::CommandBufferId::FromUnsafeValue(0x123),
                   7);

TEST_F(VideoResourceUpdaterTest, SoftwareFrame) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  scoped_refptr<VideoFrame> video_frame = CreateTestYUVVideoFrame();

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);

  // Setting to kSharedImageFormat, resources type should not change.
  video_frame->set_shared_image_format_type(
      SharedImageFormatType::kSharedImageFormat);
  resources = updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);
}

TEST_F(VideoResourceUpdaterTest, SoftwareFrameNV12) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(CreateNV12TestFrame());
  EXPECT_EQ(VideoFrameResourceType::RGBA, resources.type);

  // Use a different frame for this test since frames with the same unique_id()
  // expect to use the same resources.
  scoped_refptr<VideoFrame> video_frame = CreateNV12TestFrame();
  gl_->set_supports_texture_rg(true);
  resources = updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);

  // Setting to kSharedImageFormat, resources type should not change.
  video_frame->set_shared_image_format_type(
      SharedImageFormatType::kSharedImageFormat);
  resources = updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);
}

TEST_F(VideoResourceUpdaterTest, SoftwareFrameP016_NoR16Support) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(CreateP016TestFrame());
  EXPECT_EQ(VideoFrameResourceType::RGBA, resources.type);
}

// Ensure we end up with the right SharedImageFormat for each resource.
TEST_F(VideoResourceUpdaterTest, SoftwareFrameRGB) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  for (const auto& fmt : {PIXEL_FORMAT_XBGR, PIXEL_FORMAT_XRGB,
                          PIXEL_FORMAT_ABGR, PIXEL_FORMAT_ARGB}) {
    scoped_refptr<VideoFrame> video_frame = CreateTestRGBVideoFrame(fmt);
    VideoFrameExternalResources resources =
        updater->CreateExternalResourcesFromVideoFrame(video_frame);
    EXPECT_EQ(VideoFrameResourceType::RGBA, resources.type);
#if BUILDFLAG(IS_MAC)
    EXPECT_EQ(resources.resources[0].format,
              viz::SinglePlaneFormat::kBGRA_8888);
#else
    EXPECT_EQ(resources.resources[0].size, video_frame->coded_size());

    if (fmt == PIXEL_FORMAT_XBGR) {
      EXPECT_EQ(resources.resources[0].format,
                viz::SinglePlaneFormat::kRGBA_8888);
    } else if (fmt == PIXEL_FORMAT_XRGB) {
      EXPECT_EQ(resources.resources[0].format,
                viz::SinglePlaneFormat::kBGRA_8888);

    } else if (fmt == PIXEL_FORMAT_ABGR) {
      EXPECT_EQ(resources.resources[0].format,
                viz::SinglePlaneFormat::kRGBA_8888);

    } else if (fmt == PIXEL_FORMAT_ARGB) {
      EXPECT_EQ(resources.resources[0].format,
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
    VideoFrameExternalResources resources =
        updater->CreateExternalResourcesFromVideoFrame(video_frame);
    EXPECT_EQ(VideoFrameResourceType::RGBA, resources.type);
    EXPECT_EQ(resources.resources[0].size, video_frame->coded_size());

    auto rect = video_frame->visible_rect();

    const auto bytes_per_row = video_frame->row_bytes(VideoFrame::kARGBPlane);
    const auto bytes_per_element =
        VideoFrame::BytesPerElement(fmt, VideoFrame::kARGBPlane);
    auto* dest_pixels = gl_->last_upload() + rect.y() * bytes_per_row +
                        rect.x() * bytes_per_element;
    auto* src_pixels = video_frame->visible_data(VideoFrame::kARGBPlane);

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
  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA, resources.type);
  EXPECT_EQ(resources.resources[0].size, video_frame->coded_size());

  auto rect = video_frame->visible_rect();

  // Just used for sizing information, channel order doesn't matter.
  constexpr auto kOutputFormat = PIXEL_FORMAT_ARGB;

  const auto bytes_per_row = VideoFrame::RowBytes(
      VideoFrame::kARGBPlane, kOutputFormat, video_frame->coded_size().width());
  const auto bytes_per_element =
      VideoFrame::BytesPerElement(kOutputFormat, VideoFrame::kARGBPlane);
  auto* dest_pixels = gl_->last_upload() + rect.y() * bytes_per_row +
                      rect.x() * bytes_per_element;

  // Pixels are 0xFFFFFFFF, so channel reordering doesn't matter.
  for (int y = 0; y < rect.height(); ++y) {
    for (int x = 0; x < rect.width() * bytes_per_element; ++x) {
      const auto pos = y * bytes_per_row + x;
      ASSERT_EQ(0xFF, dest_pixels[pos]);
    }
  }
}

TEST_F(VideoResourceUpdaterTest, HighBitFrameNoF16) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  scoped_refptr<VideoFrame> video_frame = CreateTestHighBitFrame();

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);

  // Setting to kSharedImageFormat, resources type should not change.
  video_frame->set_shared_image_format_type(
      SharedImageFormatType::kSharedImageFormat);
  resources = updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);
}

class VideoResourceUpdaterTestWithF16 : public VideoResourceUpdaterTest {
 public:
  VideoResourceUpdaterTestWithF16() : VideoResourceUpdaterTest() {
    gl_->set_support_texture_half_float_linear(true);
  }
};

TEST_F(VideoResourceUpdaterTestWithF16, HighBitFrame) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  scoped_refptr<VideoFrame> video_frame = CreateTestHighBitFrame();

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);
  EXPECT_NEAR(resources.multiplier, 2.0, 0.1);
  EXPECT_NEAR(resources.offset, 0.5, 0.1);

  // Create the resource again, to test the path where the
  // resources are cached.
  VideoFrameExternalResources resources2 =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources2.type);
  EXPECT_NEAR(resources2.multiplier, 2.0, 0.1);
  EXPECT_NEAR(resources2.offset, 0.5, 0.1);
}

class VideoResourceUpdaterTestWithR16 : public VideoResourceUpdaterTest {
 public:
  VideoResourceUpdaterTestWithR16() : VideoResourceUpdaterTest() {
    auto* sii = context_provider_->SharedImageInterface();
    auto shared_image_caps = sii->GetCapabilities();
    shared_image_caps.supports_r16_shared_images = true;
    sii->SetCapabilities(shared_image_caps);

    gl_->set_support_texture_norm16(true);
  }
};

TEST_F(VideoResourceUpdaterTestWithR16, SoftwareFrameP016) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(CreateP016TestFrame());
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);
}

TEST_F(VideoResourceUpdaterTestWithR16, HighBitFrame) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  scoped_refptr<VideoFrame> video_frame = CreateTestHighBitFrame();

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);

  // Max 10-bit values as read by a sampler.
  double max_10bit_value = ((1 << 10) - 1) / 65535.0;
  EXPECT_NEAR(resources.multiplier * max_10bit_value, 1.0, 0.0001);
  EXPECT_NEAR(resources.offset, 0.0, 0.1);

  // Create the resource again, to test the path where the
  // resources are cached.
  VideoFrameExternalResources resources2 =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources2.type);
  EXPECT_NEAR(resources2.multiplier * max_10bit_value, 1.0, 0.0001);
  EXPECT_NEAR(resources2.offset, 0.0, 0.1);
}

TEST_F(VideoResourceUpdaterTest, NV12FrameSoftwareCompositor) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForSoftware();
  scoped_refptr<VideoFrame> video_frame = CreateNV12TestFrame();

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resources.type);
}

TEST_F(VideoResourceUpdaterTest, P016FrameSoftwareCompositor) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForSoftware();
  scoped_refptr<VideoFrame> video_frame = CreateP016TestFrame();

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resources.type);
}

TEST_F(VideoResourceUpdaterTest, HighBitFrameSoftwareCompositor) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForSoftware();
  scoped_refptr<VideoFrame> video_frame = CreateTestHighBitFrame();

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resources.type);
}

TEST_F(VideoResourceUpdaterTest, WonkySoftwareFrame) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  scoped_refptr<VideoFrame> video_frame = CreateWonkyTestYUVVideoFrame();

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);

  // Setting to kSharedImageFormat, resources type should not change.
  video_frame->set_shared_image_format_type(
      SharedImageFormatType::kSharedImageFormat);
  resources = updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);
}

TEST_F(VideoResourceUpdaterTest, WonkySoftwareFrameSoftwareCompositor) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForSoftware();
  scoped_refptr<VideoFrame> video_frame = CreateWonkyTestYUVVideoFrame();

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resources.type);
}

TEST_F(VideoResourceUpdaterTest, ReuseResource) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  scoped_refptr<VideoFrame> video_frame = CreateTestYUVVideoFrame();
  video_frame->set_timestamp(base::Seconds(1234));

  // Allocate the resources for a YUV video frame.
  gl_->ResetUploadCount();
  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);
  EXPECT_EQ(3u, resources.resources.size());
  EXPECT_EQ(3u, resources.release_callbacks.size());
  // Expect exactly three texture uploads, one for each plane.
  EXPECT_EQ(3, gl_->UploadCount());

  // Simulate the ResourceProvider releasing the resources back to the video
  // updater.
  for (auto& release_callback : resources.release_callbacks)
    std::move(release_callback).Run(gpu::SyncToken(), false);

  // Allocate resources for the same frame.
  gl_->ResetUploadCount();
  resources = updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);
  EXPECT_EQ(3u, resources.resources.size());
  EXPECT_EQ(3u, resources.release_callbacks.size());
  // The data should be reused so expect no texture uploads.
  EXPECT_EQ(0, gl_->UploadCount());
}

TEST_F(VideoResourceUpdaterTest, ReuseResourceNV12) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  scoped_refptr<VideoFrame> video_frame = CreateNV12TestFrame();
  video_frame->set_timestamp(base::Seconds(1234));
  gl_->set_supports_texture_rg(true);

  // Allocate the resources for a YUV video frame.
  gl_->ResetUploadCount();
  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);
  EXPECT_EQ(2u, resources.resources.size());
  EXPECT_EQ(2u, resources.release_callbacks.size());
  // Expect exactly three texture uploads, one for each plane.
  EXPECT_EQ(2, gl_->UploadCount());

  // Simulate the ResourceProvider releasing the resources back to the video
  // updater.
  for (auto& release_callback : resources.release_callbacks)
    std::move(release_callback).Run(gpu::SyncToken(), false);

  // Allocate resources for the same frame.
  gl_->ResetUploadCount();
  resources = updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);
  EXPECT_EQ(2u, resources.resources.size());
  EXPECT_EQ(2u, resources.release_callbacks.size());
  // The data should be reused so expect no texture uploads.
  EXPECT_EQ(0, gl_->UploadCount());
}

TEST_F(VideoResourceUpdaterTest, ReuseResourceNoDelete) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  scoped_refptr<VideoFrame> video_frame = CreateTestYUVVideoFrame();
  video_frame->set_timestamp(base::Seconds(1234));

  // Allocate the resources for a YUV video frame.
  gl_->ResetUploadCount();
  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);
  EXPECT_EQ(3u, resources.resources.size());
  EXPECT_EQ(3u, resources.release_callbacks.size());
  // Expect exactly three texture uploads, one for each plane.
  EXPECT_EQ(3, gl_->UploadCount());

  // Allocate resources for the same frame.
  gl_->ResetUploadCount();
  resources = updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);
  EXPECT_EQ(3u, resources.resources.size());
  EXPECT_EQ(3u, resources.release_callbacks.size());
  // The data should be reused so expect no texture uploads.
  EXPECT_EQ(0, gl_->UploadCount());
}

TEST_F(VideoResourceUpdaterTest, SoftwareFrameSoftwareCompositor) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForSoftware();
  scoped_refptr<VideoFrame> video_frame = CreateTestYUVVideoFrame();

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resources.type);
}

TEST_F(VideoResourceUpdaterTest, SoftwareFrameRGBSoftwareCompositor) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForSoftware();
  for (const auto& fmt : {PIXEL_FORMAT_XBGR, PIXEL_FORMAT_XRGB,
                          PIXEL_FORMAT_ABGR, PIXEL_FORMAT_ARGB}) {
    scoped_refptr<VideoFrame> video_frame = CreateTestRGBVideoFrame(fmt);
    VideoFrameExternalResources resources =
        updater->CreateExternalResourcesFromVideoFrame(video_frame);
    EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resources.type);
    EXPECT_EQ(resources.resources[0].format,
              viz::SinglePlaneFormat::kRGBA_8888);
  }
}

TEST_F(VideoResourceUpdaterTest, ReuseResourceSoftwareCompositor) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForSoftware();
  scoped_refptr<VideoFrame> video_frame = CreateTestYUVVideoFrame();
  video_frame->set_timestamp(base::Seconds(1234));

  // Allocate the resources for a software video frame.
  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resources.type);
  EXPECT_EQ(1u, resources.resources.size());
  EXPECT_EQ(1u, resources.release_callbacks.size());
  // Expect exactly one allocated shared bitmap.
  EXPECT_EQ(1u, shared_bitmap_reporter_.shared_bitmaps().size());
  auto shared_bitmaps_copy = shared_bitmap_reporter_.shared_bitmaps();

  // Simulate the ResourceProvider releasing the resource back to the video
  // updater.
  std::move(resources.release_callbacks[0]).Run(gpu::SyncToken(), false);

  // Allocate resources for the same frame.
  resources = updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resources.type);
  EXPECT_EQ(1u, resources.resources.size());
  EXPECT_EQ(1u, resources.release_callbacks.size());

  // Ensure that the same shared bitmap was reused.
  EXPECT_EQ(shared_bitmap_reporter_.shared_bitmaps(), shared_bitmaps_copy);
}

TEST_F(VideoResourceUpdaterTest, ReuseResourceNoDeleteSoftwareCompositor) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForSoftware();
  scoped_refptr<VideoFrame> video_frame = CreateTestYUVVideoFrame();
  video_frame->set_timestamp(base::Seconds(1234));

  // Allocate the resources for a software video frame.
  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resources.type);
  EXPECT_EQ(1u, resources.resources.size());
  EXPECT_EQ(1u, resources.release_callbacks.size());
  // Expect exactly one allocated shared bitmap.
  EXPECT_EQ(1u, shared_bitmap_reporter_.shared_bitmaps().size());
  auto shared_bitmaps_copy = shared_bitmap_reporter_.shared_bitmaps();

  // Allocate resources for the same frame.
  resources = updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resources.type);
  EXPECT_EQ(1u, resources.resources.size());
  EXPECT_EQ(1u, resources.release_callbacks.size());

  // Ensure that the same shared bitmap was reused.
  EXPECT_EQ(shared_bitmap_reporter_.shared_bitmaps(), shared_bitmaps_copy);
}

TEST_F(VideoResourceUpdaterTest, ChangeResourceSizeSoftwareCompositor) {
  constexpr gfx::Size kSize1(10, 10);
  constexpr gfx::Size kSize2(20, 20);

  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForSoftware();

  // Allocate the resources for a software video frame.
  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(
          CreateTestYUVVideoFrame(kSize1));
  // Expect exactly one allocated shared bitmap.
  EXPECT_EQ(1u, shared_bitmap_reporter_.shared_bitmaps().size());
  auto shared_bitmaps_copy = shared_bitmap_reporter_.shared_bitmaps();

  // Simulate the ResourceProvider releasing the resource back to the video
  // updater.
  std::move(resources.release_callbacks[0]).Run(gpu::SyncToken(), false);

  // Allocate resources for the next frame with a different size.
  resources = updater->CreateExternalResourcesFromVideoFrame(
      CreateTestYUVVideoFrame(kSize2));

  // The first resource was released, so it can be reused but it's the wrong
  // size. We should expect the first shared bitmap to be deleted and a new
  // shared bitmap to be allocated.
  EXPECT_EQ(1u, shared_bitmap_reporter_.shared_bitmaps().size());
  EXPECT_NE(shared_bitmap_reporter_.shared_bitmaps(), shared_bitmaps_copy);
}

TEST_F(VideoResourceUpdaterTest, CreateForHardwarePlanes_ResourceFormat) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();

  scoped_refptr<VideoFrame> video_frame = CreateTestRGBAHardwareVideoFrame();

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resources.type);
  EXPECT_EQ(1u, resources.resources.size());
  EXPECT_EQ(1u, resources.release_callbacks.size());

  video_frame = CreateTestYuvHardwareVideoFrame(PIXEL_FORMAT_I420, 3,
                                                GL_TEXTURE_RECTANGLE_ARB);

  resources = updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);
  EXPECT_EQ(3u, resources.resources.size());
  EXPECT_EQ(3u, resources.release_callbacks.size());
  EXPECT_EQ(viz::SinglePlaneFormat::kR_8, resources.resources[0].format);
  EXPECT_EQ(viz::SinglePlaneFormat::kR_8, resources.resources[1].format);
  EXPECT_EQ(viz::SinglePlaneFormat::kR_8, resources.resources[2].format);
  EXPECT_EQ(resources.resources[0].synchronization_type,
            viz::TransferableResource::SynchronizationType::kSyncToken);
  EXPECT_EQ(resources.resources[1].synchronization_type,
            viz::TransferableResource::SynchronizationType::kSyncToken);
  EXPECT_EQ(resources.resources[2].synchronization_type,
            viz::TransferableResource::SynchronizationType::kSyncToken);

  video_frame = CreateTestYuvHardwareVideoFrame(PIXEL_FORMAT_I420, 3,
                                                GL_TEXTURE_RECTANGLE_ARB);
  video_frame->metadata().read_lock_fences_enabled = true;

  resources = updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(
      resources.resources[0].synchronization_type,
      viz::TransferableResource::SynchronizationType::kGpuCommandsCompleted);
  EXPECT_EQ(
      resources.resources[1].synchronization_type,
      viz::TransferableResource::SynchronizationType::kGpuCommandsCompleted);
  EXPECT_EQ(
      resources.resources[2].synchronization_type,
      viz::TransferableResource::SynchronizationType::kGpuCommandsCompleted);
}

TEST_F(VideoResourceUpdaterTest, CreateForHardwarePlanes_SharedImageFormat) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  scoped_refptr<VideoFrame> video_frame = CreateTestYuvHardwareVideoFrame(
      PIXEL_FORMAT_I420, 1, GL_TEXTURE_RECTANGLE_ARB);
  // Setting to kSharedImageFormat, resources type should change to RGB.
  video_frame->set_shared_image_format_type(
      SharedImageFormatType::kSharedImageFormat);
  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGB, resources.type);
  EXPECT_EQ(1u, resources.resources.size());
  EXPECT_EQ(1u, resources.release_callbacks.size());
  EXPECT_EQ(viz::MultiPlaneFormat::kI420, resources.resources[0].format);
  EXPECT_EQ(resources.resources[0].synchronization_type,
            viz::TransferableResource::SynchronizationType::kSyncToken);

  video_frame = CreateTestYuvHardwareVideoFrame(PIXEL_FORMAT_I420, 1,
                                                GL_TEXTURE_RECTANGLE_ARB);
  video_frame->set_shared_image_format_type(
      SharedImageFormatType::kSharedImageFormat);
  video_frame->metadata().read_lock_fences_enabled = true;

  resources = updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(
      resources.resources[0].synchronization_type,
      viz::TransferableResource::SynchronizationType::kGpuCommandsCompleted);
}

TEST_F(VideoResourceUpdaterTest,
       CreateForHardwarePlanes_StreamTexture_CopyToNewTexture) {
  // Note that |use_stream_video_draw_quad| is true for this test.
  std::unique_ptr<VideoResourceUpdater> updater =
      CreateUpdaterForHardware(true);
  EXPECT_EQ(0u, GetSharedImageCount());
  scoped_refptr<VideoFrame> video_frame =
      CreateTestStreamTextureHardwareVideoFrame(/*needs_copy=*/false);

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::STREAM_TEXTURE, resources.type);
  EXPECT_EQ(1u, resources.resources.size());
  EXPECT_EQ((GLenum)GL_TEXTURE_EXTERNAL_OES,
            resources.resources[0].mailbox_holder.texture_target);
  EXPECT_EQ(1u, resources.release_callbacks.size());
  EXPECT_EQ(0u, GetSharedImageCount());

  // A copied stream texture should return an RGBA resource in a new
  // GL_TEXTURE_2D texture.
  video_frame = CreateTestStreamTextureHardwareVideoFrame(/*needs_copy=*/true);
  resources = updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resources.type);
  EXPECT_EQ(1u, resources.resources.size());
  EXPECT_EQ((GLenum)GL_TEXTURE_2D,
            resources.resources[0].mailbox_holder.texture_target);
  EXPECT_EQ(1u, resources.release_callbacks.size());
  EXPECT_EQ(1u, GetSharedImageCount());
}

TEST_F(VideoResourceUpdaterTest, CreateForHardwarePlanes_TextureQuad) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  EXPECT_EQ(0u, GetSharedImageCount());
  scoped_refptr<VideoFrame> video_frame =
      CreateTestStreamTextureHardwareVideoFrame(/*needs_copy=*/false);

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGBA_PREMULTIPLIED, resources.type);
  EXPECT_EQ(1u, resources.resources.size());
  EXPECT_EQ((GLenum)GL_TEXTURE_EXTERNAL_OES,
            resources.resources[0].mailbox_holder.texture_target);
  EXPECT_EQ(1u, resources.release_callbacks.size());
  EXPECT_EQ(0u, GetSharedImageCount());
}

#if BUILDFLAG(IS_WIN)
// Check that a video frame marked as containing a DComp surface turns into a
// texture draw quad that is required for overlay.
TEST_F(VideoResourceUpdaterTest, CreateForHardwarePlanes_DCompSurface) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  EXPECT_EQ(0u, GetSharedImageCount());
  scoped_refptr<VideoFrame> video_frame = CreateTestDCompSurfaceVideoFrame();

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::STREAM_TEXTURE, resources.type);
  EXPECT_EQ(1u, resources.resources.size());
  EXPECT_EQ((GLenum)GL_TEXTURE_EXTERNAL_OES,
            resources.resources[0].mailbox_holder.texture_target);
  EXPECT_EQ(1u, resources.release_callbacks.size());
  EXPECT_EQ(0u, GetSharedImageCount());

  updater->ObtainFrameResources(video_frame);

  std::unique_ptr<viz::CompositorRenderPass> pass =
      viz::CompositorRenderPass::Create();
  pass->SetNew(/*pass_id=*/viz::CompositorRenderPassId{1},
               /*output_rect=*/gfx::Rect(video_frame->coded_size()),
               /*damage_rect=*/gfx::Rect(),
               /*transform_to_root_target=*/gfx::Transform());
  updater->AppendQuads(
      /*render_pass=*/pass.get(), video_frame,
      /*transform=*/gfx::Transform(),
      /*quad_rect=*/gfx::Rect(video_frame->coded_size()),
      /*visible_quad_rect=*/gfx::Rect(video_frame->coded_size()),
      gfx::MaskFilterInfo(), /*clip_rect=*/absl::nullopt,
      /*context_opaque=*/true, /*draw_opacity=*/1.0,
      /*sorting_context_id=*/0);

  EXPECT_EQ(1u, pass->quad_list.size());

  const viz::TextureDrawQuad* quad =
      pass->quad_list.ElementAt(0)->DynamicCast<viz::TextureDrawQuad>();
  EXPECT_NE(nullptr, quad);
  EXPECT_EQ(true, quad->is_stream_video);
  EXPECT_EQ(viz::OverlayPriority::kRequired, quad->overlay_priority_hint);

  updater->ReleaseFrameResources();
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

    VideoFrameExternalResources resources =
        updater->CreateExternalResourcesFromVideoFrame(video_frame);

    ASSERT_EQ(resources.release_callbacks.size(), 1u);
    std::move(resources.release_callbacks[0]).Run(sync_token, false);
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

    VideoFrameExternalResources resources1 =
        updater->CreateExternalResourcesFromVideoFrame(video_frame);
    ASSERT_EQ(resources1.release_callbacks.size(), 1u);
    std::move(resources1.release_callbacks[0]).Run(sync_token1, false);

    VideoFrameExternalResources resources2 =
        updater->CreateExternalResourcesFromVideoFrame(video_frame);
    ASSERT_EQ(resources2.release_callbacks.size(), 1u);
    std::move(resources2.release_callbacks[0]).Run(sync_token2, false);
  }

  EXPECT_TRUE(release_sync_token_.HasData());
  EXPECT_NE(release_sync_token_, sync_token1);
  EXPECT_EQ(release_sync_token_, sync_token2);
}

// Pass mailbox sync token as is if no GL operations are performed before frame
// resources are handed off to the compositor.
TEST_F(VideoResourceUpdaterTest, PassMailboxSyncToken) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();

  scoped_refptr<VideoFrame> video_frame = CreateTestRGBAHardwareVideoFrame();

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);

  ASSERT_EQ(resources.resources.size(), 1u);
  EXPECT_TRUE(resources.resources[0].mailbox_holder.sync_token.HasData());
  EXPECT_EQ(resources.resources[0].mailbox_holder.sync_token,
            kMailboxSyncToken);
}

// Generate new sync token for compositor when copying the texture.
TEST_F(VideoResourceUpdaterTest, GenerateSyncTokenOnTextureCopy) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();

  scoped_refptr<VideoFrame> video_frame =
      CreateTestStreamTextureHardwareVideoFrame(/*needs_copy=*/true);

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);

  ASSERT_EQ(resources.resources.size(), 1u);
  EXPECT_TRUE(resources.resources[0].mailbox_holder.sync_token.HasData());
  EXPECT_NE(resources.resources[0].mailbox_holder.sync_token,
            kMailboxSyncToken);
}

// NV12 VideoFrames backed by a single native texture can be sampled out
// by GL as RGB. To use them as HW overlays we need to know the format
// of the underlying buffer, that is YUV_420_BIPLANAR.
TEST_F(VideoResourceUpdaterTest, CreateForHardwarePlanes_SingleNV12) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  EXPECT_EQ(0u, GetSharedImageCount());
  scoped_refptr<VideoFrame> video_frame =
      CreateTestHardwareVideoFrame(PIXEL_FORMAT_NV12, GL_TEXTURE_EXTERNAL_OES);

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGB, resources.type);
  EXPECT_EQ(1u, resources.resources.size());
  EXPECT_EQ((GLenum)GL_TEXTURE_EXTERNAL_OES,
            resources.resources[0].mailbox_holder.texture_target);
  EXPECT_EQ(viz::LegacyMultiPlaneFormat::kNV12, resources.resources[0].format);
  EXPECT_EQ(0u, GetSharedImageCount());
}

TEST_F(VideoResourceUpdaterTest,
       CreateForHardwarePlanes_DualNV12_ResourceFormat) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  EXPECT_EQ(0u, GetSharedImageCount());
  scoped_refptr<VideoFrame> video_frame = CreateTestYuvHardwareVideoFrame(
      PIXEL_FORMAT_NV12, 2, GL_TEXTURE_EXTERNAL_OES);

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);
  EXPECT_EQ(2u, resources.resources.size());
  EXPECT_EQ(2u, resources.release_callbacks.size());
  EXPECT_EQ((GLenum)GL_TEXTURE_EXTERNAL_OES,
            resources.resources[0].mailbox_holder.texture_target);
  EXPECT_EQ((GLenum)GL_TEXTURE_EXTERNAL_OES,
            resources.resources[1].mailbox_holder.texture_target);
  // |updater| doesn't set |buffer_format| in this case.
  EXPECT_EQ(viz::SinglePlaneFormat::kR_8, resources.resources[0].format);
  EXPECT_EQ(viz::SinglePlaneFormat::kRG_88, resources.resources[1].format);

  video_frame = CreateTestYuvHardwareVideoFrame(PIXEL_FORMAT_NV12, 2,
                                                GL_TEXTURE_RECTANGLE_ARB);
  resources = updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::YUV, resources.type);
  EXPECT_EQ(2u, resources.resources.size());
  EXPECT_EQ((GLenum)GL_TEXTURE_RECTANGLE_ARB,
            resources.resources[0].mailbox_holder.texture_target);
  EXPECT_EQ((GLenum)GL_TEXTURE_RECTANGLE_ARB,
            resources.resources[1].mailbox_holder.texture_target);
  EXPECT_EQ(viz::SinglePlaneFormat::kR_8, resources.resources[0].format);
  EXPECT_EQ(viz::SinglePlaneFormat::kRG_88, resources.resources[1].format);
  EXPECT_EQ(0u, GetSharedImageCount());
}

TEST_F(VideoResourceUpdaterTest,
       CreateForHardwarePlanes_DualNV12_SharedImageFormat) {
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  EXPECT_EQ(0u, GetSharedImageCount());
  scoped_refptr<VideoFrame> video_frame = CreateTestYuvHardwareVideoFrame(
      PIXEL_FORMAT_NV12, 1, GL_TEXTURE_RECTANGLE_ARB);
  video_frame->set_shared_image_format_type(
      SharedImageFormatType::kSharedImageFormat);
  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  // Setting to kSharedImageFormat, resources type should bo RGB.
  EXPECT_EQ(VideoFrameResourceType::RGB, resources.type);
  EXPECT_EQ(1u, resources.resources.size());
  EXPECT_EQ((GLenum)GL_TEXTURE_RECTANGLE_ARB,
            resources.resources[0].mailbox_holder.texture_target);
  EXPECT_EQ(viz::MultiPlaneFormat::kNV12, resources.resources[0].format);
  EXPECT_EQ(0u, GetSharedImageCount());

  video_frame = CreateTestYuvHardwareVideoFrame(PIXEL_FORMAT_NV12, 1,
                                                GL_TEXTURE_EXTERNAL_OES);
  video_frame->set_shared_image_format_type(
      SharedImageFormatType::kSharedImageFormat);

  resources = updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGB, resources.type);
  EXPECT_EQ(1u, resources.resources.size());
  EXPECT_EQ(1u, resources.release_callbacks.size());
  EXPECT_EQ((GLenum)GL_TEXTURE_EXTERNAL_OES,
            resources.resources[0].mailbox_holder.texture_target);
  // |updater| doesn't set |buffer_format| in this case.
  EXPECT_EQ(viz::MultiPlaneFormat::kNV12, resources.resources[0].format);
  EXPECT_EQ(0u, GetSharedImageCount());
}

TEST_F(VideoResourceUpdaterTest, CreateForHardwarePlanes_SingleP016HDR) {
  constexpr auto kHDR10ColorSpace = gfx::ColorSpace::CreateHDR10();
  gfx::HDRMetadata hdr_metadata{};
  hdr_metadata.smpte_st_2086 =
      gfx::HdrMetadataSmpteSt2086(SkNamedPrimariesExt::kP3,
                                  /*luminance_max=*/1000,
                                  /*luminance_min=*/0);
  std::unique_ptr<VideoResourceUpdater> updater = CreateUpdaterForHardware();
  EXPECT_EQ(0u, GetSharedImageCount());
  scoped_refptr<VideoFrame> video_frame = CreateTestHardwareVideoFrame(
      PIXEL_FORMAT_P016LE, GL_TEXTURE_EXTERNAL_OES);
  video_frame->set_color_space(kHDR10ColorSpace);
  video_frame->set_hdr_metadata(hdr_metadata);

  VideoFrameExternalResources resources =
      updater->CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameResourceType::RGB, resources.type);
  EXPECT_EQ(1u, resources.resources.size());
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_EXTERNAL_OES),
            resources.resources[0].mailbox_holder.texture_target);
  EXPECT_EQ(viz::LegacyMultiPlaneFormat::kP010, resources.resources[0].format);
  EXPECT_EQ(kHDR10ColorSpace, resources.resources[0].color_space);
  EXPECT_EQ(hdr_metadata, resources.resources[0].hdr_metadata);
  EXPECT_EQ(0u, GetSharedImageCount());
}

}  // namespace
}  // namespace media
