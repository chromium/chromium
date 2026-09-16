// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/vt_video_encode_accelerator_mac.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/media_buildflags.h"
#include "media/parsers/h264_parser.h"
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#include "media/parsers/h265_nalu_parser.h"
#endif
#include "media/video/video_encode_accelerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/mac/io_surface.h"

namespace media {

namespace {

using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsTrue;
using ::testing::Not;

class BaseTestVEAClient : public VideoEncodeAccelerator::Client {
 public:
  BaseTestVEAClient() = default;
  ~BaseTestVEAClient() override = default;

  void NotifyEncoderInfoChange(const VideoEncoderInfo& info) override {
    encoder_info_ = info;
  }
  void NotifyErrorStatus(const EncoderStatus& status) override {
    last_status_ = status;
  }

  const std::optional<VideoEncoderInfo>& encoder_info() const {
    return encoder_info_;
  }
  const std::optional<EncoderStatus>& last_status() const {
    return last_status_;
  }

 protected:
  std::optional<VideoEncoderInfo> encoder_info_;
  std::optional<EncoderStatus> last_status_;
};

class TestVEAClient : public BaseTestVEAClient {
 public:
  TestVEAClient(VideoEncodeAccelerator* encoder,
                VideoCodecProfile profile,
                base::OnceClosure bitstream_ready_cb)
      : encoder_(encoder),
        profile_(profile),
        bitstream_ready_cb_(std::move(bitstream_ready_cb)) {}

  size_t bitstream_buffer_count() const { return bitstream_buffer_count_; }
  size_t keyframe_count() const { return keyframe_count_; }
  const std::optional<BitstreamBufferMetadata>& last_metadata() const {
    return last_metadata_;
  }

  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override {
    output_buffer_size_ = output_buffer_size;
    ProvideBitstreamBuffer();
  }

  void BitstreamBufferReady(int32_t bitstream_buffer_id,
                            const BitstreamBufferMetadata& metadata) override {
    base::ScopedClosureRunner quit_runner(std::move(bitstream_ready_cb_));
    ++bitstream_buffer_count_;
    if (metadata.key_frame) {
      ++keyframe_count_;
    }
    last_metadata_ = metadata;
    ASSERT_GT(metadata.payload_size_bytes, 4u);
    EXPECT_TRUE(metadata.key_frame);
    auto it = mappings_.find(bitstream_buffer_id);
    ASSERT_NE(it, mappings_.end());
    base::span<const uint8_t> data =
        it->second.GetMemoryAsSpan<uint8_t>(metadata.payload_size_bytes);

    const VideoCodec codec = VideoCodecProfileToVideoCodec(profile_);
    if (codec == VideoCodec::kH264) {
      std::vector<H264NALU> nalus;
      EXPECT_TRUE(H264Parser::ParseNALUs(data, &nalus));
      EXPECT_FALSE(nalus.empty());
    }
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    else if (codec == VideoCodec::kHEVC) {
      H265NaluParser parser;
      parser.SetStream(data);
      H265NALU nalu;
      H265NaluParser::Result result;
      size_t nalu_count = 0;
      while ((result = parser.AdvanceToNextNALU(&nalu)) ==
             H265NaluParser::kOk) {
        ++nalu_count;
      }
      EXPECT_EQ(result, H265NaluParser::kEOStream);
      EXPECT_GT(nalu_count, 0u);
    }
#endif
    mappings_.erase(it);

    ProvideBitstreamBuffer();
  }

  void NotifyErrorStatus(const EncoderStatus& status) override {
    BaseTestVEAClient::NotifyErrorStatus(status);
    if (bitstream_ready_cb_) {
      std::move(bitstream_ready_cb_).Run();
    }
    FAIL() << "Unexpected encoder error: " << status.message();
  }

  void DetachEncoder() { encoder_ = nullptr; }

 private:
  void ProvideBitstreamBuffer() {
    auto region = base::UnsafeSharedMemoryRegion::Create(output_buffer_size_);
    ASSERT_TRUE(region.IsValid());
    auto mapping = region.Map();
    ASSERT_TRUE(mapping.IsValid());
    int32_t buffer_id = next_buffer_id_++;
    mappings_[buffer_id] = std::move(mapping);
    encoder_->UseOutputBitstreamBuffer(
        BitstreamBuffer(buffer_id, std::move(region), output_buffer_size_));
  }

  raw_ptr<VideoEncodeAccelerator> encoder_;
  VideoCodecProfile profile_;
  base::OnceClosure bitstream_ready_cb_;
  size_t output_buffer_size_ = 0;
  int32_t next_buffer_id_ = 0;
  size_t bitstream_buffer_count_ = 0;
  size_t keyframe_count_ = 0;
  std::optional<BitstreamBufferMetadata> last_metadata_;
  std::map<int32_t, base::WritableSharedMemoryMapping> mappings_;
};

class ErrorExpectingVEAClient : public BaseTestVEAClient {
 public:
  explicit ErrorExpectingVEAClient(base::OnceClosure error_cb)
      : error_cb_(std::move(error_cb)) {}

