// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_stream_adapter.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "base/atomic_ref_count.h"
#include "base/containers/circular_deque.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
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
#include "media/renderers/default_decoder_factory.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/video_decode_accelerator.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_adapter.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_fallback_recorder.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_utils.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
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

// How many buffers are we willing to receive while decoding is paused, before
// we give up and fall back to software?  This is not used during normal
// decoding, only when we believe that the decoder is in a state where it is not
// ready to decode.  Our goal during this state is to keep the queue of pending
// buffers very small by requesting a keyframe.
//
// Also note that this value still counts buffers that were dropped in favor of
// a more recent keyframe; it represents the maximum number of calls to
// Decode before the decoder is ready to do work.
constexpr int32_t kMaxFramesWhilePausedBeforeFallback = 256;

// Max number of non-keyframes we'll queue without a decoder before we request a
// keyframe to replace them while the decoder is paused.  Previously queued
// frames will be discarded, to prevent a lot of old frames from being dumped
// onto the newly-unpaused decoder, that are probably stale anyway.
constexpr int32_t kMaxKeyFrameIntervalWhilePaused = 8;

// Maximum number of buffers that we will queue in the decoder stream during
// normal operation.  It includes all buffers that we have not gotten an output
// for.  "Normal operation" means that we believe that the decoder is trying to
// drain the queue.  During init and reset, for example, we don't expect it.
// See above for constants used while the decoder is in one of those states.
//
// If we go over this value, then we'll reset the decoder and request a keyframe
// to try to catch up.
//
// Note: This value is chosen to be Ludicrously High(tm), so that we can see
// where reasonable limits should be via UMA.
constexpr int32_t kMaxPendingBuffers = 64;

// Absolute maximum number of pending buffers.  If, at any time while we have
// an unpaused decoder, we believe that there are this many decodes in-flight
// when a new decode request arrives, we will fall back to software decoding.
//
// This value is not used while decoding is paused.  Instead, we use
// `kMaxFramesWhilePausedBeforeFallback`, since we might have different
// tolerances for "during init/reset" and "during normal decode".
//
// Note: This value is chosen to be Ludicrously High(tm), so that we can see
// where reasonable limits should be via UMA.  Changing this changes UMA, so
// probably don't.
constexpr int32_t kAbsoluteMaxPendingBuffers = 256;

// Name we'll report for hardware decoders.
constexpr const char* kExternalDecoderName = "ExternalDecoder";

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

void RecordInitializationLatency(base::TimeDelta latency) {
  base::UmaHistogramTimes("Media.RTCVideoDecoderInitializationLatencyMs",
                          latency);
}

}  // namespace

// static
constexpr gfx::Size RTCVideoDecoderStreamAdapter::kMinResolution;

// DemuxerStream implementation that forwards DecoderBuffer from some other
// source (i.e., VideoDecoder::Decode).
class RTCVideoDecoderStreamAdapter::InternalDemuxerStream
    : public media::DemuxerStream {
 public:
  explicit InternalDemuxerStream(const media::VideoDecoderConfig& config)
      : config_(config) {}

  ~InternalDemuxerStream() override = default;

  // DemuxerStream, only return one buffer at a time so we ignore the count.
  void Read(uint32_t /*count*/, ReadCB read_cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!pending_read_);
    pending_read_ = std::move(read_cb);
    MaybeSatisfyPendingRead();
  }

  media::AudioDecoderConfig audio_decoder_config() override {
    NOTREACHED();
    return media::AudioDecoderConfig();
  }

  media::VideoDecoderConfig video_decoder_config() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return config_;
  }

  Type type() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return DemuxerStream::VIDEO;
  }

  media::StreamLiveness liveness() const override {
    // Select low-delay mode.
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return media::StreamLiveness::kLive;
  }

  void EnableBitstreamConverter() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  bool SupportsConfigChanges() override {
    // The decoder can signal a config change to us, and we'll relay it to the
    // DecoderStream that's reading from us.
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return true;
  }

  // We've been given a new DecoderBuffer for the DecoderStream to consume.
  // Queue it, and maybe send it along immediately if there's a read pending.
  void EnqueueBuffer(std::unique_ptr<PendingBuffer> pending_buffer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // If we keyframe trimming is enabled, and we're adding a keyframe, then
    // drop all the old buffers and start over.
    if (trim_ && pending_buffer->buffer->is_key_frame())
      buffers_.clear();
    buffers_.emplace_back(std::move(pending_buffer));
    MaybeSatisfyPendingRead();
  }

  // Start a reset -- drop all buffers and abort any pending read request.
  void Reset() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    buffers_.clear();
    if (pending_read_)
      std::move(pending_read_).Run(DemuxerStream::Status::kAborted, {});
  }

  // If enabled, we'll drop any queued buffers when we're given a keyframe.
  // Otherwise, we'll queue normally.
  void set_keyframe_trimming(bool trim) { trim_ = trim; }

  size_t queue_length() const { return buffers_.size(); }

 private:
  // Send more DecoderBuffers to the reader, if we can.
  void MaybeSatisfyPendingRead() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // If there aren't any queued decoder buffers, then nothing to do.
    if (buffers_.empty())
      return;

    // If the decoder stream isn't trying to read, then also nothing to do.
    if (!pending_read_)
      return;

    // See if this buffer should cause a config change.  If so, send the config
    // change first, and keep the buffer for the next call.
    if (buffers_.front()->new_config) {
      config_ = std::move(*(buffers_.front()->new_config));
      std::move(pending_read_).Run(DemuxerStream::Status::kConfigChanged, {});
      return;
    }

    auto pending_buffer = std::move(buffers_.front());
    buffers_.pop_front();

    std::move(pending_read_)
        .Run(DemuxerStream::Status::kOk, {std::move(pending_buffer->buffer)});
  }

  media::VideoDecoderConfig config_;

  // Buffers that have been sent to us, but we haven't forwarded yet.
  // These are only ptrs because CrossThread* binding seems to work that way.
  base::circular_deque<std::unique_ptr<PendingBuffer>> buffers_;

  // Read request from the stream that we haven't been able to fulfill, if any.
  ReadCB pending_read_;

  // Start in trimming mode, until we're told to stop.
  bool trim_ = true;

  SEQUENCE_CHECKER(sequence_checker_);
};

