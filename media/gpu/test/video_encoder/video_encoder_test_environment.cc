// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/test/video_encoder/video_encoder_test_environment.h"

#include <iterator>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/ranges/algorithm.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "media/base/bitrate.h"
#include "media/base/media_switches.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/raw_video.h"

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
  const SVCInterLayerPredMode inter_layer_pred_mode;
} kSVCModeParamToSVCConfig[] = {
    {"L1T1", 1, 1, SVCInterLayerPredMode::kOff},
    {"L1T2", 1, 2, SVCInterLayerPredMode::kOff},
    {"L1T3", 1, 3, SVCInterLayerPredMode::kOff},
    {"L2T1_KEY", 2, 1, SVCInterLayerPredMode::kOnKeyPic},
    {"L2T2_KEY", 2, 2, SVCInterLayerPredMode::kOnKeyPic},
    {"L2T3_KEY", 2, 3, SVCInterLayerPredMode::kOnKeyPic},
    {"L3T1_KEY", 3, 1, SVCInterLayerPredMode::kOnKeyPic},
    {"L3T2_KEY", 3, 2, SVCInterLayerPredMode::kOnKeyPic},
    {"L3T3_KEY", 3, 3, SVCInterLayerPredMode::kOnKeyPic},
    {"S2T1", 2, 1, SVCInterLayerPredMode::kOff},
    {"S2T2", 2, 2, SVCInterLayerPredMode::kOff},
    {"S2T3", 2, 3, SVCInterLayerPredMode::kOff},
    {"S3T1", 3, 1, SVCInterLayerPredMode::kOff},
    {"S3T2", 3, 2, SVCInterLayerPredMode::kOff},
    {"S3T3", 3, 3, SVCInterLayerPredMode::kOff},
};

uint32_t GetDefaultTargetBitrate(const VideoCodec codec,
                                 const gfx::Size& resolution,
                                 const uint32_t framerate,
                                 bool validation) {
  // For how these values are decided, see
  // https://docs.google.com/document/d/1Mlu-2mMOqswWaaivIWhn00dYkoTwKcjLrxxBXcWycug
  constexpr struct {
    int area;
    // bitrate[0]: for speed and quality performance
    // bitrate[1]: for validation.
    // The three values are for H264/VP8, VP9 and AV1, respectively.
    double bitrate[2][3];
  } kBitrateTable[] = {
      {0, {{77.5, 65.0, 60.0}, {100.0, 100.0, 100.0}}},
      {240 * 160, {{77.5, 65.0, 60.0}, {115.0, 100.0, 100.0}}},
      {320 * 240, {{165.0, 105.0, 105.0}, {230.0, 180.0, 180.0}}},
      {480 * 270, {{195.0, 180.0, 180.0}, {320.0, 250, 250}}},
      {640 * 480, {{550.0, 355.0, 342.5}, {690.0, 520, 520}}},
      {1280 * 720, {{1700.0, 990.0, 800.0}, {2500.0, 1500, 1200}}},
      {1920 * 1080, {{2480.0, 2060.0, 1500.0}, {4000.0, 3350.0, 2500.0}}},
  };
  size_t codec_index = 0;
  switch (codec) {
    case VideoCodec::kH264:
    case VideoCodec::kVP8:
      codec_index = 0;
      break;
    case VideoCodec::kVP9:
      codec_index = 1;
      break;
    case VideoCodec::kAV1:
      codec_index = 2;
      break;
    default:
      LOG(FATAL) << "Unknown codec: " << codec;
  }

  const int area = resolution.GetArea();
  size_t index = std::size(kBitrateTable) - 1;
  for (size_t i = 0; i < std::size(kBitrateTable); ++i) {
    if (area < kBitrateTable[i].area) {
      index = i;
      break;
    }
  }
  const int low_area = kBitrateTable[index - 1].area;
  const double low_bitrate =
      kBitrateTable[index - 1].bitrate[validation][codec_index];
  const int up_area = kBitrateTable[index].area;
  const double up_bitrate =
      kBitrateTable[index].bitrate[validation][codec_index];

  const double bitrate_in_30fps_in_kbps =
      (up_bitrate - low_bitrate) / (up_area - low_area) * (area - low_area) +
      low_bitrate;
  // This is selected as 1 in 30fps and 1.8 in 60fps.
  const double framerate_multiplier =
      0.27 * (framerate * framerate / 30.0 / 30.0) + 0.73;
  return bitrate_in_30fps_in_kbps * framerate_multiplier * 1000;
}

constexpr int kSpatialLayersResolutionScaleDenom[][3] = {
    {1, 0, 0},  // For one spatial layer.
    {2, 1, 0},  // For two spatial layers.
    {4, 2, 1},  // For three spatial layers.
};

