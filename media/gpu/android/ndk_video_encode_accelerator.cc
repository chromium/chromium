// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/ndk_video_encode_accelerator.h"

#include <optional>

#include "base/bits.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/encoder_status.h"
#include "media/base/media_serializers_base.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/gpu/android/video_accelerator_util.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/parsers/h264_level_limits.h"
#include "media/parsers/h264_parser.h"
#include "media/parsers/temporal_scalability_id_extractor.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gl/gl_switches.h"

#pragma clang attribute push DEFAULT_REQUIRES_ANDROID_API( \
    NDK_MEDIA_CODEC_MIN_API)
namespace media {

using EncoderType = VideoEncodeAccelerator::Config::EncoderType;

namespace {

// Default distance between key frames. About 100 seconds between key frames,
// the same default value we use on Windows.
constexpr uint32_t kDefaultGOPLength = 3000;

std::vector<VideoPixelFormat> GetSupportedSharedImagePixelFormats() {
  if (base::FeatureList::IsEnabled(features::kVulkanFromANGLE)) {
    // If kVulkanFromANGLE = true (e.g. Desktop Android)
    // we we get shared images with AngleVulkanImageBacking, NDK VEA can't
    // handle such shared images yet.
    return {};
  }
  return {PIXEL_FORMAT_ABGR, PIXEL_FORMAT_XBGR};
}

// Deliberately breaking naming convention rules, to match names from
// MediaCodec SDK.
constexpr int32_t BUFFER_FLAG_KEY_FRAME = 1;

enum PixelFormat {
  // Subset of MediaCodecInfo.CodecCapabilities.
  COLOR_FORMAT_YUV420_PLANAR = 19,
  COLOR_FORMAT_YUV420_SEMIPLANAR = 21,  // Same as NV12
  COLOR_FORMAT_SURFACE = 0x7f000789,
};

struct AMediaFormatDeleter {
  inline void operator()(AMediaFormat* ptr) const {
    if (ptr) {
      AMediaFormat_delete(ptr);
    }
  }
};

enum class CodecProfileLevel {
  // Subset of MediaCodecInfo.CodecProfileLevel
  AVCProfileBaseline = 0x01,
  AVCProfileMain = 0x02,
  AVCProfileExtended = 0x04,
  AVCProfileHigh = 0x08,
  AVCProfileHigh10 = 0x10,
  AVCProfileHigh422 = 0x20,
  AVCProfileHigh444 = 0x40,
  AVCProfileConstrainedBaseline = 0x10000,
  AVCProfileConstrainedHigh = 0x80000,

  AVCLevel1 = 0x01,
  AVCLevel1b = 0x02,
  AVCLevel11 = 0x04,
  AVCLevel12 = 0x08,
  AVCLevel13 = 0x10,
  AVCLevel2 = 0x20,
  AVCLevel21 = 0x40,
  AVCLevel22 = 0x80,
  AVCLevel3 = 0x100,
  AVCLevel31 = 0x200,
  AVCLevel32 = 0x400,
  AVCLevel4 = 0x800,
  AVCLevel41 = 0x1000,
  AVCLevel42 = 0x2000,
  AVCLevel5 = 0x4000,
  AVCLevel51 = 0x8000,
  AVCLevel52 = 0x10000,
  AVCLevel6 = 0x20000,
  AVCLevel61 = 0x40000,
  AVCLevel62 = 0x80000,

  VP9Profile0 = 0x01,
  VP9Profile1 = 0x02,
  VP9Profile2 = 0x04,
  VP9Profile3 = 0x08,
  VP9Profile2HDR = 0x1000,
  VP9Profile3HDR = 0x2000,
  VP9Profile2HDR10Plus = 0x4000,
  VP9Profile3HDR10Plus = 0x8000,

  VP8ProfileMain = 0x01,

  AV1ProfileMain8 = 0x1,
  AV1ProfileMain10 = 0x2,
  AV1ProfileMain10HDR10 = 0x1000,
  AV1ProfileMain10HDR10Plus = 0x2000,

