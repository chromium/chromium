// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/android/android_video_encode_accelerator.h"

#include <memory>
#include <set>
#include <tuple>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/task/sequenced_task_runner.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace media {

// Limit default max video codec size for Android to avoid
// HW codec initialization failure for resolution higher than 720p.
// Default values are from Libjingle "jsepsessiondescription.cc".
const int kMaxEncodeFrameWidth = 1280;
const int kMaxEncodeFrameHeight = 720;
const int kMaxFramerateNumerator = 30;
const int kMaxFramerateDenominator = 1;

enum PixelFormat {
  // Subset of MediaCodecInfo.CodecCapabilities.
  COLOR_FORMAT_YUV420_PLANAR = 19,
  COLOR_FORMAT_YUV420_SEMIPLANAR = 21,
};

// Because MediaCodec is thread-hostile (must be poked on a single thread) and
// has no callback mechanism (b/11990118), we must drive it by polling for
// complete frames (and available input buffers, when the codec is fully
// saturated).  This function defines the polling delay.  The value used is an
// arbitrary choice that trades off CPU utilization (spinning) against latency.
// Mirrors android_video_decode_accelerator.cc::DecodePollDelay().
static inline const base::TimeDelta EncodePollDelay() {
  // An alternative to this polling scheme could be to dedicate a new thread
  // (instead of using the ChildThread) to run the MediaCodec, and make that
  // thread use the timeout-based flavor of MediaCodec's dequeue methods when it
  // believes the codec should complete "soon" (e.g. waiting for an input
  // buffer, or waiting for a picture when it knows enough complete input
  // pictures have been fed to saturate any internal buffering).  This is
  // speculative and it's unclear that this would be a win (nor that there's a
  // reasonably device-agnostic way to fill in the "believes" above).
  return base::Milliseconds(10);
}

static inline const base::TimeDelta NoWaitTimeOut() {
  return base::Microseconds(0);
}

static bool GetSupportedColorFormatForMime(const std::string& mime,
                                           PixelFormat* pixel_format) {
  if (mime.empty())
    return false;

  std::set<int> formats = MediaCodecUtil::GetEncoderColorFormats(mime);
  if (formats.count(COLOR_FORMAT_YUV420_SEMIPLANAR) > 0)
    *pixel_format = COLOR_FORMAT_YUV420_SEMIPLANAR;
  else if (formats.count(COLOR_FORMAT_YUV420_PLANAR) > 0)
    *pixel_format = COLOR_FORMAT_YUV420_PLANAR;
  else
    return false;

  return true;
}

AndroidVideoEncodeAccelerator::AndroidVideoEncodeAccelerator() = default;

AndroidVideoEncodeAccelerator::~AndroidVideoEncodeAccelerator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

VideoEncodeAccelerator::SupportedProfiles
AndroidVideoEncodeAccelerator::GetSupportedProfiles() {
  SupportedProfiles profiles;

  const struct {
    const VideoCodec codec;
    const VideoCodecProfile profile;
  } kSupportedCodecs[] = {{VideoCodec::kVP8, VP8PROFILE_ANY},
                          {VideoCodec::kH264, H264PROFILE_BASELINE}};

  for (const auto& supported_codec : kSupportedCodecs) {
    if (supported_codec.codec == VideoCodec::kVP8 &&
        !MediaCodecUtil::IsVp8EncoderAvailable()) {
      continue;
    }

    if (supported_codec.codec == VideoCodec::kH264 &&
        !MediaCodecUtil::IsH264EncoderAvailable()) {
      continue;
    }

    bool is_software_codec = MediaCodecUtil::IsKnownUnaccelerated(
        supported_codec.codec, MediaCodecDirection::ENCODER);
    if (supported_codec.codec != VideoCodec::kH264 && is_software_codec) {
      continue;
    }

    SupportedProfile profile;
    profile.profile = supported_codec.profile;
    // It would be nice if MediaCodec exposes the maximum capabilities of
    // the encoder. Hard-code some reasonable defaults as workaround.
    profile.max_resolution.SetSize(kMaxEncodeFrameWidth, kMaxEncodeFrameHeight);
    profile.max_framerate_numerator = kMaxFramerateNumerator;
    profile.max_framerate_denominator = kMaxFramerateDenominator;
    profile.rate_control_modes = media::VideoEncodeAccelerator::kConstantMode;
    profile.is_software_codec = is_software_codec;
    profiles.push_back(profile);
  }
  return profiles;
}

