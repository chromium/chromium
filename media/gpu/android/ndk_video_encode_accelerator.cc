// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/ndk_video_encode_accelerator.h"

#include "base/android/build_info.h"
#include "base/android/jni_string.h"
#include "base/bits.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/video_frame.h"
#include "media/gpu/android/mediacodec_stubs.h"
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

using MediaFormatPtr = std::unique_ptr<AMediaFormat, AMediaFormatDeleter>;

MediaFormatPtr CreateVideoFormat(const std::string& mime,
                                 const VideoEncodeAccelerator::Config& config,
                                 int framerate,
                                 PixelFormat format) {
  const int iframe_interval = config.gop_length.value_or(kDefaultGOPLength);
  const gfx::Size& frame_size = config.input_visible_size;
  const Bitrate& bitrate = config.bitrate;
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
      NOTREACHED();
  }

  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_BIT_RATE,
                        base::saturated_cast<int32_t>(bitrate.target_bps()));
  return result;
}

BASE_FEATURE(kAndroidNdkVideoEncoder,
             "AndroidNdkVideoEncoder",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool InitMediaCodec() {
  // We need at least Android P for AMediaCodec_getInputFormat(), but in
  // Android P we have issues with CFI and dynamic linker on arm64. However
  // GetSupportedProfiles() needs Q+, so just limit to Q.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_Q) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(kAndroidNdkVideoEncoder))
    return false;

  media_gpu_android::StubPathMap paths;
  constexpr base::FilePath::CharType kMediacodecPath[] =
      FILE_PATH_LITERAL("libmediandk.so");

  paths[media_gpu_android::kModuleMediacodec].push_back(kMediacodecPath);
  if (!media_gpu_android::InitializeStubs(paths)) {
    LOG(ERROR) << "Failed on loading libmediandk.so symbols";
    return false;
  }
  return true;
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
  static const bool is_loaded = InitMediaCodec();
  return is_loaded;
}

VideoEncodeAccelerator::SupportedProfiles
NdkVideoEncodeAccelerator::GetSupportedProfiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SupportedProfiles profiles;
  if (!IsSupported())
    return profiles;

  for (auto& info : GetEncoderInfoCache()) {
    profiles.push_back(info.profile);
  }
  return profiles;
}

bool NdkVideoEncodeAccelerator::Initialize(
    const Config& config,
    Client* client,
    std::unique_ptr<MediaLog> media_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!media_codec_);
  DCHECK(client);

  if (!IsSupported()) {
    MEDIA_LOG(ERROR, log_) << "Unsupported Android version.";
    return false;
  }

  callback_weak_ptr_ = callback_weak_factory_.GetWeakPtr();
  client_ptr_factory_ = std::make_unique<base::WeakPtrFactory<Client>>(client);
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

  auto name = FindMediaCodecFor(config_);
  if (!name) {
    MEDIA_LOG(ERROR, log_) << "No suitable MedicCodec found for: "
                           << config_.AsHumanReadableString();
    return false;
  }

  auto mime = MediaCodecUtil::CodecToAndroidMimeType(codec);
  effective_framerate_ = config.initial_framerate.value_or(kDefaultFramerate);
  auto media_format = CreateVideoFormat(mime, config, effective_framerate_,
                                        COLOR_FORMAT_YUV420_SEMIPLANAR);

  // We do the following in a loop since we may need to recreate the MediaCodec
  // if it doesn't unaligned resolutions.
  auto configured_size = config_.input_visible_size;
  do {
    media_codec_.reset(AMediaCodec_createCodecByName(name->c_str()));
    if (!media_codec_) {
      MEDIA_LOG(ERROR, log_)
          << "Can't create media codec (" << name.value()
          << ") for config: " << config_.AsHumanReadableString();
      return false;
    }
    media_status_t status =
        AMediaCodec_configure(media_codec_.get(), media_format.get(), nullptr,
                              nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
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
      AMediaCodec_stop(media_codec_.get());
      media_codec_.reset();

      AMediaFormat_setInt32(media_format.get(), AMEDIAFORMAT_KEY_WIDTH,
                            aligned_size_->width());
      AMediaFormat_setInt32(media_format.get(), AMEDIAFORMAT_KEY_HEIGHT,
                            aligned_size_->height());
      configured_size = *aligned_size_;
    }
  } while (!media_codec_);

  // Set MediaCodec callbacks and switch it to async mode
  AMediaCodecOnAsyncNotifyCallback callbacks{
      &NdkVideoEncodeAccelerator::OnAsyncInputAvailable,
      &NdkVideoEncodeAccelerator::OnAsyncOutputAvailable,
      &NdkVideoEncodeAccelerator::OnAsyncFormatChanged,
      &NdkVideoEncodeAccelerator::OnAsyncError,
  };
  media_status_t status =
      AMediaCodec_setAsyncNotifyCallback(media_codec_.get(), callbacks, this);
  if (status != AMEDIA_OK) {
    MEDIA_LOG(ERROR, log_) << "Can't set media codec callback. Error "
                           << status;
    return false;
  }

  status = AMediaCodec_start(media_codec_.get());
  if (status != AMEDIA_OK) {
    MEDIA_LOG(ERROR, log_) << "Can't start media codec. Error " << status;
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

  MEDIA_LOG(INFO, log_) << "Created MediaCodec (" << name.value()
                        << ") for config: " << config_.AsHumanReadableString();
  return true;
}

void NdkVideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                       bool force_keyframe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(media_codec_);
  VideoEncoder::PendingEncode encode;
  encode.frame = std::move(frame);
  encode.key_frame = force_keyframe;
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
    uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MediaFormatPtr format(AMediaFormat_new());

  if (effective_framerate_ != framerate)
    AMediaFormat_setInt32(format.get(), AMEDIAFORMAT_KEY_FRAME_RATE, framerate);
  if (effective_bitrate_ != bitrate) {
    AMediaFormat_setInt32(format.get(), AMEDIACODEC_KEY_VIDEO_BITRATE,
                          bitrate.target_bps());
  }
  media_status_t status =
      AMediaCodec_setParameters(media_codec_.get(), format.get());

  if (status != AMEDIA_OK) {
    NotifyMediaCodecError("Failed to change bitrate and framerate", status);
    return;
  }
  effective_framerate_ = framerate;
  effective_bitrate_ = bitrate;
}

void NdkVideoEncodeAccelerator::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_ptr_factory_.reset();
  callback_weak_factory_.InvalidateWeakPtrs();
  if (media_codec_) {
    AMediaCodec_stop(media_codec_.get());

    // Internally this calls AMediaFormat_delete(), and before exiting
    // AMediaFormat_delete() drains all calls on the internal thread that
    // calls OnAsyncXXXXX() functions. (Even though this fact is not documented)
    // It means by the time we actually destruct `this`, no OnAsyncXXXXX()
    // functions will use it via saved `userdata` pointers.
    media_codec_.reset();
  }
  delete this;
}