  HEVCProfileMain = 0x01,
  HEVCProfileMain10 = 0x02,
  HEVCProfileMainStill = 0x04,
  HEVCProfileMain10HDR10 = 0x1000,
  HEVCProfileMain10HDR10Plus = 0x2000,
  Unknown = 0xFFFFFF,
};

CodecProfileLevel GetAndroidVideoProfile(VideoCodecProfile profile,
                                         bool constrained) {
  switch (profile) {
    case H264PROFILE_BASELINE:
      return constrained ? CodecProfileLevel::AVCProfileConstrainedBaseline
                         : CodecProfileLevel::AVCProfileBaseline;
    case H264PROFILE_MAIN:
      return CodecProfileLevel::AVCProfileMain;
    case H264PROFILE_EXTENDED:
      return CodecProfileLevel::AVCProfileExtended;
    case H264PROFILE_HIGH:
      return constrained ? CodecProfileLevel::AVCProfileConstrainedHigh
                         : CodecProfileLevel::AVCProfileHigh;
    case H264PROFILE_HIGH10PROFILE:
      return CodecProfileLevel::AVCProfileHigh10;
    case H264PROFILE_HIGH422PROFILE:
      return CodecProfileLevel::AVCProfileHigh422;
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return CodecProfileLevel::AVCProfileHigh444;
    case HEVCPROFILE_MAIN:
      return CodecProfileLevel::HEVCProfileMain;
    case HEVCPROFILE_MAIN10:
      return CodecProfileLevel::HEVCProfileMain10;
    case HEVCPROFILE_MAIN_STILL_PICTURE:
      return CodecProfileLevel::HEVCProfileMainStill;
    case VP8PROFILE_ANY:
      return CodecProfileLevel::VP8ProfileMain;
    case VP9PROFILE_PROFILE0:
      return CodecProfileLevel::VP9Profile0;
    case VP9PROFILE_PROFILE1:
      return CodecProfileLevel::VP9Profile1;
    case VP9PROFILE_PROFILE2:
      return CodecProfileLevel::VP9Profile2;
    case VP9PROFILE_PROFILE3:
      return CodecProfileLevel::VP9Profile3;
    case AV1PROFILE_PROFILE_MAIN:
      return CodecProfileLevel::AV1ProfileMain8;
    default:
      return CodecProfileLevel::Unknown;
  }
}

std::optional<CodecProfileLevel> GetAndroidAvcLevel(
    std::optional<uint8_t> level) {
  if (!level.has_value()) {
    return {};
  }
  switch (level.value()) {
    case H264SPS::kLevelIDC1p0:
      return CodecProfileLevel::AVCLevel1;
    case H264SPS::kLevelIDC1B:
      return CodecProfileLevel::AVCLevel1b;
    case H264SPS::kLevelIDC1p1:
      return CodecProfileLevel::AVCLevel11;
    case H264SPS::kLevelIDC1p2:
      return CodecProfileLevel::AVCLevel12;
    case H264SPS::kLevelIDC1p3:
      return CodecProfileLevel::AVCLevel13;
    case H264SPS::kLevelIDC2p0:
      return CodecProfileLevel::AVCLevel2;
    case H264SPS::kLevelIDC2p1:
      return CodecProfileLevel::AVCLevel21;
    case H264SPS::kLevelIDC2p2:
      return CodecProfileLevel::AVCLevel22;
    case H264SPS::kLevelIDC3p0:
      return CodecProfileLevel::AVCLevel3;
    case H264SPS::kLevelIDC3p1:
      return CodecProfileLevel::AVCLevel31;
    case H264SPS::kLevelIDC3p2:
      return CodecProfileLevel::AVCLevel32;
    case H264SPS::kLevelIDC4p0:
      return CodecProfileLevel::AVCLevel4;
    case H264SPS::kLevelIDC4p1:
      return CodecProfileLevel::AVCLevel41;
    case H264SPS::kLevelIDC4p2:
      return CodecProfileLevel::AVCLevel42;
    case H264SPS::kLevelIDC5p0:
      return CodecProfileLevel::AVCLevel5;
    case H264SPS::kLevelIDC5p1:
      return CodecProfileLevel::AVCLevel51;
    case H264SPS::kLevelIDC5p2:
      return CodecProfileLevel::AVCLevel52;
    case H264SPS::kLevelIDC6p0:
      return CodecProfileLevel::AVCLevel6;
    case H264SPS::kLevelIDC6p1:
      return CodecProfileLevel::AVCLevel61;
    case H264SPS::kLevelIDC6p2:
      return CodecProfileLevel::AVCLevel62;
    default:
      return {};
  }
}

std::optional<uint8_t> FindSuitableH264Level(
    const VideoEncodeAccelerator::Config& config,
    int framerate,
    const gfx::Size& frame_size,
    const Bitrate& bitrate) {
  constexpr uint32_t kH264MbSize = 16;
  uint32_t mb_width =
      base::bits::AlignUp(static_cast<uint32_t>(frame_size.width()),
                          kH264MbSize) /
      kH264MbSize;
  uint32_t mb_height =
      base::bits::AlignUp(static_cast<uint32_t>(frame_size.height()),
                          kH264MbSize) /
      kH264MbSize;

  return FindValidH264Level(config.output_profile, bitrate.target_bps(),
                            framerate, mb_width * mb_height);
}

bool GetAndroidColorValues(const gfx::ColorSpace& cs,
                           int* standard,
                           int* transfer,
                           int* range) {
  switch (cs.GetTransferID()) {
    case gfx::ColorSpace::TransferID::LINEAR:
    case gfx::ColorSpace::TransferID::LINEAR_HDR:
      *transfer = 1;  // MediaFormat.COLOR_TRANSFER_LINEAR
      break;
    case gfx::ColorSpace::TransferID::PQ:
      *transfer = 6;  // MediaFormat.COLOR_TRANSFER_ST2084
      break;
    case gfx::ColorSpace::TransferID::HLG:
      *transfer = 7;  // MediaFormat.COLOR_TRANSFER_HLG
      break;
    case gfx::ColorSpace::TransferID::BT709:
    case gfx::ColorSpace::TransferID::SMPTE170M:
    case gfx::ColorSpace::TransferID::BT2020_10:
    case gfx::ColorSpace::TransferID::BT2020_12:
    case gfx::ColorSpace::TransferID::SRGB:
    case gfx::ColorSpace::TransferID::SRGB_HDR:
      *transfer = 3;  // MediaFormat.COLOR_TRANSFER_SDR_VIDEO
      break;
    default:
      return false;
  }

  if (cs.GetPrimaryID() == gfx::ColorSpace::PrimaryID::BT709 &&
      cs.GetMatrixID() == gfx::ColorSpace::MatrixID::BT709) {
    *standard = 1;  // MediaFormat.COLOR_STANDARD_BT709
  } else if (cs.GetPrimaryID() == gfx::ColorSpace::PrimaryID::BT470BG &&
             (cs.GetMatrixID() == gfx::ColorSpace::MatrixID::BT470BG ||
              cs.GetMatrixID() == gfx::ColorSpace::MatrixID::SMPTE170M)) {
    *standard = 2;  // MediaFormat.COLOR_STANDARD_BT601_PAL
  } else if (cs.GetPrimaryID() == gfx::ColorSpace::PrimaryID::SMPTE170M &&
             (cs.GetMatrixID() == gfx::ColorSpace::MatrixID::BT470BG ||
              cs.GetMatrixID() == gfx::ColorSpace::MatrixID::SMPTE170M)) {
    *standard = 4;  // MediaFormat.COLOR_STANDARD_BT601_NTSC
  } else if (cs.GetPrimaryID() == gfx::ColorSpace::PrimaryID::BT2020 &&
             cs.GetMatrixID() == gfx::ColorSpace::MatrixID::BT2020_NCL) {
    *standard = 6;  // MediaFormat.COLOR_STANDARD_BT2020
  } else {
    return false;
  }

  *range = cs.GetRangeID() == gfx::ColorSpace::RangeID::FULL
               ? 1   // MediaFormat.COLOR_RANGE_FULL
               : 2;  // MediaFormat.COLOR_RANGE_LIMITED
  return true;
}

bool SetFormatColorSpace(AMediaFormat* format, const gfx::ColorSpace& cs) {
  DCHECK(cs.IsValid());
  int standard, transfer, range;
  if (!GetAndroidColorValues(cs, &standard, &transfer, &range)) {
    DLOG(ERROR) << "Failed to convert color space to Android color space: "
                << cs.ToString();
    return false;
  }

  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_STANDARD, standard);
  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_TRANSFER, transfer);
  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_RANGE, range);
  return true;
}

using MediaFormatPtr = std::unique_ptr<AMediaFormat, AMediaFormatDeleter>;

