// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_encoder/video_encoder_test_environment.h"

#include <algorithm>
#include <utility>

#include "build/buildflag.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "media/base/media_switches.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/video.h"

namespace media {
namespace test {
namespace {
struct CodecParamToProfile {
  const char* codec;
  const VideoCodecProfile profile;
} kCodecParamToProfile[] = {
    {"h264baseline", H264PROFILE_BASELINE},
    {"h264", H264PROFILE_BASELINE},
    {"h264main", H264PROFILE_MAIN},
    {"h264high", H264PROFILE_HIGH},
    {"vp8", VP8PROFILE_ANY},
    {"vp9", VP9PROFILE_PROFILE0},
};

const std::vector<base::Feature> kEnabledFeaturesForVideoEncoderTest = {
#if BUILDFLAG(USE_VAAPI)
    // TODO(crbug.com/828482): remove once enabled by default.
    media::kVaapiLowPowerEncoderGen9x,
    // TODO(crbug.com/811912): remove once enabled by default.
    kVaapiVP9Encoder,
#endif
};

const std::vector<base::Feature> kDisabledFeaturesForVideoEncoderTest = {
    // FFmpegVideoDecoder is used for vp8 stream whose alpha mode is opaque in
    // chromium browser. However, VpxVideoDecoder will be used to decode any vp8
    // stream for the rightness (b/138840822), and currently be experimented
    // with this feature flag. We disable the feature to use VpxVideoDecoder to
    // decode any vp8 stream in BitstreamValidator.
    kFFmpegDecodeOpaqueVP8,
};

uint32_t GetDefaultTargetBitrate(const gfx::Size& resolution,
                                 const uint32_t framerate) {
  constexpr uint32_t Mbps = 1000 * 1000;
  // Following bitrates are based on the video bitrates recommended by YouTube
  // for 16:9 SDR 30fps video.
  // (https://support.google.com/youtube/answer/1722171).
  // The bitrates don't scale linearly so we use the following lookup table as a
  // base for computing a reasonable bitrate for the specified resolution and
  // framerate.
  constexpr struct {
    gfx::Size resolution;
    uint32_t bitrate;
  } kDefaultTargetBitrates[] = {
      {gfx::Size(640, 360), 1 * Mbps},    {gfx::Size(854, 480), 2.5 * Mbps},
      {gfx::Size(1280, 720), 5 * Mbps},   {gfx::Size(1920, 1080), 8 * Mbps},
      {gfx::Size(3840, 2160), 18 * Mbps},
  };

  const auto* it = std::find_if(
      std::cbegin(kDefaultTargetBitrates), std::cend(kDefaultTargetBitrates),
      [resolution](const auto& target_bitrate) {
        return resolution.GetArea() <= target_bitrate.resolution.GetArea();
      });
  LOG_ASSERT(it != std::cend(kDefaultTargetBitrates))
      << "Target bitrate for the resolution is not found, resolution="
      << resolution.ToString();
  const double resolution_ratio =
      (resolution.GetArea() / static_cast<double>(it->resolution.GetArea()));
  const double framerate_ratio = framerate > 30 ? 1.5 : 1.0;
  return it->bitrate * resolution_ratio * framerate_ratio;
}
}  // namespace

// static
VideoEncoderTestEnvironment* VideoEncoderTestEnvironment::Create(
    const base::FilePath& video_path,
    const base::FilePath& video_metadata_path,
    bool enable_bitstream_validator,
    const base::FilePath& output_folder,
    const std::string& codec,
    size_t num_temporal_layers,
    bool save_output_bitstream,
    const FrameOutputConfig& frame_output_config) {
  if (video_path.empty()) {
    LOG(ERROR) << "No video specified";
    return nullptr;
  }
  auto video =
      std::make_unique<media::test::Video>(video_path, video_metadata_path);
  if (!video->Load(kMaxReadFrames)) {
    LOG(ERROR) << "Failed to load " << video_path;
    return nullptr;
  }

  // If the video file has the .webm format it needs to be decoded first.
  // TODO(b/151134705): Add support to cache decompressed video files.
  if (video->FilePath().MatchesExtension(FILE_PATH_LITERAL(".webm"))) {
    VLOGF(1) << "Test video " << video->FilePath()
             << " is compressed, decoding...";
    if (!video->Decode()) {
      LOG(ERROR) << "Failed to decode " << video->FilePath();
      return nullptr;
    }
  }

  if (video->PixelFormat() == VideoPixelFormat::PIXEL_FORMAT_UNKNOWN) {
    LOG(ERROR) << "Test video " << video->FilePath()
               << " has an invalid video pixel format "
               << VideoPixelFormatToString(video->PixelFormat());
    return nullptr;
  }

  const auto* it = std::find_if(
      std::begin(kCodecParamToProfile), std::end(kCodecParamToProfile),
      [codec](const auto& cp) { return cp.codec == codec; });
  if (it == std::end(kCodecParamToProfile)) {
    LOG(ERROR) << "Unknown codec: " << codec;
    return nullptr;
  }

  const VideoCodecProfile profile = it->profile;
  if (num_temporal_layers > 1u && profile != VP9PROFILE_PROFILE0) {
    LOG(ERROR) << "Temporal layer encoding supported "
               << "only if output profile is vp9";
    return nullptr;
  }

  const uint32_t bitrate =
      GetDefaultTargetBitrate(video->Resolution(), video->FrameRate());
  return new VideoEncoderTestEnvironment(
      std::move(video), enable_bitstream_validator, output_folder, profile,
      num_temporal_layers, bitrate, save_output_bitstream, frame_output_config);
}

VideoEncoderTestEnvironment::VideoEncoderTestEnvironment(
    std::unique_ptr<media::test::Video> video,
    bool enable_bitstream_validator,
    const base::FilePath& output_folder,
    VideoCodecProfile profile,
    size_t num_temporal_layers,
    uint32_t bitrate,
    bool save_output_bitstream,
    const FrameOutputConfig& frame_output_config)
    : VideoTestEnvironment(kEnabledFeaturesForVideoEncoderTest,
                           kDisabledFeaturesForVideoEncoderTest),
      video_(std::move(video)),
      enable_bitstream_validator_(enable_bitstream_validator),
      output_folder_(output_folder),
      profile_(profile),
      num_temporal_layers_(num_temporal_layers),
      bitrate_(bitrate),
      save_output_bitstream_(save_output_bitstream),
      frame_output_config_(frame_output_config),
      gpu_memory_buffer_factory_(
          gpu::GpuMemoryBufferFactory::CreateNativeType(nullptr)) {}

VideoEncoderTestEnvironment::~VideoEncoderTestEnvironment() = default;

media::test::Video* VideoEncoderTestEnvironment::Video() const {
  return video_.get();
}

bool VideoEncoderTestEnvironment::IsBitstreamValidatorEnabled() const {
  return enable_bitstream_validator_;
}

const base::FilePath& VideoEncoderTestEnvironment::OutputFolder() const {
  return output_folder_;
}

VideoCodecProfile VideoEncoderTestEnvironment::Profile() const {
  return profile_;
}

size_t VideoEncoderTestEnvironment::NumTemporalLayers() const {
  return num_temporal_layers_;
}

uint32_t VideoEncoderTestEnvironment::Bitrate() const {
  return bitrate_;
}

bool VideoEncoderTestEnvironment::SaveOutputBitstream() const {
  return save_output_bitstream_;
}

const FrameOutputConfig& VideoEncoderTestEnvironment::ImageOutputConfig()
    const {
  return frame_output_config_;
}

gpu::GpuMemoryBufferFactory*
VideoEncoderTestEnvironment::GetGpuMemoryBufferFactory() const {
  return gpu_memory_buffer_factory_.get();
}

}  // namespace test
}  // namespace media