  void RequireBitstreamBuffers(unsigned int,
                               const gfx::Size&,
                               size_t) override {}
  void BitstreamBufferReady(int32_t, const BitstreamBufferMetadata&) override {
    FAIL() << "Unexpected bitstream buffer ready";
  }
  void NotifyErrorStatus(const EncoderStatus& status) override {
    BaseTestVEAClient::NotifyErrorStatus(status);
    if (error_cb_) {
      std::move(error_cb_).Run();
    }
  }

 private:
  base::OnceClosure error_cb_;
};

class TestOverlayImageRepresentation : public gpu::OverlayImageRepresentation {
 public:
  TestOverlayImageRepresentation(gpu::SharedImageManager* manager,
                                 gpu::SharedImageBacking* backing,
                                 gpu::MemoryTypeTracker* tracker,
                                 gfx::ScopedIOSurface io_surface,
                                 base::OnceClosure destroy_cb)
      : gpu::OverlayImageRepresentation(manager, backing, tracker),
        io_surface_(std::move(io_surface)),
        destroy_cb_(std::move(destroy_cb)) {}

  ~TestOverlayImageRepresentation() override {
    if (destroy_cb_) {
      std::move(destroy_cb_).Run();
    }
  }

 protected:
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override {
    return true;
  }
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override {}
  gfx::ScopedIOSurface GetIOSurface() const override { return io_surface_; }

 private:
  gfx::ScopedIOSurface io_surface_;
  base::OnceClosure destroy_cb_;
};

class TestIOSurfaceBacking : public gpu::SharedImageBacking {
 public:
  TestIOSurfaceBacking(const gpu::Mailbox& mailbox,
                       viz::SharedImageFormat format,
                       const gfx::Size& size,
                       gfx::ScopedIOSurface io_surface,
                       base::OnceClosure representation_destroyed_cb)
      : gpu::SharedImageBacking(
            mailbox,
            gpu::SharedImageInfo(format,
                                 size,
                                 gfx::ColorSpace::CreateSRGB(),
                                 kTopLeft_GrSurfaceOrigin,
                                 kOpaque_SkAlphaType,
                                 gpu::SHARED_IMAGE_USAGE_SCANOUT,
                                 "TestIOSurfaceBacking"),
            format.EstimatedSizeInBytes(size),
            /*is_thread_safe=*/true),
        io_surface_(std::move(io_surface)),
        representation_destroyed_cb_(std::move(representation_destroyed_cb)) {}

  gpu::SharedImageBackingType GetType() const override {
    return gpu::SharedImageBackingType::kTest;
  }
  gfx::Rect ClearedRect() const override { return gfx::Rect(size()); }
  void SetClearedRect(const gfx::Rect& cleared_rect) override {}
  void Update(gfx::GpuFenceHandle in_fence) override {}

 protected:
  std::unique_ptr<gpu::OverlayImageRepresentation> ProduceOverlay(
      gpu::SharedImageManager* manager,
      gpu::MemoryTypeTracker* tracker) override {
    return std::make_unique<TestOverlayImageRepresentation>(
        manager, this, tracker, io_surface_,
        std::move(representation_destroyed_cb_));
  }

 private:
  gfx::ScopedIOSurface io_surface_;
  base::OnceClosure representation_destroyed_cb_;
};

class TestCommandBufferHelper : public CommandBufferHelper {
 public:
  TestCommandBufferHelper(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      gpu::SharedImageManager* shared_image_manager)
      : CommandBufferHelper(std::move(task_runner)),
        shared_image_manager_(shared_image_manager) {}

  void WaitForSyncToken(gpu::SyncToken sync_token,
                        base::OnceClosure done_cb) override {
    std::move(done_cb).Run();
  }
  gpu::SharedImageStub* GetSharedImageStub() override { return nullptr; }
  gpu::MemoryTypeTracker* GetMemoryTypeTracker() override { return &tracker_; }
  gpu::SharedImageManager* GetSharedImageManager() override {
    return shared_image_manager_;
  }

 private:
  ~TestCommandBufferHelper() override = default;
  raw_ptr<gpu::SharedImageManager> shared_image_manager_;
  gpu::MemoryTypeTracker tracker_{nullptr};
};

}  // namespace

TEST(VTVideoEncodeAcceleratorTest, CalculatePsnr_PerfectMatch) {
  // MSE == 0 should return the max cap of 128.0 dB.
  EXPECT_DOUBLE_EQ(
      VTVideoEncodeAccelerator::CalculatePsnrForTesting(0.0, PIXEL_FORMAT_I420),
      128.0);
  EXPECT_DOUBLE_EQ(
      VTVideoEncodeAccelerator::CalculatePsnrForTesting(0.0, PIXEL_FORMAT_NV12),
      128.0);
  EXPECT_DOUBLE_EQ(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                       0.0, PIXEL_FORMAT_P010LE),
                   128.0);
}