bool NdkVideoEncodeAccelerator::SetInputBufferLayout(
    const gfx::Size& configured_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(media_codec_);
  DCHECK(!configured_size.IsEmpty());

  MediaFormatPtr input_format(AMediaCodec_getInputFormat(media_codec_.get()));
  if (!input_format)
    return false;

  // Non 16x16 aligned resolutions don't work well with MediaCodec
  // unfortunately, see https://crbug.com/1084702 for details. It seems they
  // only work when stride/y_plane_height information is provided.
  const auto aligned_size =
      gfx::Size(base::bits::AlignDown(configured_size.width(), 16),
                base::bits::AlignDown(configured_size.height(), 16));

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

  if (!require_aligned_resolution)
    return true;

  // If the size is already aligned, nothing to do.
  if (config_.input_visible_size == aligned_size)
    return true;

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

  if (media_codec_input_buffers_.empty() || pending_frames_.empty())
    return;

  scoped_refptr<VideoFrame> frame = std::move(pending_frames_.front().frame);
  bool key_frame = pending_frames_.front().key_frame;
  pending_frames_.pop_front();

  if (key_frame) {
    // Signal to the media codec that it needs to include a key frame
    MediaFormatPtr format(AMediaFormat_new());
    AMediaFormat_setInt32(format.get(), AMEDIACODEC_KEY_REQUEST_SYNC_FRAME, 0);
    media_status_t status =
        AMediaCodec_setParameters(media_codec_.get(), format.get());

    if (status != AMEDIA_OK) {
      NotifyMediaCodecError("Failed to request a keyframe", status);
      return;
    }
  }

  size_t buffer_idx = media_codec_input_buffers_.front();
  media_codec_input_buffers_.pop_front();

  size_t capacity = 0;
  uint8_t* buffer_ptr =
      AMediaCodec_getInputBuffer(media_codec_.get(), buffer_idx, &capacity);
  if (!buffer_ptr) {
    NotifyError("Can't obtain input buffer from media codec.",
                kPlatformFailureError);
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
    auto message = base::StringPrintf(
        "Frame doesn't fit into the input buffer. queued_size: %zu capacity: "
        "%zu",
        queued_size, capacity);
    NotifyError(message, kPlatformFailureError);
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
  }

  if (!converted) {
    NotifyError("Failed to copy pixels to input buffer.",
                kPlatformFailureError);
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
      media_codec_.get(), buffer_idx, /*offset=*/0, queued_size,
      generate_timestamp.InMicroseconds(), flags);
  if (status != AMEDIA_OK) {
    NotifyMediaCodecError("Failed to queueInputBuffer", status);
    return;
  }
}

void NdkVideoEncodeAccelerator::NotifyMediaCodecError(std::string message,
                                                      media_status_t error) {
  auto message_and_code = base::StringPrintf("%s MediaCodec error code: %d",
                                             message.c_str(), error);
  NotifyError(message_and_code, kPlatformFailureError);
}

void NdkVideoEncodeAccelerator::NotifyError(base::StringPiece message,
                                            Error code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MEDIA_LOG(ERROR, log_) << message;
  LOG(ERROR) << message;
  if (!error_occurred_) {
    client_ptr_factory_->GetWeakPtr()->NotifyError(code);
    error_occurred_ = true;
  }
}

void NdkVideoEncodeAccelerator::OnAsyncInputAvailable(AMediaCodec* codec,
                                                      void* userdata,
                                                      int32_t index) {
  auto* self = reinterpret_cast<NdkVideoEncodeAccelerator*>(userdata);
  DCHECK(self);

  self->task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NdkVideoEncodeAccelerator::OnInputAvailable,
                                self->callback_weak_ptr_, index));
}

void NdkVideoEncodeAccelerator::OnInputAvailable(int32_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  media_codec_input_buffers_.push_back(index);
  FeedInput();
}

void NdkVideoEncodeAccelerator::OnAsyncOutputAvailable(
    AMediaCodec* codec,
    void* userdata,
    int32_t index,
    AMediaCodecBufferInfo* bufferInfo) {
  auto* self = reinterpret_cast<NdkVideoEncodeAccelerator*>(userdata);
  DCHECK(self);

  self->task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NdkVideoEncodeAccelerator::OnOutputAvailable,
                                self->callback_weak_ptr_, index, *bufferInfo));
}

void NdkVideoEncodeAccelerator::OnOutputAvailable(int32_t index,
                                                  AMediaCodecBufferInfo info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  media_codec_output_buffers_.push_back({index, info});
  DrainOutput();
}

void NdkVideoEncodeAccelerator::OnAsyncError(AMediaCodec* codec,
                                             void* userdata,
                                             media_status_t error,
                                             int32_t actionCode,
                                             const char* detail) {
  auto* self = reinterpret_cast<NdkVideoEncodeAccelerator*>(userdata);
  DCHECK(self);

  self->task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NdkVideoEncodeAccelerator::NotifyMediaCodecError,
                     self->callback_weak_ptr_, "Media codec async error.",
                     error));
}