MediaFormatPtr CreateVideoFormat(const VideoEncodeAccelerator::Config& config,
                                 int framerate,
                                 const gfx::Size& frame_size,
                                 const Bitrate& bitrate,
                                 std::optional<gfx::ColorSpace> cs,
                                 int num_temporal_layers,
                                 PixelFormat format) {
  int iframe_interval = config.gop_length.value_or(kDefaultGOPLength);
  const auto codec = VideoCodecProfileToVideoCodec(config.output_profile);
  const auto mime = MediaCodecUtil::CodecToAndroidMimeType(codec);
  MediaFormatPtr result(AMediaFormat_new());
  AMediaFormat_setString(result.get(), AMEDIAFORMAT_KEY_MIME, mime.c_str());

  if (codec == VideoCodec::kH264) {
    std::optional<uint8_t> level = config.h264_output_level;
    if (!level.has_value()) {
      level = FindSuitableH264Level(config, framerate, frame_size, bitrate);
    }
    auto android_level = GetAndroidAvcLevel(level);
    if (!android_level.has_value()) {
      DLOG(ERROR) << "Invalid level, can't create MediaFormat.";
      return nullptr;
    }
    int profile = static_cast<int>(GetAndroidVideoProfile(
        config.output_profile, config.is_constrained_h264));
    AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_PROFILE, profile);
    AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_LEVEL,
                          static_cast<int>(android_level.value()));
  }
  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_WIDTH,
                        frame_size.width());
  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_HEIGHT,
                        frame_size.height());

  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_FRAME_RATE, framerate);
  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_I_FRAME_INTERVAL,
                        iframe_interval);
  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_COLOR_FORMAT, format);

  if (config.require_low_delay) {
    // Android docs recommend not setting the latency key with H264 baseline
    // profile since some devices will fail configure. Since latency=1 means
    // no b-frames for h.264 and baseline doesn't support b-frames, this should
    // be okay. See https://crbug.com/409110228
    if (config.output_profile != H264PROFILE_BASELINE) {
      AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_LATENCY, 1);
    }
    // MediaCodec supports two priorities: 0 - realtime, 1 - best effort
    AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_PRIORITY, 0);
  }

  constexpr int32_t BITRATE_MODE_VBR = 1;
  constexpr int32_t BITRATE_MODE_CBR = 2;
  switch (bitrate.mode()) {
    case Bitrate::Mode::kConstant:
      AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_BITRATE_MODE,
                            BITRATE_MODE_CBR);
      break;
    case Bitrate::Mode::kVariable:
      AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_BITRATE_MODE,
                            BITRATE_MODE_VBR);
      break;
    default:
      NOTREACHED();
  }

  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_BIT_RATE,
                        base::saturated_cast<int32_t>(bitrate.target_bps()));

  if (cs && cs->IsValid()) {
    SetFormatColorSpace(result.get(), *cs);
  }

  if (num_temporal_layers > 1) {
    // NDK doesn't have a value for KEY_MAX_B_FRAMES, and temporal SVC can't
    // function without it. So we make do with a handmade constant.
    constexpr const char* AMEDIAFORMAT_KEY_MAX_B_FRAMES = "max-bframes";
    AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_MAX_B_FRAMES, 0);

    auto svc_layer_config =
        base::StringPrintf("android.generic.%d", num_temporal_layers);
    AMediaFormat_setString(result.get(), AMEDIAFORMAT_KEY_TEMPORAL_LAYERING,
                           svc_layer_config.c_str());
  }

  return result;
}

bool IsHardwareCodec(const std::string& codec_name) {
  for (const auto& info : GetEncoderInfoCache()) {
    if (info.name == codec_name) {
      return !info.profile.is_software_codec;
    }
  }
  LOG(ERROR) << "Unknown codec name: " << codec_name;
  return false;
}

std::optional<std::string> FindMediaCodecFor(
    const VideoEncodeAccelerator::Config& config) {
  std::optional<std::string> encoder_name;
  for (const auto& info : GetEncoderInfoCache()) {
    const auto& profile = info.profile;
    if (profile.profile != config.output_profile) {
      continue;
    }

    const auto& input_size = config.input_visible_size;
    if (profile.min_resolution.width() > input_size.width()) {
      continue;
    }
    if (profile.min_resolution.height() > input_size.height()) {
      continue;
    }
    if (profile.max_resolution.width() < input_size.width()) {
      continue;
    }
    if (profile.max_resolution.height() < input_size.height()) {
      continue;
    }

    // NOTE: We don't check bitrate mode here since codecs don't
    // always specify the bitrate mode. Per code inspection, VBR
    // support is announced if a codec doesn't specify anything.

    double max_supported_framerate =
        static_cast<double>(profile.max_framerate_numerator) /
        profile.max_framerate_denominator;
    if (config.framerate > max_supported_framerate) {
      continue;
    }

    if (profile.is_software_codec) {
      if (config.required_encoder_type == EncoderType::kSoftware) {
        return info.name;
      }

      // Note the encoder name in case we don't find a hardware encoder.
      if (config.required_encoder_type == EncoderType::kNoPreference &&
          !encoder_name) {
        encoder_name = info.name;
      }
    } else {
      // Always prefer the hardware encoder if it exists.
      if (config.required_encoder_type == EncoderType::kHardware ||
          config.required_encoder_type == EncoderType::kNoPreference) {
        return info.name;
      }
    }
  }
  return encoder_name;
}

// AVC and HEVC encoders produce parameters sets as a separate buffers
// with BUFFER_FLAG_CODEC_CONFIG flag, these parameters sets need to be
// preserved and appended at the beginning of the bitstream.
// Av1, Vp9 encoders produce extra data describing the stream, but this data
// is already known via other channels and is not expected by decoders.
// For such encoders we don't put it into the bitstream.
// Vp8 doesn't produce configuration buffers.
// More Info:
// https://developer.android.com/reference/android/media/MediaCodec#CSD
bool ProfileNeedsConfigDataInBitstream(VideoCodecProfile profile) {
  switch (VideoCodecProfileToVideoCodec(profile)) {
    case VideoCodec::kH264:
    case VideoCodec::kHEVC:
      return true;
    case VideoCodec::kAV1:
    case VideoCodec::kVP9:
    case VideoCodec::kVP8:
      return false;
    default:
      NOTREACHED()
          << "Configuration for unsupported codecs shouldn't come this far.";
  }
}

void WaitForSyncTokenOnGpuThread(
    scoped_refptr<CommandBufferHelper> command_buffer_helper,
    gpu::SyncToken sync_token,
    base::OnceClosure done_cb) {
  command_buffer_helper->WaitForSyncToken(sync_token, std::move(done_cb));
}

constexpr std::string_view kEncoderStatusHistogramPrefix =
    "Media.VideoEncoder.NDKVEA.EncodeStatus.";

std::string GetEncoderStatusHistogramName(VideoCodecProfile profile) {
  return base::StrCat(
      {kEncoderStatusHistogramPrefix,
       GetCodecNameForUMA(VideoCodecProfileToVideoCodec(profile))});
}

constexpr std::string_view kInitStatusHistogramPrefix =
    "Media.VideoEncoder.NDKVEA.InitStatus.";

std::string GetInitStatusHistogramName(VideoCodecProfile profile) {
  return base::StrCat(
      {kInitStatusHistogramPrefix,
       GetCodecNameForUMA(VideoCodecProfileToVideoCodec(profile))});
}

bool ShouldUseSurfaceInput() {
  if (__builtin_available(android 35, *)) {
    // Limit surface input to Android 15+ (API Level: 35), because we see issues
    // on older devices.
    if (base::FeatureList::IsEnabled(media::kSurfaceInputForAndroidVEA)) {
      return true;
    }
  }
  return false;
}
}  // namespace

NdkVideoEncodeAccelerator::PendingEncode::PendingEncode(
    scoped_refptr<VideoFrame> frame,
    const VideoEncoder::EncodeOptions& options)
    : frame(std::move(frame)), options(options) {}
NdkVideoEncodeAccelerator::PendingEncode::~PendingEncode() = default;
NdkVideoEncodeAccelerator::PendingEncode::PendingEncode(PendingEncode&&) =
    default;
NdkVideoEncodeAccelerator::PendingEncode&
NdkVideoEncodeAccelerator::PendingEncode::operator=(PendingEncode&&) = default;

