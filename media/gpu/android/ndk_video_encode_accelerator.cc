// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/ndk_video_encode_accelerator.h"

#include "base/bits.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/encoder_status.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/gpu/android/video_accelerator_util.h"
#include "third_party/libyuv/include/libyuv.h"

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
    if (ptr)
      AMediaFormat_delete(ptr);
  }
};

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

MediaFormatPtr CreateVideoFormat(const std::string& mime,
                                 int iframe_interval,
                                 int framerate,
                                 bool require_low_delay,
                                 const gfx::Size& frame_size,
                                 const Bitrate& bitrate,
                                 absl::optional<gfx::ColorSpace> cs,
                                 PixelFormat format) {
  MediaFormatPtr result(AMediaFormat_new());
  AMediaFormat_setString(result.get(), AMEDIAFORMAT_KEY_MIME, mime.c_str());
  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_WIDTH,
                        frame_size.width());
  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_HEIGHT,
                        frame_size.height());

  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_FRAME_RATE, framerate);
  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_I_FRAME_INTERVAL,
                        iframe_interval);
  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_COLOR_FORMAT, format);
  if (require_low_delay) {
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
      NOTREACHED();
  }

  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_BIT_RATE,
                        base::saturated_cast<int32_t>(bitrate.target_bps()));

  if (cs && cs->IsValid()) {
    SetFormatColorSpace(result.get(), *cs);
  }

  return result;
}

absl::optional<std::string> FindMediaCodecFor(
    const VideoEncodeAccelerator::Config& config) {
  absl::optional<std::string> encoder_name;
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

    if (config.initial_framerate) {
      double max_supported_framerate =
          static_cast<double>(profile.max_framerate_numerator) /
          profile.max_framerate_denominator;
      if (config.initial_framerate.value() > max_supported_framerate) {
        continue;
      }
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

bool NdkVideoEncodeAccelerator::IsSupported() {
  return NdkMediaCodecWrapper::IsSupported();
}

VideoEncodeAccelerator::SupportedProfiles
NdkVideoEncodeAccelerator::GetSupportedProfiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SupportedProfiles profiles;
  if (!IsSupported())
    return profiles;

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

  if (!IsSupported()) {
    MEDIA_LOG(ERROR, log_) << "Unsupported Android version.";
    return false;
  }

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

  effective_framerate_ = config.initial_framerate.value_or(kDefaultFramerate);
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
    const absl::optional<gfx::Size>& size) {
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
    AMediaFormat_setInt32(format.get(), AMEDIACODEC_KEY_VIDEO_BITRATE,
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
    // Signal to the media codec that it needs to include a key frame
    MediaFormatPtr format(AMediaFormat_new());
    AMediaFormat_setInt32(format.get(), AMEDIACODEC_KEY_REQUEST_SYNC_FRAME, 0);
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
      PIXEL_FORMAT_NV12, VideoFrame::kUVPlane, visible_size);
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
        frame->visible_data(VideoFrame::kYPlane),
        frame->stride(VideoFrame::kYPlane),
        frame->visible_data(VideoFrame::kUPlane),
        frame->stride(VideoFrame::kUPlane),
        frame->visible_data(VideoFrame::kVPlane),
        frame->stride(VideoFrame::kVPlane), dst_y, dst_stride_y, dst_uv,
        dst_stride_uv, visible_size.width(), visible_size.height());
  } else if (frame->format() == PIXEL_FORMAT_NV12) {
    converted = !libyuv::NV12Copy(frame->visible_data(VideoFrame::kYPlane),
                                  frame->stride(VideoFrame::kYPlane),
                                  frame->visible_data(VideoFrame::kUVPlane),
                                  frame->stride(VideoFrame::kUVPlane), dst_y,
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

  auto mime = MediaCodecUtil::CodecToAndroidMimeType(
      VideoCodecProfileToVideoCodec(config_.output_profile));
  auto configured_size = aligned_size_.value_or(config_.input_visible_size);
  auto media_format = CreateVideoFormat(
      mime, config_.gop_length.value_or(kDefaultGOPLength),
      effective_framerate_, config_.require_low_delay, configured_size,
      effective_bitrate_, encoder_color_space_, COLOR_FORMAT_YUV420_SEMIPLANAR);

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
