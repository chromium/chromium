// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/android/ndk_video_encode_accelerator.h"

#include <optional>

#include "base/bits.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/notimplemented.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/encoder_status.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/gpu/android/video_accelerator_util.h"
#include "media/parsers/h264_level_limits.h"
#include "media/parsers/h264_parser.h"
#include "media/parsers/temporal_scalability_id_extractor.h"
#include "third_party/libyuv/include/libyuv.h"

#pragma clang attribute push DEFAULT_REQUIRES_ANDROID_API( \
    NDK_MEDIA_CODEC_MIN_API)
namespace media {

using EncoderType = VideoEncodeAccelerator::Config::EncoderType;

namespace {

// Default distance between key frames. About 100 seconds between key frames,
// the same default value we use on Windows.
constexpr uint32_t kDefaultGOPLength = 3000;

// Deliberately breaking naming convention rules, to match names from
// MediaCodec SDK.
constexpr int32_t BUFFER_FLAG_KEY_FRAME = 1;

enum PixelFormat {
  // Subset of MediaCodecInfo.CodecCapabilities.
  COLOR_FORMAT_YUV420_PLANAR = 19,
  COLOR_FORMAT_YUV420_SEMIPLANAR = 21,  // Same as NV12
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
    AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_LATENCY, 1);
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
      NOTREACHED_IN_MIGRATION();
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

}  // namespace

NdkVideoEncodeAccelerator::NdkVideoEncodeAccelerator(
    scoped_refptr<base::SequencedTaskRunner> runner)
    : task_runner_(std::move(runner)) {}

NdkVideoEncodeAccelerator::~NdkVideoEncodeAccelerator() {
  // It's supposed to be cleared by Destroy(), it basically checks
  // that we destroy `this` correctly.
  DCHECK(!media_codec_);
}

VideoEncodeAccelerator::SupportedProfiles
NdkVideoEncodeAccelerator::GetSupportedProfiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SupportedProfiles profiles;
  for (auto& info : GetEncoderInfoCache()) {
    const auto codec = VideoCodecProfileToVideoCodec(info.profile.profile);
    switch (codec) {
      case VideoCodec::kHEVC:
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        if (base::FeatureList::IsEnabled(kPlatformHEVCEncoderSupport) &&
            // Currently only 8bit NV12 and I420 encoding is supported, so limit
            // this to main profile only just like other platforms.
            info.profile.profile == VideoCodecProfile::HEVCPROFILE_MAIN &&
            // Some devices may report to have a software HEVC encoder,
            // however based on tests, they are not always working well,
            // so limit the support to HW only for now.
            !info.profile.is_software_codec) {
          profiles.push_back(info.profile);
        }
#endif
        break;
      default:
        profiles.push_back(info.profile);
        break;
    }
  }
  return profiles;
}

bool NdkVideoEncodeAccelerator::Initialize(
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
  if (codec != VideoCodec::kH264 && codec == VideoCodec::kHEVC) {
    config_.required_encoder_type = EncoderType::kHardware;
  }

  if (config.input_format != PIXEL_FORMAT_I420 &&
      config.input_format != PIXEL_FORMAT_NV12) {
    MEDIA_LOG(ERROR, log_) << "Unexpected combo: " << config.input_format
                           << ", " << GetProfileName(config.output_profile);
    return false;
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

  if (!ResetMediaCodec()) {
    return false;
  }

  // Conservative upper bound for output buffer size: decoded size + 2KB.
  // Adding 2KB just in case the frame is really small, we don't want to
  // end up with no space for a video codec's headers.
  const size_t output_buffer_capacity =
      VideoFrame::AllocationSize(config.input_format,
                                 config.input_visible_size) +
      2048;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncodeAccelerator::Client::RequireBitstreamBuffers,
                     client_ptr_factory_->GetWeakPtr(), 1,
                     config.input_visible_size, output_buffer_capacity));

  return true;
}

void NdkVideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                       bool force_keyframe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(media_codec_);
  VideoEncoder::PendingEncode encode;
  encode.frame = std::move(frame);
  encode.options = VideoEncoder::EncodeOptions(force_keyframe);
  pending_frames_.push_back(std::move(encode));
  FeedInput();
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

base::TimeDelta NdkVideoEncodeAccelerator::AssignMonotonicTimestamp(
    base::TimeDelta real_timestamp) {
  base::TimeDelta step = base::Seconds(1) / effective_framerate_;
  auto result = next_timestamp_;
  generated_to_real_timestamp_map_[result] = real_timestamp;
  next_timestamp_ += step;
  return result;
}

base::TimeDelta NdkVideoEncodeAccelerator::RetrieveRealTimestamp(
    base::TimeDelta monotonic_timestamp) {
  base::TimeDelta result;
  auto it = generated_to_real_timestamp_map_.find(monotonic_timestamp);
  if (it != generated_to_real_timestamp_map_.end()) {
    result = it->second;
    generated_to_real_timestamp_map_.erase(it);
  }
  return result;
}