bool AndroidVideoEncodeAccelerator::Initialize(
    const Config& config,
    Client* client,
    std::unique_ptr<MediaLog> media_log) {
  DVLOG(3) << __func__ << " " << config.AsHumanReadableString();
  DCHECK(!media_codec_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client);
  log_ = std::move(media_log);
  client_ptr_factory_ = std::make_unique<base::WeakPtrFactory<Client>>(client);

  if (config.input_format != PIXEL_FORMAT_I420) {
    MEDIA_LOG(ERROR, log_) << "Unexpected combo: " << config.input_format
                           << ", " << GetProfileName(config.output_profile);
    return false;
  }

  std::string mime_type;
  VideoCodec codec;
  // The client should be prepared to feed at least this many frames into the
  // encoder before being returned any output frames, since the encoder may
  // need to hold onto some subset of inputs as reference pictures.
  uint32_t frame_input_count;
  uint32_t i_frame_interval;
  if (config.output_profile == VP8PROFILE_ANY) {
    codec = VideoCodec::kVP8;
    mime_type = "video/x-vnd.on2.vp8";
    frame_input_count = 1;
    i_frame_interval = IFRAME_INTERVAL_VPX;
  } else if (config.output_profile == H264PROFILE_BASELINE ||
             config.output_profile == H264PROFILE_MAIN) {
    codec = VideoCodec::kH264;
    mime_type = "video/avc";
    frame_input_count = 30;
    i_frame_interval = IFRAME_INTERVAL_H264;
  } else {
    return false;
  }

  frame_size_ = config.input_visible_size;
  last_set_bitrate_ = config.bitrate.target_bps();

  // Only consider using MediaCodec if it's likely backed by hardware or we
  // don't have a software encoder bundled.
  auto required_encoder_type = config.required_encoder_type;
  if (codec != VideoCodec::kH264) {
    required_encoder_type = Config::EncoderType::kHardware;
  }

  const bool is_software_codec =
      MediaCodecUtil::IsKnownUnaccelerated(codec, MediaCodecDirection::ENCODER);
  if (required_encoder_type == Config::EncoderType::kHardware &&
      is_software_codec) {
    MEDIA_LOG(ERROR, log_) << "No hardware encoding support for "
                           << GetCodecName(codec);
    return false;
  }

  // We need to add the ability to select by name if we want to support software
  // encoding like the NDK encoder does.
  if (required_encoder_type == Config::EncoderType::kSoftware &&
      !is_software_codec) {
    MEDIA_LOG(ERROR, log_) << "No software encoding support for "
                           << GetCodecName(codec);
    return false;
  }

  PixelFormat pixel_format = COLOR_FORMAT_YUV420_SEMIPLANAR;
  if (!GetSupportedColorFormatForMime(mime_type, &pixel_format)) {
    MEDIA_LOG(ERROR, log_) << "No color format support.";
    return false;
  }
  media_codec_ = MediaCodecBridgeImpl::CreateVideoEncoder(
      codec, config.input_visible_size, config.bitrate.target_bps(),
      INITIAL_FRAMERATE, i_frame_interval, pixel_format);

  if (!media_codec_) {
    MEDIA_LOG(ERROR, log_) << "Failed to create/start the codec: "
                           << config.input_visible_size.ToString();
    return false;
  }

  if (!SetInputBufferLayout()) {
    MEDIA_LOG(ERROR, log_) << "Can't get input buffer layout from MediaCodec";
    return false;
  }

  // Conservative upper bound for output buffer size: decoded size + 2KB.
  const size_t output_buffer_capacity =
      VideoFrame::AllocationSize(config.input_format,
                                 config.input_visible_size) +
      2048;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncodeAccelerator::Client::RequireBitstreamBuffers,
                     client_ptr_factory_->GetWeakPtr(), frame_input_count,
                     config.input_visible_size, output_buffer_capacity));
  return true;
}