// static
std::unique_ptr<RTCVideoDecoderStreamAdapter>
RTCVideoDecoderStreamAdapter::Create(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    base::WeakPtr<media::DecoderFactory> decoder_factory,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    const gfx::ColorSpace& render_color_space,
    const webrtc::SdpVideoFormat& format) {
  DVLOG(1) << __func__ << "(" << format.name << ")";

  const webrtc::VideoCodecType video_codec_type =
      webrtc::PayloadStringToCodecType(format.name);

  // Bail early for unknown codecs.
  if (WebRtcToMediaVideoCodec(video_codec_type) == media::VideoCodec::kUnknown)
    return nullptr;

  if (!gpu_factories &&
      !base::FeatureList::IsEnabled(media::kExposeSwDecodersToWebRTC)) {
    // Interpret "no gpu factories" to mean "sw only", even though we don't
    // technically know if `decoder_factory` can create hw decoders or not.  To
    // make it unambiguous, and probably save a thread hop, fail immediately.
    return nullptr;
  }

  // Avoid the thread hop if the decoder is known not to support the config.
  // TODO(sandersd): Predict size from level.
  media::VideoDecoderConfig config(
      WebRtcToMediaVideoCodec(webrtc::PayloadStringToCodecType(format.name)),
      WebRtcVideoFormatToMediaVideoCodecProfile(format),
      media::VideoDecoderConfig::AlphaMode::kIsOpaque, media::VideoColorSpace(),
      media::kNoTransformation, kDefaultSize, gfx::Rect(kDefaultSize),
      kDefaultSize, media::EmptyExtraData(),
      media::EncryptionScheme::kUnencrypted);

  config.set_is_rtc(true);

  // TODO(https://crbug.com/1274904): Fail early to match DecoderAdapter.
  if (gpu_factories &&
      gpu_factories->IsDecoderConfigSupported(config) ==
          media::GpuVideoAcceleratorFactories::Supported::kFalse) {
    base::UmaHistogramBoolean("Media.RTCVideoDecoderInitDecodeSuccess", false);
    return nullptr;
  }

  // InitializeSync doesn't really initialize anything; it just posts the work
  // to the media thread.  If init fails, then we'll fall back on the first
  // decode after we notice.
  auto rtc_video_decoder_adapter =
      base::WrapUnique(new RTCVideoDecoderStreamAdapter(
          gpu_factories, decoder_factory, std::move(media_task_runner),
          render_color_space, config, format));
  return rtc_video_decoder_adapter;
}

RTCVideoDecoderStreamAdapter::RTCVideoDecoderStreamAdapter(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    base::WeakPtr<media::DecoderFactory> decoder_factory,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    const gfx::ColorSpace& render_color_space,
    const media::VideoDecoderConfig& config,
    const webrtc::SdpVideoFormat& format)
    : media_task_runner_(std::move(media_task_runner)),
      gpu_factories_(gpu_factories),
      decoder_factory_(decoder_factory),
      render_color_space_(render_color_space),
      format_(format),
      config_(config),
      max_pending_buffer_count_(kAbsoluteMaxPendingBuffers) {
  DVLOG(1) << __func__;
  // Default to hw-accelerated decoder, in case something checks before decoding
  // a frame. It's unclear what we should report in the long run, but for now,
  // it's better to report hardware since that's all we support anyway.
  decoder_info_.implementation_name = kExternalDecoderName;
  decoder_info_.is_hardware_accelerated = true;
  DETACH_FROM_SEQUENCE(decoding_sequence_checker_);
  // This is normally constructed on the media thread, but the first one is
  // constructed immediately so that we can post to the media thread.
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

RTCVideoDecoderStreamAdapter::~RTCVideoDecoderStreamAdapter() {
  DVLOG(1) << __func__;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  {
    // It doesn't really need to be guarded by a lock since it's only accessed
    // off-thread here, but this forces memory barriers and such.
    base::AutoLock auto_lock(lock_);
    RecordMaxInFlightDecodesLockedOnMedia();

    if (contributes_to_decoder_count_) {
      contributes_to_decoder_count_ = false;  // paranoia
      GetDecoderCounter()->DecrementCount();
    }
  }
}

void RTCVideoDecoderStreamAdapter::InitializeOrReinitializeSync() {
  DVLOG(3) << __func__;
  TRACE_EVENT0("webrtc",
               "RTCVideoDecoderStreamAdapter::InitializeOrReinitializeSync");

  // Can be called on |worker_thread_| or |decoding_thread_|.
  DCHECK(!media_task_runner_->RunsTasksInCurrentSequence());

  // Anything that's posted to the media thread after this must start with a
  // keyframe, since we're about to post a task to reset the state.
  key_frame_required_ = true;

  // Allow init to complete asynchronously, since we'll probably succeed.
  // Trying to do it synchronously can block the mojo pipe, and deadlock.
  PostCrossThreadTask(
      *media_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          &RTCVideoDecoderStreamAdapter::RestartDecoderStreamOnMedia,
          weak_this_));
}

