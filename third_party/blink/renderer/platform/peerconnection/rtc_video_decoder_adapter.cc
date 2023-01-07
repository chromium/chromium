// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_adapter.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/overlay_info.h"
#include "media/base/video_types.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/video_decode_accelerator.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_fallback_recorder.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_utils.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/video/video_frame.h"
#include "third_party/webrtc/api/video_codecs/vp9_profile.h"
#include "third_party/webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "third_party/webrtc/rtc_base/ref_count.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"
#include "ui/gfx/color_space.h"

namespace WTF {

template <>
struct CrossThreadCopier<media::VideoDecoderConfig>
    : public CrossThreadCopierPassThrough<media::VideoDecoderConfig> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

namespace {

// Any reasonable size, will be overridden by the decoder anyway.
constexpr gfx::Size kDefaultSize(640, 480);

// Maximum number of buffers that we will queue in |pending_buffers_|.
constexpr int32_t kMaxPendingBuffers = 8;

// Maximum number of timestamps that will be maintained in |decode_timestamps_|.
// Really only needs to be a bit larger than the maximum reorder distance (which
// is presumably 0 for WebRTC), but being larger doesn't hurt much.
constexpr int32_t kMaxDecodeHistory = 32;

// Maximum number of consecutive frames that can fail to decode before
// requesting fallback to software decode.
constexpr int32_t kMaxConsecutiveErrors = 5;

// Number of RTCVideoDecoder instances right now that have started decoding.
class DecoderCounter {
 public:
  int Count() { return count_.load(); }

  void IncrementCount() {
    int c = ++count_;
    DCHECK_GT(c, 0);
  }

  void DecrementCount() {
    int c = --count_;
    DCHECK_GE(c, 0);
  }

 private:
  std::atomic_int count_{0};
};

DecoderCounter* GetDecoderCounter() {
  static DecoderCounter s_counter;
  // Note that this will init only in the first call in the ctor, so it's still
  // single threaded.
  return &s_counter;
}

void FinishWait(base::WaitableEvent* waiter, bool* result_out, bool result) {
  DVLOG(3) << __func__ << "(" << result << ")";
  *result_out = result;
  waiter->Signal();
}

void OnRequestOverlayInfo(bool decoder_requires_restart_for_overlay,
                          media::ProvideOverlayInfoCB overlay_info_cb) {
  // Android overlays are not supported.
  if (overlay_info_cb)
    std::move(overlay_info_cb).Run(media::OverlayInfo());
}

void RecordInitializationLatency(base::TimeDelta latency) {
  base::UmaHistogramTimes("Media.RTCVideoDecoderInitializationLatencyMs",
                          latency);
}

void RecordReinitializationLatency(base::TimeDelta latency) {
  base::UmaHistogramTimes("Media.RTCVideoDecoderReinitializationLatencyMs",
                          latency);
}

bool HasSoftwareFallback(media::VideoCodec video_codec) {
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  return video_codec != media::VideoCodec::kH264;
#else
  return true;
#endif
}

scoped_refptr<media::DecoderBuffer> ConvertToDecoderBuffer(
    const webrtc::EncodedImage& input_image) {
  std::vector<uint32_t> spatial_layer_frame_size;
  const int max_sl_index = input_image.SpatialIndex().value_or(0);
  for (int i = 0; i <= max_sl_index; i++) {
    const absl::optional<size_t>& frame_size =
        input_image.SpatialLayerFrameSize(i);
    if (!frame_size)
      continue;
    spatial_layer_frame_size.push_back(
        base::checked_cast<uint32_t>(*frame_size));
  }

  // TODO(sandersd): What is |render_time_ms|?
  scoped_refptr<media::DecoderBuffer> buffer;
  if (spatial_layer_frame_size.size() > 1) {
    const uint8_t* side_data =
        reinterpret_cast<const uint8_t*>(spatial_layer_frame_size.data());
    size_t side_data_size =
        spatial_layer_frame_size.size() * sizeof(uint32_t) / sizeof(uint8_t);
    buffer = media::DecoderBuffer::CopyFrom(
        input_image.data(), input_image.size(), side_data, side_data_size);
  } else {
    buffer =
        media::DecoderBuffer::CopyFrom(input_image.data(), input_image.size());
  }
  buffer->set_timestamp(base::Microseconds(input_image.Timestamp()));
  buffer->set_is_key_frame(input_image._frameType ==
                           webrtc::VideoFrameType::kVideoFrameKey);
  return buffer;
}
}  // namespace

void RTCVideoDecoderAdapter::InitializeOnMediaThread(
    const media::VideoDecoderConfig& config,
    CrossThreadOnceFunction<void(bool)> init_cb,
    base::TimeTicks start_time,
    std::string* decoder_name) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);

  // On ReinitializeSync() calls, |video_decoder_| may already be set.
  if (!video_decoder_) {
    // TODO(sandersd): Plumb a real log sink here so that we can contribute to
    // the media-internals UI. The current log just discards all messages.
    media_log_ = std::make_unique<media::NullMediaLog>();
    start_time_ = start_time;
    video_decoder_ = gpu_factories_->CreateVideoDecoder(
        media_log_.get(), WTF::BindRepeating(&OnRequestOverlayInfo));

    if (!video_decoder_) {
      PostCrossThreadTask(*media_task_runner_.get(), FROM_HERE,
                          CrossThreadBindOnce(std::move(init_cb), false));
      return;
    }
  }

  media::VideoDecoder::OutputCB output_cb = ConvertToBaseRepeatingCallback(
      CrossThreadBindRepeating(&RTCVideoDecoderAdapter::OnOutput, weak_this_));
  video_decoder_->Initialize(
      config, /*low_delay=*/false,
      /*cdm_context=*/nullptr,
      base::BindOnce(
          [](base::OnceCallback<void(bool)> cb, std::string* decoder_name,
             media::VideoDecoder* video_decoder, media::DecoderStatus status) {
            *decoder_name =
                media::GetDecoderName(video_decoder->GetDecoderType());
            std::move(cb).Run(status.is_ok());
          },
          ConvertToBaseOnceCallback(std::move(init_cb)),
          CrossThreadUnretained(decoder_name),
          CrossThreadUnretained(video_decoder_.get())),
      output_cb, base::DoNothing());
}