void AndroidVideoEncodeAccelerator::MaybeStartIOTimer() {
  if (!io_timer_.IsRunning() &&
      (num_buffers_at_codec_ > 0 || !pending_frames_.empty())) {
    io_timer_.Start(FROM_HERE, EncodePollDelay(), this,
                    &AndroidVideoEncodeAccelerator::DoIOTask);
  }
}

void AndroidVideoEncodeAccelerator::MaybeStopIOTimer() {
  if (io_timer_.IsRunning() &&
      (num_buffers_at_codec_ == 0 && pending_frames_.empty())) {
    io_timer_.Stop();
  }
}

void AndroidVideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                           bool force_keyframe) {
  DVLOG(3) << __PRETTY_FUNCTION__ << ": " << force_keyframe;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (frame->format() != PIXEL_FORMAT_I420) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kUnsupportedFrameFormat,
         "Unexpected format: " + VideoPixelFormatToString(frame->format())});
    return;
  }
  if (frame->visible_rect().size() != frame_size_) {
    NotifyErrorStatus({EncoderStatus::Codes::kInvalidInputFrame,
                       "Unexpected resolution: got " +
                           frame->visible_rect().size().ToString() +
                           ", expected " + frame_size_.ToString()});
    return;
  }
  pending_frames_.emplace(
      std::make_tuple(std::move(frame), force_keyframe, base::Time::Now()));
  DoIOTask();
}

void AndroidVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DVLOG(3) << __PRETTY_FUNCTION__ << ": bitstream_buffer_id=" << buffer.id();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  available_bitstream_buffers_.push_back(std::move(buffer));
  DoIOTask();
}

void AndroidVideoEncodeAccelerator::RequestEncodingParametersChange(
    const Bitrate& bitrate,
    uint32_t framerate,
    const std::optional<gfx::Size>& size) {
  // If this is changed to use variable bitrate encoding, change the mode check
  // to check that the mode matches the current mode.
  if (bitrate.mode() != Bitrate::Mode::kConstant) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kEncoderUnsupportedConfig,
         "Unexpected bitrate mode: " +
             base::NumberToString(static_cast<int>(bitrate.mode()))});
    return;
  }
  if (size.has_value()) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                       "Update output frame size is not supported"});
    return;
  }
  DVLOG(3) << __PRETTY_FUNCTION__ << ": bitrate: " << bitrate.ToString()
           << ", framerate: " << framerate;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (bitrate.target_bps() != last_set_bitrate_) {
    last_set_bitrate_ = bitrate.target_bps();
    media_codec_->SetVideoBitrate(bitrate.target_bps(), framerate);
  }
  // Note: Android's MediaCodec doesn't allow mid-stream adjustments to
  // framerate, so we ignore that here.  This is OK because Android only uses
  // the framerate value from MediaFormat during configure() as a proxy for
  // bitrate, and we set that explicitly.
}

void AndroidVideoEncodeAccelerator::Destroy() {
  DVLOG(3) << __PRETTY_FUNCTION__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_ptr_factory_.reset();
  if (media_codec_) {
    if (io_timer_.IsRunning())
      io_timer_.Stop();
    media_codec_->Stop();
  }
  delete this;
}

void AndroidVideoEncodeAccelerator::DoIOTask() {
  QueueInput();
  DequeueOutput();
  MaybeStartIOTimer();
  MaybeStopIOTimer();
}

