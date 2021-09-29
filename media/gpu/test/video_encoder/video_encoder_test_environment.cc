// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_encoder/video_encoder_test_environment.h"

#include <algorithm>
#include <utility>

#include "base/strings/pattern.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "media/base/media_switches.h"
#include "media/base/video_bitrate_allocation.h"
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

constexpr double kSpatialLayersBitrateScaleFactors[][3] = {
    {1.00, 0.00, 0.00},  // For one spatial layer.
    {0.30, 0.70, 0.00},  // For two spatial layers.
    {0.07, 0.23, 0.70},  // For three spatial layers.
};
constexpr double kTemporalLayersBitrateScaleFactors[][3] = {
    {1.00, 0.00, 0.00},  // For one temporal layer.
    {0.55, 0.45, 0.00},  // For two temporal layers.
    {0.50, 0.20, 0.30},  // For three temporal layers.
};
constexpr int kSpatialLayersResolutionScaleDenom[][3] = {
    {1, 0, 0},  // For one spatial layer.
    {2, 1, 0},  // For two spatial layers.
    {4, 2, 1},  // For three spatial layers.
};

const std::vector<base::Feature> kEnabledFeaturesForVideoEncoderTest = {
#if BUILDFLAG(USE_VAAPI)
    // TODO(crbug.com/828482): remove once enabled by default.
    media::kVaapiLowPowerEncoderGen9x,
    // TODO(crbug.com/811912): remove once enabled by default.
    kVaapiVP9Encoder,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // TODO(crbug.com/1186051): remove once enabled by default.
    kVaapiVp9kSVCHWEncoding,
#endif
#endif
};

const std::vector<base::Feature> kDisabledFeaturesForVideoEncoderTest = {
    // FFmpegVideoDecoder is used for vp8 stream whose alpha mode is opaque in
    // chromium browser. However, VpxVideoDecoder will be used to decode any vp8
    // stream for the rightness (b/138840822), and currently be experimented
    // with this feature flag. We disable the feature to use VpxVideoDecoder to
    // decode any vp8 stream in BitstreamValidator.
    kFFmpegDecodeOpaqueVP8,
#if BUILDFLAG(USE_VAAPI)
    // Disable this feature so that the encoder test can test a resolution
    // which is denied for the sake of performance. See crbug.com/1008491.
    kVaapiEnforceVideoMinMaxResolution,
#endif
};

uint32_t GetDefaultTargetBitrate(const gfx::Size& resolution,
                                 const uint32_t framerate) {
  // This calculation is based on tinyurl.com/cros-platform-video-encoding.
  return resolution.GetArea() * 0.1 * framerate;
}

std::vector<VideoEncodeAccelerator::Config::SpatialLayer>
GetDefaultSpatialLayers(const VideoBitrateAllocation& bitrate,
                        const Video* video,
                        size_t num_spatial_layers,
                        size_t num_temporal_layers) {
  // Returns empty spatial layer config because one temporal layer stream is
  // equivalent to a simple stream.
  if (num_temporal_layers == 1u && num_spatial_layers == 1u)
    return {};

  std::vector<VideoEncodeAccelerator::Config::SpatialLayer> spatial_layers;
  for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
    VideoEncodeAccelerator::Config::SpatialLayer spatial_layer;
    const int resolution_denom =
        kSpatialLayersResolutionScaleDenom[num_spatial_layers - 1][sid];
    LOG_IF(WARNING, video->Resolution().width() % resolution_denom != 0)
        << "width of SL#" << sid << " is not dividable by " << resolution_denom;
    LOG_IF(WARNING, video->Resolution().height() % resolution_denom != 0)
        << "height of SL#" << sid << " is not dividable by "
        << resolution_denom;
    spatial_layer.width = video->Resolution().width() / resolution_denom;
    spatial_layer.height = video->Resolution().height() / resolution_denom;
    uint32_t spatial_layer_bitrate = 0;
    for (size_t tid = 0; tid < num_temporal_layers; ++tid)
      spatial_layer_bitrate += bitrate.GetBitrateBps(sid, tid);
    spatial_layer.bitrate_bps = spatial_layer_bitrate;
    spatial_layer.framerate = video->FrameRate();
    spatial_layer.num_of_temporal_layers = num_temporal_layers;
    // Note: VideoEncodeAccelerator currently ignores this max_qp parameter.
    spatial_layer.max_qp = 30u;
    spatial_layers.push_back(spatial_layer);
  }
  return spatial_layers;
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
    size_t num_spatial_layers,
    bool save_output_bitstream,
    absl::optional<uint32_t> encode_bitrate,
    bool reverse,
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
  if ((num_temporal_layers > 1u || num_spatial_layers > 1u) &&
      profile != VP9PROFILE_PROFILE0 &&
      !(profile >= H264PROFILE_MIN && profile <= H264PROFILE_HIGH)) {
    LOG(ERROR) << "SVC encoding supported "
               << "only if output profile is h264 or vp9";
    return nullptr;
  }

  const uint32_t bitrate = encode_bitrate.value_or(
      GetDefaultTargetBitrate(video->Resolution(), video->FrameRate()));
  return new VideoEncoderTestEnvironment(
      std::move(video), enable_bitstream_validator, output_folder, profile,
      num_temporal_layers, num_spatial_layers, bitrate, save_output_bitstream,
      reverse, frame_output_config);
}