absl::optional<RTCVideoDecoderFallbackReason>
RTCVideoDecoderAdapter::FallbackOrRegisterConcurrentInstanceOnce(
    media::VideoCodec codec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoding_sequence_checker_);

  // If this is the first decode, then increment the count of working decoders.
  if (!have_started_decoding_) {
    have_started_decoding_ = true;
    GetDecoderCounter()->IncrementCount();
  }

  base::AutoLock auto_lock(lock_);
  // Don't allow hardware decode for small videos if there are too many
  // decoder instances.  This includes the case where our resolution drops while
  // too many decoders exist.
  if (HasSoftwareFallback(codec) && current_resolution_ < kMinResolution &&
      GetDecoderCounter()->Count() > kMaxDecoderInstances) {
    // Decrement the count and clear the flag, so that other decoders don't
    // fall back also.
    have_started_decoding_ = false;
    GetDecoderCounter()->DecrementCount();
    // TODO(b/246460597): Add the fallback reason about too many concurrent
    // instances.
    return RTCVideoDecoderFallbackReason::kPreviousErrorOnDecode;
  }

  return absl::nullopt;
}

absl::optional<RTCVideoDecoderFallbackReason>
RTCVideoDecoderAdapter::NeedSoftwareFallback(
    const media::VideoCodec codec,
    const media::DecoderBuffer& buffer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoding_sequence_checker_);
  // Fall back to software decoding if there's no support for VP9 spatial
  // layers. See https://crbug.com/webrtc/9304.
  const bool is_spatial_layer_buffer = buffer.side_data_size() > 0;
  if (codec == media::VideoCodec::kVP9 && is_spatial_layer_buffer &&
      !Vp9HwSupportForSpatialLayers()) {
    // D3D11 supports decoding the VP9 kSVC stream, but DXVA not. Currently just
    // a reasonably temporary measure. Once the DXVA supports decoding VP9 kSVC
    // stream, the boolean |need_fallback_to_software| should be removed, and if
    // the OS is windows but not win7, we will return true in
    // 'Vp9HwSupportForSpatialLayers' instead of false.
#if BUILDFLAG(IS_WIN)
    if (video_decoder_->GetDecoderType() == media::VideoDecoderType::kD3D11 &&
        base::FeatureList::IsEnabled(media::kD3D11Vp9kSVCHWDecoding)) {
      return absl::nullopt;
    }
#endif
    return RTCVideoDecoderFallbackReason::kSpatialLayers;
  }

  return absl::nullopt;
}