NdkVideoEncodeAccelerator::NdkVideoEncodeAccelerator(
    scoped_refptr<base::SequencedTaskRunner> runner)
    : task_runner_(std::move(runner)),
      // We just need an arbitrary non-zero value for the first timestamp
      // due to issues with EGL surface path.
      next_timestamp_(base::TimeTicks::Now().since_origin()),
      use_surface_as_input_(ShouldUseSurfaceInput()) {}

NdkVideoEncodeAccelerator::~NdkVideoEncodeAccelerator() {
  // It's supposed to be cleared by Destroy(), it basically checks
  // that we destroy `this` correctly.
  DCHECK(!media_codec_);

  if (!error_occurred_ && have_encoded_frames_) {
    base::UmaHistogramEnumeration(
        GetEncoderStatusHistogramName(config_.output_profile),
        EncoderStatus::Codes::kOk);
  }
}

VideoEncodeAccelerator::SupportedProfiles
NdkVideoEncodeAccelerator::GetSupportedProfiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SupportedProfiles profiles;
  for (auto& info : GetEncoderInfoCache()) {
    profiles.push_back(info.profile);
    if (use_surface_as_input_) {
      auto& profile = profiles.back();
      profile.gpu_supported_pixel_formats =
          GetSupportedSharedImagePixelFormats();
      profile.supports_gpu_shared_images =
          !profile.gpu_supported_pixel_formats.empty();
    }
  }
  return profiles;
}

EncoderStatus NdkVideoEncodeAccelerator::Initialize(
    const Config& config,
    VideoEncodeAccelerator::Client* client,
    std::unique_ptr<MediaLog> media_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!media_codec_);
  DCHECK(client);

  client_ptr_factory_ =
      std::make_unique<base::WeakPtrFactory<VideoEncodeAccelerator::Client>>(
          client);
  config_ = config;
  effective_bitrate_ = config.bitrate;
  log_ = std::move(media_log);
  VideoCodec codec = VideoCodecProfileToVideoCodec(config.output_profile);

  // These should already be filtered out by VideoEncodeAcceleratorUtil.
  if (codec != VideoCodec::kH264) {
    config_.required_encoder_type = EncoderType::kHardware;
  }

  effective_framerate_ = config.framerate;
  num_temporal_layers_ =
      config_.HasTemporalLayer()
          ? config_.spatial_layers.front().num_of_temporal_layers
          : 1;
  if (num_temporal_layers_ > 1) {
    svc_parser_ = std::make_unique<TemporalScalabilityIdExtractor>(
        codec, num_temporal_layers_);
  }

  const EncoderStatus status = ResetMediaCodec();

  base::UmaHistogramEnumeration(
      GetInitStatusHistogramName(config_.output_profile), status.code());

  if (!status.is_ok()) {
    return status;
  }

  const size_t bitstream_buffer_size = EstimateBitstreamBufferSize(
      config_.bitrate, config_.framerate, config.input_visible_size);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncodeAccelerator::Client::RequireBitstreamBuffers,
                     client_ptr_factory_->GetWeakPtr(), 1,
                     config.input_visible_size, bitstream_buffer_size));

  NotifyEncoderInfo();
  return {EncoderStatus::Codes::kOk};
}

void NdkVideoEncodeAccelerator::NotifyEncoderInfo() {
  CHECK(media_codec_);
  std::string codec_name = "unknown";
  char* name_ptr = nullptr;
  media_status_t status = AMediaCodec_getName(media_codec_->codec(), &name_ptr);
  if (status == AMEDIA_OK && name_ptr) {
    codec_name = std::string(name_ptr);
    AMediaCodec_releaseName(media_codec_->codec(), name_ptr);
  }

  for (const auto& info : GetEncoderInfoCache()) {
    if (info.name == codec_name) {
      // TODO(crbug.com/382015342): Set the bitrate limits when we can get them
      // through MediaCodec API.
      encoder_info_.resolution_rate_limits.emplace_back(
          info.profile.max_resolution, /*min_start_bitrate_bps=*/0,
          /*min_bitrate_bps=*/0, /*max_bitrate_bps=*/0,
          info.profile.max_framerate_numerator,
          info.profile.max_framerate_denominator);
    }
  }

  encoder_info_.supports_native_handle = false;
  encoder_info_.has_trusted_rate_controller = false;
  encoder_info_.is_hardware_accelerated = IsHardwareCodec(codec_name);
  encoder_info_.supports_simulcast = false;
  encoder_info_.reports_average_qp = true;
  if (codec_name == "c2.cr52.avc.encoder") {
    encoder_info_.reports_average_qp = false;
  }
  encoder_info_.supports_frame_size_change = false;
  if (use_surface_as_input_) {
    encoder_info_.gpu_supported_pixel_formats =
        GetSupportedSharedImagePixelFormats();
    encoder_info_.supports_gpu_shared_images =
        !encoder_info_.gpu_supported_pixel_formats.empty();
  } else {
    encoder_info_.supports_gpu_shared_images = false;
    encoder_info_.gpu_supported_pixel_formats.clear();
  }
  const char* input_type_str = "buffer";
  if (use_surface_as_input_) {
    input_type_str = encoder_info_.supports_gpu_shared_images
                         ? "surface_with_shared_images"
                         : "surface";
  }
  encoder_info_.implementation_name =
      base::StringPrintf("NdkVideoEncodeAccelerator(%s) input: %s",
                         codec_name.c_str(), input_type_str);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncodeAccelerator::Client::NotifyEncoderInfoChange,
                     client_ptr_factory_->GetWeakPtr(), encoder_info_));
}

void NdkVideoEncodeAccelerator::Encode(
    scoped_refptr<VideoFrame> frame,
    const VideoEncoder::EncodeOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(media_codec_);
  PendingEncode encode(std::move(frame), options);
  if (encode.frame->HasSharedImage()) {
    encode.sync_state = SyncState::kNeedsSync;
  }
  pending_frames_.push_back(std::move(encode));
  FeedInput();
}

void NdkVideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                       bool force_keyframe) {
  Encode(std::move(frame), VideoEncoder::EncodeOptions(force_keyframe));
}

void NdkVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  available_bitstream_buffers_.push_back(std::move(buffer));
  DrainOutput();
}

void NdkVideoEncodeAccelerator::RequestEncodingParametersChange(
    const Bitrate& bitrate,
    uint32_t framerate,
    const std::optional<gfx::Size>& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (size.has_value()) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                       "Update output frame size is not supported"});
    return;
  }

  MediaFormatPtr format(AMediaFormat_new());

  if (effective_framerate_ != framerate)
    AMediaFormat_setInt32(format.get(), AMEDIAFORMAT_KEY_FRAME_RATE, framerate);
  if (effective_bitrate_ != bitrate) {
    // AMEDIACODEC_KEY_VIDEO_BITRATE is not exposed until SDK 31.
    AMediaFormat_setInt32(format.get(),
                          "video-bitrate" /*AMEDIACODEC_KEY_VIDEO_BITRATE*/,
                          bitrate.target_bps());
  }
  media_status_t status =
      AMediaCodec_setParameters(media_codec_->codec(), format.get());

  if (status != AMEDIA_OK) {
    NotifyMediaCodecError(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                          status, "Failed to change bitrate and framerate");
    return;
  }
  effective_framerate_ = framerate;
  effective_bitrate_ = bitrate;
}