void NdkVideoEncodeAccelerator::FeedInput() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(media_codec_);

  if (error_occurred_)
    return;

  if (!media_codec_->HasInput() || pending_frames_.empty()) {
    return;
  }

  if (pending_color_space_) {
    return;
  }

  size_t buffer_idx = media_codec_->TakeInput();

  const auto frame_cs = pending_frames_.front().frame->ColorSpace();
  if (!encoder_color_space_ || *encoder_color_space_ != frame_cs) {
    if (!have_encoded_frames_) {
      encoder_color_space_ = frame_cs;
      SetEncoderColorSpace();
    } else {
      // Flush codec and wait for outputs to recreate the codec.
      pending_color_space_ = frame_cs;
      media_status_t status = AMediaCodec_queueInputBuffer(
          media_codec_->codec(), buffer_idx, /*offset=*/0, 0, 0,
          AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
      if (status != AMEDIA_OK) {
        NotifyMediaCodecError(EncoderStatus::Codes::kEncoderHardwareDriverError,
                              status, "Failed to queueInputBuffer");
      }
      return;
    }
  }

  have_encoded_frames_ = true;
  scoped_refptr<VideoFrame> frame = std::move(pending_frames_.front().frame);
  bool key_frame = pending_frames_.front().options.key_frame;
  pending_frames_.pop_front();

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

  size_t capacity = 0;
  uint8_t* buffer_ptr =
      AMediaCodec_getInputBuffer(media_codec_->codec(), buffer_idx, &capacity);
  if (!buffer_ptr) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderHardwareDriverError,
                       "Can't obtain input buffer from media codec"});
    return;
  }

  const auto visible_size =
      aligned_size_.value_or(frame->visible_rect().size());

  uint8_t* dst_y = buffer_ptr;
  const int dst_stride_y = input_buffer_stride_;
  const int uv_plane_offset =
      input_buffer_yplane_height_ * input_buffer_stride_;
  uint8_t* dst_uv = buffer_ptr + uv_plane_offset;
  const int dst_stride_uv = input_buffer_stride_;

  const gfx::Size uv_plane_size = VideoFrame::PlaneSizeInSamples(
      PIXEL_FORMAT_NV12, VideoFrame::Plane::kUV, visible_size);
  const size_t queued_size =
      // size of Y-plane plus padding till UV-plane
      uv_plane_offset +
      // size of all UV-plane lines but the last one
      (uv_plane_size.height() - 1) * dst_stride_uv +
      // size of the very last line in UV-plane (it's not padded to full stride)
      uv_plane_size.width() * 2;

  if (queued_size > capacity) {
    NotifyErrorStatus({EncoderStatus::Codes::kInvalidInputFrame,
                       base::StringPrintf("Frame doesn't fit into the input "
                                          "buffer. queued_size: %zu capacity: "
                                          "%zu",
                                          queued_size, capacity)});
    return;
  }

  bool converted = false;
  if (frame->format() == PIXEL_FORMAT_I420) {
    converted = !libyuv::I420ToNV12(
        frame->visible_data(VideoFrame::Plane::kY),
        frame->stride(VideoFrame::Plane::kY),
        frame->visible_data(VideoFrame::Plane::kU),
        frame->stride(VideoFrame::Plane::kU),
        frame->visible_data(VideoFrame::Plane::kV),
        frame->stride(VideoFrame::Plane::kV), dst_y, dst_stride_y, dst_uv,
        dst_stride_uv, visible_size.width(), visible_size.height());
  } else if (frame->format() == PIXEL_FORMAT_NV12) {
    converted = !libyuv::NV12Copy(frame->visible_data(VideoFrame::Plane::kY),
                                  frame->stride(VideoFrame::Plane::kY),
                                  frame->visible_data(VideoFrame::Plane::kUV),
                                  frame->stride(VideoFrame::Plane::kUV), dst_y,
                                  dst_stride_y, dst_uv, dst_stride_uv,
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

  // MediaCodec uses timestamps for rate control purposes, but we can't rely
  // on real frame timestamps to be consistent with configured frame rate.
  // That's why we map real frame timestamps to generate ones that a
  // monotonically increase according to the configured frame rate.
  // We do the opposite for each output buffer, to restore accurate frame
  // timestamps.
  auto generate_timestamp = AssignMonotonicTimestamp(frame->timestamp());
  uint64_t flags = 0;  // Unfortunately BUFFER_FLAG_KEY_FRAME has no effect here
  media_status_t status = AMediaCodec_queueInputBuffer(
      media_codec_->codec(), buffer_idx, /*offset=*/0, queued_size,
      generate_timestamp.InMicroseconds(), flags);
  if (status != AMEDIA_OK) {
    NotifyMediaCodecError(EncoderStatus::Codes::kEncoderHardwareDriverError,
                          status, "Failed to queueInputBuffer");
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
  MEDIA_LOG(ERROR, log_) << status.message();
  LOG(ERROR) << "Call NotifyErrorStatus(): code="
             << static_cast<int>(status.code())
             << ", message=" << status.message();
  if (!error_occurred_) {
    client_ptr_factory_->GetWeakPtr()->NotifyErrorStatus(status);
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
  const size_t mc_buffer_size = static_cast<size_t>(mc_buffer_info.size);

  // Check that the first buffer in the queue contains config data.
  if ((mc_buffer_info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) == 0)
    return false;

  // We already have the info we need from `output_buffer`
  std::ignore = media_codec_->TakeOutput();

  size_t capacity = 0;
  uint8_t* buf_data = AMediaCodec_getOutputBuffer(
      media_codec_->codec(), output_buffer.buffer_index, &capacity);

  if (!buf_data) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderFailedEncode,
                       "Can't obtain output buffer from media codec"});
    return false;
  }

  if (mc_buffer_info.offset + mc_buffer_size > capacity) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kEncoderFailedEncode,
         base::StringPrintf("Invalid output buffer layout."
                            "offset: %d size: %zu capacity: %zu",
                            mc_buffer_info.offset, mc_buffer_size, capacity)});
    return false;
  }

  config_data_.resize(mc_buffer_size);
  memcpy(config_data_.data(), buf_data + mc_buffer_info.offset, mc_buffer_size);
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
      if (!ResetMediaCodec()) {
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

  size_t capacity = 0;
  uint8_t* buf_data = AMediaCodec_getOutputBuffer(
      media_codec_->codec(), output_buffer.buffer_index, &capacity);

  if (!buf_data) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderFailedEncode,
                       "Can't obtain output buffer from media codec"});
    return;
  }

  if (mc_buffer_info.offset + mc_buffer_size > capacity) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kEncoderFailedEncode,
         base::StringPrintf("Invalid output buffer layout."
                            "offset: %d size: %zu capacity: %zu",
                            mc_buffer_info.offset, mc_buffer_size, capacity)});
    return;
  }

  base::UnsafeSharedMemoryRegion region = bitstream_buffer.TakeRegion();
  auto mapping =
      region.MapAt(bitstream_buffer.offset(), bitstream_buffer.size());
  if (!mapping.IsValid()) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kSystemAPICallError, "Failed to map SHM"});
    return;
  }

  uint8_t* output_dst = mapping.GetMemoryAs<uint8_t>();
  if (config_size > 0) {
    memcpy(output_dst, config_data_.data(), config_size);
    output_dst += config_size;
  }
  memcpy(output_dst, buf_data, mc_buffer_size);

  auto timestamp = RetrieveRealTimestamp(
      base::Microseconds(mc_buffer_info.presentationTimeUs));
  auto metadata = BitstreamBufferMetadata(mc_buffer_size + config_size,
                                          key_frame, timestamp);
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
    if (!svc_parser_->ParseChunk(base::span(output_dst, mc_buffer_size),
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

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncodeAccelerator::Client::BitstreamBufferReady,
                     client_ptr_factory_->GetWeakPtr(), bitstream_buffer.id(),
                     metadata));
  AMediaCodec_releaseOutputBuffer(media_codec_->codec(),
                                  output_buffer.buffer_index, false);
}