absl::variant<RTCVideoDecoderAdapter::DecodeResult,
              RTCVideoDecoderFallbackReason>
RTCVideoDecoderAdapter::EnqueueBuffer(
    scoped_refptr<media::DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoding_sequence_checker_);
  if (auto fallback_reason = NeedSoftwareFallback(config_.codec(), *buffer))
    return *fallback_reason;
  if (auto fallback_reason =
          FallbackOrRegisterConcurrentInstanceOnce(config_.codec())) {
    return *fallback_reason;
  }

  base::AutoLock auto_lock(lock_);
  if (status_ == Status::kNeedKeyFrame) {
    // We discarded previous frame because we have too many pending buffers (see
    // logic) below. Now we need to wait for the key frame and discard
    // everything else.
    if (!buffer->is_key_frame()) {
      DVLOG(2) << "Discard non-key frame";
      return DecodeResult::kErrorRequestKeyFrame;
    }
    DVLOG(2) << "Key frame received, resume decoding";
    // ok, we got key frame and can continue decoding
    ChangeStatus(Status::kOk);
  }

  // Queue for decoding.
  if (status_ == Status::kError)
    return RTCVideoDecoderFallbackReason::kPreviousErrorOnDecode;

  if (HasSoftwareFallback(config_.codec()) &&
      pending_buffers_.size() >= kMaxPendingBuffers) {
    // We are severely behind. Drop pending buffers and request a keyframe to
    // catch up as quickly as possible.
    DVLOG(2) << "Pending buffers overflow";
    pending_buffers_.clear();
    // Actually we just discarded a frame. We must wait for the key frame and
    // drop any other non-key frame.
    if (++consecutive_error_count_ > kMaxConsecutiveErrors) {
      decode_timestamps_.clear();
      ChangeStatus(Status::kError);
      return RTCVideoDecoderFallbackReason::kConsecutivePendingBufferOverflow;
    }
    ChangeStatus(Status::kNeedKeyFrame);
    return DecodeResult::kErrorRequestKeyFrame;
  }

  pending_buffers_.push_back(std::move(buffer));
  return DecodeResult::kOk;
}

void RTCVideoDecoderAdapter::DecodeOnMediaThread() {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);

  int max_decode_requests = video_decoder_->GetMaxDecodeRequests();
  while (outstanding_decode_requests_ < max_decode_requests) {
    scoped_refptr<media::DecoderBuffer> buffer;
    {
      base::AutoLock auto_lock(lock_);

      // Take the first pending buffer.
      if (pending_buffers_.empty())
        return;
      buffer = pending_buffers_.front();
      pending_buffers_.pop_front();

      // Record the timestamp.
      while (decode_timestamps_.size() >= kMaxDecodeHistory)
        decode_timestamps_.pop_front();
      decode_timestamps_.push_back(buffer->timestamp());
    }

    // Submit for decoding.
    outstanding_decode_requests_++;
    video_decoder_->Decode(
        std::move(buffer),
        WTF::BindRepeating(&RTCVideoDecoderAdapter::OnDecodeDone, weak_this_));
  }
}

void RTCVideoDecoderAdapter::FlushOnMediaThread(
    WTF::CrossThreadOnceClosure flush_success_cb,
    WTF::CrossThreadOnceClosure flush_fail_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);

  // Remove any pending tasks.
  {
    base::AutoLock auto_lock(lock_);
    pending_buffers_.clear();
  }

  // Send EOS frame for flush.
  video_decoder_->Decode(
      media::DecoderBuffer::CreateEOSBuffer(),
      WTF::BindOnce(
          [](WTF::CrossThreadOnceClosure flush_success,
             WTF::CrossThreadOnceClosure flush_fail,
             media::DecoderStatus status) {
            if (status.is_ok())
              std::move(flush_success).Run();
            else
              std::move(flush_fail).Run();
          },
          std::move(flush_success_cb), std::move(flush_fail_cb)));
}

void RTCVideoDecoderAdapter::OnDecodeDone(media::DecoderStatus status) {
  DVLOG(3) << __func__ << "(" << status.group() << ":"
           << static_cast<int>(status.code()) << ")";
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);

  outstanding_decode_requests_--;

  if (!status.is_ok() &&
      status.code() != media::DecoderStatus::Codes::kAborted) {
    DVLOG(2) << "Entering permanent error state";
    base::UmaHistogramSparse("Media.RTCVideoDecoderError",
                             static_cast<int>(status.code()));

    base::AutoLock auto_lock(lock_);
    ChangeStatus(Status::kError);
    pending_buffers_.clear();
    decode_timestamps_.clear();
    return;
  }

  DecodeOnMediaThread();
}