void NdkVideoEncodeAccelerator::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_ptr_factory_.reset();
  if (media_codec_) {
    media_codec_->Stop();

    // Internally this calls AMediaFormat_delete(), and before exiting
    // AMediaFormat_delete() drains all calls on the internal thread that
    // calls OnAsyncXXXXX() functions. (Even though this fact is not documented)
    // It means by the time we actually destruct `this`, no OnAsyncXXXXX()
    // functions will use it via saved `userdata` pointers.
    media_codec_.reset();
  }
  gl_renderer_.reset();
  metrics_helper_.reset();
  delete this;
}

bool NdkVideoEncodeAccelerator::IsFlushSupported() {
  // While MediaCodec supports marking an input buffer as end-of-stream, the
  // documentation indicates that returning to a normal state is only supported
  // for decoders:
  //
  // https://developer.android.com/reference/android/media/MediaCodec#states
  //
  // Since we haven't yet encountered any encoders which won't eventually return
  // outputs given enough time and recreating codecs is expensive, we opt to not
  // implement flush and have VEA clients instead wait for all outputs to flush.
  return false;
}

void NdkVideoEncodeAccelerator::SetCommandBufferHelperCB(
    base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()>
        get_command_buffer_helper_cb,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner) {
  if (!use_surface_as_input_) {
    return;
  }
  gpu_task_runner_ = std::move(gpu_task_runner);
  gpu_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(get_command_buffer_helper_cb),
      base::BindOnce(&NdkVideoEncodeAccelerator::OnCommandBufferHelperAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NdkVideoEncodeAccelerator::OnCommandBufferHelperAvailable(
    scoped_refptr<CommandBufferHelper> command_buffer_helper) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  command_buffer_helper_ = std::move(command_buffer_helper);
  if (!command_buffer_helper_) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderInitializationError,
                       "Can't obtain CommandBufferHelper"});
    return;
  }
  gl_renderer_->SetSharedImageManager(
      command_buffer_helper_->GetSharedImageManager());

  // Call FeedInput() in case we have pending frames waiting for
  // synchronization.
  FeedInput();
}

bool NdkVideoEncodeAccelerator::SetInputBufferLayout(
    const gfx::Size& configured_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(media_codec_);
  DCHECK(!configured_size.IsEmpty());

  MediaFormatPtr input_format(
      AMediaCodec_getInputFormat(media_codec_->codec()));
  if (!input_format) {
    return false;
  }

  // Non 16x16 aligned resolutions don't work well with MediaCodec
  // unfortunately, see https://crbug.com/1084702 for details. It seems they
  // only work when stride/y_plane_height information is provided.
  const auto aligned_size = gfx::Size(
      base::bits::AlignDownDeprecatedDoNotUse(configured_size.width(), 16),
      base::bits::AlignDownDeprecatedDoNotUse(configured_size.height(), 16));

  bool require_aligned_resolution = false;
  if (!AMediaFormat_getInt32(input_format.get(), AMEDIAFORMAT_KEY_STRIDE,
                             &input_buffer_stride_)) {
    input_buffer_stride_ = aligned_size.width();
    require_aligned_resolution = true;
  }
  if (!AMediaFormat_getInt32(input_format.get(), AMEDIAFORMAT_KEY_SLICE_HEIGHT,
                             &input_buffer_yplane_height_)) {
    input_buffer_yplane_height_ = aligned_size.height();
    require_aligned_resolution = true;
  }

  if (!require_aligned_resolution) {
    return true;
  }

  // If the size is already aligned, nothing to do.
  if (config_.input_visible_size == aligned_size) {
    return true;
  }

  // Otherwise, we need to crop to the nearest 16x16 alignment.
  if (aligned_size.IsEmpty()) {
    MEDIA_LOG(ERROR, log_) << "MediaCodec on this platform requires 16x16 "
                              "alignment, which is not possible for: "
                           << config_.input_visible_size.ToString();
    return false;
  }

  aligned_size_ = aligned_size;

  MEDIA_LOG(INFO, log_)
      << "MediaCodec encoder requires 16x16 aligned resolution. Cropping to "
      << aligned_size_->ToString();

  return true;
}

base::TimeDelta NdkVideoEncodeAccelerator::RecordFrameTimestamps(
    base::TimeDelta real_timestamp) {
  base::TimeDelta step = base::Seconds(1) / effective_framerate_;
  auto result = next_timestamp_;
  generated_to_real_timestamp_map_[result] = {real_timestamp,
                                              base::TimeTicks::Now()};
  next_timestamp_ += step;
  return result;
}

std::optional<NdkVideoEncodeAccelerator::FrameTimestampInfo>
NdkVideoEncodeAccelerator::RetrieveFrameTimestamps(
    base::TimeDelta monotonic_timestamp) {
  auto it = generated_to_real_timestamp_map_.find(monotonic_timestamp);
  if (it != generated_to_real_timestamp_map_.end()) {
    FrameTimestampInfo result = it->second;
    generated_to_real_timestamp_map_.erase(it);
    return result;
  }
  return std::nullopt;
}

void NdkVideoEncodeAccelerator::FeedInput() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(media_codec_);

  if (error_occurred_) {
    // Do not feed more data if an error has occurred.
    return;
  }

  if (pending_frames_.empty()) {
    // There are no frames to be encoded.
    return;
  }

  if (!media_codec_->HasInput() && !use_surface_as_input_) {
    // The encode is in a mode where it uses input buffers to feed new frames,
    // but we have no input buffers available.
    return;
  }

  if (pending_color_space_) {
    // The encoder is being reconfigured to handle a new color space.
    return;
  }

  auto& next_encode = pending_frames_.front();
  auto& frame = next_encode.frame;
  bool key_frame = next_encode.options.key_frame;
  // Handle frame synchronization before encoding, this dos nothing for
  // frames that don't have shared images.
  switch (next_encode.sync_state) {
    case SyncState::kReadyForEncoding:
      // The frame is ready, so we can proceed with encoding.
      break;
    case SyncState::kNeedsSync: {
      // This frame requires synchronization. We start the sync process and
      // transition the state to kSyncInProgress.
      if (!command_buffer_helper_) {
        // We don't have CommandBufferHelper yet, let's wait till it's set.
        return;
      }
      next_encode.sync_state = SyncState::kSyncInProgress;
      auto sync_done_callback = base::BindPostTaskToCurrentDefault(
          base::BindOnce(&NdkVideoEncodeAccelerator::OnSyncDone,
                         weak_ptr_factory_.GetWeakPtr(), frame->unique_id()));
      gpu_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&WaitForSyncTokenOnGpuThread, command_buffer_helper_,
                         frame->acquire_sync_token(),
                         std::move(sync_done_callback)));
      return;
    }
    case SyncState::kSyncInProgress:
      // Synchronization is already in progress for this frame, so we wait.
      return;
  }

  const auto frame_cs = frame->ColorSpace();
  if (!encoder_color_space_ || *encoder_color_space_ != frame_cs) {
    if (!have_encoded_frames_) {
      encoder_color_space_ = frame_cs;
      SetEncoderColorSpace();
    } else {
      // Flush codec and wait for outputs to recreate the codec.
      pending_color_space_ = frame_cs;
      media_status_t status = SendEndOfStream();
      if (status != AMEDIA_OK) {
        NotifyMediaCodecError(EncoderStatus::Codes::kEncoderHardwareDriverError,
                              status, "Failed to queueInputBuffer");
      }
      return;
    }
  }

  if (key_frame) {
    // AMEDIACODEC_KEY_REQUEST_SYNC_FRAME is not exposed until SDK 31.
    // Signal to the media codec that it needs to include a key frame
    MediaFormatPtr format(AMediaFormat_new());
    AMediaFormat_setInt32(
        format.get(), "request-sync" /*AMEDIACODEC_KEY_REQUEST_SYNC_FRAME*/, 0);
    media_status_t status =
        AMediaCodec_setParameters(media_codec_->codec(), format.get());

    if (status != AMEDIA_OK) {
      NotifyMediaCodecError(EncoderStatus::Codes::kEncoderFailedEncode, status,
                            "Failed to request a keyframe");
      return;
    }
  }

  // MediaCodec uses timestamps for rate control purposes, but we can't rely
  // on real frame timestamps to be consistent with configured frame rate.
  // That's why we map real frame timestamps to generate ones that a
  // monotonically increase according to the configured frame rate.
  // We do the opposite for each output buffer, to restore accurate frame
  // timestamps.
  auto timestamp = RecordFrameTimestamps(frame->timestamp());

  if (use_surface_as_input_) {
    FeedGLSurface(std::move(frame), timestamp);
  } else {
    FeedInputBuffer(std::move(frame), timestamp);
  }
  have_encoded_frames_ = true;
  pending_frames_.pop_front();
}