bool RTCVideoDecoderStreamAdapter::Configure(const Settings& settings) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoding_sequence_checker_);
  TRACE_EVENT0("webrtc", "RTCVideoDecoderStreamAdapter::Configure");

  video_codec_type_ = settings.codec_type();
  DCHECK_EQ(webrtc::PayloadStringToCodecType(format_.name), video_codec_type_);

  base::AutoLock auto_lock(lock_);
  init_decode_complete_ = true;
  // Interpret "no gpu factories" to mean "no hw decoders", regardless of
  // whether or not the decoder factory can produce them.  Probably, it can't.
  if (!gpu_factories_)
    prefer_software_decoders_ = true;
  const webrtc::RenderResolution& resolution = settings.max_render_resolution();
  if (resolution.Valid()) {
    // This lets our initial decoder selection see something that's at least
    // maybe related to the stream, rather than our default guess.
    current_resolution_ = gfx::Size(resolution.Width(), resolution.Height());
    config_.set_coded_size(current_resolution_);
    config_.set_visible_rect(gfx::Rect(current_resolution_));
    config_.set_natural_size(current_resolution_);
  }

  // Now that we have a guess at the resolution, try to init.
  InitializeOrReinitializeSync();
  AttemptLogInitializationState_Locked();
  return !has_error_;
}

void RTCVideoDecoderStreamAdapter::AttemptLogInitializationState_Locked() {
  lock_.AssertAcquired();
  TRACE_EVENT0(
      "webrtc",
      "RTCVideoDecoderStreamAdapter::AttemptLogInitializationState_Locked");

  // Don't log more than once.
  if (logged_init_status_)
    return;

  // Don't log anything until both InitDecode and Initialize have completed,
  // unless we failed.  Log failures immediately, since both steps might not
  // ever complete.
  if (!has_error_ && (!init_complete_ || !init_decode_complete_))
    return;

  logged_init_status_ = true;

  base::UmaHistogramBoolean("Media.RTCVideoDecoderInitDecodeSuccess",
                            !has_error_);
  if (!has_error_) {
    UMA_HISTOGRAM_ENUMERATION(
        "Media.RTCVideoDecoderProfile",
        WebRtcVideoFormatToMediaVideoCodecProfile(format_),
        media::VIDEO_CODEC_PROFILE_MAX + 1);
  }
}

int32_t RTCVideoDecoderStreamAdapter::Decode(
    const webrtc::EncodedImage& input_image,
    bool missing_frames,
    int64_t render_time_ms) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoding_sequence_checker_);
  TRACE_EVENT0("webrtc", "RTCVideoDecoderStreamAdapter::Decode");

#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  const bool has_software_fallback =
      video_codec_type_ != webrtc::kVideoCodecH264;
#else
  const bool has_software_fallback = true;