void RTCVideoDecoderAdapter::OnOutput(scoped_refptr<media::VideoFrame> frame) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);

  const base::TimeDelta timestamp = frame->timestamp();
  webrtc::VideoFrame rtc_frame =
      webrtc::VideoFrame::Builder()
          .set_video_frame_buffer(rtc::scoped_refptr<WebRtcVideoFrameAdapter>(
              new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
                  std::move(frame))))
          .set_timestamp_rtp(static_cast<uint32_t>(timestamp.InMicroseconds()))
          .set_timestamp_us(0)
          .set_rotation(webrtc::kVideoRotation_0)
          .build();

  base::AutoLock auto_lock(lock_);

  // Record time to first frame if we haven't yet.
  if (start_time_) {
    // We haven't recorded the first frame time yet, so do so now.
    base::UmaHistogramTimes("Media.RTCVideoDecoderFirstFrameLatencyMs",
                            base::TimeTicks::Now() - *start_time_);
    start_time_.reset();
  }

  // Update `current_resolution_`, in case it's changed.  This lets us fall back
  // to software, or avoid doing so, if we're over the decoder limit.
  current_resolution_ =
      static_cast<int32_t>(rtc_frame.width()) * rtc_frame.height();

  if (!base::Contains(decode_timestamps_, timestamp)) {
    DVLOG(2) << "Discarding frame with timestamp " << timestamp;
    return;
  }

  // Assumes that Decoded() can be safely called with the lock held, which
  // apparently it can be because RTCVideoDecoder does the same.
  DCHECK(decode_complete_callback_);
  decode_complete_callback_->Decoded(rtc_frame);
  consecutive_error_count_ = 0;
}

// static
std::unique_ptr<RTCVideoDecoderAdapter> RTCVideoDecoderAdapter::Create(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    const webrtc::SdpVideoFormat& format) {
  DVLOG(1) << __func__ << "(" << format.name << ")";

  const webrtc::VideoCodecType video_codec_type =
      webrtc::PayloadStringToCodecType(format.name);

  if (!Platform::Current()->IsWebRtcHWH264DecodingEnabled(video_codec_type))
    return nullptr;

  // Bail early for unknown codecs.
  if (WebRtcToMediaVideoCodec(video_codec_type) == media::VideoCodec::kUnknown)
    return nullptr;

  // Avoid the thread hop if the decoder is known not to support the config.
  // TODO(sandersd): Predict size from level.
  media::VideoDecoderConfig config(
      WebRtcToMediaVideoCodec(webrtc::PayloadStringToCodecType(format.name)),
      WebRtcVideoFormatToMediaVideoCodecProfile(format),
      media::VideoDecoderConfig::AlphaMode::kIsOpaque, media::VideoColorSpace(),
      media::kNoTransformation, kDefaultSize, gfx::Rect(kDefaultSize),
      kDefaultSize, media::EmptyExtraData(),
      media::EncryptionScheme::kUnencrypted);

  std::unique_ptr<RTCVideoDecoderAdapter> rtc_video_decoder_adapter;
  if (gpu_factories->IsDecoderConfigSupported(config) !=
      media::GpuVideoAcceleratorFactories::Supported::kFalse) {
    // Synchronously verify that the decoder can be initialized.
    rtc_video_decoder_adapter =
        base::WrapUnique(new RTCVideoDecoderAdapter(gpu_factories, config));
    if (rtc_video_decoder_adapter->InitializeSync(config)) {
      return rtc_video_decoder_adapter;
    }
    // Initialization failed - post delete task and try next supported
    // implementation, if any.
    gpu_factories->GetTaskRunner()->DeleteSoon(
        FROM_HERE, std::move(rtc_video_decoder_adapter));
  }

  // To mirror what RTCVideoDecoderStreamAdapter does a little more closely,
  // record an init failure here.  Otherwise, we only ever record successes.
  base::UmaHistogramBoolean("Media.RTCVideoDecoderInitDecodeSuccess", false);

  return nullptr;
}