void AndroidVideoEncodeAccelerator::QueueInput() {
  if (error_occurred_ || pending_frames_.empty())
    return;

  int input_buf_index = 0;
  MediaCodecResult result =
      media_codec_->DequeueInputBuffer(NoWaitTimeOut(), &input_buf_index);
  if (!result.is_ok()) {
    DCHECK(result.code() == MediaCodecResult::Codes::kTryAgainLater ||
           result.code() == MediaCodecResult::Codes::kError);
    if (result.code() == MediaCodecResult::Codes::kError) {
      NotifyErrorStatus({EncoderStatus::Codes::kEncoderHardwareDriverError,
                         "MediaCodec error in DequeueInputBuffer",
                         std::move(result)});
      return;
    }
  }

  const PendingFrames::value_type& input = pending_frames_.front();
  bool is_key_frame = std::get<1>(input);
  if (is_key_frame) {
    // Ideally MediaCodec would honor BUFFER_FLAG_SYNC_FRAME so we could
    // indicate this in the QueueInputBuffer() call below and guarantee _this_
    // frame be encoded as a key frame, but sadly that flag is ignored.
    // Instead, we request a key frame "soon".
    media_codec_->RequestKeyFrameSoon();
  }
  scoped_refptr<VideoFrame> frame = std::get<0>(input);

  uint8_t* buffer = nullptr;
  size_t capacity = 0;
  result = media_codec_->GetInputBuffer(input_buf_index, &buffer, &capacity);
  if (!result.is_ok()) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderHardwareDriverError,
                       "MediaCodec error in GetInputBuffer",
                       std::move(result)});
    return;
  }
  const auto visible_size =
      aligned_size_.value_or(frame->visible_rect().size());

  uint8_t* dst_y = buffer;
  const int dst_stride_y = input_buffer_stride_;
  const int uv_plane_offset =
      input_buffer_yplane_height_ * input_buffer_stride_;
  uint8_t* dst_uv = buffer + uv_plane_offset;
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
                       "Frame doesn't fit into the input buffer. queue_size: " +
                           base::NumberToString(queued_size) +
                           "capacity: " + base::NumberToString(capacity)});
    return;
  }

  // Why NV12?  Because COLOR_FORMAT_YUV420_SEMIPLANAR.  See comment at other
  // mention of that constant.
  bool converted = !libyuv::I420ToNV12(
      frame->visible_data(VideoFrame::Plane::kY),
      frame->stride(VideoFrame::Plane::kY),
      frame->visible_data(VideoFrame::Plane::kU),
      frame->stride(VideoFrame::Plane::kU),
      frame->visible_data(VideoFrame::Plane::kV),
      frame->stride(VideoFrame::Plane::kV), dst_y, dst_stride_y, dst_uv,
      dst_stride_uv, visible_size.width(), visible_size.height());
  if (!converted) {
    NotifyErrorStatus({EncoderStatus::Codes::kFormatConversionError,
                       "Failed to I420ToNV12()"});
    return;
  }

  // MediaCodec encoder assumes the presentation timestamps to be monotonically
  // increasing at initialized framerate. But in Chromium, the video capture
  // may be paused for a while or drop some frames, so the timestamp in input
  // frames won't be continuous. Here we cache the timestamps of input frames,
  // mapping to the generated |presentation_timestamp_|, and will read them out
  // after encoding. Then encoder can work happily always and we can preserve
  // the timestamps in captured frames for other purpose.
  presentation_timestamp_ += base::Microseconds(
      base::Time::kMicrosecondsPerSecond / INITIAL_FRAMERATE);
  DCHECK(frame_timestamp_map_.find(presentation_timestamp_) ==
         frame_timestamp_map_.end());
  frame_timestamp_map_[presentation_timestamp_] = frame->timestamp();

  result = media_codec_->QueueInputBuffer(input_buf_index, nullptr, queued_size,
                                          presentation_timestamp_);
  UMA_HISTOGRAM_TIMES("Media.AVDA.InputQueueTime",
                      base::Time::Now() - std::get<2>(input));
  if (!result.is_ok()) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderHardwareDriverError,
                       "Failed to QueueInputBuffer", std::move(result)});
    return;
  }
  ++num_buffers_at_codec_;
  DCHECK(static_cast<int32_t>(frame_timestamp_map_.size()) ==
         num_buffers_at_codec_);

  pending_frames_.pop();
}