#endif

  // Don't allow hardware decode for small videos if there are too many
  // decoder instances.  This includes the case where our resolution drops while
  // too many decoders exist.  When DecoderStream supports software decoders,
  // this should be moved to DecoderSelector.  If we don't contribute to the
  // decoder count already, then just keep going.
  {
    base::AutoLock auto_lock(lock_);
    // If we don't prefer software decoders, and we're not already contributing
    // to the decoder count, then do so now.  Note that we wait until the first
    // decode, since that's when things actually get initialized.  It's common
    // for sites to create unused rtc codecs.
    if (!contributes_to_decoder_count_ && !prefer_software_decoders_) {
      contributes_to_decoder_count_ = true;
      GetDecoderCounter()->IncrementCount();
    }

    // Note that it's okay to FallBackToSoftware_Locked without a software
    // fallback; it will use the hw decoder anyway.  It's only because chrome sw
    // decoders are not always enabled for RTC that we have to be careful -- we
    // don't want to fall back to rtc software decoders when the only option is
    // chrome hw.
    // TODO(liberato): since DecoderStream selects sw anyway for low res, should
    // this even be needed?  We never check if we actually have a hw impl, only
    // that we haven't specifically asked to prefer a sw one.  DecoderStream
    // might have chosen sw anyway, assuming that `kMinResolution` agrees with
    // whatever threshold it uses.
    if (has_software_fallback && contributes_to_decoder_count_ &&
        current_resolution_.GetArea() < kMinResolution.GetArea() &&
        GetDecoderCounter()->Count() > kMaxDecoderInstances) {
      return FallBackToSoftware_Locked();
    }

    // Fall back to software decoding if there's no support for VP9 spatial
    // layers. See https://crbug.com/webrtc/9304.
    // TODO(chromium:1187565): Update
    // RTCVideoDecoderFactory::QueryCodecSupport() if RTCVideoDecoderStream is
    // changed to handle SW decoding and not return
    // WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE.

    // This adapter is different from rtc_video_decoder_adapter.
    // If it's software(libvpx) decoder, currently libvpx can decode vp9 kSVC
    // stream properly. So only when |decoder_info_.is_hardware_accelerated| is
    // true, we will do the decoder capability check.
    if (video_codec_type_ == webrtc::kVideoCodecVP9 &&
        input_image.SpatialIndex().value_or(0) > 0 &&
        !RTCVideoDecoderAdapter::Vp9HwSupportForSpatialLayers(
            video_decoder_type_) &&
        decoder_configured_ && decoder_info_.is_hardware_accelerated) {
      DLOG(ERROR) << __func__
                  << " fallback to software due to decoder doesn't support "
                     "decoding VP9 multiple spatial layers.";
      RecordRTCVideoDecoderFallbackReason(
          config_.codec(), RTCVideoDecoderFallbackReason::kSpatialLayers);
      return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
    }
  }

  if (missing_frames) {
    DVLOG(2) << "Missing frames";
    // We probably can't handle broken frames. Request a key frame and wait
    // until we get it.
    key_frame_required_ = true;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  if (key_frame_required_) {
    // We discarded previous frame because we have too many pending buffers (see
    // logic) below. Now we need to wait for the key frame and discard
    // everything else.
    if (input_image._frameType != webrtc::VideoFrameType::kVideoFrameKey) {
      DVLOG(2) << "Discard non-key frame";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    DVLOG(2) << "Key frame received, resume decoding";
    // ok, we got key frame and can continue decoding.
    key_frame_required_ = false;
    buffers_since_last_keyframe_ = 0;
  }

  std::vector<uint32_t> spatial_layer_frame_size;
  int max_sl_index = input_image.SpatialIndex().value_or(0);
  for (int i = 0; i <= max_sl_index; i++) {
    auto frame_size = input_image.SpatialLayerFrameSize(i);
    if (!frame_size)
      continue;
    spatial_layer_frame_size.push_back(
        base::checked_cast<uint32_t>(*frame_size));
  }

  // Convert to media::DecoderBuffer.
  // TODO(sandersd): What is |render_time_ms|?
  auto pending_buffer = std::make_unique<PendingBuffer>();
  if (spatial_layer_frame_size.size() > 1) {
    const uint8_t* side_data =
        reinterpret_cast<const uint8_t*>(spatial_layer_frame_size.data());
    size_t side_data_size =
        spatial_layer_frame_size.size() * sizeof(uint32_t) / sizeof(uint8_t);
    pending_buffer->buffer = media::DecoderBuffer::CopyFrom(
        input_image.data(), input_image.size(), side_data, side_data_size);
  } else {
    pending_buffer->buffer =
        media::DecoderBuffer::CopyFrom(input_image.data(), input_image.size());
  }
  pending_buffer->buffer->set_timestamp(
      base::Microseconds(input_image.Timestamp()));
  pending_buffer->buffer->set_is_key_frame(
      input_image._frameType == webrtc::VideoFrameType::kVideoFrameKey);

  // Detect config changes, and include the new config if needed.
  if (ShouldReinitializeForSettingHDRColorSpace(input_image)) {
    pending_buffer->new_config = config_;
    pending_buffer->new_config->set_color_space_info(
        media::VideoColorSpace::FromGfxColorSpace(
            blink::WebRtcToGfxColorSpace(*input_image.ColorSpace())));
  }

  // Queue for decoding.
  {
    base::AutoLock auto_lock(lock_);
    // TODO(crbug.com/1150098): We could destroy and re-create `decoder_stream_`
    // here, to reset the decoder state.  For now, just fail.
    if (has_error_) {
      DLOG(ERROR) << __func__ << " decoding failed.";

      // Try again, but prefer software decoders this time.
      RecordRTCVideoDecoderFallbackReason(
          config_.codec(),
          RTCVideoDecoderFallbackReason::kPreviousErrorOnDecode);
      // Since we now need a keyframe, request one.
      return FallBackToSoftware_Locked();
    }

    if (IsDecodingPaused_Locked()) {
      // If we don't have a decoder, then try to keep the amount of catch-up
      // work to a minimum.  `pending_buffer_count_` indicates the number of
      // buffers that we have sent, including those that were dropped.
      if (has_software_fallback &&
          pending_buffer_count_ > kMaxFramesWhilePausedBeforeFallback) {
        RecordRTCVideoDecoderFallbackReason(
            config_.codec(), RTCVideoDecoderFallbackReason::
                                 kConsecutivePendingBufferOverflowDuringInit);
        return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
      }
      if (!pending_buffer->buffer->is_key_frame() &&
          buffers_since_last_keyframe_ > kMaxKeyFrameIntervalWhilePaused) {
        // Too many frames since the last keyframe -- request a new one and
        // don't queue anything else until we get it.
        key_frame_required_ = true;
        return WEBRTC_VIDEO_CODEC_ERROR;
      }
      // Else a keyframe, or we haven't sent too many since the last one.
      pending_buffer_count_++;
      buffers_since_last_keyframe_++;
    } else if (pending_buffer_count_ >= max_pending_buffer_count_) {
      // We are severely behind. Drop pending buffers and request a keyframe to
      // catch up as quickly as possible.
      DVLOG(2) << "Pending buffers overflow";
      // Actually we just discarded a frame. We must wait for the key frame and
      // drop any other non-key frame.
      key_frame_required_ = true;

      // If we hit the absolute limit, then give up.
      if (has_software_fallback &&
          pending_buffer_count_ >= kAbsoluteMaxPendingBuffers) {
        has_error_ = true;
        PostCrossThreadTask(
            *media_task_runner_.get(), FROM_HERE,
            CrossThreadBindOnce(
                &RTCVideoDecoderStreamAdapter::ShutdownOnMediaThread,
                weak_this_));
        DLOG(ERROR) << __func__ << " too many errors / pending buffers.";
        RecordRTCVideoDecoderFallbackReason(
            config_.codec(),
            RTCVideoDecoderFallbackReason::kConsecutivePendingBufferOverflow);
        // We might want to try FallBackToSoftware_Locked(), but for now, don't.
        return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
      }

      // Note that this is approximate, since there might be decodes in flight.
      // If they complete, then this might get decremented (clamped to 0), so
      // we'll underestimate the queue length a bit until it stabilizes.
      pending_buffer_count_ = 0;
      // Increase to the absolute max while decoding is paused.  It'll be
      // lowered as we drain the queue.
      max_pending_buffer_count_ = kAbsoluteMaxPendingBuffers;
      pending_reset_ = true;

      PostCrossThreadTask(
          *media_task_runner_.get(), FROM_HERE,
          CrossThreadBindOnce(&RTCVideoDecoderStreamAdapter::ResetOnMediaThread,
                              weak_this_));

      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    pending_buffer_count_++;
  }

  // It would be nice to do this on the current thread, but we'd have to hop to
  // the media thread anyway if we needed to do any work.  So just hop to keep
  // it simpler.
  PostCrossThreadTask(
      *media_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&RTCVideoDecoderStreamAdapter::DecodeOnMediaThread,
                          weak_this_, std::move(pending_buffer)));

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoDecoderStreamAdapter::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* callback) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoding_sequence_checker_);
  DCHECK(callback);
  TRACE_EVENT0("webrtc",
               "RTCVideoDecoderStreamAdapter::RegisterDecodeCompleteCallback");

  base::AutoLock auto_lock(lock_);
  decode_complete_callback_ = callback;
  if (has_error_) {
    RecordRTCVideoDecoderFallbackReason(
        config_.codec(),
        RTCVideoDecoderFallbackReason::kPreviousErrorOnRegisterCallback);
    return FallBackToSoftware_Locked();
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoDecoderStreamAdapter::Release() {
  DVLOG(1) << __func__;
  TRACE_EVENT0("webrtc", "RTCVideoDecoderStreamAdapter::Release");

  base::AutoLock auto_lock(lock_);

  // We don't know how long it'll take for shutdown on the media thread to
  // cancel our weak ptrs, so make sure that nobody sends any frames after this.
  decode_complete_callback_ = nullptr;

  PostCrossThreadTask(
      *media_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&RTCVideoDecoderStreamAdapter::ShutdownOnMediaThread,
                          weak_this_));

  return has_error_ ? WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE
                    : WEBRTC_VIDEO_CODEC_OK;
}