TEST(VTVideoEncodeAcceleratorTest, CalculatePsnr_8Bit) {
  // Test PSNR calculations for 8-bit formats (max_value = 255).
  // PSNR = 10 * log10(255^2 / 1) = 48.1308 dB.
  EXPECT_NEAR(
      VTVideoEncodeAccelerator::CalculatePsnrForTesting(1.0, PIXEL_FORMAT_I420),
      48.13, 0.01);
  EXPECT_NEAR(
      VTVideoEncodeAccelerator::CalculatePsnrForTesting(1.0, PIXEL_FORMAT_NV12),
      48.13, 0.01);

  // PSNR = 10 * log10(255^2 / 100) = 28.1308 dB.
  EXPECT_NEAR(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                  100.0, PIXEL_FORMAT_I420),
              28.13, 0.01);
  EXPECT_NEAR(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                  100.0, PIXEL_FORMAT_NV12),
              28.13, 0.01);
}

TEST(VTVideoEncodeAcceleratorTest, CalculatePsnr_10Bit) {
  // Test PSNR calculations for 10-bit formats (max_value = 1023).
  // PSNR = 10 * log10(1023^2 / 1) = 60.1975 dB.
  EXPECT_NEAR(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                  1.0, PIXEL_FORMAT_P010LE),
              60.20, 0.01);

  // PSNR = 10 * log10(1023^2 / 100) = 40.1975 dB.
  EXPECT_NEAR(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                  100.0, PIXEL_FORMAT_P010LE),
              40.20, 0.01);
}

TEST(VTVideoEncodeAcceleratorTest, CalculatePsnr_CapMaxPSNR) {
  // Extremely small MSE should be capped at 128.0 dB (lossless).
  EXPECT_DOUBLE_EQ(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                       1e-9, PIXEL_FORMAT_I420),
                   128.0);
  EXPECT_DOUBLE_EQ(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                       1e-9, PIXEL_FORMAT_NV12),
                   128.0);
  EXPECT_DOUBLE_EQ(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                       1e-9, PIXEL_FORMAT_P010LE),
                   128.0);
}

void VerifySupportedProfilesAdvertiseGpuFormats(bool rgb_enabled) {
  using SupportedProfile = VideoEncodeAccelerator::SupportedProfile;

  std::vector<base::test::FeatureRef> enabled = {
      kVTVideoEncodeAcceleratorOpaqueSharedImageEncode};
  std::vector<base::test::FeatureRef> disabled;
  (rgb_enabled ? enabled : disabled)
      .push_back(kVTVideoEncodeAcceleratorOpaqueRgbSharedImageEncode);
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  enabled.push_back(kPlatformHEVCEncoderSupport);
  enabled.push_back(kPlatformHEVCHbdEncoderSupport);
#endif
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(enabled, disabled);

  base::test::TaskEnvironment task_environment;
  std::unique_ptr<VideoEncodeAccelerator> encoder(
      new VTVideoEncodeAccelerator());
  const auto profiles = encoder->GetSupportedProfiles();
  ASSERT_THAT(profiles, Not(IsEmpty()));

  const std::vector<VideoPixelFormat> expected_8bit =
      rgb_enabled
          ? std::vector<VideoPixelFormat>{PIXEL_FORMAT_NV12, PIXEL_FORMAT_ARGB,
                                          PIXEL_FORMAT_XRGB}
          : std::vector<VideoPixelFormat>{PIXEL_FORMAT_NV12};

  // HW and SW VT sessions share the same CVPixelBuffer input path.
  EXPECT_THAT(
      profiles,
      Each(Field(&SupportedProfile::supports_gpu_shared_images, IsTrue())));

  // At least one 8-bit profile must exist and advertise the 8-bit set.
  EXPECT_THAT(profiles,
              Contains(Field(&SupportedProfile::gpu_supported_pixel_formats,
                             ElementsAreArray(expected_8bit))));

  for (const auto& profile : profiles) {
    SCOPED_TRACE(GetProfileName(profile.profile));
    switch (profile.profile) {
      // High bit depth profiles are unaffected by the RGB feature.
      case HEVCPROFILE_MAIN10:
        EXPECT_THAT(profile.gpu_supported_pixel_formats,
                    ElementsAre(PIXEL_FORMAT_P010LE));
        break;
      case HEVCPROFILE_REXT:
        EXPECT_THAT(
            profile.gpu_supported_pixel_formats,
            ElementsAre(AnyOf(PIXEL_FORMAT_NV16, PIXEL_FORMAT_NV24,
                              PIXEL_FORMAT_P210LE, PIXEL_FORMAT_P410LE)));
        break;
      default:
        EXPECT_THAT(profile.gpu_supported_pixel_formats,
                    ElementsAreArray(expected_8bit));
    }
  }
}

TEST(VTVideoEncodeAcceleratorTest, SupportedProfilesAdvertiseGpuFormats) {
  VerifySupportedProfilesAdvertiseGpuFormats(/*rgb_enabled=*/false);
}

TEST(VTVideoEncodeAcceleratorTest,
     SupportedProfilesAdvertiseGpuFormatsWithRgb) {
  VerifySupportedProfilesAdvertiseGpuFormats(/*rgb_enabled=*/true);
}

#if BUILDFLAG(ENABLE_PLATFORM_HEVC) && \
    BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