void AndroidVideoEncodeAccelerator::DequeueOutput() {
  if (error_occurred_ || available_bitstream_buffers_.empty() ||
      num_buffers_at_codec_ == 0) {
    return;
  }

  int32_t buf_index = 0;
  size_t offset = 0;
  size_t size = 0;
  bool key_frame = false;

  base::TimeDelta presentaion_timestamp;
  MediaCodecResult result = media_codec_->DequeueOutputBuffer(
      NoWaitTimeOut(), &buf_index, &offset, &size, &presentaion_timestamp,
      nullptr, &key_frame);
  switch (result.code()) {
    case MediaCodecResult::Codes::kTryAgainLater:
      return;

    case MediaCodecResult::Codes::kError:
      NotifyErrorStatus({EncoderStatus::Codes::kEncoderFailedEncode,
                         "MediaCodec error in DequeueOutputBuffer",
                         std::move(result)});
      // Unreachable because of previous statement, but included for clarity.
      return;

    case MediaCodecResult::Codes::kOutputFormatChanged:
      return;

    case MediaCodecResult::Codes::kOutputBuffersChanged:
      return;

    case MediaCodecResult::Codes::kOk:
      DCHECK_GE(buf_index, 0);
      break;

    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  const auto it = frame_timestamp_map_.find(presentaion_timestamp);
  CHECK(it != frame_timestamp_map_.end(), base::NotFatalUntil::M130);
  const base::TimeDelta frame_timestamp = it->second;
  frame_timestamp_map_.erase(it);

  BitstreamBuffer bitstream_buffer =
      std::move(available_bitstream_buffers_.back());
  available_bitstream_buffers_.pop_back();
  base::UnsafeSharedMemoryRegion region = bitstream_buffer.TakeRegion();
  auto mapping =
      region.MapAt(bitstream_buffer.offset(), bitstream_buffer.size());
  if (!mapping.IsValid()) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kSystemAPICallError, "Failed to map SHM"});
    return;
  }
  if (size > bitstream_buffer.size()) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kEncoderFailedEncode,
         "Encoded buffer too large: " + base::NumberToString(size) + ">" +
             base::NumberToString(bitstream_buffer.size())});
    return;
  }
  result = media_codec_->CopyFromOutputBuffer(buf_index, offset,
                                              mapping.memory(), size);
  if (!result.is_ok()) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderFailedEncode,
                       "MediaCodec error in CopyFromOutputBuffer",
                       std::move(result)});
    return;
  }
  media_codec_->ReleaseOutputBuffer(buf_index, false);
  --num_buffers_at_codec_;

  auto metadata = BitstreamBufferMetadata(size, key_frame, frame_timestamp);
  if (aligned_size_) {
    metadata.encoded_size = *aligned_size_;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncodeAccelerator::Client::BitstreamBufferReady,
                     client_ptr_factory_->GetWeakPtr(), bitstream_buffer.id(),
                     metadata));
}

bool AndroidVideoEncodeAccelerator::SetInputBufferLayout() {
  // Non 16x16 aligned resolutions don't work well with MediaCodec
  // unfortunately, see https://crbug.com/1084702 for details. It seems they
  // only work when stride/y_plane_height information is provided.
  gfx::Size encoded_size;
  MediaCodecResult result = media_codec_->GetInputFormat(
      &input_buffer_stride_, &input_buffer_yplane_height_, &encoded_size);
  if (!result.is_ok()) {
    return false;
  }

  // If the size is the same, nothing to do.
  if (encoded_size == frame_size_) {
    return true;
  }

  aligned_size_ = encoded_size;

  // Give the client a chance to handle realignment itself.
  DCHECK_EQ(aligned_size_->width() % 16, 0);
  DCHECK_EQ(aligned_size_->height() % 16, 0);
  VideoEncoderInfo encoder_info;
  encoder_info.requested_resolution_alignment = 16;
  encoder_info.apply_alignment_to_all_simulcast_layers = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncodeAccelerator::Client::NotifyEncoderInfoChange,
                     client_ptr_factory_->GetWeakPtr(), encoder_info));

  MEDIA_LOG(INFO, log_)
      << "MediaCodec encoder requires 16x16 aligned resolution. Cropping to "
      << aligned_size_->ToString();

  return true;
}

void AndroidVideoEncodeAccelerator::NotifyErrorStatus(EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!status.is_ok());
  CHECK(log_);
  MEDIA_LOG(ERROR, log_) << status.message();
  LOG(ERROR) << "Call NotifyErrorStatus(): code="
             << static_cast<int>(status.code())
             << ", message=" << status.message();
  if (!error_occurred_) {
    client_ptr_factory_->GetWeakPtr()->NotifyErrorStatus(status);
    error_occurred_ = true;
  }
}
}  // namespace media
