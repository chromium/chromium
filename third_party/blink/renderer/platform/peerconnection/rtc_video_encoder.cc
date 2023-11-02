// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder.h"

#include <memory>
#include <numeric>
#include <vector>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/bitrate.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/capture/capture_switches.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/h264_parser.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/webrtc/convert_to_webrtc_video_frame_buffer.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_gfx.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "third_party/webrtc/modules/video_coding/include/video_error_codes.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

namespace {
class SignaledValue {
 public:
  SignaledValue() : event(nullptr), val(nullptr) {}
  SignaledValue(base::WaitableEvent* event, int32_t* val)
      : event(event), val(val) {
    DCHECK(event);
  }

  ~SignaledValue() {
    if (IsValid() && !event->IsSignaled()) {
      NOTREACHED() << "never signaled";
      event->Signal();
    }
  }

  // Move-only.
  SignaledValue(const SignaledValue&) = delete;
  SignaledValue& operator=(const SignaledValue&) = delete;
  SignaledValue(SignaledValue&& other) : event(other.event), val(other.val) {
    other.event = nullptr;
    other.val = nullptr;
  }
  SignaledValue& operator=(SignaledValue&& other) {
    event = other.event;
    val = other.val;
    other.event = nullptr;
    other.val = nullptr;
    return *this;
  }

  void Signal() {
    if (!IsValid())
      return;
    event->Signal();
    event = nullptr;
  }

  void Set(int32_t v) {
    if (!val)
      return;
    *val = v;
  }

  bool IsValid() { return event; }

 private:
  base::WaitableEvent* event;
  int32_t* val;
};

class ScopedSignaledValue {
 public:
  ScopedSignaledValue() = default;
  ScopedSignaledValue(base::WaitableEvent* event, int32_t* val)
      : sv(event, val) {}
  explicit ScopedSignaledValue(SignaledValue sv) : sv(std::move(sv)) {}

  ~ScopedSignaledValue() { sv.Signal(); }

  ScopedSignaledValue(const ScopedSignaledValue&) = delete;
  ScopedSignaledValue& operator=(const ScopedSignaledValue&) = delete;
  ScopedSignaledValue(ScopedSignaledValue&& other) : sv(std::move(other.sv)) {
    DCHECK(!other.sv.IsValid());
  }
  ScopedSignaledValue& operator=(ScopedSignaledValue&& other) {
    sv.Signal();
    sv = std::move(other.sv);
    DCHECK(!other.sv.IsValid());
    return *this;
  }

  // Set |v|, signal |sv|, and invalidate |sv|. If |sv| is already invalidated
  // at the call, this has no effect.
  void SetAndReset(int32_t v) {
    sv.Set(v);
    reset();
  }

  // Invalidate |sv|. The invalidated value will be set by move assignment
  // operator.
  void reset() { *this = ScopedSignaledValue(); }

 private:
  SignaledValue sv;
};

class EncodedDataWrapper : public webrtc::EncodedImageBufferInterface {
 public:
  EncodedDataWrapper(uint8_t* data,
                     size_t size,
                     base::OnceClosure reuse_buffer_callback)
      : data_(data),
        size_(size),
        reuse_buffer_callback_(std::move(reuse_buffer_callback)) {}
  ~EncodedDataWrapper() override {
    DCHECK(reuse_buffer_callback_);
    std::move(reuse_buffer_callback_).Run();
  }
  const uint8_t* data() const override { return data_; }
  uint8_t* data() override { return data_; }
  size_t size() const override { return size_; }

 private:
  uint8_t* const data_;
  const size_t size_;
  base::OnceClosure reuse_buffer_callback_;
};

bool ConvertKbpsToBps(uint32_t bitrate_kbps, uint32_t* bitrate_bps) {
  if (!base::IsValueInRangeForNumericType<uint32_t>(bitrate_kbps *
                                                    UINT64_C(1000))) {
    return false;
  }
  *bitrate_bps = bitrate_kbps * 1000;
  return true;
}
}  // namespace

namespace WTF {

template <>
struct CrossThreadCopier<webrtc::VideoEncoder::RateControlParameters>
    : public CrossThreadCopierPassThrough<
          webrtc::VideoEncoder::RateControlParameters> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<
    std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>>
    : public CrossThreadCopierPassThrough<
          std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<SignaledValue> {
  static SignaledValue Copy(SignaledValue sv) {
    return sv;  // this is a move in fact.
  }
};
}  // namespace WTF

namespace blink {

namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "RTCVideoEncoderShutdownReason" in src/tools/metrics/histograms/enums.xml.
enum class RTCVideoEncoderShutdownReason {
  kSuccessfulRelease = 0,
  kInvalidArgument = 1,
  kIllegalState = 2,
  kPlatformFailure = 3,
  kMaxValue = kPlatformFailure,
};

static_assert(static_cast<int>(RTCVideoEncoderShutdownReason::kMaxValue) ==
                  media::VideoEncodeAccelerator::kErrorMax + 1,
              "RTCVideoEncoderShutdownReason should follow "
              "VideoEncodeAccelerator::Error (+1 for the success case)");

webrtc::VideoEncoder::EncoderInfo CopyToWebrtcEncoderInfo(
    const media::VideoEncoderInfo& enc_info) {
  webrtc::VideoEncoder::EncoderInfo info;
  info.implementation_name = enc_info.implementation_name;
  info.supports_native_handle = enc_info.supports_native_handle;
  info.has_trusted_rate_controller = enc_info.has_trusted_rate_controller;
  info.is_hardware_accelerated = enc_info.is_hardware_accelerated;
  info.supports_simulcast = enc_info.supports_simulcast;
  info.is_qp_trusted = enc_info.reports_average_qp;
  static_assert(
      webrtc::kMaxSpatialLayers >= media::VideoEncoderInfo::kMaxSpatialLayers,
      "webrtc::kMaxSpatiallayers is less than "
      "media::VideoEncoderInfo::kMaxSpatialLayers");
  for (size_t i = 0; i < std::size(enc_info.fps_allocation); ++i) {
    if (enc_info.fps_allocation[i].empty())
      continue;
    info.fps_allocation[i] =
        absl::InlinedVector<uint8_t, webrtc::kMaxTemporalStreams>(
            enc_info.fps_allocation[i].begin(),
            enc_info.fps_allocation[i].end());
  }
  for (const auto& limit : enc_info.resolution_bitrate_limits) {
    info.resolution_bitrate_limits.emplace_back(
        limit.frame_size.GetArea(), limit.min_start_bitrate_bps,
        limit.min_bitrate_bps, limit.max_bitrate_bps);
  }
  return info;
}

media::VideoEncodeAccelerator::Config::InterLayerPredMode
CopyFromWebRtcInterLayerPredMode(
    const webrtc::InterLayerPredMode inter_layer_pred) {
  switch (inter_layer_pred) {
    case webrtc::InterLayerPredMode::kOff:
      return media::VideoEncodeAccelerator::Config::InterLayerPredMode::kOff;
    case webrtc::InterLayerPredMode::kOn:
      return media::VideoEncodeAccelerator::Config::InterLayerPredMode::kOn;
    case webrtc::InterLayerPredMode::kOnKeyPic:
      return media::VideoEncodeAccelerator::Config::InterLayerPredMode::
          kOnKeyPic;
  }
}

// Create VEA::Config::SpatialLayer from |codec_settings|. If some config of
// |codec_settings| is not supported, returns false.
bool CreateSpatialLayersConfig(
    const webrtc::VideoCodec& codec_settings,
    std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>*
        spatial_layers,
    media::VideoEncodeAccelerator::Config::InterLayerPredMode*
        inter_layer_pred) {
  absl::optional<webrtc::ScalabilityMode> scalability_mode =
      codec_settings.GetScalabilityMode();

  if (codec_settings.codecType == webrtc::kVideoCodecVP9 &&
      codec_settings.VP9().numberOfSpatialLayers > 1 &&
      !RTCVideoEncoder::Vp9HwSupportForSpatialLayers()) {
    DVLOG(1)
        << "VP9 SVC not yet supported by HW codecs, falling back to software.";
    return false;
  }

  // We fill SpatialLayer only in temporal layer or spatial layer encoding.
  switch (codec_settings.codecType) {
    case webrtc::kVideoCodecH264:
      if (scalability_mode.has_value() &&
          *scalability_mode != webrtc::ScalabilityMode::kL1T1) {
        DVLOG(1)
            << "H264 temporal layers not yet supported by HW codecs, but use"
            << " HW codecs and leave the fallback decision to a webrtc client"
            << " by seeing metadata in webrtc::CodecSpecificInfo";

        return true;
      }
      break;
    case webrtc::kVideoCodecVP8: {
      int number_of_temporal_layers = 1;
      if (scalability_mode.has_value()) {
        switch (*scalability_mode) {
          case webrtc::ScalabilityMode::kL1T1:
            number_of_temporal_layers = 1;
            break;
          case webrtc::ScalabilityMode::kL1T2:
            number_of_temporal_layers = 2;
            break;
          case webrtc::ScalabilityMode::kL1T3:
            number_of_temporal_layers = 3;
            break;
          default:
            // Other modes not supported.
            return false;
        }
      }
      if (number_of_temporal_layers > 1) {
        if (codec_settings.mode == webrtc::VideoCodecMode::kScreensharing) {
          // This is a VP8 stream with screensharing using temporal layers for
          // temporal scalability. Since this implementation does not yet
          // implement temporal layers, fall back to software codec, if cfm and
          // board is known to have a CPU that can handle it.
          if (base::FeatureList::IsEnabled(
                  features::kWebRtcScreenshareSwEncoding)) {
            // TODO(sprang): Add support for temporal layers so we don't need
            // fallback. See eg http://crbug.com/702017
            DVLOG(1) << "Falling back to software encoder.";
            return false;
          }
        }
        // Though there is no SVC in VP8 spec. We allocate 1 element in
        // spatial_layers for temporal layer encoding.
        spatial_layers->resize(1u);
        auto& sl = (*spatial_layers)[0];
        sl.width = codec_settings.width;
        sl.height = codec_settings.height;
        if (!ConvertKbpsToBps(codec_settings.startBitrate, &sl.bitrate_bps))
          return false;
        sl.framerate = codec_settings.maxFramerate;
        sl.max_qp = base::saturated_cast<uint8_t>(codec_settings.qpMax);
        sl.num_of_temporal_layers =
            base::saturated_cast<uint8_t>(number_of_temporal_layers);
      }
      break;
    }
    case webrtc::kVideoCodecVP9:
      // Since one TL and one SL can be regarded as one simple stream,
      // SpatialLayer is not filled.
      if (codec_settings.VP9().numberOfTemporalLayers > 1 ||
          codec_settings.VP9().numberOfSpatialLayers > 1) {
        spatial_layers->clear();
        for (size_t i = 0; i < codec_settings.VP9().numberOfSpatialLayers;
             ++i) {
          const webrtc::SpatialLayer& rtc_sl = codec_settings.spatialLayers[i];
          // We ignore non active spatial layer and don't proceed further. There
          // must NOT be an active higher spatial layer than non active spatial
          // layer.
          if (!rtc_sl.active)
            break;
          spatial_layers->emplace_back();
          auto& sl = spatial_layers->back();
          sl.width = base::checked_cast<int32_t>(rtc_sl.width);
          sl.height = base::checked_cast<int32_t>(rtc_sl.height);
          if (!ConvertKbpsToBps(rtc_sl.targetBitrate, &sl.bitrate_bps))
            return false;
          sl.framerate = base::saturated_cast<int32_t>(rtc_sl.maxFramerate);
          sl.max_qp = base::saturated_cast<uint8_t>(rtc_sl.qpMax);
          sl.num_of_temporal_layers =
              base::saturated_cast<uint8_t>(rtc_sl.numberOfTemporalLayers);
        }

        if (spatial_layers->size() == 1 &&
            spatial_layers->at(0).num_of_temporal_layers == 1) {
          // Don't report spatial layers if only the base layer is active and we
          // have no temporar layers configured.
          spatial_layers->clear();
        } else {
          *inter_layer_pred = CopyFromWebRtcInterLayerPredMode(
              codec_settings.VP9().interLayerPred);
        }
      }
      break;
    default:
      break;
  }
  return true;
}

class PendingFrame {
 public:
  PendingFrame(const base::TimeDelta& media_timestamp,
               int32_t rtp_timestamp,
               int64_t capture_time_ms,
               const std::vector<gfx::Size>& resolutions)
      : media_timestamp_(media_timestamp),
        rtp_timestamp_(rtp_timestamp),
        capture_time_ms_(capture_time_ms),
        resolutions_(resolutions) {}