TEST(VTVideoEncodeAcceleratorTest, AdvertisesHevcRextOnlyOnAppleSilicon) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kVTVideoEncodeAcceleratorOpaqueSharedImageEncode,
       kPlatformHEVCEncoderSupport, kPlatformHEVCHbdEncoderSupport},
      {});
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<VideoEncodeAccelerator> encoder(
      new VTVideoEncodeAccelerator());
  auto profiles = encoder->GetSupportedProfiles();

  std::vector<const VideoEncodeAccelerator::SupportedProfile*> rext_hw;
  std::vector<const VideoEncodeAccelerator::SupportedProfile*> rext_sw;
  for (const auto& profile : profiles) {
    if (profile.profile != HEVCPROFILE_REXT) {
      continue;
    }
    if (profile.is_software_codec) {
      rext_sw.push_back(&profile);
    } else {
      rext_hw.push_back(&profile);
    }
  }
  EXPECT_TRUE(rext_sw.empty());
#if defined(ARCH_CPU_X86_FAMILY)
  // Intel VideoToolbox does not get HEVCPROFILE_REXT in the compile-time list.
  EXPECT_TRUE(rext_hw.empty());
#else
  // Apple Silicon compiles RExt into GetSupportedVideoCodecProfiles(), but
  // each combo is only advertised if CanCreateHardwareCompressionSession()
  // succeeds. CQ/swarming Macs often cannot create those HW sessions.
  if (rext_hw.empty()) {
    GTEST_SKIP() << "HEVC RExt hardware encode is not available";
  }
  // Four combos × min-resolution variants × portrait copies. At least the four
  // session formats must appear when hardware encode is present.
  bool saw_nv16 = false, saw_nv24 = false, saw_p210 = false, saw_p410 = false;
  for (const auto* profile : rext_hw) {
    ASSERT_TRUE(profile->chroma_sampling.has_value());
    ASSERT_TRUE(profile->bit_depth.has_value());
    ASSERT_EQ(profile->gpu_supported_pixel_formats.size(), 1u);
    const VideoPixelFormat dest = profile->gpu_supported_pixel_formats[0];
    if (*profile->chroma_sampling == VideoChromaSampling::k422 &&
        *profile->bit_depth == 8) {
      saw_nv16 = true;
      EXPECT_EQ(dest, PIXEL_FORMAT_NV16);
    } else if (*profile->chroma_sampling == VideoChromaSampling::k444 &&
               *profile->bit_depth == 8) {
      saw_nv24 = true;
      EXPECT_EQ(dest, PIXEL_FORMAT_NV24);
    } else if (*profile->chroma_sampling == VideoChromaSampling::k422 &&
               *profile->bit_depth == 10) {
      saw_p210 = true;
      EXPECT_EQ(dest, PIXEL_FORMAT_P210LE);
    } else if (*profile->chroma_sampling == VideoChromaSampling::k444 &&
               *profile->bit_depth == 10) {
      saw_p410 = true;
      EXPECT_EQ(dest, PIXEL_FORMAT_P410LE);
    } else {
      ADD_FAILURE() << "Unexpected RExt chroma/bit-depth combo";
    }
  }
  EXPECT_TRUE(saw_nv16);
  EXPECT_TRUE(saw_nv24);
  EXPECT_TRUE(saw_p210);
  EXPECT_TRUE(saw_p410);
#endif  // defined(ARCH_CPU_X86_FAMILY)
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC) &&
        // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

namespace {

std::vector<VideoCodecProfile> GetTestEncodeCodecProfiles(
    base::test::ScopedFeatureList& scoped_feature_list) {
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  scoped_feature_list.InitWithFeatures(
      {kPlatformHEVCEncoderSupport,
       kVTVideoEncodeAcceleratorOpaqueSharedImageEncode},
      {});
  return {H264PROFILE_BASELINE, HEVCPROFILE_MAIN};
#else
  scoped_feature_list.InitAndEnableFeature(
      kVTVideoEncodeAcceleratorOpaqueSharedImageEncode);
  return {H264PROFILE_BASELINE};
#endif
}

}  // namespace