void NdkVideoEncodeAccelerator::OnSyncDone(VideoFrame::ID frame_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_frames_.empty() ||
      pending_frames_.front().frame->unique_id() != frame_id) {
    // This can happen if an error occurred and the queue was cleared.
    return;
  }

  DCHECK_EQ(pending_frames_.front().sync_state, SyncState::kSyncInProgress);
  pending_frames_.front().sync_state = SyncState::kReadyForEncoding;

  // Now when the sync token for a shared image frame has been waited on
  // we should initiate encoding again.
  FeedInput();
}

void NdkVideoEncodeAccelerator::FeedInputBuffer(scoped_refptr<VideoFrame> frame,
                                                base::TimeDelta timestamp) {
  const size_t buffer_idx = media_codec_->TakeInput();
  auto mc_input_buffer = media_codec_->GetInputBuffer(buffer_idx);
  if (mc_input_buffer.empty()) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderHardwareDriverError,
                       "Can't obtain input buffer from media codec"});
    return;
  }

  const auto visible_size =
      aligned_size_.value_or(frame->visible_rect().size());

  const int dst_stride_y = input_buffer_stride_;
  const int dst_stride_uv = input_buffer_stride_;
  const gfx::Size uv_plane_size = VideoFrame::PlaneSizeInSamples(
      PIXEL_FORMAT_NV12, VideoFrame::Plane::kUV, visible_size);

  const size_t y_plane_len = input_buffer_yplane_height_ * input_buffer_stride_;
  const size_t uv_plane_len =
      // size of all UV-plane lines but the last one
      (uv_plane_size.height() - 1) * dst_stride_uv +
      // size of the very last line in UV-plane (it's not padded to full stride)
      uv_plane_size.width() * 2;

  const size_t queued_size = y_plane_len + uv_plane_len;

  if (queued_size > mc_input_buffer.size()) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kInvalidInputFrame,
         base::StringPrintf("Frame doesn't fit into the input "
                            "buffer. queued_size: %zu capacity: "
                            "%zu",
                            queued_size, mc_input_buffer.size())});
    return;
  }

  auto dst_y = mc_input_buffer.first(y_plane_len);
  auto dst_uv = mc_input_buffer.subspan(y_plane_len, uv_plane_len);

  bool converted = false;
  if (frame->format() == PIXEL_FORMAT_I420) {
    converted =
        !libyuv::I420ToNV12(frame->visible_data(VideoFrame::Plane::kY),
                            frame->stride(VideoFrame::Plane::kY),
                            frame->visible_data(VideoFrame::Plane::kU),
                            frame->stride(VideoFrame::Plane::kU),
                            frame->visible_data(VideoFrame::Plane::kV),
                            frame->stride(VideoFrame::Plane::kV), dst_y.data(),
                            dst_stride_y, dst_uv.data(), dst_stride_uv,
                            visible_size.width(), visible_size.height());
  } else if (frame->format() == PIXEL_FORMAT_NV12) {
    converted =
        !libyuv::NV12Copy(frame->visible_data(VideoFrame::Plane::kY),
                          frame->stride(VideoFrame::Plane::kY),
                          frame->visible_data(VideoFrame::Plane::kUV),
                          frame->stride(VideoFrame::Plane::kUV), dst_y.data(),
                          dst_stride_y, dst_uv.data(), dst_stride_uv,
                          visible_size.width(), visible_size.height());
  } else {
    NotifyErrorStatus({EncoderStatus::Codes::kUnsupportedFrameFormat,
                       "Unexpected frame format: " +
                           VideoPixelFormatToString(frame->format())});
    return;
  }

  if (!converted) {
    NotifyErrorStatus({EncoderStatus::Codes::kFormatConversionError,
                       "Failed to copy pixels to input buffer"});
    return;
  }

  uint64_t flags = 0;  // Unfortunately BUFFER_FLAG_KEY_FRAME has no effect here
  media_status_t status = AMediaCodec_queueInputBuffer(
      media_codec_->codec(), buffer_idx, /*offset=*/0, queued_size,
      timestamp.InMicroseconds(), flags);
  if (status != AMEDIA_OK) {
    NotifyMediaCodecError(EncoderStatus::Codes::kEncoderHardwareDriverError,
                          status, "Failed to queueInputBuffer");
    return;
  }
}

media_status_t NdkVideoEncodeAccelerator::SendEndOfStream() {
  if (use_surface_as_input_) {
    return AMediaCodec_signalEndOfInputStream(media_codec_->codec());
  }
  size_t buffer_idx = media_codec_->TakeInput();
  return AMediaCodec_queueInputBuffer(
      media_codec_->codec(), buffer_idx, /*offset=*/0, /*size=*/0,
      /*presentationTimeUs=*/0,
      /*flags=*/AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
}

void NdkVideoEncodeAccelerator::FeedGLSurface(scoped_refptr<VideoFrame> frame,
                                              base::TimeDelta timestamp) {
  DCHECK(use_surface_as_input_);
  if (!gl_renderer_) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderHardwareDriverError,
                       "GL renderer is not initialized"});
    return;
  }

  // RenderVideoFrame() submits the rendered frame to the MediaCodec's input
  // surface.
  auto render_status =
      gl_renderer_->RenderVideoFrame(frame, timestamp + base::TimeTicks());
  if (!render_status.is_ok()) {
    NotifyErrorStatus(std::move(render_status));
    MEDIA_LOG(ERROR, log_) << "Most recent frame: "
                           << frame->AsHumanReadableString();
    return;
  }
}