  const base::TimeDelta media_timestamp_;
  const int32_t rtp_timestamp_;
  const int64_t capture_time_ms_;
  const std::vector<gfx::Size> resolutions_;
  size_t produced_frames_ = 0;
};

webrtc::VideoCodecType ProfileToWebRtcVideoCodecType(
    media::VideoCodecProfile profile) {
  if (profile >= media::VP8PROFILE_MIN && profile <= media::VP8PROFILE_MAX) {
    return webrtc::kVideoCodecVP8;
  } else if (profile == media::VP9PROFILE_MIN) {
    return webrtc::kVideoCodecVP9;
  } else if (profile >= media::H264PROFILE_MIN &&
             profile <= media::H264PROFILE_MAX) {
    return webrtc::kVideoCodecH264;
  } else if (profile >= media::AV1PROFILE_MIN &&
             profile <= media::AV1PROFILE_MAX) {
    return webrtc::kVideoCodecAV1;
  }
  NOTREACHED() << "Invalid profile " << GetProfileName(profile);
  return webrtc::kVideoCodecGeneric;
}

void RecordInitEncodeUMA(int32_t init_retval,
                         media::VideoCodecProfile profile) {
  base::UmaHistogramBoolean("Media.RTCVideoEncoderInitEncodeSuccess",
                            init_retval == WEBRTC_VIDEO_CODEC_OK);
  if (init_retval != WEBRTC_VIDEO_CODEC_OK)
    return;
  UMA_HISTOGRAM_ENUMERATION("Media.RTCVideoEncoderProfile", profile,
                            media::VIDEO_CODEC_PROFILE_MAX + 1);
}

void RecordEncoderShutdownReasonUMA(RTCVideoEncoderShutdownReason reason,
                                    webrtc::VideoCodecType type) {
  switch (type) {
    case webrtc::VideoCodecType::kVideoCodecH264:
      base::UmaHistogramEnumeration("Media.RTCVideoEncoderShutdownReason.H264",
                                    reason);
      break;
    case webrtc::VideoCodecType::kVideoCodecVP8:
      base::UmaHistogramEnumeration("Media.RTCVideoEncoderShutdownReason.VP8",
                                    reason);
      break;
    case webrtc::VideoCodecType::kVideoCodecVP9:
      base::UmaHistogramEnumeration("Media.RTCVideoEncoderShutdownReason.VP9",
                                    reason);
      break;
    case webrtc::VideoCodecType::kVideoCodecAV1:
      base::UmaHistogramEnumeration("Media.RTCVideoEncoderShutdownReason.AV1",
                                    reason);
      break;
    default:
      base::UmaHistogramEnumeration("Media.RTCVideoEncoderShutdownReason.Other",
                                    reason);
  }
}
}  // namespace