VideoBitrateAllocation CreateBitrateAllocation(
    const VideoCodec codec,
    const gfx::Size& resolution,
    uint32_t frame_rate,
    std::optional<uint32_t> encode_bitrate,
    size_t num_spatial_layers,
    size_t num_temporal_layers,
    bool is_vbr,
    bool validation) {
  // If |encode_bitrate| is specified, we use the default way of distributing it
  // to layers.
  if ((num_spatial_layers == 1 && num_temporal_layers == 1) || encode_bitrate) {
    const uint32_t target_bitrate = encode_bitrate.value_or(
        GetDefaultTargetBitrate(codec, resolution, frame_rate, validation));
    // TODO(b/181797390): Reconsider if this peak bitrate is reasonable.
    const Bitrate bitrate =
        is_vbr ? media::Bitrate::VariableBitrate(
                     target_bitrate, /*peak_bps=*/target_bitrate * 2)
               : media::Bitrate::ConstantBitrate(target_bitrate);
    return AllocateDefaultBitrateForTesting(num_spatial_layers,
                                            num_temporal_layers, bitrate);
  }
  std::vector<uint32_t> spatial_layer_bitrates(num_spatial_layers);
  for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
    const int resolution_denom =
        kSpatialLayersResolutionScaleDenom[num_spatial_layers - 1][sid];
    LOG_IF(WARNING, resolution.width() % resolution_denom != 0)
        << "width of SL#" << sid << " is not dividable by " << resolution_denom;
    LOG_IF(WARNING, resolution.height() % resolution_denom != 0)
        << "height of SL#" << sid << " is not dividable by "
        << resolution_denom;
    const gfx::Size spatial_layer_resolution(
        resolution.width() / resolution_denom,
        resolution.height() / resolution_denom);
    spatial_layer_bitrates[sid] = GetDefaultTargetBitrate(
        codec, spatial_layer_resolution, frame_rate, validation);
  }
  return AllocateBitrateForDefaultEncodingWithBitrates(
      spatial_layer_bitrates, num_temporal_layers, is_vbr);
}

std::vector<VideoEncodeAccelerator::Config::SpatialLayer>
GetDefaultSpatialLayers(const VideoBitrateAllocation& bitrate,
                        const gfx::Size& resolution,
                        uint32_t frame_rate,
                        size_t num_spatial_layers,
                        size_t num_temporal_layers) {
  // Returns empty spatial layer config because one temporal layer stream is
  // equivalent to a simple stream.
  if (num_spatial_layers == 1 && num_temporal_layers == 1) {
    return {};
  }

  std::vector<VideoEncodeAccelerator::Config::SpatialLayer> spatial_layers;
  for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
    VideoEncodeAccelerator::Config::SpatialLayer spatial_layer;
    const int resolution_denom =
        kSpatialLayersResolutionScaleDenom[num_spatial_layers - 1][sid];
    LOG_IF(WARNING, resolution.width() % resolution_denom != 0)
        << "width of SL#" << sid << " is not dividable by " << resolution_denom;
    LOG_IF(WARNING, resolution.height() % resolution_denom != 0)
        << "height of SL#" << sid << " is not dividable by "
        << resolution_denom;
    spatial_layer.width = resolution.width() / resolution_denom;
    spatial_layer.height = resolution.height() / resolution_denom;
    uint32_t spatial_layer_bitrate = 0;
    for (size_t tid = 0; tid < num_temporal_layers; ++tid)
      spatial_layer_bitrate += bitrate.GetBitrateBps(sid, tid);
    spatial_layer.bitrate_bps = spatial_layer_bitrate;
    spatial_layer.framerate = frame_rate;
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
    TestType test_type,
    const base::FilePath& video_path,
    const base::FilePath& video_metadata_path,
    const base::FilePath& output_folder,
    const std::string& codec,
    const std::string& svc_mode,
    VideoEncodeAccelerator::Config::ContentType content_type,
    bool save_output_bitstream,
    std::optional<uint32_t> encode_bitrate,
    Bitrate::Mode bitrate_mode,
    bool reverse,
    const FrameOutputConfig& frame_output_config,
    const std::vector<base::test::FeatureRef>& enabled_features,
    const std::vector<base::test::FeatureRef>& disabled_features) {
  if (video_path.empty()) {
    LOG(ERROR) << "No video specified";
    return nullptr;
  }
  auto video = RawVideo::Create(
      video_path, video_metadata_path,
      /*read_all_frames=*/test_type == TestType::kQualityPerformance);
  if (!video) {
    LOG(ERROR) << "Failed to prepare input source for " << video_path;
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
  auto inter_layer_pred_mode = SVCInterLayerPredMode::kOff;
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

  const bool is_vbr = bitrate_mode == media::Bitrate::Mode::kVariable;
  const VideoCodec video_codec = VideoCodecProfileToVideoCodec(profile);
  if (is_vbr && video_codec != VideoCodec::kH264) {
    LOG(ERROR) << "VBR is only supported for H264 encoding";
    return nullptr;
  }
  const VideoBitrateAllocation bitrate_allocation = CreateBitrateAllocation(
      video_codec, video->Resolution(), video->FrameRate(), encode_bitrate,
      num_spatial_layers, num_temporal_layers, is_vbr,
      test_type == TestType::kValidation);

  // TODO(b/182008564) Add checks to make sure no features are duplicated, and
  // there is no intersection between the enabled and disabled set.
  std::vector<base::test::FeatureRef> combined_enabled_features(
      enabled_features);
  std::vector<base::test::FeatureRef> combined_disabled_features(
      disabled_features);
#if BUILDFLAG(USE_VAAPI)
  // TODO(crbug.com/41380519): remove once enabled by default.
  combined_enabled_features.push_back(media::kVaapiLowPowerEncoderGen9x);

  // Disable this feature so that the encoder test can test a resolution
  // which is denied for the sake of performance. See crbug.com/1008491.
  combined_disabled_features.push_back(
      media::kVaapiEnforceVideoMinMaxResolution);
#endif

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_VAAPI)
  combined_enabled_features.push_back(media::kAcceleratedVideoEncodeLinux);
#endif

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  combined_enabled_features.push_back(media::kChromeOSHWVBREncoding);
#endif

  return new VideoEncoderTestEnvironment(
      test_type, std::move(video), output_folder, video_path.BaseName(),
      profile, inter_layer_pred_mode, num_spatial_layers, num_temporal_layers,
      content_type, bitrate_allocation, save_output_bitstream, reverse,
      frame_output_config, combined_enabled_features,
      combined_disabled_features);
}