void NdkVideoEncodeAccelerator::NotifyMediaCodecError(
    EncoderStatus encoder_status,
    media_status_t media_codec_status,
    std::string message) {
  NotifyErrorStatus({encoder_status.code(),
                     base::StringPrintf("%s MediaCodec error code: %d",
                                        message.c_str(), media_codec_status)});
}

void NdkVideoEncodeAccelerator::NotifyErrorStatus(EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!status.is_ok());
  MEDIA_LOG(ERROR, log_) << EncoderStatusCodeToString(status.code()) << " "
                         << status.message();
  if (!error_occurred_) {
    base::UmaHistogramEnumeration(
        GetEncoderStatusHistogramName(config_.output_profile), status.code());

    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoEncodeAccelerator::Client::NotifyErrorStatus,
                       client_ptr_factory_->GetWeakPtr(), status));
    error_occurred_ = true;
  }
}

void NdkVideoEncodeAccelerator::OnInputAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FeedInput();
}

void NdkVideoEncodeAccelerator::OnOutputAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DrainOutput();
  // When using a surface as input, the `OnInputAvailable()` callback is not
  // triggered because we are not using input buffers. Instead, we feed frames
  // by rendering them to a surface. Backpressure is handled by the rendering
  // pipeline, which means the `pending_frames_` queue is usually empty.
  //
  // We call `FeedInput()` here to handle cases where we were waiting for the
  // encoder to reconfigure with a new color space. This call is unconditional
  // because `FeedInput()` already performs all the necessary checks.
  FeedInput();
}

void NdkVideoEncodeAccelerator::OnError(media_status_t error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyMediaCodecError(EncoderStatus::Codes::kEncoderFailedEncode, error,
                        "Async media codec error");
}

bool NdkVideoEncodeAccelerator::DrainConfig() {
  if (!media_codec_->HasOutput()) {
    return false;
  }

  NdkMediaCodecWrapper::OutputInfo output_buffer = media_codec_->PeekOutput();
  AMediaCodecBufferInfo& mc_buffer_info = output_buffer.info;

  // Check that the first buffer in the queue contains config data.
  if ((mc_buffer_info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) == 0)
    return false;

  // We already have the info we need from `output_buffer`
  std::ignore = media_codec_->TakeOutput();

  auto out_buffer_data = media_codec_->GetOutputBuffer(output_buffer);
  if (out_buffer_data.empty()) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderFailedEncode,
                       "Can't obtain output buffer from media codec"});
    return false;
  }

  if (ProfileNeedsConfigDataInBitstream(config_.output_profile)) {
    config_data_.assign(out_buffer_data.begin(), out_buffer_data.end());
  }
  AMediaCodec_releaseOutputBuffer(media_codec_->codec(),
                                  output_buffer.buffer_index, false);
  return true;
}

void NdkVideoEncodeAccelerator::DrainOutput() {
  if (error_occurred_)
    return;

  // Config data (e.g. PPS and SPS for H.264) needs to be handled differently,
  // because we save it for later rather than giving it as an output
  // straight away.
  if (DrainConfig())
    return;

  if (!media_codec_->HasOutput() || available_bitstream_buffers_.empty()) {
    return;
  }

  NdkMediaCodecWrapper::OutputInfo output_buffer = media_codec_->TakeOutput();
  AMediaCodecBufferInfo& mc_buffer_info = output_buffer.info;
  const size_t mc_buffer_size = static_cast<size_t>(mc_buffer_info.size);

  if ((mc_buffer_info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0) {
    if (pending_color_space_) {
      DCHECK_EQ(mc_buffer_size, 0u);
      encoder_color_space_ = pending_color_space_;
      pending_color_space_.reset();
      if (!ResetMediaCodec().is_ok()) {
        NotifyErrorStatus(
            {EncoderStatus::Codes::kEncoderFailedEncode,
             "Failed to recreate media codec for color space change."});
      }

      // Encoding will continue when MediaCodec signals OnInputAvailable().
    }
    return;
  }

  const bool key_frame = (mc_buffer_info.flags & BUFFER_FLAG_KEY_FRAME) != 0;

  BitstreamBuffer bitstream_buffer =
      std::move(available_bitstream_buffers_.back());
  available_bitstream_buffers_.pop_back();

  const size_t config_size = key_frame ? config_data_.size() : 0u;
  if (config_size + mc_buffer_size > bitstream_buffer.size()) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kEncoderFailedEncode,
         base::StringPrintf("Encoded output is too large. mc output size: %zu"
                            " bitstream buffer size: %zu"
                            " config size: %zu",
                            mc_buffer_size, bitstream_buffer.size(),
                            config_size)});
    return;
  }

  auto out_buffer_data = media_codec_->GetOutputBuffer(output_buffer);
  if (out_buffer_data.empty()) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderFailedEncode,
                       "Can't obtain output buffer from media codec"});
    return;
  }

  base::ScopedClosureRunner release_buffer(base::BindOnce(
      [](NdkMediaCodecWrapper* media_codec, int index) {
        AMediaCodec_releaseOutputBuffer(media_codec->codec(), index, false);
      },
      base::Unretained(media_codec_.get()), output_buffer.buffer_index));

  base::UnsafeSharedMemoryRegion region = bitstream_buffer.TakeRegion();
  auto mapping =
      region.MapAt(bitstream_buffer.offset(), bitstream_buffer.size());
  if (!mapping.IsValid()) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kSystemAPICallError, "Failed to map SHM"});
    return;
  }

  auto output_dst = mapping.GetMemoryAsSpan<uint8_t>();
  if (config_size > 0) {
    output_dst.copy_prefix_from(config_data_);
    output_dst = output_dst.subspan(config_size);
  }

  output_dst.copy_prefix_from(out_buffer_data);
  auto timestamp_info = RetrieveFrameTimestamps(
      base::Microseconds(mc_buffer_info.presentationTimeUs));
  if (!timestamp_info.has_value()) {
    MEDIA_LOG(ERROR, log_) << "Failed to find timestamp for encoded frame. ts:"
                           << mc_buffer_info.presentationTimeUs;
    NOTREACHED(base::NotFatalUntil::M150)
        << "Failed to find timestamp for encoded frame. ts:"
        << mc_buffer_info.presentationTimeUs;
    timestamp_info = FrameTimestampInfo();
  }
  auto metadata = BitstreamBufferMetadata(
      mc_buffer_size + config_size, key_frame, timestamp_info->real_timestamp);
  if (aligned_size_) {
    metadata.encoded_size = aligned_size_;
  }
  if (encoder_color_space_) {
    metadata.encoded_color_space = *encoder_color_space_;
  }

  if (num_temporal_layers_ > 1) {
    DCHECK(svc_parser_);
    if (key_frame) {
      input_since_keyframe_count_ = 0;
    }

    TemporalScalabilityIdExtractor::BitstreamMetadata bits_md;
    if (!svc_parser_->ParseChunk(output_dst.first(mc_buffer_size),
                                 input_since_keyframe_count_, bits_md)) {
      NotifyErrorStatus({EncoderStatus::Codes::kEncoderHardwareDriverError,
                         "Parse bitstream failed"});
      return;
    }

    switch (VideoCodecProfileToVideoCodec(config_.output_profile)) {
      case VideoCodec::kH264:
        metadata.h264.emplace().temporal_idx = bits_md.temporal_id;
        break;
      default:
        NOTIMPLEMENTED() << "SVC is only supported for H.264.";
        break;
    }
    ++input_since_keyframe_count_;
  }

  auto encoding_latency =
      base::TimeTicks::Now() - timestamp_info->encode_start_time;
  metrics_helper_->EncodeOneFrame(key_frame, encoding_latency);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncodeAccelerator::Client::BitstreamBufferReady,
                     client_ptr_factory_->GetWeakPtr(), bitstream_buffer.id(),
                     metadata));
}