namespace features {

// Fallback from hardware encoder (if available) to software, for WebRTC
// screensharing that uses temporal scalability.
BASE_FEATURE(kWebRtcScreenshareSwEncoding,
             "WebRtcScreenshareSwEncoding",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

// This private class of RTCVideoEncoder does the actual work of communicating
// with a media::VideoEncodeAccelerator for handling video encoding.  It can
// be created on any thread, but should subsequently be posted to (and Destroy()
// called on) a single thread.
//
// This class separates state related to the thread that RTCVideoEncoder
// operates on from the thread that |gpu_factories_| provides for accelerator
// operations (presently the media thread).
class RTCVideoEncoder::Impl
    : public media::VideoEncodeAccelerator::Client,
      public base::RefCountedThreadSafe<RTCVideoEncoder::Impl> {
 public:
  Impl(media::GpuVideoAcceleratorFactories* gpu_factories,
       webrtc::VideoCodecType video_codec_type,
       webrtc::VideoContentType video_content_type);
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  // Create the VEA and call Initialize() on it.  Called once per instantiation,
  // and then the instance is bound forevermore to whichever thread made the
  // call.
  // RTCVideoEncoder expects to be able to call this function synchronously from
  // its own thread, hence the |init_event| argument.
  void CreateAndInitializeVEA(
      const gfx::Size& input_visible_size,
      uint32_t bitrate,
      media::VideoCodecProfile profile,
      bool is_constrained_h264,
      const std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>&
          spatial_layers,
      media::VideoEncodeAccelerator::Config::InterLayerPredMode
          inter_layer_pred,
      SignaledValue init_event);

  webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const;

  // Enqueue a frame from WebRTC for encoding.
  // RTCVideoEncoder expects to be able to call this function synchronously from
  // its own thread, hence the |encode_event| argument.
  void Enqueue(const webrtc::VideoFrame* input_frame,
               bool force_keyframe,
               SignaledValue encode_event);

  // RTCVideoEncoder is given a buffer to be passed to WebRTC through the
  // RTCVideoEncoder::ReturnEncodedImage() function.  When that is complete,
  // the buffer is returned to Impl by its index using this function.
  void UseOutputBitstreamBufferId(int32_t bitstream_buffer_id);

  // Request encoding parameter change for the underlying encoder.
  void RequestEncodingParametersChange(
      const webrtc::VideoEncoder::RateControlParameters& parameters);

  void RegisterEncodeCompleteCallback(SignaledValue scoped_event,
                                      webrtc::EncodedImageCallback* callback);

  // Destroy this Impl's encoder.  The destructor is not explicitly called, as
  // Impl is a base::RefCountedThreadSafe.
  void Destroy(SignaledValue event);

  webrtc::VideoCodecType video_codec_type() const { return video_codec_type_; }

  // media::VideoEncodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(
      int32_t bitstream_buffer_id,
      const media::BitstreamBufferMetadata& metadata) override;
  void NotifyError(media::VideoEncodeAccelerator::Error error) override;
  void NotifyEncoderInfoChange(const media::VideoEncoderInfo& info) override;

 private:
  friend class base::RefCountedThreadSafe<Impl>;

  enum {
    kInputBufferExtraCount = 1,  // The number of input buffers allocated, more
                                 // than what is requested by
                                 // VEA::RequireBitstreamBuffers().
    kOutputBufferCount = 3,
  };

  ~Impl() override;

  // Logs the |error| and |str| sent from |location| and NotifyError()s forward.
  void LogAndNotifyError(const base::Location& location,
                         const String& str,
                         media::VideoEncodeAccelerator::Error error);

  // Perform encoding on an input frame from the input queue.
  void EncodeOneFrame();

  // Perform encoding on an input frame from the input queue using VEA native
  // input mode.  The input frame must be backed with GpuMemoryBuffer buffers.
  void EncodeOneFrameWithNativeInput();

  // Creates a GpuMemoryBuffer frame filled with black pixels. Returns true if
  // the frame is successfully created; false otherwise.
  bool CreateBlackGpuMemoryBufferFrame(const gfx::Size& natural_size);

  // Notify that an input frame is finished for encoding. |index| is the index
  // of the completed frame in |input_buffers_|.
  void InputBufferReleased(int index);

  // Checks if the frame size is different than hardware accelerator
  // requirements.
  bool RequiresSizeChange(const media::VideoFrame& frame) const;

  // Return an encoded output buffer to WebRTC.
  void ReturnEncodedImage(const webrtc::EncodedImage& image,
                          const webrtc::CodecSpecificInfo& info,
                          int32_t bitstream_buffer_id);

  // Records |failed_timestamp_match_| value after a session.
  void RecordTimestampMatchUMA() const;

  // Get a list of the spatial layer resolutions that are currently active,
  // meaning the are configured, have active=true and have non-zero bandwidth
  // allocated to them.
  // Returns an empty list is spatial layers are not used.
  std::vector<gfx::Size> ActiveSpatialResolutions() const;

  // This is attached to |gpu_task_runner_|, not the thread class is constructed
  // on.
  SEQUENCE_CHECKER(sequence_checker_);

  // Factory for creating VEAs, shared memory buffers, etc.
  media::GpuVideoAcceleratorFactories* gpu_factories_;

  // webrtc::VideoEncoder expects InitEncode() and Encode() to be synchronous.
  // Do this by waiting on the |async_init_event_| when initialization
  // completes, on |async_encode_event_| when encoding completes and on both
  // when an error occurs.
  ScopedSignaledValue async_init_event_;
  ScopedSignaledValue async_encode_event_;

  // The underlying VEA to perform encoding on.
  std::unique_ptr<media::VideoEncodeAccelerator> video_encoder_;

  // Metadata for pending frames, matched to encoded frames using timestamps.
  WTF::Deque<PendingFrame> pending_frames_;

  // Indicates that timestamp match failed and we should no longer attempt
  // matching.
  bool failed_timestamp_match_{false};

  // Next input frame.  Since there is at most one next frame, a single-element
  // queue is sufficient.
  const webrtc::VideoFrame* input_next_frame_{nullptr};

  // Whether to encode a keyframe next.
  bool input_next_frame_keyframe_{false};

  // Frame sizes.
  gfx::Size input_frame_coded_size_;
  gfx::Size input_visible_size_;

  // Shared memory buffers for input/output with the VEA.
  Vector<std::unique_ptr<base::MappedReadOnlyRegion>> input_buffers_;

  Vector<std::pair<base::UnsafeSharedMemoryRegion,
                   base::WritableSharedMemoryMapping>>
      output_buffers_;

  // Input buffers ready to be filled with input from Encode().  As a LIFO since
  // we don't care about ordering.
  Vector<int> input_buffers_free_;

  // The number of output buffers ready to be filled with output from the
  // encoder.
  int output_buffers_free_count_{0};

  // Whether to send the frames to VEA as native buffer. Native buffer allows
  // VEA to pass the buffer to the encoder directly without further processing.
  bool use_native_input_{false};

  // A black GpuMemoryBuffer frame used when the video track is disabled.
  scoped_refptr<media::VideoFrame> black_gmb_frame_;

  // webrtc::VideoEncoder encode complete callback.
  webrtc::EncodedImageCallback* encoded_image_callback_{nullptr};

  // The video codec type, as reported to WebRTC.
  const webrtc::VideoCodecType video_codec_type_;

  // The content type, as reported to WebRTC (screenshare vs realtime video).
  const webrtc::VideoContentType video_content_type_;

  // This has the same information as |encoder_info_.preferred_pixel_formats|
  // but can be used on |sequence_checker_| without acquiring the lock.
  absl::InlinedVector<webrtc::VideoFrameBuffer::Type,
                      webrtc::kMaxPreferredPixelFormats>
      preferred_pixel_formats_;

  // The reslutions of active spatial layer, only used when |Vp9Metadata| is
  // contained in |BitstreamBufferMetadata|. it will be updated when key frame
  // is produced.
  std::vector<gfx::Size> current_spatial_layer_resolutions_;

  // Index of the highest spatial layer with bandwidth allocated for it.
  size_t highest_active_spatial_index_{0};

  // We cannot immediately return error conditions to the WebRTC user of this
  // class, as there is no error callback in the webrtc::VideoEncoder interface.
  // Instead, we cache an error status here and return it the next time an
  // interface entry point is called.
  int32_t status_ GUARDED_BY_CONTEXT(sequence_checker_){
      WEBRTC_VIDEO_CODEC_UNINITIALIZED};

  // Protect |encoder_info_|. |encoder_info_| is read or written on
  // |gpu_task_runner_| in Impl. It can be read in RTCVideoEncoder on other
  // threads.
  mutable base::Lock lock_;

  webrtc::VideoEncoder::EncoderInfo encoder_info_ GUARDED_BY(lock_);
};

RTCVideoEncoder::Impl::Impl(media::GpuVideoAcceleratorFactories* gpu_factories,
                            webrtc::VideoCodecType video_codec_type,
                            webrtc::VideoContentType video_content_type)
    : gpu_factories_(gpu_factories),
      video_codec_type_(video_codec_type),
      video_content_type_(video_content_type) {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  // The default values of EncoderInfo.
  // TODO(crbug.com/1228804): These settings should be set at the time
  // RTCVideoEncoder is constructed instead of done here.
  encoder_info_.scaling_settings = webrtc::VideoEncoder::ScalingSettings::kOff;
#if BUILDFLAG(IS_ANDROID)
  // MediaCodec requires 16x16 alignment, see https://crbug.com/1084702.
  encoder_info_.requested_resolution_alignment = 16;
  encoder_info_.apply_alignment_to_all_simulcast_layers = true;
#else
  encoder_info_.requested_resolution_alignment = 1;
  encoder_info_.apply_alignment_to_all_simulcast_layers = false;
#endif
  encoder_info_.supports_native_handle = true;
  encoder_info_.implementation_name = "ExternalEncoder";
  encoder_info_.has_trusted_rate_controller = false;
  encoder_info_.is_hardware_accelerated = true;
  encoder_info_.is_qp_trusted = true;
  encoder_info_.fps_allocation[0] = {
      webrtc::VideoEncoder::EncoderInfo::kMaxFramerateFraction};
  DCHECK(encoder_info_.resolution_bitrate_limits.empty());
  encoder_info_.supports_simulcast = false;
  preferred_pixel_formats_ = {webrtc::VideoFrameBuffer::Type::kI420};
  encoder_info_.preferred_pixel_formats = preferred_pixel_formats_;
}

void RTCVideoEncoder::Impl::CreateAndInitializeVEA(
    const gfx::Size& input_visible_size,
    uint32_t bitrate,
    media::VideoCodecProfile profile,
    bool is_constrained_h264,
    const std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>&
        spatial_layers,
    media::VideoEncodeAccelerator::Config::InterLayerPredMode inter_layer_pred,
    SignaledValue init_event) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  status_ = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  async_init_event_ = ScopedSignaledValue(std::move(init_event));
  async_encode_event_.reset();