VideoEncoderTestEnvironment::VideoEncoderTestEnvironment(
    TestType test_type,
    std::unique_ptr<media::test::RawVideo> video,
    const base::FilePath& output_folder,
    const base::FilePath& output_bitstream_file_base_name,
    VideoCodecProfile profile,
    SVCInterLayerPredMode inter_layer_pred_mode,
    size_t num_spatial_layers,
    size_t num_temporal_layers,
    VideoEncodeAccelerator::Config::ContentType content_type,
    const VideoBitrateAllocation& bitrate,
    bool save_output_bitstream,
    bool reverse,
    const FrameOutputConfig& frame_output_config,
    const std::vector<base::test::FeatureRef>& enabled_features,
    const std::vector<base::test::FeatureRef>& disabled_features)
    : VideoTestEnvironment(enabled_features, disabled_features),
      test_type_(test_type),
      video_(std::move(video)),
      output_folder_(output_folder),
      output_bitstream_file_base_name_(output_bitstream_file_base_name),
      profile_(profile),
      inter_layer_pred_mode_(inter_layer_pred_mode),
      bitrate_(bitrate),
      spatial_layers_(GetDefaultSpatialLayers(bitrate_,
                                              video_->Resolution(),
                                              video_->FrameRate(),
                                              num_spatial_layers,
                                              num_temporal_layers)),
      content_type_(content_type),
      save_output_bitstream_(save_output_bitstream),
      reverse_(reverse),
      frame_output_config_(frame_output_config),
      gpu_memory_buffer_factory_(
          gpu::GpuMemoryBufferFactory::CreateNativeType(nullptr)) {}

VideoEncoderTestEnvironment::~VideoEncoderTestEnvironment() = default;

media::test::RawVideo* VideoEncoderTestEnvironment::Video() const {
  return video_.get();
}

media::test::RawVideo* VideoEncoderTestEnvironment::GenerateNV12Video() {
  if (!nv12_video_) {
    nv12_video_ = video_->CreateNV12Video();
    CHECK(nv12_video_);
  }
  return nv12_video_.get();
}

VideoEncoderTestEnvironment::TestType VideoEncoderTestEnvironment::RunTestType()
    const {
  return test_type_;
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

SVCInterLayerPredMode VideoEncoderTestEnvironment::InterLayerPredMode() const {
  return inter_layer_pred_mode_;
}

VideoEncodeAccelerator::Config::ContentType
VideoEncoderTestEnvironment::ContentType() const {
  return content_type_;
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
    bool svc_enable,
    int spatial_idx,
    int temporal_idx) const {
  base::FilePath::StringPieceType extension = codec == VideoCodec::kH264
                                                  ? FILE_PATH_LITERAL("h264")
                                                  : FILE_PATH_LITERAL("ivf");
  auto output_bitstream_filepath =
      OutputFolder()
          .Append(GetTestOutputFilePath())
          .Append(output_bitstream_file_base_name_.ReplaceExtension(extension));
  if (svc_enable) {
    auto file_name_suffix =
        base::StringPrintf(".SL%d.TL%d", spatial_idx, temporal_idx);
    output_bitstream_filepath =
        output_bitstream_filepath.InsertBeforeExtensionASCII(file_name_suffix);
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
}  // namespace test
}  // namespace media