bool NdkVideoEncodeAccelerator::ResetMediaCodec() {
  DCHECK(!pending_color_space_);

  have_encoded_frames_ = false;

  if (media_codec_) {
    media_codec_->Stop();
    media_codec_.reset();
  }

  auto name = FindMediaCodecFor(config_);
  if (!name) {
    MEDIA_LOG(ERROR, log_) << "No suitable MedicCodec found for: "
                           << config_.AsHumanReadableString();
    return false;
  }

  auto configured_size = aligned_size_.value_or(config_.input_visible_size);
  auto media_format =
      CreateVideoFormat(config_, effective_framerate_, configured_size,
                        effective_bitrate_, encoder_color_space_,
                        num_temporal_layers_, COLOR_FORMAT_YUV420_SEMIPLANAR);
  if (!media_format) {
    MEDIA_LOG(ERROR, log_) << "Fail to create media format for: "
                           << config_.AsHumanReadableString();
    return false;
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
      return false;
    }
    media_status_t status = AMediaCodec_configure(
        media_codec_->codec(), media_format.get(), nullptr, nullptr,
        AMEDIACODEC_CONFIGURE_FLAG_ENCODE);

    if (status != AMEDIA_OK) {
      MEDIA_LOG(ERROR, log_) << "Can't configure media codec. Error " << status;
      return false;
    }

    if (!SetInputBufferLayout(configured_size)) {
      MEDIA_LOG(ERROR, log_) << "Can't get input buffer layout from MediaCodec";
      return false;
    }

    if (aligned_size_.value_or(configured_size) != configured_size) {
      // Give the client a chance to handle realignment itself.
      VideoEncoderInfo encoder_info;
      encoder_info.requested_resolution_alignment = 16;
      encoder_info.apply_alignment_to_all_simulcast_layers = true;
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &VideoEncodeAccelerator::Client::NotifyEncoderInfoChange,
              client_ptr_factory_->GetWeakPtr(), encoder_info));

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
  if (status != AMEDIA_OK) {
    MEDIA_LOG(ERROR, log_) << "Can't start media codec. Error " << status;
    return false;
  }

  MEDIA_LOG(INFO, log_) << "Created MediaCodec (" << name.value()
                        << ") for config: " << config_.AsHumanReadableString();

  return true;
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