webrtc::VideoDecoder::DecoderInfo RTCVideoDecoderStreamAdapter::GetDecoderInfo()
    const {
  base::AutoLock auto_lock(lock_);
  return decoder_info_;
}

void RTCVideoDecoderStreamAdapter::InitializeOnMediaThread(
    const media::VideoDecoderConfig& config,
    InitCB init_cb) {
  DVLOG(3) << __func__;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc",
               "RTCVideoDecoderStreamAdapter::InitializeOnMediaThread");

  // There's no re-init these days.  If we ever need to re-init, such as to
  // clear an error, then `decoder_stream_` and `demuxer_stream_` should be
  // recreated rather than re-used.
  DCHECK(!decoder_stream_);

  // TODO(sandersd): Plumb a real log sink here so that we can contribute to
  // the media-internals UI. The current log just discards all messages.
  media_log_ = std::make_unique<media::NullMediaLog>();

  // Encryption is not supported.
  media::CdmContext* cdm_context = nullptr;

  // First init.  Set everything up.
  demuxer_stream_ = std::make_unique<InternalDemuxerStream>(config);

  auto traits =
      std::make_unique<media::DecoderStreamTraits<media::DemuxerStream::VIDEO>>(
          media_log_.get());
  {
    base::AutoLock auto_lock(lock_);
    traits->SetPreferNonPlatformDecoders(prefer_software_decoders_);
  }

  media::RequestOverlayInfoCB request_overlay_cb = base::DoNothing();
  auto create_decoders_cb = base::BindRepeating(
      [](scoped_refptr<base::SequencedTaskRunner> task_runner,
         base::WeakPtr<media::DecoderFactory> decoder_factory,
         media::GpuVideoAcceleratorFactories* gpu_factories,
         const gfx::ColorSpace render_color_space, media::MediaLog* media_log,
         const media::RequestOverlayInfoCB& request_overlay_cb) {
        std::vector<std::unique_ptr<media::VideoDecoder>> video_decoders;
        media::DecoderFactory* decoder_factory_ptr = decoder_factory.get();
        if (decoder_factory_ptr) {
          decoder_factory_ptr->CreateVideoDecoders(
              std::move(task_runner), gpu_factories, media_log,
              request_overlay_cb, render_color_space, &video_decoders);
        }
        return video_decoders;
      },
      media_task_runner_, decoder_factory_, base::Unretained(gpu_factories_),
      render_color_space_, media_log_.get(), std::move(request_overlay_cb));

  decoder_stream_ = std::make_unique<media::VideoDecoderStream>(
      std::move(traits), media_task_runner_, std::move(create_decoders_cb),
      media_log_.get());
  decoder_stream_->set_decoder_change_observer(base::BindRepeating(
      &RTCVideoDecoderStreamAdapter::OnDecoderChanged, weak_this_));
  decoder_stream_->Initialize(
      demuxer_stream_.get(), ConvertToBaseOnceCallback(std::move(init_cb)),
      cdm_context, base::DoNothing() /* statistics_cb */,
      base::DoNothing() /* waiting_cb */);
}