// Verifies that a camera capture frame backed by a mappable NV12 IOSurface
// SharedImage (STORAGE_MAPPABLE_SHARED_IMAGE) is accepted by the encoder and
// successfully encoded into a valid keyframe bitstream buffer without error.
TEST(VTVideoEncodeAcceleratorTest, EncodeCameraCaptureNv12Frame) {
  base::test::ScopedFeatureList scoped_feature_list;
  const std::vector<VideoCodecProfile> profiles =
      GetTestEncodeCodecProfiles(scoped_feature_list);
  base::test::TaskEnvironment task_environment;
  const gfx::Size frame_size(640, 480);

  for (VideoCodecProfile profile : profiles) {
    std::unique_ptr<VideoEncodeAccelerator> encoder(
        new VTVideoEncodeAccelerator());

    base::RunLoop encode_loop;
    TestVEAClient client(encoder.get(), profile, encode_loop.QuitClosure());

    VideoEncodeAccelerator::Config config(
        PIXEL_FORMAT_NV12, frame_size, profile,
        Bitrate::ConstantBitrate(300000u), 30,
        VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer,
        VideoEncodeAccelerator::Config::ContentType::kCamera);

    ASSERT_TRUE(encoder->Initialize(config, &client, nullptr).is_ok())
        << GetProfileName(profile);

    ASSERT_TRUE(client.encoder_info().has_value());
    EXPECT_TRUE(client.encoder_info()->supports_gpu_shared_images);
    EXPECT_THAT(client.encoder_info()->gpu_supported_pixel_formats,
                ElementsAre(PIXEL_FORMAT_NV12));

    // Create a mappable NV12 SharedImage backed by an IOSurface, matching
    // camera capture on macOS.
    gfx::ScopedIOSurface io_surface =
        gfx::CreateIOSurface(frame_size, viz::MultiPlaneFormat::kNV12);
    ASSERT_TRUE(io_surface);
    gfx::GpuMemoryBufferHandle gmb_handle(std::move(io_surface));

    // Matches VideoCaptureImpl::OnBufferReady in
    // third_party/blink/renderer/platform/video_capture/video_capture_impl.cc:
    // Camera capture SharedImages on macOS are created with GLES2_READ |
    // RASTER_READ | DISPLAY_READ | SCANOUT | MACOS_VIDEO_TOOLBOX.
    auto test_sii = base::MakeRefCounted<gpu::TestSharedImageInterface>();
    gpu::SharedImageInfo si_info(
        viz::MultiPlaneFormat::kNV12, frame_size,
        gfx::ColorSpace::CreateREC709(),
        gpu::SHARED_IMAGE_USAGE_GLES2_READ |
            gpu::SHARED_IMAGE_USAGE_RASTER_READ |
            gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
            gpu::SHARED_IMAGE_USAGE_SCANOUT |
            gpu::SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX,
        "CameraCaptureFrame");
    auto shared_image = test_sii->CreateSharedImage(
        si_info, gpu::kNullSurfaceHandle,
        gfx::BufferUsage::SCANOUT_VEA_CPU_READ, std::move(gmb_handle));

    auto frame = VideoFrame::WrapMappableSharedImage(
        std::move(shared_image), gpu::SyncToken(),
        VideoFrame::ReleaseMailboxCB(), gfx::Rect(frame_size), frame_size,
        base::Microseconds(0));
    ASSERT_TRUE(frame);
    EXPECT_EQ(frame->storage_type(), VideoFrame::STORAGE_MAPPABLE_SHARED_IMAGE);
    EXPECT_TRUE(frame->HasMappableSharedImage());

    encoder->Encode(frame, /*force_keyframe=*/true);
    encode_loop.Run();

    // Verify that the camera frame was encoded into a valid keyframe bitstream.
    EXPECT_EQ(client.bitstream_buffer_count(), 1u);
    EXPECT_EQ(client.keyframe_count(), 1u);
    EXPECT_FALSE(client.last_status().has_value());
  }
}
// Verifies that opaque RGB SharedImages (STORAGE_OPAQUE, e.g. from WebGL,
// WebGPU, or Canvas) in ARGB and XRGB formats are accepted when
// kVTVideoEncodeAcceleratorOpaqueRgbSharedImageEncode is enabled, resolved via
// CommandBufferHelper without CPU readback, and successfully encoded into valid
// keyframe bitstream buffers without error.
TEST(VTVideoEncodeAcceleratorTest, EncodeOpaqueSharedImageRgbFrames) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::vector<base::test::FeatureRef> enabled_features = {
      kVTVideoEncodeAcceleratorOpaqueSharedImageEncode,
      kVTVideoEncodeAcceleratorOpaqueRgbSharedImageEncode,
  };
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  enabled_features.push_back(kPlatformHEVCEncoderSupport);
#endif
  scoped_feature_list.InitWithFeatures(enabled_features, {});
  const std::vector<VideoCodecProfile> profiles = {
      H264PROFILE_BASELINE,
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
      HEVCPROFILE_MAIN,
#endif
  };
  base::test::TaskEnvironment task_environment;
  const gfx::Size frame_size(640, 480);

  for (VideoCodecProfile profile : profiles) {
    for (VideoPixelFormat rgb_format : {PIXEL_FORMAT_ARGB, PIXEL_FORMAT_XRGB}) {
      gpu::SharedImageManager shared_image_manager;
      auto command_buffer_helper =
          base::MakeRefCounted<TestCommandBufferHelper>(
              task_environment.GetMainThreadTaskRunner(),
              &shared_image_manager);

      auto* vt_encoder = new VTVideoEncodeAccelerator();
      std::unique_ptr<VideoEncodeAccelerator> encoder(vt_encoder);

      base::RunLoop encode_loop;
      TestVEAClient client(encoder.get(), profile, encode_loop.QuitClosure());

      VideoEncodeAccelerator::Config config(
          PIXEL_FORMAT_NV12, frame_size, profile,
          Bitrate::ConstantBitrate(300000u), 30,
          VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer,
          VideoEncodeAccelerator::Config::ContentType::kCamera);

      ASSERT_TRUE(encoder->Initialize(config, &client, nullptr).is_ok())
          << GetProfileName(profile);

      ASSERT_TRUE(client.encoder_info().has_value());
      EXPECT_TRUE(client.encoder_info()->supports_gpu_shared_images);
      EXPECT_THAT(
          client.encoder_info()->gpu_supported_pixel_formats,
          ElementsAre(PIXEL_FORMAT_NV12, PIXEL_FORMAT_ARGB, PIXEL_FORMAT_XRGB));

      vt_encoder->SetCommandBufferHelperCB(
          base::BindRepeating(
              [](scoped_refptr<CommandBufferHelper> helper) { return helper; },
              command_buffer_helper),
          task_environment.GetMainThreadTaskRunner());

      // Create an opaque RGB SharedImage backed by an IOSurface in
      // SharedImageManager, matching WebGPU/WebGL output.
      gfx::ScopedIOSurface io_surface =
          gfx::CreateIOSurface(frame_size, viz::SinglePlaneFormat::kBGRA_8888);
      ASSERT_TRUE(io_surface);

      // Matches DrawingBuffer (WebGL) and WebGPUSwapBufferProvider (WebGPU)
      // SharedImage usage flags. Notably omits
      // SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX, verifying that SCANOUT
      // allows encoding opaque SharedImages produced by general rendering
      // pipelines that were not specifically tagged for VideoToolbox at
      // allocation time.
      gpu::SharedImageMetadata metadata;
      metadata.format = viz::SinglePlaneFormat::kBGRA_8888;
      metadata.size = frame_size;
      metadata.color_space = gfx::ColorSpace::CreateSRGB();
      metadata.surface_origin = kTopLeft_GrSurfaceOrigin;
      metadata.alpha_type = kOpaque_SkAlphaType;
      metadata.usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                       gpu::SHARED_IMAGE_USAGE_SCANOUT |
                       gpu::SHARED_IMAGE_USAGE_GLES2_READ |
                       gpu::SHARED_IMAGE_USAGE_WEBGPU_READ;
      auto shared_image = gpu::ClientSharedImage::CreateForTesting(metadata);

      base::RunLoop destroy_loop;
      gpu::MemoryTypeTracker tracker(nullptr);
      auto backing = std::make_unique<TestIOSurfaceBacking>(
          shared_image->mailbox(), viz::SinglePlaneFormat::kBGRA_8888,
          frame_size, std::move(io_surface), destroy_loop.QuitClosure());
      auto factory_ref =
          shared_image_manager.Register(std::move(backing), &tracker);

      auto frame = VideoFrame::WrapSharedImage(
          rgb_format, shared_image, gpu::SyncToken(),
          VideoFrame::ReleaseMailboxCB(), gfx::Rect(frame_size), frame_size,
          base::Microseconds(0));
      ASSERT_TRUE(frame);
      EXPECT_EQ(frame->storage_type(), VideoFrame::STORAGE_OPAQUE);
      EXPECT_TRUE(frame->HasSharedImage());
      EXPECT_FALSE(frame->HasMappableSharedImage());

      encoder->Encode(frame, /*force_keyframe=*/true);
      encode_loop.Run();

      // Verify that the opaque RGB frame was encoded into a valid keyframe
      // bitstream, and that sRGB input is converted and tagged as BT.709.
      EXPECT_EQ(client.bitstream_buffer_count(), 1u);
      EXPECT_EQ(client.keyframe_count(), 1u);
      ASSERT_TRUE(client.last_metadata().has_value());
      EXPECT_EQ(client.last_metadata()->encoded_color_space,
                gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                                gfx::ColorSpace::TransferID::BT709_APPLE,
                                gfx::ColorSpace::MatrixID::BT709,
                                gfx::ColorSpace::RangeID::FULL));
      EXPECT_FALSE(client.last_status().has_value());

      client.DetachEncoder();
      encoder.reset();
      // Verify that destroying the encoder releases the SharedImage
      // representation, allowing the backing to be cleanly destroyed.
      destroy_loop.Run();
      factory_ref.reset();
    }
  }
}
TEST(VTVideoEncodeAcceleratorTest,
     RejectsOpaqueSharedImageArgbFrameWhenFeatureDisabled) {
  struct FeatureConfig {
    bool opaque_enabled;
    bool rgb_enabled;
  };
  for (const auto& [opaque_enabled, rgb_enabled] : {
           FeatureConfig{/*opaque_enabled=*/false, /*rgb_enabled=*/false},
           FeatureConfig{/*opaque_enabled=*/false, /*rgb_enabled=*/true},
           FeatureConfig{/*opaque_enabled=*/true, /*rgb_enabled=*/false},
       }) {
    base::test::ScopedFeatureList scoped_feature_list;
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    enabled_features.push_back(kPlatformHEVCEncoderSupport);
#endif
    if (opaque_enabled) {
      enabled_features.push_back(
          kVTVideoEncodeAcceleratorOpaqueSharedImageEncode);
    } else {
      disabled_features.push_back(
          kVTVideoEncodeAcceleratorOpaqueSharedImageEncode);
    }
    if (rgb_enabled) {
      enabled_features.push_back(
          kVTVideoEncodeAcceleratorOpaqueRgbSharedImageEncode);
    } else {
      disabled_features.push_back(
          kVTVideoEncodeAcceleratorOpaqueRgbSharedImageEncode);
    }
    scoped_feature_list.InitWithFeatures(enabled_features, disabled_features);

    const std::vector<VideoCodecProfile> profiles = {
        H264PROFILE_BASELINE,
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
        HEVCPROFILE_MAIN,
#endif
    };
    base::test::TaskEnvironment task_environment;
    const gfx::Size frame_size(640, 480);

    for (VideoCodecProfile profile : profiles) {
      auto* vt_encoder = new VTVideoEncodeAccelerator();
      std::unique_ptr<VideoEncodeAccelerator> encoder(vt_encoder);

      base::RunLoop error_loop;
      ErrorExpectingVEAClient client(error_loop.QuitClosure());

      VideoEncodeAccelerator::Config config(
          PIXEL_FORMAT_NV12, frame_size, profile,
          Bitrate::ConstantBitrate(300000u), 30,
          VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer,
          VideoEncodeAccelerator::Config::ContentType::kCamera);

      ASSERT_TRUE(encoder->Initialize(config, &client, nullptr).is_ok())
          << GetProfileName(profile);

      ASSERT_TRUE(client.encoder_info().has_value());
      if (opaque_enabled) {
        EXPECT_TRUE(client.encoder_info()->supports_gpu_shared_images);
        static constexpr VideoPixelFormat kExpectedFormats[] = {
            PIXEL_FORMAT_NV12};
        EXPECT_EQ(client.encoder_info()->gpu_supported_pixel_formats,
                  std::vector<VideoPixelFormat>(std::begin(kExpectedFormats),
                                                std::end(kExpectedFormats)));
      } else {
        EXPECT_FALSE(client.encoder_info()->supports_gpu_shared_images);
      }

      gpu::SharedImageMetadata metadata;
      metadata.format = viz::SinglePlaneFormat::kBGRA_8888;
      metadata.size = frame_size;
      metadata.color_space = gfx::ColorSpace::CreateSRGB();
      metadata.surface_origin = kTopLeft_GrSurfaceOrigin;
      metadata.alpha_type = kOpaque_SkAlphaType;
      metadata.usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                       gpu::SHARED_IMAGE_USAGE_SCANOUT;
      auto shared_image = gpu::ClientSharedImage::CreateForTesting(metadata);

      auto frame = VideoFrame::WrapSharedImage(
          PIXEL_FORMAT_ARGB, shared_image, gpu::SyncToken(),
          VideoFrame::ReleaseMailboxCB(), gfx::Rect(frame_size), frame_size,
          base::Microseconds(0));
      ASSERT_TRUE(frame);

      encoder->Encode(frame, /*force_keyframe=*/true);
      error_loop.Run();

      ASSERT_TRUE(client.last_status().has_value());
      EXPECT_EQ(client.last_status()->code(),
                EncoderStatus::Codes::kEncoderFailedEncode);
    }
  }
}