  // Check for overflow converting bitrate (kilobits/sec) to bits/sec.
  uint32_t bitrate_bps;
  if (!ConvertKbpsToBps(bitrate, &bitrate_bps)) {
    LogAndNotifyError(FROM_HERE, "Overflow converting bitrate from kbps to bps",
                      media::VideoEncodeAccelerator::kInvalidArgumentError);
    async_init_event_.SetAndReset(WEBRTC_VIDEO_CODEC_ERR_PARAMETER);
    return;
  }

  // Check that |profile| supports |input_visible_size|.
  if (base::FeatureList::IsEnabled(features::kWebRtcUseMinMaxVEADimensions)) {
    const auto vea_supported_profiles =
        gpu_factories_->GetVideoEncodeAcceleratorSupportedProfiles().value_or(
            media::VideoEncodeAccelerator::SupportedProfiles());

    for (const auto& vea_profile : vea_supported_profiles) {
      if (vea_profile.profile == profile &&
          (input_visible_size.width() > vea_profile.max_resolution.width() ||
           input_visible_size.height() > vea_profile.max_resolution.height() ||
           input_visible_size.width() < vea_profile.min_resolution.width() ||
           input_visible_size.height() < vea_profile.min_resolution.height())) {
        LogAndNotifyError(
            FROM_HERE,
            base::StringPrintf(
                "Requested dimensions (%s) beyond accelerator limits (%s - %s)",
                input_visible_size.ToString().c_str(),
                vea_profile.min_resolution.ToString().c_str(),
                vea_profile.max_resolution.ToString().c_str())
                .c_str(),
            media::VideoEncodeAccelerator::kInvalidArgumentError);
        return;
      }
    }
  }

  video_encoder_ = gpu_factories_->CreateVideoEncodeAccelerator();
  if (!video_encoder_) {
    LogAndNotifyError(FROM_HERE, "Error creating VideoEncodeAccelerator",
                      media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  input_visible_size_ = input_visible_size;
  media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_I420;
  auto storage_type =
      media::VideoEncodeAccelerator::Config::StorageType::kShmem;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableVideoCaptureUseGpuMemoryBuffer) &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVideoCaptureUseGpuMemoryBuffer) &&
      video_content_type_ != webrtc::VideoContentType::SCREENSHARE) {
    // Use import mode for camera when GpuMemoryBuffer-based video capture is
    // enabled.
    pixel_format = media::PIXEL_FORMAT_NV12;
    storage_type =
        media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;
    use_native_input_ = true;

    preferred_pixel_formats_ = {webrtc::VideoFrameBuffer::Type::kNV12};
    base::AutoLock lock(lock_);
    encoder_info_.preferred_pixel_formats = preferred_pixel_formats_;
  }
  const media::VideoEncodeAccelerator::Config config(
      pixel_format, input_visible_size_, profile,
      media::Bitrate::ConstantBitrate(bitrate_bps), absl::nullopt,
      absl::nullopt, absl::nullopt, is_constrained_h264, storage_type,
      video_content_type_ == webrtc::VideoContentType::SCREENSHARE
          ? media::VideoEncodeAccelerator::Config::ContentType::kDisplay
          : media::VideoEncodeAccelerator::Config::ContentType::kCamera,
      spatial_layers, inter_layer_pred);
  if (!video_encoder_->Initialize(config, this,
                                  std::make_unique<media::NullMediaLog>())) {
    LogAndNotifyError(FROM_HERE, "Error initializing video_encoder",
                      media::VideoEncodeAccelerator::kInvalidArgumentError);
    return;
  }

  current_spatial_layer_resolutions_.clear();
  for (const auto& layer : spatial_layers)
    current_spatial_layer_resolutions_.emplace_back(layer.width, layer.height);
  highest_active_spatial_index_ =
      spatial_layers.empty() ? 0u : spatial_layers.size() - 1;

  // RequireBitstreamBuffers or NotifyError will be called and the waiter will
  // be signaled.
}

webrtc::VideoEncoder::EncoderInfo RTCVideoEncoder::Impl::GetEncoderInfo()
    const {
  base::AutoLock lock(lock_);
  return encoder_info_;
}

void RTCVideoEncoder::Impl::NotifyEncoderInfoChange(
    const media::VideoEncoderInfo& info) {
  base::AutoLock lock(lock_);
  encoder_info_ = CopyToWebrtcEncoderInfo(info);
}

void RTCVideoEncoder::Impl::Enqueue(const webrtc::VideoFrame* input_frame,
                                    bool force_keyframe,
                                    SignaledValue encode_event) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!input_next_frame_);

  if (status_ != WEBRTC_VIDEO_CODEC_OK) {
    encode_event.Set(status_);
    encode_event.Signal();
    return;
  }

  // If there are no free input and output buffers, drop the frame to avoid a
  // deadlock. If there is a free input buffer and |use_native_input_| is false,
  // EncodeOneFrame will run and unblock Encode(). If there are no free input
  // buffers but there is a free output buffer, InputBufferReleased() will be
  // called later to unblock Encode().
  //
  // The caller of Encode() holds a webrtc lock. The deadlock happens when:
  // (1) Encode() is waiting for the frame to be encoded in EncodeOneFrame().
  // (2) There are no free input buffers and they cannot be freed because
  //     the encoder has no output buffers.
  // (3) Output buffers cannot be freed because OnEncodedImage() is queued
  //     on libjingle worker thread to be run. But the worker thread is waiting
  //     for the same webrtc lock held by the caller of Encode().
  //
  // Dropping a frame is fine. The encoder has been filled with all input
  // buffers. Returning an error in Encode() is not fatal and WebRTC will just
  // continue. If this is a key frame, WebRTC will request a key frame again.
  // Besides, webrtc will drop a frame if Encode() blocks too long.
  if (!use_native_input_ && input_buffers_free_.empty() &&
      output_buffers_free_count_ == 0) {
    DVLOG(2) << "Run out of input and output buffers. Drop the frame.";
    encode_event.Set(WEBRTC_VIDEO_CODEC_ERROR);
    encode_event.Signal();
    return;
  }
  input_next_frame_ = input_frame;
  input_next_frame_keyframe_ = force_keyframe;
  async_encode_event_ = ScopedSignaledValue(std::move(encode_event));

  // If |use_native_input_| is true, then we always queue the frame to the
  // encoder since no intermediate buffer is needed in RTCVideoEncoder.
  if (use_native_input_) {
    EncodeOneFrameWithNativeInput();
    return;
  }

  if (!input_buffers_free_.empty())
    EncodeOneFrame();
}

void RTCVideoEncoder::Impl::UseOutputBitstreamBufferId(
    int32_t bitstream_buffer_id) {
  DVLOG(3) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (video_encoder_) {
    video_encoder_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
        bitstream_buffer_id,
        output_buffers_[bitstream_buffer_id].first.Duplicate(),
        output_buffers_[bitstream_buffer_id].first.GetSize()));
    output_buffers_free_count_++;
  }
}

void RTCVideoEncoder::Impl::RequestEncodingParametersChange(
    const webrtc::VideoEncoder::RateControlParameters& parameters) {
  DVLOG(3) << __func__ << " bitrate=" << parameters.bitrate.ToString()
           << ", framerate=" << parameters.framerate_fps;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status_ != WEBRTC_VIDEO_CODEC_OK)
    return;

  // Destroy() against this has been called. Don't proceed the change request.
  if (!video_encoder_)
    return;

  // This is a workaround to zero being temporarily provided, as part of the
  // initial setup, by WebRTC.
  media::VideoBitrateAllocation allocation;
  if (parameters.bitrate.get_sum_bps() == 0u) {
    allocation.SetBitrate(0, 0, 1u);
  }
  uint32_t framerate =
      std::max(1u, static_cast<uint32_t>(parameters.framerate_fps + 0.5));

  highest_active_spatial_index_ = 0;
  for (size_t spatial_id = 0;
       spatial_id < media::VideoBitrateAllocation::kMaxSpatialLayers;
       ++spatial_id) {
    bool spatial_layer_active = false;
    for (size_t temporal_id = 0;
         temporal_id < media::VideoBitrateAllocation::kMaxTemporalLayers;
         ++temporal_id) {
      // TODO(sprang): Clean this up if/when webrtc struct moves to int.
      uint32_t temporal_layer_bitrate = base::checked_cast<int>(
          parameters.bitrate.GetBitrate(spatial_id, temporal_id));
      if (!allocation.SetBitrate(spatial_id, temporal_id,
                                 temporal_layer_bitrate)) {
        LOG(WARNING) << "Overflow in bitrate allocation: "
                     << parameters.bitrate.ToString();
        break;
      }
      if (temporal_layer_bitrate > 0)
        spatial_layer_active = true;
    }

    if (spatial_layer_active &&
        spatial_id < current_spatial_layer_resolutions_.size())
      highest_active_spatial_index_ = spatial_id;
  }
  DCHECK_EQ(allocation.GetSumBps(), parameters.bitrate.get_sum_bps());
  video_encoder_->RequestEncodingParametersChange(allocation, framerate);
}