RTCVideoDecoderAdapter::RTCVideoDecoderAdapter(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    const media::VideoDecoderConfig& config)
    : media_task_runner_(gpu_factories->GetTaskRunner()),
      gpu_factories_(gpu_factories),
      config_(config) {
  DVLOG(1) << __func__;
  DETACH_FROM_SEQUENCE(decoding_sequence_checker_);
  DETACH_FROM_SEQUENCE(media_sequence_checker_);
  decoder_info_.implementation_name = "ExternalDecoder (Unknown)";
  decoder_info_.is_hardware_accelerated = true;

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

RTCVideoDecoderAdapter::~RTCVideoDecoderAdapter() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (have_started_decoding_)
    GetDecoderCounter()->DecrementCount();
}

bool RTCVideoDecoderAdapter::InitializeSync(
    const media::VideoDecoderConfig& config) {
  TRACE_EVENT0("webrtc", "RTCVideoDecoderAdapter::InitializeSync");
  DVLOG(3) << __func__;
  // This function is called on a decoder thread.
  DCHECK(!media_task_runner_->RunsTasksInCurrentSequence());
  auto start_time = base::TimeTicks::Now();
  std::string decoder_name;

  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  bool result = false;
  base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto init_cb =
      CrossThreadBindOnce(&FinishWait, CrossThreadUnretained(&waiter),
                          CrossThreadUnretained(&result));
  if (PostCrossThreadTask(
          *media_task_runner_.get(), FROM_HERE,
          CrossThreadBindOnce(&RTCVideoDecoderAdapter::InitializeOnMediaThread,
                              CrossThreadUnretained(this), config,
                              std::move(init_cb), start_time,
                              CrossThreadUnretained(&decoder_name)))) {
    // TODO(crbug.com/1076817) Remove if a root cause is found.
    if (!waiter.TimedWait(base::Seconds(10))) {
      RecordInitializationLatency(base::TimeTicks::Now() - start_time);
      return false;
    }

    RecordInitializationLatency(base::TimeTicks::Now() - start_time);
  }

  decoder_info_.implementation_name = "ExternalDecoder (" + decoder_name + ")";
  return result;
}

bool RTCVideoDecoderAdapter::Configure(const Settings& settings) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoding_sequence_checker_);

  if (WebRtcToMediaVideoCodec(settings.codec_type()) != config_.codec())
    return false;

  base::AutoLock auto_lock(lock_);

  // Save the initial resolution so that we can fall back later, if needed.
  current_resolution_ =
      static_cast<int32_t>(settings.max_render_resolution().Width()) *
      settings.max_render_resolution().Height();

  const bool init_success = status_ != Status::kError;
  base::UmaHistogramBoolean("Media.RTCVideoDecoderInitDecodeSuccess",
                            init_success);

  if (init_success) {
    UMA_HISTOGRAM_ENUMERATION("Media.RTCVideoDecoderProfile", config_.profile(),
                              media::VIDEO_CODEC_PROFILE_MAX + 1);
  }
  return init_success;
}

int32_t RTCVideoDecoderAdapter::Decode(const webrtc::EncodedImage& input_image,
                                       bool missing_frames,
                                       int64_t render_time_ms) {
  auto result = DecodeInternal(input_image, missing_frames, render_time_ms);
  if (const auto* fallback_reason =
          absl::get_if<RTCVideoDecoderFallbackReason>(&result)) {
    RecordRTCVideoDecoderFallbackReason(config_.codec(), *fallback_reason);
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }

  return *absl::get_if<DecodeResult>(&result) == DecodeResult::kOk
             ? WEBRTC_VIDEO_CODEC_OK
             : WEBRTC_VIDEO_CODEC_ERROR;
}

absl::variant<RTCVideoDecoderAdapter::DecodeResult,
              RTCVideoDecoderFallbackReason>