EncoderStatus NdkVideoEncodeAccelerator::ResetMediaCodec() {
  DCHECK(!pending_color_space_);

  have_encoded_frames_ = false;

  if (media_codec_) {
    media_codec_->Stop();
    media_codec_.reset();
  }
  gl_renderer_.reset();

  auto name = FindMediaCodecFor(config_);
  if (!name) {
    MEDIA_LOG(ERROR, log_) << "No suitable MedicCodec found for: "
                           << config_.AsHumanReadableString();
    return {EncoderStatus::Codes::kEncoderUnsupportedCodec};
  }

  auto configured_size = aligned_size_.value_or(config_.input_visible_size);
  PixelFormat pixel_format = use_surface_as_input_
                                 ? COLOR_FORMAT_SURFACE
                                 : COLOR_FORMAT_YUV420_SEMIPLANAR;
  auto media_format = CreateVideoFormat(
      config_, effective_framerate_, configured_size, effective_bitrate_,
      encoder_color_space_, num_temporal_layers_, pixel_format);
  if (!media_format) {
    MEDIA_LOG(ERROR, log_) << "Fail to create media format for: "
                           << config_.AsHumanReadableString();
    return {EncoderStatus::Codes::kEncoderUnsupportedConfig};
  }

  // We do the following in a loop since we may need to recreate the MediaCodec
  // if it doesn't unaligned resolutions.
  do {
    media_codec_ =
        NdkMediaCodecWrapper::CreateByCodecName(*name, this, task_runner_);
    if (!media_codec_) {
      MEDIA_LOG(ERROR, log_)
          << "Can't create media codec (" << name.value()
          << ") for config: " << config_.AsHumanReadableString();
      return {EncoderStatus::Codes::kEncoderInitializationError};
    }
    media_status_t status = AMediaCodec_configure(
        media_codec_->codec(), media_format.get(), nullptr, nullptr,
        AMEDIACODEC_CONFIGURE_FLAG_ENCODE);

    if (status != AMEDIA_OK) {
      MEDIA_LOG(ERROR, log_) << "Can't configure media codec. Error " << status;
      return {EncoderStatus::Codes::kEncoderInitializationError};
    }

    if (use_surface_as_input_) {
      ANativeWindow* surface;
      status = AMediaCodec_createInputSurface(media_codec_->codec(), &surface);
      if (status != AMEDIA_OK) {
        MEDIA_LOG(ERROR, log_)
            << "Can't create input surface. Error " << status;
        return {EncoderStatus::Codes::kEncoderInitializationError};
      }

      input_surface_ = gl::ScopedANativeWindow::Adopt(surface);
      gl_renderer_ = std::make_unique<VideoFrameGLSurfaceRenderer>(
          std::move(input_surface_));
      if (command_buffer_helper_) {
        gl_renderer_->SetSharedImageManager(
            command_buffer_helper_->GetSharedImageManager());
      }
      auto gl_renderer_status = gl_renderer_->Initialize();
      if (!gl_renderer_status.is_ok()) {
        MEDIA_LOG(ERROR, log_) << "Failed to initialize GL renderer: "
                               << gl_renderer_status.message();
        return gl_renderer_status;
      }

      // We exit the "loop", since the reset of the code below deals with
      // the layout and workarounds for input buffers, which are unused
      // for surface input.
      break;
    }

    if (!SetInputBufferLayout(configured_size)) {
      MEDIA_LOG(ERROR, log_) << "Can't get input buffer layout from MediaCodec";
      return {EncoderStatus::Codes::kEncoderInitializationError};
    }

    if (aligned_size_.value_or(configured_size) != configured_size) {
      // Give the client a chance to handle realignment itself.
      encoder_info_.requested_resolution_alignment = 16;
      encoder_info_.apply_alignment_to_all_simulcast_layers = true;
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &VideoEncodeAccelerator::Client::NotifyEncoderInfoChange,
              client_ptr_factory_->GetWeakPtr(), encoder_info_));

      // We must recreate the MediaCodec now since setParameters() doesn't work
      // consistently across devices and versions of Android.
      media_codec_->Stop();
      media_codec_.reset();

      AMediaFormat_setInt32(media_format.get(), AMEDIAFORMAT_KEY_WIDTH,
                            aligned_size_->width());
      AMediaFormat_setInt32(media_format.get(), AMEDIAFORMAT_KEY_HEIGHT,
                            aligned_size_->height());
      configured_size = *aligned_size_;
    }
  } while (!media_codec_);

  media_status_t status = media_codec_->Start();
  if (status == AMEDIACODEC_ERROR_INSUFFICIENT_RESOURCE) {
    MEDIA_LOG(ERROR, log_) << "No more encoders available. Error " << status;
    return {EncoderStatus::Codes::kOutOfPlatformEncoders};
  }
  if (status != AMEDIA_OK) {
    MEDIA_LOG(ERROR, log_) << "Can't start media codec. Error " << status;
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }

  metrics_helper_ = std::make_unique<VEAEncodingLatencyMetricsHelper>(
      "Media.VideoEncoder.NDKVEA.EncodingLatency.",
      VideoCodecProfileToVideoCodec(config_.output_profile));

  MEDIA_LOG(INFO, log_) << "Created MediaCodec (" << name.value()
                        << ") for config: " << config_.AsHumanReadableString();

  return {EncoderStatus::Codes::kOk};
}

void NdkVideoEncodeAccelerator::SetEncoderColorSpace() {
  DCHECK(!have_encoded_frames_);
  DCHECK(encoder_color_space_);

  if (!encoder_color_space_->IsValid()) {
    return;
  }

  MediaFormatPtr format(AMediaFormat_new());
  if (!SetFormatColorSpace(format.get(), *encoder_color_space_)) {
    return;
  }

  auto status = AMediaCodec_setParameters(media_codec_->codec(), format.get());
  if (status != AMEDIA_OK) {
    DLOG(ERROR) << "Failed to set color space parameters: " << status;
    return;
  }

  DVLOG(1) << "Set color space to: " << encoder_color_space_->ToString();
}

}  // namespace media
#pragma clang attribute pop