void RTCVideoDecoderStreamAdapter::OnInitializeDone(base::TimeTicks start_time,
                                                    bool success) {
  TRACE_EVENT1("webrtc", "RTCVideoDecoderStreamAdapter::OnInitializeDone",
               "success", success);
  RecordInitializationLatency(base::TimeTicks::Now() - start_time);
  {
    base::AutoLock auto_lock(lock_);
    init_complete_ = true;

    if (!success) {
      has_error_ = true;
      // TODO(crbug.com/1150103): Is it guaranteed that there will be a next
      // decode call to signal the error?  If not, then we should use the decode
      // callback if we have one yet.
    } else {
      AdjustQueueLength_Locked();
    }

    AttemptLogInitializationState_Locked();
  }

  if (success)
    AttemptRead();
}

void RTCVideoDecoderStreamAdapter::DecodeOnMediaThread(
    std::unique_ptr<PendingBuffer> pending_buffer) {
  DVLOG(4) << __func__;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCVideoDecoderStreamAdapter::DecodeOnMediaThread");
  {
    base::AutoLock auto_lock(lock_);

    // If we're in the error state, then do nothing.  `Decode()` will notify
    // about the error.
    if (has_error_)
      return;

    // Update the max recorded pending buffers.  This is kept up-to-date on the
    // decoder thread when the buffer is queued.
    RecordMaxInFlightDecodesLockedOnMedia();
  }

  // Remember that this timestamp has already been added to the list.
  // Do not call with the lock held, since it might call us back with decoded
  // frames before returning.
  demuxer_stream_->EnqueueBuffer(std::move(pending_buffer));

  // Kickstart reading output, if we're not already.
  AttemptRead();
}

void RTCVideoDecoderStreamAdapter::OnFrameReady(
    media::VideoDecoderStream::ReadResult result) {
  DVLOG(3) << __func__;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT1("webrtc", "RTCVideoDecoderStreamAdapter::OnFrameReady",
               "success", result.has_value());

  pending_read_ = false;

  switch (result.code()) {
    case media::DecoderStatus::Codes::kOk:
      break;
    case media::DecoderStatus::Codes::kAborted:
      // We're doing a Reset(), so just ignore it and keep going.
      return;
    default:
      DVLOG(2) << "Entering permanent error state";
      base::UmaHistogramSparse("Media.RTCVideoDecoderStream.Error.OnFrameReady",
                               static_cast<int>(result.code()));
      {
        base::AutoLock auto_lock(lock_);
        has_error_ = true;
        pending_buffer_count_ = 0;
      }
      return;
  }

  scoped_refptr<media::VideoFrame> frame = std::move(result).value();
  DCHECK(frame);

  const base::TimeDelta timestamp = frame->timestamp();
  webrtc::VideoFrame rtc_frame =
      webrtc::VideoFrame::Builder()
          .set_video_frame_buffer(rtc::scoped_refptr<webrtc::VideoFrameBuffer>(
              new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
                  std::move(frame))))
          .set_timestamp_rtp(static_cast<uint32_t>(timestamp.InMicroseconds()))
          .set_timestamp_us(0)
          .set_rotation(webrtc::kVideoRotation_0)
          .build();

  {
    base::AutoLock auto_lock(lock_);

    // Record time to first frame if we haven't yet.
    if (start_time_) {
      // We haven't recorded the first frame time yet, so do so now.
      base::UmaHistogramTimes("Media.RTCVideoDecoderFirstFrameLatencyMs",
                              base::TimeTicks::Now() - *start_time_);
      start_time_.reset();
    }

    // Update `current_resolution_`, in case it's changed.  This lets us fall
    // back to software, or avoid doing so, if we're over the decoder limit.
    current_resolution_ = gfx::Size(rtc_frame.width(), rtc_frame.height());

    // Assumes that Decoded() can be safely called with the lock held, which
    // apparently it can be because RTCVideoDecoder does the same.

    // Since we can reset the queue length while things are in flight, just
    // clamp to zero.  We could choose to discard this frame, too, since it was
    // before a reset was issued.
    if (pending_buffer_count_ > 0)
      pending_buffer_count_--;
    if (decode_complete_callback_)
      decode_complete_callback_->Decoded(rtc_frame);
    AdjustQueueLength_Locked();
  }

  // Try to read the next output, if any, regardless if this succeeded.
  AttemptRead();
}