TEST(VTVideoEncodeAcceleratorTest, RejectsOpaqueSharedImagesInI420Session) {
  struct FeatureConfig {
    bool rgb_enabled;
  };
  for (const auto& [rgb_enabled] : {
           FeatureConfig{/*rgb_enabled=*/false},
           FeatureConfig{/*rgb_enabled=*/true},
       }) {
    base::test::ScopedFeatureList scoped_feature_list;
    std::vector<base::test::FeatureRef> enabled_features = {
        kVTVideoEncodeAcceleratorOpaqueSharedImageEncode,
    };
    std::vector<base::test::FeatureRef> disabled_features;
    if (rgb_enabled) {
      enabled_features.push_back(
          kVTVideoEncodeAcceleratorOpaqueRgbSharedImageEncode);
    } else {
      disabled_features.push_back(
          kVTVideoEncodeAcceleratorOpaqueRgbSharedImageEncode);
    }
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    enabled_features.push_back(kPlatformHEVCEncoderSupport);
#endif
    scoped_feature_list.InitWithFeatures(enabled_features, disabled_features);

    base::test::TaskEnvironment task_environment;
    const gfx::Size frame_size(640, 480);

    // Test that both NV12 and ARGB opaque SharedImages are rejected in an I420
    // session because `gpu_supported_pixel_formats_` is left empty in
    // `Initialize()`.
    std::vector<VideoPixelFormat> formats_to_test = {PIXEL_FORMAT_NV12};
    if (rgb_enabled) {
      formats_to_test.push_back(PIXEL_FORMAT_ARGB);
    }

    for (VideoPixelFormat frame_format : formats_to_test) {
      auto* vt_encoder = new VTVideoEncodeAccelerator();
      std::unique_ptr<VideoEncodeAccelerator> encoder(vt_encoder);

      base::RunLoop error_loop;
      ErrorExpectingVEAClient client(error_loop.QuitClosure());

      VideoEncodeAccelerator::Config config(
          PIXEL_FORMAT_I420, frame_size, H264PROFILE_BASELINE,
          Bitrate::ConstantBitrate(300000u), 30,
          VideoEncodeAccelerator::Config::StorageType::kShmem,
          VideoEncodeAccelerator::Config::ContentType::kCamera);

      ASSERT_TRUE(encoder->Initialize(config, &client, nullptr).is_ok());

      gpu::SharedImageMetadata metadata;
      metadata.format = frame_format == PIXEL_FORMAT_NV12
                            ? viz::MultiPlaneFormat::kNV12
                            : viz::SinglePlaneFormat::kBGRA_8888;
      metadata.size = frame_size;
      metadata.color_space = gfx::ColorSpace::CreateSRGB();
      metadata.surface_origin = kTopLeft_GrSurfaceOrigin;
      metadata.alpha_type = kOpaque_SkAlphaType;
      metadata.usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                       gpu::SHARED_IMAGE_USAGE_SCANOUT |
                       gpu::SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX;
      auto shared_image = gpu::ClientSharedImage::CreateForTesting(metadata);

      auto frame = VideoFrame::WrapSharedImage(
          frame_format, shared_image, gpu::SyncToken(),
          VideoFrame::ReleaseMailboxCB(), gfx::Rect(frame_size), frame_size,
          base::Microseconds(0));
      ASSERT_TRUE(frame);

      encoder->Encode(frame, /*force_keyframe=*/true);
      error_loop.Run();

      ASSERT_TRUE(client.last_status().has_value());
      EXPECT_EQ(client.last_status()->code(),
                EncoderStatus::Codes::kEncoderFailedEncode);
    }
  }
}

