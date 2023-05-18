// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_encoder/video_encoder_test_environment.h"

#include <iterator>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/ranges/algorithm.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "media/base/bitrate.h"
#include "media/base/media_switches.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
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
    {"av1", AV1PROFILE_PROFILE_MAIN},
};

struct SVCConfig {
  const char* svc_mode;
  const size_t num_spatial_layers;
  const size_t num_temporal_layers;
  const VideoEncodeAccelerator::Config::InterLayerPredMode
      inter_layer_pred_mode;
} kSVCModeParamToSVCConfig[] = {
    {"L1T1", 1, 1, VideoEncodeAccelerator::Config::InterLayerPredMode::kOff},
    {"L1T2", 1, 2, VideoEncodeAccelerator::Config::InterLayerPredMode::kOff},
    {"L1T3", 1, 3, VideoEncodeAccelerator::Config::InterLayerPredMode::kOff},
    {"L2T1_KEY", 2, 1,
     VideoEncodeAccelerator::Config::InterLayerPredMode::kOnKeyPic},
    {"L2T2_KEY", 2, 2,
     VideoEncodeAccelerator::Config::InterLayerPredMode::kOnKeyPic},
    {"L2T3_KEY", 2, 3,
     VideoEncodeAccelerator::Config::InterLayerPredMode::kOnKeyPic},
    {"L3T1_KEY", 3, 1,
     VideoEncodeAccelerator::Config::InterLayerPredMode::kOnKeyPic},
    {"L3T2_KEY", 3, 2,
     VideoEncodeAccelerator::Config::InterLayerPredMode::kOnKeyPic},
    {"L3T3_KEY", 3, 3,
     VideoEncodeAccelerator::Config::InterLayerPredMode::kOnKeyPic},
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
  if (num_spatial_layers == 1 && num_temporal_layers == 1) {
    return {};
  }

  constexpr int kSpatialLayersResolutionScaleDenom[][3] = {
      {1, 0, 0},  // For one spatial layer.
      {2, 1, 0},  // For two spatial layers.
      {4, 2, 1},  // For three spatial layers.
  };

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
    const std::string& svc_mode,
    bool save_output_bitstream,
    absl::optional<uint32_t> encode_bitrate,
    Bitrate::Mode bitrate_mode,
    bool reverse,
    const FrameOutputConfig& frame_output_config,
    const std::vector<base::test::FeatureRef>& enabled_features,
    const std::vector<base::test::FeatureRef>& disabled_features) {
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

  const auto* codec_it = base::ranges::find(kCodecParamToProfile, codec,
                                            &CodecParamToProfile::codec);
  if (codec_it == std::end(kCodecParamToProfile)) {
    LOG(ERROR) << "Unknown codec: " << codec;
    return nullptr;
  }
  const VideoCodecProfile profile = codec_it->profile;

  size_t num_temporal_layers = 1u;
  size_t num_spatial_layers = 1u;
  auto inter_layer_pred_mode =
      VideoEncodeAccelerator::Config::InterLayerPredMode::kOff;
  const auto* svc_it = base::ranges::find(kSVCModeParamToSVCConfig, svc_mode,
                                          &SVCConfig::svc_mode);
  if (svc_it == std::end(kSVCModeParamToSVCConfig)) {
    LOG(ERROR) << "Unsupported svc_mode: " << svc_mode;
    return nullptr;
  }
  num_spatial_layers = svc_it->num_spatial_layers;
  num_temporal_layers = svc_it->num_temporal_layers;
  inter_layer_pred_mode = svc_it->inter_layer_pred_mode;

  if (num_spatial_layers > 1u && profile != VP9PROFILE_PROFILE0) {
    LOG(ERROR) << "Spatial layer encoding is supported only if output profile "
               << "is vp9";
    return nullptr;
  }

  // TODO(b/182008564) Add checks to make sure no features are duplicated, and
  // there is no intersection between the enabled and disabled set.
  std::vector<base::test::FeatureRef> combined_enabled_features(
      enabled_features);
  std::vector<base::test::FeatureRef> combined_disabled_features(
      disabled_features);
  combined_disabled_features.push_back(media::kFFmpegDecodeOpaqueVP8);
#if BUILDFLAG(USE_VAAPI)
  // TODO(crbug.com/828482): remove once enabled by default.
  combined_enabled_features.push_back(media::kVaapiLowPowerEncoderGen9x);
  // TODO(crbug.com/811912): remove once enabled by default.
  combined_enabled_features.push_back(media::kVaapiVP9Encoder);

  // Disable this feature so that the encoder test can test a resolution
  // which is denied for the sake of performance. See crbug.com/1008491.
  combined_disabled_features.push_back(
      media::kVaapiEnforceVideoMinMaxResolution);
#endif

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_VAAPI)
  // TODO(crbug.com/1186051): remove once enabled by default.
  combined_enabled_features.push_back(media::kVaapiVp9kSVCHWEncoding);
  // TODO(b/202926617): remove once enabled by default.
  combined_enabled_features.push_back(media::kVaapiVp8TemporalLayerHWEncoding);