void RTCVideoDecoderStreamAdapter::AttemptRead() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCVideoDecoderStreamAdapter::AttemptRead");
  {
    base::AutoLock auto_lock(lock_);

    // Only one read may be in-flight at once.  We'll try again once the
    // previous read completes.  If a reset is in progress, a read is not
    // allowed to start. We also may not read until DecoderStream init
    // completes.
    if (pending_read_ || pending_reset_ || !init_complete_ || has_error_)
      return;

    // We don't care if there are any pending decodes; keep a read running even
    // if there aren't any.  This way, we don't have to count correctly.

    pending_read_ = true;
  }

  // Do not call this with the lock held, since it might deliver a frame to us
  // before it returns.
  decoder_stream_->Read(
      base::BindOnce(&RTCVideoDecoderStreamAdapter::OnFrameReady, weak_this_));
}

bool RTCVideoDecoderStreamAdapter::ShouldReinitializeForSettingHDRColorSpace(
    const webrtc::EncodedImage& input_image) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoding_sequence_checker_);
  TRACE_EVENT0("webrtc",
               "RTCVideoDecoderStreamAdapter::"
               "ShouldReinitializeForSettingHDRColorSpace");

  if (config_.profile() == media::VP9PROFILE_PROFILE2 &&
      input_image.ColorSpace()) {
    const gfx::ColorSpace& new_color_space =
        blink::WebRtcToGfxColorSpace(*input_image.ColorSpace());
    if (!config_.color_space_info().IsSpecified() ||
        new_color_space != config_.color_space_info().ToGfxColorSpace()) {
      return true;
    }
  }

  return false;
}

void RTCVideoDecoderStreamAdapter::ResetOnMediaThread() {
  DVLOG(3) << __func__;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  {
    base::AutoLock auto_lock(lock_);
    // `pending_reset_` should have been set for us already, so that decoding
    // is paused immediately.
    DCHECK(pending_reset_);
  }
  TRACE_EVENT0("webrtc", "RTCVideoDecoderStreamAdapter::ResetOnMediaThread");
  // A pending read is okay.  We may decide to reset at any time, even if a read
  // is in progress.  It'll be aborted when we reset `decoder_stream_`, and no
  // new read will be issued until the reset completes.

  demuxer_stream_->Reset();
  // We might want to go into trimming mode here, and turn it back off when we
  // get the reset callback.  However, this would require that ::Decode also
  // understands to request keyframes more often else it won't do much.
  decoder_stream_->Reset(base::BindOnce(
      &RTCVideoDecoderStreamAdapter::OnResetCompleteOnMediaThread, weak_this_));
}

void RTCVideoDecoderStreamAdapter::OnResetCompleteOnMediaThread() {
  DVLOG(3) << __func__;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!pending_read_);

  {
    base::AutoLock auto_lock(lock_);

    DCHECK(pending_reset_);
    pending_reset_ = false;

    AdjustQueueLength_Locked();
  }
  AttemptRead();
}

void RTCVideoDecoderStreamAdapter::AdjustQueueLength_Locked() {
  // After an init or reset, we can have a larger backlog of queued
  // DecoderBuffers than we'd normally allow.  Since decoding is effectively
  // paused, we let the backlog grow since it doesn't indicate that we're
  // running behind in any meaningful sense; hopefully we'll catch up once we
  // turn the decoder on.  Once decoding un-pauses, we need to get back to a
  // sane upper limit, without spuriously tripping the queue length check in the
  // process.  New decodes will still arrive, and decoding only has to be fast
  // enough on average.  So, the queue might get longer as we (on average) work
  // through the backlog.  `kMaxPendingBuffers` is what we believe is the
  // maximum backlog we will see, if decoding is "fast enough".

  // So, we lower the max allowable limit as we drain buffers, but allow that
  // the queue can grow by up to `kMaxPendingBuffers` at any time from the
  // lowest limit we've observed.  This has the side-effect of resetting the
  // limit to `kMaxPendingBuffers` if we ever do work through the backlog.
  lock_.AssertAcquired();
  if (pending_buffer_count_ + kMaxPendingBuffers < max_pending_buffer_count_)
    max_pending_buffer_count_ = pending_buffer_count_ + kMaxPendingBuffers;
}

void RTCVideoDecoderStreamAdapter::ShutdownOnMediaThread() {
  DVLOG(3) << __func__;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCVideoDecoderStreamAdapter::ShutdownOnMediaThread");

  base::AutoLock auto_lock(lock_);
  weak_this_factory_.InvalidateWeakPtrs();
  weak_this_ = weak_this_factory_.GetWeakPtr();
  decoder_stream_.reset();
  demuxer_stream_.reset();

  RecordMaxInFlightDecodesLockedOnMedia();

  pending_reset_ = false;
  pending_read_ = false;
  init_complete_ = false;
  init_decode_complete_ = false;
  logged_init_status_ = false;
  pending_buffer_count_ = 0;
  max_reported_buffer_count_ = 0;
  max_buffer_count_metric_ = nullptr;
  // `has_error_` might or might not be set.
}