bool NdkVideoEncodeAccelerator::DrainConfig() {
  if (media_codec_output_buffers_.empty())
    return false;

  MCOutput output_buffer = media_codec_output_buffers_.front();
  AMediaCodecBufferInfo& mc_buffer_info = output_buffer.info;
  const size_t mc_buffer_size = static_cast<size_t>(mc_buffer_info.size);

  // Check that the first buffer in the queue contains config data.
  if ((mc_buffer_info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) == 0)
    return false;

  media_codec_output_buffers_.pop_front();
  size_t capacity = 0;
  uint8_t* buf_data = AMediaCodec_getOutputBuffer(
      media_codec_.get(), output_buffer.buffer_index, &capacity);

  if (!buf_data) {
    NotifyError("Can't obtain output buffer from media codec.",
                kPlatformFailureError);
    return false;
  }

  if (mc_buffer_info.offset + mc_buffer_size > capacity) {
    auto message = base::StringPrintf(
        "Invalid output buffer layout."
        "offset: %d size: %zu capacity: %zu",
        mc_buffer_info.offset, mc_buffer_size, capacity);
    NotifyError(message, kPlatformFailureError);
    return false;
  }

  config_data_.resize(mc_buffer_size);
  memcpy(config_data_.data(), buf_data + mc_buffer_info.offset, mc_buffer_size);
  AMediaCodec_releaseOutputBuffer(media_codec_.get(),
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

  if (media_codec_output_buffers_.empty() ||
      available_bitstream_buffers_.empty()) {
    return;
  }

  MCOutput output_buffer = media_codec_output_buffers_.front();
  AMediaCodecBufferInfo& mc_buffer_info = output_buffer.info;
  const size_t mc_buffer_size = static_cast<size_t>(mc_buffer_info.size);
  media_codec_output_buffers_.pop_front();

  if ((mc_buffer_info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0)
    return;
  const bool key_frame = (mc_buffer_info.flags & BUFFER_FLAG_KEY_FRAME) != 0;

  BitstreamBuffer bitstream_buffer =
      std::move(available_bitstream_buffers_.back());
  available_bitstream_buffers_.pop_back();

  const size_t config_size = key_frame ? config_data_.size() : 0u;
  if (config_size + mc_buffer_size > bitstream_buffer.size()) {
    auto message = base::StringPrintf(
        "Encoded output is too large. mc output size: %zu"
        " bitstream buffer size: %zu"
        " config size: %zu",
        mc_buffer_size, bitstream_buffer.size(), config_size);
    NotifyError(message, kPlatformFailureError);
    return;
  }

  size_t capacity = 0;
  uint8_t* buf_data = AMediaCodec_getOutputBuffer(
      media_codec_.get(), output_buffer.buffer_index, &capacity);

  if (!buf_data) {
    NotifyError("Can't obtain output buffer from media codec.",
                kPlatformFailureError);
    return;
  }

  if (mc_buffer_info.offset + mc_buffer_size > capacity) {
    auto message = base::StringPrintf(
        "Invalid output buffer layout."
        "offset: %d size: %zu capacity: %zu",
        mc_buffer_info.offset, mc_buffer_size, capacity);
    NotifyError(message, kPlatformFailureError);
    return;
  }

  base::UnsafeSharedMemoryRegion region = bitstream_buffer.TakeRegion();
  auto mapping =
      region.MapAt(bitstream_buffer.offset(), bitstream_buffer.size());
  if (!mapping.IsValid()) {
    NotifyError("Failed to map SHM", kPlatformFailureError);
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
  if (aligned_size_)
    metadata.encoded_size = aligned_size_;

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncodeAccelerator::Client::BitstreamBufferReady,
                     client_ptr_factory_->GetWeakPtr(), bitstream_buffer.id(),
                     metadata));
  AMediaCodec_releaseOutputBuffer(media_codec_.get(),
                                  output_buffer.buffer_index, false);
}

}  // namespace media