#endif

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_VAAPI)
  combined_enabled_features.push_back(media::kVaapiVideoEncodeLinux);
#endif

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  combined_enabled_features.push_back(media::kChromeOSHWVBREncoding);
#endif

  const uint32_t target_bitrate = encode_bitrate.value_or(
      GetDefaultTargetBitrate(video->Resolution(), video->FrameRate()));
  // TODO(b/181797390): Reconsider if this peak bitrate is reasonable.
  const media::Bitrate bitrate =
      bitrate_mode == media::Bitrate::Mode::kVariable
          ? media::Bitrate::VariableBitrate(target_bitrate,
                                            /*peak_bps=*/target_bitrate * 2)
          : media::Bitrate::ConstantBitrate(target_bitrate);
  if (bitrate.mode() == media::Bitrate::Mode::kVariable &&
      VideoCodecProfileToVideoCodec(profile) != VideoCodec::kH264) {
    LOG(ERROR) << "VBR is only supported for H264 encoding";
    return nullptr;
  }
  return new VideoEncoderTestEnvironment(
      std::move(video), enable_bitstream_validator, output_folder, profile,
      inter_layer_pred_mode, num_spatial_layers, num_temporal_layers, bitrate,
      save_output_bitstream, reverse, frame_output_config,
      combined_enabled_features, combined_disabled_features);
}

VideoEncoderTestEnvironment::VideoEncoderTestEnvironment(
    std::unique_ptr<media::test::Video> video,
    bool enable_bitstream_validator,
    const base::FilePath& output_folder,
    VideoCodecProfile profile,
    VideoEncodeAccelerator::Config::InterLayerPredMode inter_layer_pred_mode,
    size_t num_spatial_layers,
    size_t num_temporal_layers,
    const Bitrate& bitrate,
    bool save_output_bitstream,
    bool reverse,
    const FrameOutputConfig& frame_output_config,
    const std::vector<base::test::FeatureRef>& enabled_features,
    const std::vector<base::test::FeatureRef>& disabled_features)
    : VideoTestEnvironment(enabled_features, disabled_features),
      video_(std::move(video)),
      enable_bitstream_validator_(enable_bitstream_validator),
      output_folder_(output_folder),
      profile_(profile),
      inter_layer_pred_mode_(inter_layer_pred_mode),
      bitrate_(AllocateDefaultBitrateForTesting(num_spatial_layers,
                                                num_temporal_layers,
                                                bitrate)),
      spatial_layers_(GetDefaultSpatialLayers(bitrate_,
                                              video_.get(),
                                              num_spatial_layers,
                                              num_temporal_layers)),
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

VideoEncodeAccelerator::Config::InterLayerPredMode
VideoEncoderTestEnvironment::InterLayerPredMode() const {
  return inter_layer_pred_mode_;
}

const VideoBitrateAllocation& VideoEncoderTestEnvironment::BitrateAllocation()
    const {
  return bitrate_;
}

bool VideoEncoderTestEnvironment::SaveOutputBitstream() const {
  return save_output_bitstream_;
}

base::FilePath VideoEncoderTestEnvironment::OutputFilePath(
    const VideoCodec& codec,
    const base::FilePath& base_name,
    bool svc_enable,
    int spatial_idx,
    int temporal_idx) const {
  base::FilePath::StringPieceType extension = codec == VideoCodec::kH264
                                                  ? FILE_PATH_LITERAL("h264")
                                                  : FILE_PATH_LITERAL("ivf");
  auto output_bitstream_filepath =
      OutputFolder()
          .Append(GetTestOutputFilePath())
          .Append(base_name.ReplaceExtension(extension));
  if (svc_enable) {
    output_bitstream_filepath =
        output_bitstream_filepath.InsertBeforeExtensionASCII(
            FILE_PATH_LITERAL(".SL") + base::NumberToString(spatial_idx) +
            FILE_PATH_LITERAL(".TL") + base::NumberToString(temporal_idx));
  }

  return output_bitstream_filepath;
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