VideoEncoderTestEnvironment::VideoEncoderTestEnvironment(
    std::unique_ptr<media::test::Video> video,
    bool enable_bitstream_validator,
    const base::FilePath& output_folder,
    VideoCodecProfile profile,
    size_t num_temporal_layers,
    size_t num_spatial_layers,
    uint32_t bitrate,
    bool save_output_bitstream,
    bool reverse,
    const FrameOutputConfig& frame_output_config)
    : VideoTestEnvironment(kEnabledFeaturesForVideoEncoderTest,
                           kDisabledFeaturesForVideoEncoderTest),
      video_(std::move(video)),
      enable_bitstream_validator_(enable_bitstream_validator),
      output_folder_(output_folder),
      profile_(profile),
      num_temporal_layers_(num_temporal_layers),
      num_spatial_layers_(num_spatial_layers),
      bitrate_(GetDefaultVideoBitrateAllocation(bitrate)),
      spatial_layers_(GetDefaultSpatialLayers(bitrate_,
                                              video_.get(),
                                              num_spatial_layers_,
                                              num_temporal_layers_)),
      save_output_bitstream_(save_output_bitstream),
      reverse_(reverse),
      frame_output_config_(frame_output_config),
      gpu_memory_buffer_factory_(
          gpu::GpuMemoryBufferFactory::CreateNativeType(nullptr)) {}

VideoEncoderTestEnvironment::~VideoEncoderTestEnvironment() = default;

media::test::Video* VideoEncoderTestEnvironment::Video() const {
  return video_.get();
}

media::test::Video* VideoEncoderTestEnvironment::GenerateNV12Video() {
  if (!nv12_video_) {
    nv12_video_ = video_->ConvertToNV12();
    CHECK(nv12_video_);
  }
  return nv12_video_.get();
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

const std::vector<VideoEncodeAccelerator::Config::SpatialLayer>&
VideoEncoderTestEnvironment::SpatialLayers() const {
  return spatial_layers_;
}

VideoBitrateAllocation
VideoEncoderTestEnvironment::GetDefaultVideoBitrateAllocation(
    uint32_t bitrate) const {
  VideoBitrateAllocation bitrate_allocation;
  DCHECK_LE(num_spatial_layers_, 3u);
  DCHECK_LE(num_temporal_layers_, 3u);
  if (num_spatial_layers_ == 1u && num_temporal_layers_ == 1u) {
    bitrate_allocation.SetBitrate(0, 0, bitrate);
    return bitrate_allocation;
  }

  for (size_t sid = 0; sid < num_spatial_layers_; ++sid) {
    const double bitrate_factor =
        kSpatialLayersBitrateScaleFactors[num_spatial_layers_ - 1][sid];
    uint32_t sl_bitrate = bitrate * bitrate_factor;

    for (size_t tl_idx = 0; tl_idx < num_temporal_layers_; ++tl_idx) {
      const double factor =
          kTemporalLayersBitrateScaleFactors[num_temporal_layers_ - 1][tl_idx];
      bitrate_allocation.SetBitrate(
          sid, tl_idx, base::checked_cast<int>(sl_bitrate * factor));
    }
  }

  return bitrate_allocation;
}

VideoBitrateAllocation VideoEncoderTestEnvironment::Bitrate() const {
  return bitrate_;
}

bool VideoEncoderTestEnvironment::SaveOutputBitstream() const {
  return save_output_bitstream_;
}

bool VideoEncoderTestEnvironment::Reverse() const {
  return reverse_;
}

const FrameOutputConfig& VideoEncoderTestEnvironment::ImageOutputConfig()
    const {
  return frame_output_config_;
}

gpu::GpuMemoryBufferFactory*
VideoEncoderTestEnvironment::GetGpuMemoryBufferFactory() const {
  return gpu_memory_buffer_factory_.get();
}

bool VideoEncoderTestEnvironment::IsKeplerUsed() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const VideoCodec codec = VideoCodecProfileToVideoCodec(Profile());
  if (codec != VideoCodec::kVP8)
    return false;
  const static std::string board = base::SysInfo::GetLsbReleaseBoard();
  if (board == "unknown") {
    LOG(WARNING) << "Failed to get chromeos board name";
    return false;
  }
  const char* kKeplerBoards[] = {"buddy*", "guado*", "rikku*"};
  for (const char* b : kKeplerBoards) {
    if (base::MatchPattern(board, b))
      return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
}
}  // namespace test
}  // namespace media