void RTCVideoEncoder::Impl::Destroy(SignaledValue event) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordTimestampMatchUMA();
  if (video_encoder_) {
    video_encoder_.reset();
    status_ = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    RecordEncoderShutdownReasonUMA(
        RTCVideoEncoderShutdownReason::kSuccessfulRelease, video_codec_type_);
  }

  async_init_event_.reset();
  async_encode_event_.reset();
  event.Signal();
}

void RTCVideoEncoder::Impl::RecordTimestampMatchUMA() const {
  base::UmaHistogramBoolean("Media.RTCVideoEncoderTimestampMatchSuccess",
                            !failed_timestamp_match_);
}

std::vector<gfx::Size> RTCVideoEncoder::Impl::ActiveSpatialResolutions() const {
  if (current_spatial_layer_resolutions_.empty())
    return {};
  DCHECK_LT(highest_active_spatial_index_,
            current_spatial_layer_resolutions_.size());
  return std::vector<gfx::Size>(current_spatial_layer_resolutions_.begin(),
                                current_spatial_layer_resolutions_.begin() +
                                    highest_active_spatial_index_ + 1);
}

void RTCVideoEncoder::Impl::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  DVLOG(3) << __func__ << " input_count=" << input_count
           << ", input_coded_size=" << input_coded_size.ToString()
           << ", output_buffer_size=" << output_buffer_size;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto scoped_event = std::move(async_init_event_);
  if (!video_encoder_)
    return;

  input_frame_coded_size_ = input_coded_size;

  // |input_buffers_| is only needed in non import mode.
  if (!use_native_input_) {
    const wtf_size_t num_input_buffers = input_count + kInputBufferExtraCount;
    input_buffers_free_.resize(num_input_buffers);
    input_buffers_.resize(num_input_buffers);
    for (wtf_size_t i = 0; i < num_input_buffers; i++) {
      input_buffers_free_[i] = i;
      input_buffers_[i] = nullptr;
    }
  }

  for (int i = 0; i < kOutputBufferCount; ++i) {
    base::UnsafeSharedMemoryRegion region =
        gpu_factories_->CreateSharedMemoryRegion(output_buffer_size);
    base::WritableSharedMemoryMapping mapping = region.Map();
    if (!mapping.IsValid()) {
      LogAndNotifyError(FROM_HERE, "failed to create output buffer",
                        media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    output_buffers_.push_back(
        std::make_pair(std::move(region), std::move(mapping)));
  }

  // Immediately provide all output buffers to the VEA.
  for (wtf_size_t i = 0; i < output_buffers_.size(); ++i) {
    video_encoder_->UseOutputBitstreamBuffer(
        media::BitstreamBuffer(i, output_buffers_[i].first.Duplicate(),
                               output_buffers_[i].first.GetSize()));
    output_buffers_free_count_++;
  }
  DCHECK_EQ(status_, WEBRTC_VIDEO_CODEC_UNINITIALIZED);
  status_ = WEBRTC_VIDEO_CODEC_OK;

  scoped_event.SetAndReset(WEBRTC_VIDEO_CODEC_OK);
}

void RTCVideoEncoder::Impl::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const media::BitstreamBufferMetadata& metadata) {
  DVLOG(3) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id
           << ", payload_size=" << metadata.payload_size_bytes
           << ", key_frame=" << metadata.key_frame
           << ", timestamp ms=" << metadata.timestamp.InMilliseconds();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (bitstream_buffer_id < 0 ||
      bitstream_buffer_id >= static_cast<int>(output_buffers_.size())) {
    LogAndNotifyError(FROM_HERE, "invalid bitstream_buffer_id",
                      media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  void* output_mapping_memory =
      output_buffers_[bitstream_buffer_id].second.memory();
  if (metadata.payload_size_bytes >
      output_buffers_[bitstream_buffer_id].second.size()) {
    LogAndNotifyError(FROM_HERE, "invalid payload_size",
                      media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  output_buffers_free_count_--;

  // Find RTP and capture timestamps by going through |pending_timestamps_|.
  // Derive it from current time otherwise.
  absl::optional<uint32_t> rtp_timestamp;
  absl::optional<int64_t> capture_timestamp_ms;
  absl::optional<std::vector<gfx::Size>> expected_resolutions;
  if (!failed_timestamp_match_) {
    // Pop timestamps until we have a match.
    while (!pending_frames_.empty()) {
      auto& front_frame = pending_frames_.front();
      const bool end_of_picture = !metadata.vp9 || metadata.vp9->end_of_picture;
      if (front_frame.media_timestamp_ == metadata.timestamp) {
        rtp_timestamp = front_frame.rtp_timestamp_;
        capture_timestamp_ms = front_frame.capture_time_ms_;
        expected_resolutions = front_frame.resolutions_;
        const size_t num_resolutions =
            std::max(front_frame.resolutions_.size(), size_t{1});
        ++front_frame.produced_frames_;

        if (front_frame.produced_frames_ == num_resolutions &&
            !end_of_picture) {
          // The top layer must always have the end-of-picture indicator.
          LogAndNotifyError(
              FROM_HERE, "missing end-of-picture",
              media::VideoEncodeAccelerator::kPlatformFailureError);
          return;
        }
        if (end_of_picture) {
          // Remove pending timestamp at the top spatial layer in the case of
          // SVC encoding.
          if (!front_frame.resolutions_.empty() &&
              front_frame.produced_frames_ != front_frame.resolutions_.size()) {
            // At least one resolution was not produced.
            LogAndNotifyError(
                FROM_HERE, "missing resolution",
                media::VideoEncodeAccelerator::kPlatformFailureError);
            return;
          }
          pending_frames_.pop_front();
        }
        break;
      }
      // Timestamp does not match front of the pending frames list.
      if (end_of_picture)
        pending_frames_.pop_front();
    }
    DCHECK(rtp_timestamp.has_value());
  }
  if (!rtp_timestamp.has_value() || !capture_timestamp_ms.has_value()) {
    failed_timestamp_match_ = true;
    pending_frames_.clear();
    const int64_t current_time_ms =
        rtc::TimeMicros() / base::Time::kMicrosecondsPerMillisecond;
    // RTP timestamp can wrap around. Get the lower 32 bits.
    rtp_timestamp = static_cast<uint32_t>(current_time_ms * 90);
    capture_timestamp_ms = current_time_ms;
  }

  webrtc::EncodedImage image;
  image.SetEncodedData(rtc::make_ref_counted<EncodedDataWrapper>(
      static_cast<uint8_t*>(output_mapping_memory), metadata.payload_size_bytes,
      media::BindToCurrentLoop(
          base::BindOnce(&RTCVideoEncoder::Impl::UseOutputBitstreamBufferId,
                         WrapRefCounted(this), bitstream_buffer_id))));
  image._encodedWidth = input_visible_size_.width();
  image._encodedHeight = input_visible_size_.height();
  image.SetTimestamp(rtp_timestamp.value());
  image.capture_time_ms_ = capture_timestamp_ms.value();
  image._frameType =
      (metadata.key_frame ? webrtc::VideoFrameType::kVideoFrameKey
                          : webrtc::VideoFrameType::kVideoFrameDelta);
  image.content_type_ = video_content_type_;
  // Default invalid qp value is -1 in webrtc::EncodedImage and
  // media::BitstreamBufferMetadata, and libwebrtc would parse bitstream to get
  // the qp if |qp_| is less than zero.
  image.qp_ = metadata.qp;

  webrtc::CodecSpecificInfo info;
  info.codecType = video_codec_type_;
  switch (video_codec_type_) {
    case webrtc::kVideoCodecH264: {
      webrtc::CodecSpecificInfoH264& h264 = info.codecSpecific.H264;
      h264.packetization_mode = webrtc::H264PacketizationMode::NonInterleaved;
      h264.idr_frame = metadata.key_frame;
      if (metadata.h264) {
        h264.temporal_idx = metadata.h264->temporal_idx;
        h264.base_layer_sync = metadata.h264->layer_sync;
      } else {
        h264.temporal_idx = webrtc::kNoTemporalIdx;
        h264.base_layer_sync = false;
      }
    } break;
    case webrtc::kVideoCodecVP8:
      info.codecSpecific.VP8.keyIdx = -1;
      break;
    case webrtc::kVideoCodecVP9: {
      webrtc::CodecSpecificInfoVP9& vp9 = info.codecSpecific.VP9;
      if (metadata.vp9) {
        // Temporal and/or spatial layer stream.
        if (!metadata.vp9->spatial_layer_resolutions.empty() &&
            expected_resolutions != metadata.vp9->spatial_layer_resolutions) {
          LogAndNotifyError(
              FROM_HERE, "Encoded SVC resolution set does not match request.",
              media::VideoEncodeAccelerator::kPlatformFailureError);
          return;
        }

        const uint8_t spatial_index = metadata.vp9->spatial_idx;
        if (spatial_index >= current_spatial_layer_resolutions_.size()) {
          LogAndNotifyError(
              FROM_HERE, "invalid spatial index",
              media::VideoEncodeAccelerator::kPlatformFailureError);
          return;
        }
        image.SetSpatialIndex(spatial_index);
        image._encodedWidth =
            current_spatial_layer_resolutions_[spatial_index].width();
        image._encodedHeight =
            current_spatial_layer_resolutions_[spatial_index].height();

        vp9.first_frame_in_picture = spatial_index == 0;
        vp9.inter_pic_predicted = metadata.vp9->inter_pic_predicted;
        vp9.non_ref_for_inter_layer_pred =
            !metadata.vp9->referenced_by_upper_spatial_layers;
        vp9.temporal_idx = metadata.vp9->temporal_idx;
        vp9.temporal_up_switch = metadata.vp9->temporal_up_switch;
        vp9.inter_layer_predicted =
            metadata.vp9->reference_lower_spatial_layers;
        vp9.num_ref_pics = metadata.vp9->p_diffs.size();
        for (size_t i = 0; i < metadata.vp9->p_diffs.size(); ++i)
          vp9.p_diff[i] = metadata.vp9->p_diffs[i];
        vp9.ss_data_available = metadata.key_frame;
        vp9.first_active_layer = 0;
        vp9.num_spatial_layers = current_spatial_layer_resolutions_.size();
        if (vp9.ss_data_available) {
          vp9.spatial_layer_resolution_present = true;
          vp9.gof.num_frames_in_gof = 0;
          for (size_t i = 0; i < vp9.num_spatial_layers; ++i) {
            vp9.width[i] = current_spatial_layer_resolutions_[i].width();
            vp9.height[i] = current_spatial_layer_resolutions_[i].height();
          }
        }
        vp9.flexible_mode = true;
        vp9.gof_idx = 0;
        info.end_of_picture = metadata.vp9->end_of_picture;
      } else {
        // Simple stream, neither temporal nor spatial layer stream.
        vp9.flexible_mode = false;
        vp9.temporal_idx = webrtc::kNoTemporalIdx;
        vp9.temporal_up_switch = true;
        vp9.inter_layer_predicted = false;
        vp9.gof_idx = 0;
        vp9.num_spatial_layers = 1;
        vp9.first_frame_in_picture = true;
        vp9.spatial_layer_resolution_present = false;
        vp9.inter_pic_predicted = !metadata.key_frame;
        vp9.ss_data_available = metadata.key_frame;
        if (vp9.ss_data_available) {
          vp9.spatial_layer_resolution_present = true;
          vp9.width[0] = image._encodedWidth;
          vp9.height[0] = image._encodedHeight;
          vp9.gof.num_frames_in_gof = 1;
          vp9.gof.temporal_idx[0] = 0;
          vp9.gof.temporal_up_switch[0] = false;
          vp9.gof.num_ref_pics[0] = 1;
          vp9.gof.pid_diff[0][0] = 1;
        }
        info.end_of_picture = true;
      }
    } break;
    default:
      break;
  }

  if (!encoded_image_callback_)
    return;

  const auto result = encoded_image_callback_->OnEncodedImage(image, &info);
  if (result.error != webrtc::EncodedImageCallback::Result::OK) {
    DVLOG(2)
        << "ReturnEncodedImage(): webrtc::EncodedImageCallback::Result.error = "
        << result.error;
  }
}

void RTCVideoEncoder::Impl::NotifyError(
    media::VideoEncodeAccelerator::Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int32_t retval = WEBRTC_VIDEO_CODEC_ERROR;
  switch (error) {
    case media::VideoEncodeAccelerator::kInvalidArgumentError:
      retval = WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
      RecordEncoderShutdownReasonUMA(
          RTCVideoEncoderShutdownReason::kInvalidArgument, video_codec_type_);
      break;
    case media::VideoEncodeAccelerator::kIllegalStateError:
      RecordEncoderShutdownReasonUMA(
          RTCVideoEncoderShutdownReason::kIllegalState, video_codec_type_);
      retval = WEBRTC_VIDEO_CODEC_ERROR;
      break;
    case media::VideoEncodeAccelerator::kPlatformFailureError:
      // Some platforms(i.e. Android) do not have SW H264 implementation so
      // check if it is available before asking for fallback.
      retval = video_codec_type_ != webrtc::kVideoCodecH264 ||
                       webrtc::H264Encoder::IsSupported()
                   ? WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE
                   : WEBRTC_VIDEO_CODEC_ERROR;
      RecordEncoderShutdownReasonUMA(
          RTCVideoEncoderShutdownReason::kPlatformFailure, video_codec_type_);
  }
  video_encoder_.reset();

  status_ = retval;

  async_init_event_.SetAndReset(retval);
  async_encode_event_.SetAndReset(retval);
}

RTCVideoEncoder::Impl::~Impl() {
  DCHECK(!video_encoder_);
}

void RTCVideoEncoder::Impl::LogAndNotifyError(
    const base::Location& location,
    const String& str,
    media::VideoEncodeAccelerator::Error error) {
  static const char* const kErrorNames[] = {
      "kIllegalStateError", "kInvalidArgumentError", "kPlatformFailureError"};
  static_assert(
      std::size(kErrorNames) == media::VideoEncodeAccelerator::kErrorMax + 1,
      "Different number of errors and textual descriptions");
  DLOG(ERROR) << location.ToString() << kErrorNames[error] << " - " << str;
  NotifyError(error);
}

void RTCVideoEncoder::Impl::EncodeOneFrame() {
  TRACE_EVENT0("webrtc", "RTCVideoEncoder::Impl::EncodeOneFrame");
  DVLOG(3) << "Impl::EncodeOneFrame()";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(input_next_frame_);
  DCHECK(!input_buffers_free_.empty());

  // EncodeOneFrame() may re-enter InputBufferReleased() if VEA::Encode() fails,
  // we receive a VEA::NotifyError(), and the media::VideoFrame we pass to
  // Encode() gets destroyed early.  Handle this by resetting our
  // input_next_frame_* state before we hand off the VideoFrame to the VEA.
  const webrtc::VideoFrame* next_frame = input_next_frame_;
  const bool next_frame_keyframe = input_next_frame_keyframe_;
  input_next_frame_ = nullptr;
  input_next_frame_keyframe_ = false;
  if (!video_encoder_) {
    async_encode_event_.SetAndReset(WEBRTC_VIDEO_CODEC_ERROR);
    return;
  }

  const base::TimeDelta timestamp =
      base::Microseconds(next_frame->timestamp_us());

  scoped_refptr<media::VideoFrame> frame;
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer =
      next_frame->video_frame_buffer();

  // All non-native frames require a copy because we can't tell if non-copy
  // conditions are met.
  bool requires_copy_or_scale =
      buffer->type() != webrtc::VideoFrameBuffer::Type::kNative;
  if (!requires_copy_or_scale) {
    const WebRtcVideoFrameAdapter* frame_adapter =
        static_cast<WebRtcVideoFrameAdapter*>(buffer.get());
    frame = frame_adapter->getMediaVideoFrame();
    frame->set_timestamp(timestamp);
    const media::VideoFrame::StorageType storage = frame->storage_type();
    const bool is_memory_based_frame =
        storage == media::VideoFrame::STORAGE_UNOWNED_MEMORY ||
        storage == media::VideoFrame::STORAGE_OWNED_MEMORY ||
        storage == media::VideoFrame::STORAGE_SHMEM;
    const bool is_gmb_frame =
        storage == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER;
    requires_copy_or_scale =
        RequiresSizeChange(*frame) || !(is_memory_based_frame || is_gmb_frame);
  }

  if (requires_copy_or_scale) {
    TRACE_EVENT0("webrtc",
                 "RTCVideoEncoder::Impl::EncodeOneFrame::CopyOrScale");
    // Native buffer scaling is performed by WebRtcVideoFrameAdapter, which may
    // be more efficient in some cases. E.g. avoiding I420 conversion or scaling
    // from a middle layer instead of top layer.
    //
    // Native buffer scaling is only supported when `input_frame_coded_size_`
    // and `input_visible_size_` strides match. This ensures the strides of the
    // frame that we pass to the encoder fits the input requirements.
    bool native_buffer_scaling =
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
        buffer->type() == webrtc::VideoFrameBuffer::Type::kNative &&
        input_frame_coded_size_ == input_visible_size_;
#else
        // TODO(https://crbug.com/1307206): Android (e.g. android-pie-arm64-rel)
        // and CrOS does not support the native buffer scaling path. Investigate
        // why and find a way to enable it, if possible.
        false;
#endif
    if (native_buffer_scaling) {
      DCHECK_EQ(buffer->type(), webrtc::VideoFrameBuffer::Type::kNative);
      auto scaled_buffer = buffer->Scale(input_visible_size_.width(),
                                         input_visible_size_.height());
      auto mapped_buffer =
          scaled_buffer->GetMappedFrameBuffer(preferred_pixel_formats_);
      if (!mapped_buffer) {
        mapped_buffer = scaled_buffer->ToI420();
      }
      if (!mapped_buffer) {
        LogAndNotifyError(FROM_HERE, "Failed to map or convert buffer to I420",
                          media::VideoEncodeAccelerator::kPlatformFailureError);
        NOTREACHED();
        return;
      }
      DCHECK_NE(mapped_buffer->type(), webrtc::VideoFrameBuffer::Type::kNative);
      frame = ConvertFromMappedWebRtcVideoFrameBuffer(mapped_buffer, timestamp);
      if (!frame) {
        LogAndNotifyError(
            FROM_HERE,
            "Failed to convert WebRTC mapped buffer to media::VideoFrame",
            media::VideoEncodeAccelerator::kPlatformFailureError);
        NOTREACHED();
        return;
      }
    } else {
      const int index = input_buffers_free_.back();
      if (!input_buffers_[index]) {
        const size_t input_frame_buffer_size =
            media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_I420,
                                              input_frame_coded_size_);
        input_buffers_[index] = std::make_unique<base::MappedReadOnlyRegion>(
            base::ReadOnlySharedMemoryRegion::Create(input_frame_buffer_size));
        if (!input_buffers_[index]) {
          LogAndNotifyError(
              FROM_HERE, "Failed to create input buffer",
              media::VideoEncodeAccelerator::kPlatformFailureError);
          return;
        }
      }

      auto& region = input_buffers_[index]->region;
      auto& mapping = input_buffers_[index]->mapping;
      frame = media::VideoFrame::WrapExternalData(
          media::PIXEL_FORMAT_I420, input_frame_coded_size_,
          gfx::Rect(input_visible_size_), input_visible_size_,
          static_cast<uint8_t*>(mapping.memory()), mapping.size(), timestamp);
      if (!frame) {
        LogAndNotifyError(FROM_HERE, "failed to create frame",
                          media::VideoEncodeAccelerator::kPlatformFailureError);
        async_encode_event_.SetAndReset(WEBRTC_VIDEO_CODEC_ERROR);
        return;
      }

      // |frame| is STORAGE_UNOWNED_MEMORY at this point. Writing the data is
      // allowed.
      // Do a strided copy and scale (if necessary) the input frame to match
      // the input requirements for the encoder.
      // TODO(magjed): Downscale with an image pyramid instead.
      rtc::scoped_refptr<webrtc::I420BufferInterface> i420_buffer =
          next_frame->video_frame_buffer()->ToI420();
      if (libyuv::I420Scale(
              i420_buffer->DataY(), i420_buffer->StrideY(),
              i420_buffer->DataU(), i420_buffer->StrideU(),
              i420_buffer->DataV(), i420_buffer->StrideV(), next_frame->width(),
              next_frame->height(),
              frame->GetWritableVisibleData(media::VideoFrame::kYPlane),
              frame->stride(media::VideoFrame::kYPlane),
              frame->GetWritableVisibleData(media::VideoFrame::kUPlane),
              frame->stride(media::VideoFrame::kUPlane),
              frame->GetWritableVisibleData(media::VideoFrame::kVPlane),
              frame->stride(media::VideoFrame::kVPlane),
              frame->visible_rect().width(), frame->visible_rect().height(),
              libyuv::kFilterBox)) {
        LogAndNotifyError(FROM_HERE, "Failed to copy buffer",
                          media::VideoEncodeAccelerator::kPlatformFailureError);
        async_encode_event_.SetAndReset(WEBRTC_VIDEO_CODEC_ERROR);
        return;
      }

      // |frame| becomes STORAGE_SHMEM. Writing the buffer is not permitted
      // after here.
      frame->BackWithSharedMemory(&region);

      input_buffers_free_.pop_back();
      frame->AddDestructionObserver(media::BindToCurrentLoop(
          WTF::BindOnce(&RTCVideoEncoder::Impl::InputBufferReleased,
                        scoped_refptr<RTCVideoEncoder::Impl>(this), index)));
    }
  }
  if (!failed_timestamp_match_) {
    DCHECK(!base::Contains(pending_frames_, timestamp,
                           &PendingFrame::media_timestamp_));
    pending_frames_.emplace_back(timestamp, next_frame->timestamp(),
                                 next_frame->render_time_ms(),
                                 ActiveSpatialResolutions());
  }
  video_encoder_->Encode(frame, next_frame_keyframe);
  async_encode_event_.SetAndReset(WEBRTC_VIDEO_CODEC_OK);
}

void RTCVideoEncoder::Impl::EncodeOneFrameWithNativeInput() {
  TRACE_EVENT0("webrtc",
               "RTCVideoEncoder::Impl::EncodeOneFrameWithNativeInput");
  DVLOG(3) << "Impl::EncodeOneFrameWithNativeInput()";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(input_buffers_.empty() && input_buffers_free_.empty());
  DCHECK(input_next_frame_);

  const webrtc::VideoFrame* next_frame = input_next_frame_;
  const bool next_frame_keyframe = input_next_frame_keyframe_;
  input_next_frame_ = nullptr;
  input_next_frame_keyframe_ = false;

  if (!video_encoder_) {
    async_encode_event_.SetAndReset(WEBRTC_VIDEO_CODEC_ERROR);
    return;
  }

  scoped_refptr<media::VideoFrame> frame;
  if (next_frame->video_frame_buffer()->type() !=
      webrtc::VideoFrameBuffer::Type::kNative) {
    // If we get a non-native frame it's because the video track is disabled and
    // WebRTC VideoBroadcaster replaces the camera frame with a black YUV frame.
    if (!black_gmb_frame_) {
      gfx::Size natural_size(next_frame->width(), next_frame->height());
      if (!CreateBlackGpuMemoryBufferFrame(natural_size)) {
        DVLOG(2) << "Failed to allocate native buffer for black frame";
        async_encode_event_.SetAndReset(WEBRTC_VIDEO_CODEC_ERROR);
        return;
      }
    }
    frame = media::VideoFrame::WrapVideoFrame(
        black_gmb_frame_, black_gmb_frame_->format(),
        black_gmb_frame_->visible_rect(), black_gmb_frame_->natural_size());
  } else {
    frame = static_cast<WebRtcVideoFrameAdapter*>(
                next_frame->video_frame_buffer().get())
                ->getMediaVideoFrame();
  }
  frame->set_timestamp(base::Microseconds(next_frame->timestamp_us()));

  if (frame->storage_type() != media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    async_encode_event_.SetAndReset(WEBRTC_VIDEO_CODEC_ERROR);
    LogAndNotifyError(FROM_HERE, "frame isn't GpuMemoryBuffer based VideoFrame",
                      media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  if (!failed_timestamp_match_) {
    DCHECK(!base::Contains(pending_frames_, frame->timestamp(),
                           &PendingFrame::media_timestamp_));
    pending_frames_.emplace_back(frame->timestamp(), next_frame->timestamp(),
                                 next_frame->render_time_ms(),
                                 ActiveSpatialResolutions());
  }
  video_encoder_->Encode(frame, next_frame_keyframe);
  async_encode_event_.SetAndReset(WEBRTC_VIDEO_CODEC_OK);
}

bool RTCVideoEncoder::Impl::CreateBlackGpuMemoryBufferFrame(
    const gfx::Size& natural_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto gmb = gpu_factories_->CreateGpuMemoryBuffer(
      natural_size, gfx::BufferFormat::YUV_420_BIPLANAR,
      gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE);

  if (!gmb || !gmb->Map()) {
    black_gmb_frame_ = nullptr;
    return false;
  }
  // Fills the NV12 frame with YUV black (0x00, 0x80, 0x80).
  const auto gmb_size = gmb->GetSize();
  memset(static_cast<uint8_t*>(gmb->memory(0)), 0x0,
         gmb->stride(0) * gmb_size.height());
  memset(static_cast<uint8_t*>(gmb->memory(1)), 0x80,
         gmb->stride(1) * gmb_size.height() / 2);
  gmb->Unmap();

  gpu::MailboxHolder empty_mailboxes[media::VideoFrame::kMaxPlanes];
  black_gmb_frame_ = media::VideoFrame::WrapExternalGpuMemoryBuffer(
      gfx::Rect(gmb_size), natural_size, std::move(gmb), empty_mailboxes,
      base::NullCallback(), base::TimeDelta());
  return true;
}

void RTCVideoEncoder::Impl::InputBufferReleased(int index) {
  DVLOG(3) << "Impl::InputBufferReleased(): index=" << index;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!use_native_input_);

  // Destroy() against this has been called. Don't proceed the frame completion.
  if (!video_encoder_)
    return;

  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(input_buffers_.size()));
  input_buffers_free_.push_back(index);
  if (input_next_frame_)
    EncodeOneFrame();
}

bool RTCVideoEncoder::Impl::RequiresSizeChange(
    const media::VideoFrame& frame) const {
  return (frame.coded_size() != input_frame_coded_size_ ||
          frame.visible_rect() != gfx::Rect(input_visible_size_));
}

void RTCVideoEncoder::Impl::RegisterEncodeCompleteCallback(
    SignaledValue event,
    webrtc::EncodedImageCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3) << __func__;

  if (status_ == WEBRTC_VIDEO_CODEC_OK)
    encoded_image_callback_ = callback;
  event.Set(status_);
  event.Signal();
}

RTCVideoEncoder::RTCVideoEncoder(
    media::VideoCodecProfile profile,
    bool is_constrained_h264,
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : profile_(profile),
      is_constrained_h264_(is_constrained_h264),
      gpu_factories_(gpu_factories),
      gpu_task_runner_(gpu_factories->GetTaskRunner()) {
  DVLOG(1) << "RTCVideoEncoder(): profile=" << GetProfileName(profile);
}

RTCVideoEncoder::~RTCVideoEncoder() {
  DVLOG(3) << __func__;
  Release();
  DCHECK(!impl_.get());
}

int32_t RTCVideoEncoder::InitEncode(
    const webrtc::VideoCodec* codec_settings,
    const webrtc::VideoEncoder::Settings& settings) {
  DVLOG(1) << __func__ << " codecType=" << codec_settings->codecType
           << ", width=" << codec_settings->width
           << ", height=" << codec_settings->height
           << ", startBitrate=" << codec_settings->startBitrate;

  if (profile_ >= media::H264PROFILE_MIN &&
      profile_ <= media::H264PROFILE_MAX &&
      (codec_settings->width % 2 != 0 || codec_settings->height % 2 != 0)) {
    DLOG(ERROR)
        << "Input video size is " << codec_settings->width << "x"
        << codec_settings->height << ", "
        << "but hardware H.264 encoder only supports even sized frames.";
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }

  if (impl_)
    Release();

  impl_ =
      new Impl(gpu_factories_, ProfileToWebRtcVideoCodecType(profile_),
               (codec_settings->mode == webrtc::VideoCodecMode::kScreensharing)
                   ? webrtc::VideoContentType::SCREENSHARE
                   : webrtc::VideoContentType::UNSPECIFIED);

  std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>
      spatial_layers;
  auto inter_layer_pred =
      media::VideoEncodeAccelerator::Config::InterLayerPredMode::kOff;
  if (!CreateSpatialLayersConfig(*codec_settings, &spatial_layers,
                                 &inter_layer_pred)) {
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }

  // This wait is necessary because this task is completed in GPU process
  // asynchronously but WebRTC API is synchronous.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::WaitableEvent initialization_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  int32_t initialization_retval = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  PostCrossThreadTask(
      *gpu_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          &RTCVideoEncoder::Impl::CreateAndInitializeVEA,
          scoped_refptr<Impl>(impl_),
          gfx::Size(codec_settings->width, codec_settings->height),
          codec_settings->startBitrate, profile_, is_constrained_h264_,
          spatial_layers, inter_layer_pred,
          SignaledValue(&initialization_waiter, &initialization_retval)));

  // webrtc::VideoEncoder expects this call to be synchronous.
  initialization_waiter.Wait();
  RecordInitEncodeUMA(initialization_retval, profile_);
  return initialization_retval;
}

int32_t RTCVideoEncoder::Encode(
    const webrtc::VideoFrame& input_image,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  DVLOG(3) << __func__;
  if (!impl_.get()) {
    DVLOG(3) << "Encoder is not initialized";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  const bool want_key_frame =
      frame_types && frame_types->size() &&
      frame_types->front() == webrtc::VideoFrameType::kVideoFrameKey;
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::WaitableEvent encode_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  int32_t encode_retval = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  PostCrossThreadTask(
      *gpu_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&RTCVideoEncoder::Impl::Enqueue,
                          scoped_refptr<Impl>(impl_),
                          CrossThreadUnretained(&input_image), want_key_frame,
                          SignaledValue(&encode_waiter, &encode_retval)));

  // webrtc::VideoEncoder expects this call to be synchronous.
  encode_waiter.Wait();
  DVLOG(3) << "Encode(): returning encode_retval=" << encode_retval;
  return encode_retval;
}

int32_t RTCVideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  DVLOG(3) << __func__;
  if (!impl_.get()) {
    DVLOG(3) << "Encoder is not initialized";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::WaitableEvent register_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  int32_t register_retval = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  PostCrossThreadTask(
      *gpu_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          &RTCVideoEncoder::Impl::RegisterEncodeCompleteCallback,
          scoped_refptr<Impl>(impl_),
          SignaledValue(&register_waiter, &register_retval),
          CrossThreadUnretained(callback)));
  register_waiter.Wait();
  return register_retval;
}