RTCVideoDecoderAdapter::DecodeInternal(const webrtc::EncodedImage& input_image,
                                       bool missing_frames,
                                       int64_t render_time_ms) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoding_sequence_checker_);

  if (missing_frames) {
    DVLOG(2) << "Missing frames";
    // We probably can't handle broken frames. Request a key frame.
    return DecodeResult::kErrorRequestKeyFrame;
  }

  if (ShouldReinitializeForSettingHDRColorSpace(input_image)) {
    config_.set_color_space_info(
        blink::WebRtcToMediaVideoColorSpace(*input_image.ColorSpace()));
    if (!ReinitializeSync(config_))
      return RTCVideoDecoderFallbackReason::kReinitializationFailed;
    if (input_image._frameType != webrtc::VideoFrameType::kVideoFrameKey)
      return DecodeResult::kErrorRequestKeyFrame;
  }

  auto enqueue_res = EnqueueBuffer(ConvertToDecoderBuffer(input_image));
  const auto* ret = absl::get_if<DecodeResult>(&enqueue_res);
  if (ret && *ret == DecodeResult::kOk) {
    PostCrossThreadTask(
        *media_task_runner_.get(), FROM_HERE,
        CrossThreadBindOnce(&RTCVideoDecoderAdapter::DecodeOnMediaThread,
                            weak_this_));
  }

  return enqueue_res;
}

int32_t RTCVideoDecoderAdapter::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* callback) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoding_sequence_checker_);
  DCHECK(callback);

  base::AutoLock auto_lock(lock_);
  decode_complete_callback_ = callback;
  if (status_ == Status::kError) {
    RecordRTCVideoDecoderFallbackReason(
        config_.codec(),
        RTCVideoDecoderFallbackReason::kPreviousErrorOnRegisterCallback);
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoDecoderAdapter::Release() {
  DVLOG(1) << __func__;

  base::AutoLock auto_lock(lock_);
  pending_buffers_.clear();
  decode_timestamps_.clear();
  return status_ == Status::kError ? WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE
                                   : WEBRTC_VIDEO_CODEC_OK;
}

bool RTCVideoDecoderAdapter::ShouldReinitializeForSettingHDRColorSpace(
    const webrtc::EncodedImage& input_image) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoding_sequence_checker_);

  if (config_.profile() == media::VP9PROFILE_PROFILE2 &&
      input_image.ColorSpace()) {
    const media::VideoColorSpace& new_color_space =
        blink::WebRtcToMediaVideoColorSpace(*input_image.ColorSpace());
    if (!config_.color_space_info().IsSpecified() ||
        new_color_space != config_.color_space_info()) {
      return true;
    }
  }
  return false;
}

bool RTCVideoDecoderAdapter::ReinitializeSync(
    const media::VideoDecoderConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoding_sequence_checker_);

  base::TimeTicks start_time = base::TimeTicks::Now();
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  bool result = false;
  base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto init_cb =
      CrossThreadBindOnce(&FinishWait, CrossThreadUnretained(&waiter),
                          CrossThreadUnretained(&result));
  std::string decoder_name;
  WTF::CrossThreadOnceClosure flush_success_cb = CrossThreadBindOnce(
      &RTCVideoDecoderAdapter::InitializeOnMediaThread, weak_this_, config,
      std::move(init_cb),
      /*start_time=*/base::TimeTicks(),
      /*decoder_name=*/CrossThreadUnretained(&decoder_name));
  WTF::CrossThreadOnceClosure flush_fail_cb =
      CrossThreadBindOnce(&FinishWait, CrossThreadUnretained(&waiter),
                          CrossThreadUnretained(&result), false);
  if (PostCrossThreadTask(
          *media_task_runner_.get(), FROM_HERE,
          CrossThreadBindOnce(&RTCVideoDecoderAdapter::FlushOnMediaThread,
                              weak_this_, std::move(flush_success_cb),
                              std::move(flush_fail_cb)))) {
    waiter.Wait();
    RecordReinitializationLatency(base::TimeTicks::Now() - start_time);
  }
  return result;
}

void RTCVideoDecoderAdapter::ChangeStatus(Status new_status) {
  // It is impossible to recover once status becomes kError.
  if (status_ != Status::kError)
    status_ = new_status;
}

// static
int RTCVideoDecoderAdapter::GetCurrentDecoderCountForTesting() {
  return GetDecoderCounter()->Count();
}

// static
void RTCVideoDecoderAdapter::IncrementCurrentDecoderCountForTesting() {
  GetDecoderCounter()->IncrementCount();
}

// static
void RTCVideoDecoderAdapter::DecrementCurrentDecoderCountForTesting() {
  GetDecoderCounter()->DecrementCount();
}

// static
bool RTCVideoDecoderAdapter::Vp9HwSupportForSpatialLayers() {
  return base::FeatureList::IsEnabled(media::kVp9kSVCHWDecoding);
}

}  // namespace blink