TEST(VTVideoEncodeAcceleratorTest, RejectsUnsupportedOpaqueSharedImageFormats) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::vector<base::test::FeatureRef> enabled_features = {
      kVTVideoEncodeAcceleratorOpaqueSharedImageEncode,
      kVTVideoEncodeAcceleratorOpaqueRgbSharedImageEncode,
  };
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  enabled_features.push_back(kPlatformHEVCEncoderSupport);
#endif
  scoped_feature_list.InitWithFeatures(enabled_features, {});

  base::test::TaskEnvironment task_environment;
  const gfx::Size frame_size(640, 480);

  // Even when both opaque SI and RGB feature flags are enabled, non-whitelisted
  // formats (such as ABGR and XBGR) must be rejected by
  // CanEncodeOpaqueSharedImage().
  for (VideoPixelFormat unsupported_format :
       {PIXEL_FORMAT_ABGR, PIXEL_FORMAT_XBGR}) {
    auto* vt_encoder = new VTVideoEncodeAccelerator();
    std::unique_ptr<VideoEncodeAccelerator> encoder(vt_encoder);

    base::RunLoop error_loop;
    ErrorExpectingVEAClient client(error_loop.QuitClosure());

    VideoEncodeAccelerator::Config config(
        PIXEL_FORMAT_NV12, frame_size, H264PROFILE_BASELINE,
        Bitrate::ConstantBitrate(300000u), 30,
        VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer,
        VideoEncodeAccelerator::Config::ContentType::kCamera);

    ASSERT_TRUE(encoder->Initialize(config, &client, nullptr).is_ok());

    gpu::SharedImageMetadata metadata;
    metadata.format = viz::SinglePlaneFormat::kRGBA_8888;
    metadata.size = frame_size;
    metadata.color_space = gfx::ColorSpace::CreateSRGB();
    metadata.surface_origin = kTopLeft_GrSurfaceOrigin;
    metadata.alpha_type = kOpaque_SkAlphaType;
    metadata.usage =
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;
    auto shared_image = gpu::ClientSharedImage::CreateForTesting(metadata);

    auto frame = VideoFrame::WrapSharedImage(
        unsupported_format, shared_image, gpu::SyncToken(),
        VideoFrame::ReleaseMailboxCB(), gfx::Rect(frame_size), frame_size,
        base::Microseconds(0));
    ASSERT_TRUE(frame);

    encoder->Encode(frame, /*force_keyframe=*/true);
    error_loop.Run();

    ASSERT_TRUE(client.last_status().has_value());
    EXPECT_EQ(client.last_status()->code(),
              EncoderStatus::Codes::kEncoderFailedEncode);
  }
}

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
TEST(VTVideoEncodeAcceleratorTest,
     RejectsInvalidInputFormatForHevcHighBitDepthProfiles) {
  base::test::TaskEnvironment task_environment;
  const gfx::Size frame_size(640, 480);

  for (VideoCodecProfile profile : {HEVCPROFILE_MAIN10, HEVCPROFILE_REXT}) {
    std::unique_ptr<VideoEncodeAccelerator> encoder(
        new VTVideoEncodeAccelerator());
    ErrorExpectingVEAClient client(base::NullCallback());

    VideoEncodeAccelerator::Config config(
        PIXEL_FORMAT_NV12, frame_size, profile,
        Bitrate::ConstantBitrate(300000u), 30,
        VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer,
        VideoEncodeAccelerator::Config::ContentType::kCamera);

    EXPECT_FALSE(
        encoder->Initialize(config, &client, std::make_unique<NullMediaLog>())
            .is_ok())
        << GetProfileName(profile);
  }
}
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

}  // namespace media