int32_t RTCVideoEncoder::Release() {
  DVLOG(3) << __func__;
  if (!impl_.get())
    return WEBRTC_VIDEO_CODEC_OK;

  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::WaitableEvent release_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  PostCrossThreadTask(
      *gpu_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&RTCVideoEncoder::Impl::Destroy,
                          scoped_refptr<Impl>(impl_),
                          SignaledValue(&release_waiter, nullptr /* val */)));
  release_waiter.Wait();
  impl_ = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

void RTCVideoEncoder::SetRates(
    const webrtc::VideoEncoder::RateControlParameters& parameters) {
  DVLOG(3) << __func__ << " new_bit_rate=" << parameters.bitrate.ToString()
           << ", frame_rate=" << parameters.framerate_fps;
  if (!impl_.get()) {
    DVLOG(3) << "Encoder is not initialized";
    return;
  }

  PostCrossThreadTask(
      *gpu_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          &RTCVideoEncoder::Impl::RequestEncodingParametersChange,
          scoped_refptr<Impl>(impl_), parameters));
  return;
}

webrtc::VideoEncoder::EncoderInfo RTCVideoEncoder::GetEncoderInfo() const {
  webrtc::VideoEncoder::EncoderInfo info;
#if BUILDFLAG(IS_ANDROID)
  // MediaCodec requires 16x16 alignment, see https://crbug.com/1084702. We
  // normally override this in |impl_|, but sometimes this method is called
  // before |impl_| is created, so we need to override it here too.
  info.requested_resolution_alignment = 16;
  info.apply_alignment_to_all_simulcast_layers = true;
#endif

  if (impl_)
    info = impl_->GetEncoderInfo();
  return info;
}

// static
bool RTCVideoEncoder::Vp9HwSupportForSpatialLayers() {
#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)
  return base::FeatureList::IsEnabled(media::kVaapiVp9kSVCHWEncoding);
#else
  // Spatial layers are not supported by hardware encoders.
  return false;
#endif
}

}  // namespace blink