void RTCVideoDecoderStreamAdapter::OnDecoderChanged(
    media::VideoDecoder* decoder) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock auto_lock(lock_);
  TRACE_EVENT1("webrtc", "RTCVideoDecoderStreamAdapter::OnDecoderChanged",
               "decoder",
               (decoder ? static_cast<int>(decoder->GetDecoderType()) : -1));

  // The demuxer should trim to keyframes if and only if we don't have a decoder
  // to consume them.  This prevents the queue from getting too long.
  if (demuxer_stream_)
    demuxer_stream_->set_keyframe_trimming(!decoder);

  if (!decoder) {
    decoder_configured_ = false;
    pending_buffer_count_ = 0;
    return;
  }

  if (!decoder_configured_) {
    // Switching from no decoder back to having a decoder, so reset the buffer
    // count to what's still pending after keyframe trimming.
    //
    // Note that this is approximate; some buffers might have been drained
    // already by the DecoderStream.  However, the pending buffer count is
    // always clamped to zero, so that's okay.
    pending_buffer_count_ = demuxer_stream_->queue_length();
  }

  decoder_configured_ = true;
  decoder_info_.is_hardware_accelerated = decoder->IsPlatformDecoder();
  video_decoder_type_ = decoder->GetDecoderType();

  // In order not to break the RTC statistics collection, name these
  // software(libvpx and FFmpeg) decoders in a way that
  // third_party/webrtc/video/receive_statistics_proxy2.cc can understand.
  if (decoder->IsPlatformDecoder()) {
    std::string implementation_name_suffix =
        " (" + media::GetDecoderName(decoder->GetDecoderType()) + ")";
    decoder_info_.implementation_name =
        kExternalDecoderName + implementation_name_suffix;
    return;
  }

  switch (video_decoder_type_) {
    case media::VideoDecoderType::kVpx:
      decoder_info_.implementation_name = "libvpx (DecoderStream)";
      break;
    case media::VideoDecoderType::kFFmpeg:
      decoder_info_.implementation_name = "FFmpeg (DecoderStream)";
      break;
    default:
      decoder_info_.implementation_name =
          media::GetDecoderName(decoder->GetDecoderType()) + " (DecoderStream)";
  }
}

void RTCVideoDecoderStreamAdapter::RecordMaxInFlightDecodesLockedOnMedia() {
  lock_.AssertAcquired();
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // If we've reported this maximum before, then don't waste the IPC.  This also
  // covers the case where `!pending_buffer_count_`, since the reported count
  // starts at zero.  Also, don't record if this isn't a new maximum.
  if (pending_buffer_count_ <= max_reported_buffer_count_)
    return;

  if (!max_buffer_count_metric_) {
    max_buffer_count_metric_ =
        base::SingleSampleMetricsFactory::Get()->CreateCustomCountsMetric(
            "Media.RTCVideoDecoderMaxInFlightDecodes", 0,
            kAbsoluteMaxPendingBuffers + 1, 100);
  }

  // It's unclear if the factory can fail, so simply don't record if it does.
  if (max_buffer_count_metric_) {
    max_buffer_count_metric_->SetSample(
        static_cast<int>(pending_buffer_count_));
  }

  // Mark it as recorded either way, so we don't keep trying.
  max_reported_buffer_count_ = pending_buffer_count_;
}

void RTCVideoDecoderStreamAdapter::RestartDecoderStreamOnMedia() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc",
               "RTCVideoDecoderStreamAdapter::RestartDecoderStreamOnMedia");

  // Shut down and begin re-init.  It's okay if there has not been an init
  // before this.
  ShutdownOnMediaThread();
  base::TimeTicks start_time = base::TimeTicks::Now();
  {
    base::AutoLock auto_lock(lock_);
    start_time_ = start_time;
    has_error_ = false;
  }
  auto init_cb = CrossThreadBindOnce(
      &RTCVideoDecoderStreamAdapter::OnInitializeDone, weak_this_, start_time);
  InitializeOnMediaThread(config_, std::move(init_cb));
}

int32_t RTCVideoDecoderStreamAdapter::FallBackToSoftware_Locked() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoding_sequence_checker_);
  lock_.AssertAcquired();
  TRACE_EVENT0("webrtc",
               "RTCVideoDecoderStreamAdapter::FallBackToSoftware_Locked");

  // We will either prefer software decoders by asking DecodersStream, or prefer
  // them by asking rtc to use rtc sw decoders.  Either way, we don't contribute
  // to the decoder count any more.  Remember that, if we request sw but get hw
  // anyway, then we still don't contribute to the decoder count since it means
  // that there's no alternative.  It's approximate, but all of this logic
  // should be moved into DecoderSelector anyway eventually, where it can be
  // done exactly.  That would also prevent destroying the adapter here, just to
  // end up with the same hw decoder anyway.
  if (contributes_to_decoder_count_) {
    contributes_to_decoder_count_ = false;
    GetDecoderCounter()->DecrementCount();
  }

  // If there aren't chrome sw decoders for DecoderStream to use, then give up
  // and ask rtc to do it.
  if (!base::FeatureList::IsEnabled(media::kExposeSwDecodersToWebRTC)) {
    // Oh well.
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }

  // Note that they might already be preferred, which is okay.
  prefer_software_decoders_ = true;
  InitializeOrReinitializeSync();

  // Request a keyframe.
  return WEBRTC_VIDEO_CODEC_ERROR;
}

bool RTCVideoDecoderStreamAdapter::IsDecodingPaused_Locked() const {
  lock_.AssertAcquired();
  return pending_reset_ || !decoder_configured_;
}

}  // namespace blink
