// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder.h"

#include <memory>
#include <numeric>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "media/base/bitrate.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/platform_features.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/capture/capture_switches.h"
#include "media/media_buildflags.h"
#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"
#include "media/parsers/h264_parser.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/webrtc/convert_to_webrtc_video_frame_buffer.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_gfx.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "third_party/webrtc/modules/video_coding/include/video_error_codes.h"
#include "third_party/webrtc/modules/video_coding/svc/simulcast_to_svc_converter.h"
#include "third_party/webrtc/modules/video_coding/utility/simulcast_utility.h"
#include "third_party/webrtc/rtc_base/time_utils.h"
#include "ui/gfx/buffer_format_util.h"

namespace {

media::SVCScalabilityMode ToSVCScalabilityMode(
    const std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>&
        spatial_layers,
    media::SVCInterLayerPredMode inter_layer_pred) {
  if (spatial_layers.empty()) {
    return media::SVCScalabilityMode::kL1T1;
  }
  return GetSVCScalabilityMode(spatial_layers.size(),
                               spatial_layers[0].num_of_temporal_layers,
                               inter_layer_pred);
}

class SignaledValue {
 public:
  SignaledValue() : event(nullptr), val(nullptr) {}
  SignaledValue(base::WaitableEvent* event, int32_t* val)
      : event(event), val(val) {
    DCHECK(event);
  }

  ~SignaledValue() {
    if (IsValid() && !event->IsSignaled()) {
      NOTREACHED_IN_MIGRATION() << "never signaled";
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
  raw_ptr<base::WaitableEvent> event;
  raw_ptr<int32_t> val;
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

// TODO(https://crbug.com/1448809): Move to base/memory/ref_counted_memory.h
class RefCountedWritableSharedMemoryMapping
    : public ThreadSafeRefCounted<RefCountedWritableSharedMemoryMapping> {
 public:
  explicit RefCountedWritableSharedMemoryMapping(
      base::WritableSharedMemoryMapping mapping)
      : mapping_(std::move(mapping)) {}

  RefCountedWritableSharedMemoryMapping(
      const RefCountedWritableSharedMemoryMapping&) = delete;
  RefCountedWritableSharedMemoryMapping& operator=(
      const RefCountedWritableSharedMemoryMapping&) = delete;

  const unsigned char* front() const {
    return static_cast<unsigned char*>(mapping_.memory());
  }
  unsigned char* front() {
    return static_cast<unsigned char*>(mapping_.memory());
  }
  size_t size() const { return mapping_.size(); }

 private:
  friend class ThreadSafeRefCounted<RefCountedWritableSharedMemoryMapping>;
  ~RefCountedWritableSharedMemoryMapping() = default;

  const base::WritableSharedMemoryMapping mapping_;
};

class EncodedDataWrapper : public webrtc::EncodedImageBufferInterface {
 public:
  EncodedDataWrapper(
      const scoped_refptr<RefCountedWritableSharedMemoryMapping>&& mapping,
      size_t size,
      base::OnceClosure reuse_buffer_callback)
      : mapping_(std::move(mapping)),
        size_(size),
        reuse_buffer_callback_(std::move(reuse_buffer_callback)) {}
  ~EncodedDataWrapper() override {
    DCHECK(reuse_buffer_callback_);
    std::move(reuse_buffer_callback_).Run();
  }
  const uint8_t* data() const override { return mapping_->front(); }
  uint8_t* data() override { return mapping_->front(); }
  size_t size() const override { return size_; }

 private:
  const scoped_refptr<RefCountedWritableSharedMemoryMapping> mapping_;
  const size_t size_;
  base::OnceClosure reuse_buffer_callback_;
};

struct FrameChunk {
  FrameChunk(const webrtc::VideoFrame& input_image, bool force_keyframe)
      : video_frame_buffer(input_image.video_frame_buffer()),
        timestamp(input_image.rtp_timestamp()),
        timestamp_us(input_image.timestamp_us()),
        render_time_ms(input_image.render_time_ms()),
        force_keyframe(force_keyframe) {
    DCHECK(video_frame_buffer);
  }

  const rtc::scoped_refptr<webrtc::VideoFrameBuffer> video_frame_buffer;
  // TODO(b/241349739): timestamp and timestamp_us should be unified as one
  // base::TimeDelta.
  const uint32_t timestamp;
  const uint64_t timestamp_us;
  const int64_t render_time_ms;

  const bool force_keyframe;
};

bool ConvertKbpsToBps(uint32_t bitrate_kbps, uint32_t* bitrate_bps) {
  if (!base::IsValueInRangeForNumericType<uint32_t>(bitrate_kbps *
                                                    UINT64_C(1000))) {
    return false;
  }
  *bitrate_bps = bitrate_kbps * 1000;
  return true;
}

uint8_t GetDropFrameThreshold(const webrtc::VideoCodec& codec_settings) {
  // This drop frame threshold is same as WebRTC.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/codecs/vp9/libvpx_vp9_encoder.cc
  if (codec_settings.GetFrameDropEnabled() &&
      base::FeatureList::IsEnabled(
          media::kWebRTCHardwareVideoEncoderFrameDrop)) {
    return 30;
  }
  return 0;
}

webrtc::VideoBitrateAllocation AllocateBitrateForVEAConfig(
    const media::VideoEncodeAccelerator::Config& config) {
  // The same bitrate factors as the software encoder.
  // https://source.chromium.org/chromium/chromium/src/+/main:media/video/vpx_video_encoder.cc;l=131;drc=d383d0b3e4f76789a6de2a221c61d3531f4c59da
  constexpr double kTemporalLayersBitrateScaleFactors[][3] = {
      {1.00, 0.00, 0.00},  // For one temporal layer.
      {0.60, 0.40, 0.00},  // For two temporal layers.
      {0.50, 0.20, 0.30},  // For three temporal layers.
  };
  DCHECK_EQ(config.bitrate.mode(), media::Bitrate::Mode::kConstant);
  webrtc::VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, config.bitrate.target_bps());

  for (size_t sid = 0; sid < config.spatial_layers.size(); ++sid) {
    const auto& sl = config.spatial_layers[sid];
    CHECK_EQ(sl.num_of_temporal_layers <= 3, true);
    for (size_t tid = 0; tid < sl.num_of_temporal_layers; ++tid) {
      const double factor =
          kTemporalLayersBitrateScaleFactors[sl.num_of_temporal_layers - 1]
                                            [tid];
      bitrate_allocation.SetBitrate(sid, tid, sl.bitrate_bps * factor);
    }
  }
  return bitrate_allocation;
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
struct CrossThreadCopier<FrameChunk>
    : public CrossThreadCopierPassThrough<FrameChunk> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<media::VideoEncodeAccelerator::Config>
    : public CrossThreadCopierPassThrough<
          media::VideoEncodeAccelerator::Config> {
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

namespace features {

// Enabled-by-default, except for Android where SW encoder for H264 is not
// available. The existence of this flag remains only for testing purposes.
BASE_FEATURE(kForceSoftwareForLowResolutions,
             "ForceSoftwareForLowResolutions",
#if !BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When disabled, SW is forced at <360p. When enabled, SW is forced at <=360p.
// Only applicable when `kForceSoftwareForLowResolutions` is enabled.
BASE_FEATURE(kForcingSoftwareIncludes360,
             "ForcingSoftwareIncludes360",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Avoids large latencies to build up by dropping frames when the number of
// frames that are sent to a hardware video encoder reaches a certain limit.
// See b/298660336 for details.
BASE_FEATURE(kVideoEncoderLimitsFramesInEncoder,
             "VideoEncoderLimitsFramesInEncoder",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the encoder instance is preserved on Release() call.
// Reinitialization of the encoder will reuse the instance with the new
// resolution. See b/1466102 for details.
BASE_FEATURE(kKeepEncoderInstanceOnRelease,
             "KeepEncoderInstanceOnRelease",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the supports_simulcast will be always reported to webrtc
// and incoming simulcast codec config will be rewritten as an SVC config.
BASE_FEATURE(kRtcVideoEncoderConvertSimulcastToSvc,
             "RtcVideoEncoderConvertSimulcastToSvc",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace features

namespace {
media::SVCInterLayerPredMode CopyFromWebRtcInterLayerPredMode(
    const webrtc::InterLayerPredMode inter_layer_pred) {
  switch (inter_layer_pred) {
    case webrtc::InterLayerPredMode::kOff:
      return media::SVCInterLayerPredMode::kOff;
    case webrtc::InterLayerPredMode::kOn:
      return media::SVCInterLayerPredMode::kOn;
    case webrtc::InterLayerPredMode::kOnKeyPic:
      return media::SVCInterLayerPredMode::kOnKeyPic;
  }
}

// Create VEA::Config::SpatialLayer from |codec_settings|. If some config of
// |codec_settings| is not supported, returns false.
bool CreateSpatialLayersConfig(
    const webrtc::VideoCodec& codec_settings,
    std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>*
        spatial_layers,
    media::SVCInterLayerPredMode* inter_layer_pred,
    gfx::Size* highest_active_resolution) {
  std::optional<webrtc::ScalabilityMode> scalability_mode =
      codec_settings.GetScalabilityMode();
  *highest_active_resolution =
      gfx::Size(codec_settings.width, codec_settings.height);

  if (codec_settings.codecType == webrtc::kVideoCodecVP9 &&
      codec_settings.VP9().numberOfSpatialLayers > 1 &&
      !media::IsVp9kSVCHWEncodingEnabled()) {
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
        std::optional<gfx::Size> top_res;
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

          if (!top_res.has_value()) {
            top_res = gfx::Size(rtc_sl.width, rtc_sl.height);
          } else if (top_res->width() < rtc_sl.width) {
            DCHECK_GE(rtc_sl.height, top_res->width());
            top_res = gfx::Size(rtc_sl.width, rtc_sl.height);
          }
        }

        if (top_res.has_value()) {
          *highest_active_resolution = *top_res;
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
    case webrtc::kVideoCodecAV1:
      // No hardware encoder supports for AV1 either temporal layer or spatial
      // layer encoding.
      if (scalability_mode.value_or(webrtc::ScalabilityMode::kL1T1) !=
          webrtc::ScalabilityMode::kL1T1) {
        return false;
      }
      break;
    default:
      break;
  }
  return true;
}

struct ActiveSpatialLayers {
  // `spatial_index` considered active if
  // `begin_index <= spatial_index < end_index`
  size_t begin_index = 0;
  size_t end_index = 0;
  size_t size() const { return end_index - begin_index; }
};

struct FrameInfo {
 public:
  FrameInfo(const base::TimeDelta& media_timestamp,
            int32_t rtp_timestamp,
            int64_t capture_time_ms,
            const ActiveSpatialLayers& active_spatial_layers)
      : media_timestamp_(media_timestamp),
        rtp_timestamp_(rtp_timestamp),
        capture_time_ms_(capture_time_ms),
        active_spatial_layers_(active_spatial_layers) {}

  const base::TimeDelta media_timestamp_;
  const int32_t rtp_timestamp_;
  const int64_t capture_time_ms_;
  const ActiveSpatialLayers active_spatial_layers_;
  size_t produced_frames_ = 0;
};

webrtc::VideoCodecType ProfileToWebRtcVideoCodecType(
    media::VideoCodecProfile profile) {
  switch (media::VideoCodecProfileToVideoCodec(profile)) {
    case media::VideoCodec::kH264:
      return webrtc::kVideoCodecH264;
    case media::VideoCodec::kVP8:
      return webrtc::kVideoCodecVP8;
    case media::VideoCodec::kVP9:
      return webrtc::kVideoCodecVP9;
    case media::VideoCodec::kAV1:
      return webrtc::kVideoCodecAV1;
#if BUILDFLAG(RTC_USE_H265)
    case media::VideoCodec::kHEVC:
      return webrtc::kVideoCodecH265;
#endif
    default:
      NOTREACHED_IN_MIGRATION()
          << "Invalid profile " << GetProfileName(profile);
      return webrtc::kVideoCodecGeneric;
  }
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

void RecordEncoderStatusUMA(const media::EncoderStatus& status,
                            webrtc::VideoCodecType type) {
  std::string histogram_name = "Media.RTCVideoEncoderStatus.";
  switch (type) {
    case webrtc::VideoCodecType::kVideoCodecH264:
      histogram_name += "H264";
      break;
    case webrtc::VideoCodecType::kVideoCodecVP8:
      histogram_name += "VP8";
      break;
    case webrtc::VideoCodecType::kVideoCodecVP9:
      histogram_name += "VP9";
      break;
    case webrtc::VideoCodecType::kVideoCodecAV1:
      histogram_name += "AV1";
      break;
#if BUILDFLAG(RTC_USE_H265)
    case webrtc::VideoCodecType::kVideoCodecH265:
      histogram_name += "H265";
      break;
#endif  // BUILDFLAG(RTC_USE_H265)
    default:
      histogram_name += "Other";
      break;
  }
  base::UmaHistogramEnumeration(histogram_name, status.code());
}

bool IsZeroCopyEnabled(webrtc::VideoContentType content_type) {
  if (content_type == webrtc::VideoContentType::SCREENSHARE) {
    // Zero copy screen capture.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // The zero-copy capture is available for all sources in ChromeOS
    // Ash-chrome.
    return base::FeatureList::IsEnabled(blink::features::kZeroCopyTabCapture);
#else
    // Currently, zero copy capture screenshare is available only for tabs.
    // Since it is impossible to determine the content source, tab, window or
    // monitor, we don't configure VideoEncodeAccelerator with NV12
    // GpuMemoryBuffer instead we configure I420 SHMEM as if it is not zero
    // copy, and we convert the NV12 GpuMemoryBuffer to I420 SHMEM in
    // RtcVideoEncoder::Impl::Encode().
    // TODO(b/267995715): Solve this problem by calling Initialize() in the
    // first frame.
    return false;
#endif
  }
  // Zero copy video capture from other sources (e.g. camera).
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableVideoCaptureUseGpuMemoryBuffer) &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kVideoCaptureUseGpuMemoryBuffer);
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
// with a media::VideoEncodeAccelerator for handling video encoding. It can
// be created on any thread, but should subsequently be executed on
// |gpu_task_runner| including destructor.
//
// This class separates state related to the thread that RTCVideoEncoder
// operates on from the thread that |gpu_factories_| provides for accelerator
// operations (presently the media thread).
class RTCVideoEncoder::Impl : public media::VideoEncodeAccelerator::Client {
 public:
  using UpdateEncoderInfoCallback = base::RepeatingCallback<void(
      media::VideoEncoderInfo,
      std::vector<webrtc::VideoFrameBuffer::Type>)>;
  Impl(media::GpuVideoAcceleratorFactories* gpu_factories,
       scoped_refptr<media::MojoVideoEncoderMetricsProviderFactory>
           encoder_metrics_provider_factory,
       webrtc::VideoCodecType video_codec_type,
       std::optional<webrtc::ScalabilityMode> scalability_mode,
       webrtc::VideoContentType video_content_type,
       UpdateEncoderInfoCallback update_encoder_info_callback,
       base::RepeatingClosure execute_software_fallback,
       base::WeakPtr<Impl>& weak_this_for_client);

  ~Impl() override;
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  // Create the VEA and call Initialize() on it.  Called once per instantiation,
  // and then the instance is bound forevermore to whichever thread made the
  // call.
  // RTCVideoEncoder expects to be able to call this function synchronously from
  // its own thread, hence the |init_event| argument.
  void CreateAndInitializeVEA(
      const media::VideoEncodeAccelerator::Config& vea_config,
      SignaledValue init_event);

  // Enqueue a frame from WebRTC for encoding. This function is called
  // asynchronously from webrtc encoder thread. When the error is caused, it is
  // reported by NotifyErrorStatus().
  void Enqueue(FrameChunk frame_chunk);

  // Request encoding parameter change for the underlying encoder with
  // additional size change. Requires the encoder to be in flushed state.
  void RequestEncodingParametersChangeWithSizeChange(
      const webrtc::VideoEncoder::RateControlParameters& parameters,
      const gfx::Size& input_visible_size,
      const media::VideoCodecProfile& profile,
      const media::SVCInterLayerPredMode& inter_layer_pred,
      const std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>&
          spatial_layers,
      SignaledValue event);

  // Request encoding parameter change for the underlying encoder.
  void RequestEncodingParametersChange(
      const webrtc::VideoEncoder::RateControlParameters& parameters);

  void RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback);

  webrtc::VideoCodecType video_codec_type() const { return video_codec_type_; }

  // media::VideoEncodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(
      int32_t bitstream_buffer_id,
      const media::BitstreamBufferMetadata& metadata) override;
  void NotifyErrorStatus(const media::EncoderStatus& status) override;
  void NotifyEncoderInfoChange(const media::VideoEncoderInfo& info) override;

#if BUILDFLAG(RTC_USE_H265)
  void SetH265ParameterSetsTrackerForTesting(
      std::unique_ptr<H265ParameterSetsTracker> tracker);
#endif
  void Suspend(SignaledValue event);

  void Drain(SignaledValue event);
  void DrainCompleted(bool success);

  void SetSimulcastToSvcConverter(std::optional<webrtc::SimulcastToSvcConverter>
                                      simulcast_to_svc_converter);

 private:
  // proxy to pass weak reference to webrtc which could be invalidated when
  // frame size changes and new output buffers are allocated.
  class EncodedBufferReferenceHolder {
   public:
    explicit EncodedBufferReferenceHolder(base::WeakPtr<Impl> impl)
        : impl_(impl) {
      weak_this_ = weak_this_factory_.GetWeakPtr();
    }
    ~EncodedBufferReferenceHolder() = default;
    base::WeakPtr<EncodedBufferReferenceHolder> GetWeakPtr() {
      return weak_this_;
    }
    void BitstreamBufferAvailable(int bitstream_buffer_id) {
      if (Impl* impl = impl_.get()) {
        impl->BitstreamBufferAvailable(bitstream_buffer_id);
      }
    }

   private:
    base::WeakPtr<Impl> impl_;
    base::WeakPtr<EncodedBufferReferenceHolder> weak_this_;
    base::WeakPtrFactory<EncodedBufferReferenceHolder> weak_this_factory_{this};
  };

  void RequestEncodingParametersChangeInternal(
      const webrtc::VideoEncoder::RateControlParameters& parameters,
      const std::optional<gfx::Size>& input_visible_size);

  enum {
    kInputBufferExtraCount = 1,  // The number of input buffers allocated, more
                                 // than what is requested by
                                 // VEA::RequireBitstreamBuffers().
    kOutputBufferCount = 3,
    kMaxFramesInEncoder = 15,  // Max number of frames the encoder is allowed
                               // to hold before dropping input frames.
                               // Avoids large delay buildups.
                               // See b/298660336 for details.
  };

  // Perform encoding on an input frame from the input queue.
  void EncodeOneFrame(FrameChunk frame_chunk);

  // Perform encoding on an input frame from the input queue using VEA native
  // input mode.  The input frame must be backed with GpuMemoryBuffer buffers.
  void EncodeOneFrameWithNativeInput(FrameChunk frame_chunk);

  // Creates a MappableSI frame filled with black pixels. Returns true if
  // the frame is successfully created; false otherwise.
  bool CreateBlackMappableSIFrame(const gfx::Size& natural_size);

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

  // Gets ActiveSpatialLayers that are currently active,
  // meaning the are configured, have active=true and have non-zero bandwidth
  // allocated to them.
  // Returns an empty list if a layer encoding is not used.
  ActiveSpatialLayers GetActiveSpatialLayers() const;

  // Call VideoEncodeAccelerator::UseOutputBitstreamBuffer() for a buffer whose
  // id is |bitstream_buffer_id|.
  void UseOutputBitstreamBuffer(int32_t bitstream_buffer_id);

  // RTCVideoEncoder is given a buffer to be passed to WebRTC through the
  // RTCVideoEncoder::ReturnEncodedImage() function.  When that is complete,
  // the buffer is returned to Impl by its index using this function.
  void BitstreamBufferAvailable(int32_t bitstream_buffer_id);

  // This is attached to |gpu_task_runner_|, not the thread class is constructed
  // on.
  SEQUENCE_CHECKER(sequence_checker_);

  // Factory for creating VEAs, shared memory buffers, etc.
  const raw_ptr<media::GpuVideoAcceleratorFactories> gpu_factories_;

  scoped_refptr<media::MojoVideoEncoderMetricsProviderFactory>
      encoder_metrics_provider_factory_;
  std::unique_ptr<media::VideoEncoderMetricsProvider> encoder_metrics_provider_;

  // webrtc::VideoEncoder expects InitEncode() to be synchronous. Do this by
  // waiting on the |async_init_event_| when initialization completes.
  ScopedSignaledValue async_init_event_;

  // The underlying VEA to perform encoding on.
  std::unique_ptr<media::VideoEncodeAccelerator> video_encoder_;

  // Metadata for frames passed to Encode(), matched to encoded frames using
  // timestamps.
  WTF::Deque<FrameInfo> submitted_frames_;

  // Indicates that timestamp match failed and we should no longer attempt
  // matching.
  bool failed_timestamp_match_{false};

  // The pending frames to be encoded with the boolean representing whether the
  // frame must be encoded keyframe.
  WTF::Deque<FrameChunk> pending_frames_;

  // Frame sizes.
  gfx::Size input_frame_coded_size_;
  gfx::Size input_visible_size_;

  // Shared memory buffers for input/output with the VEA.
  Vector<std::unique_ptr<base::MappedReadOnlyRegion>> input_buffers_;

  Vector<std::pair<base::UnsafeSharedMemoryRegion,
                   scoped_refptr<RefCountedWritableSharedMemoryMapping>>>
      output_buffers_;

  // The number of input buffers requested by hardware video encoder.
  size_t input_buffers_requested_count_{0};

  // The number of frames that are sent to a hardware video encoder by Encode()
  // and the encoder holds them.
  size_t frames_in_encoder_count_{0};

  // Input buffers ready to be filled with input from Encode().  As a LIFO since
  // we don't care about ordering.
  Vector<int> input_buffers_free_;

  // The number of output buffers that have been sent to a hardware video
  // encoder by VideoEncodeAccelerator::UseOutputBitstreamBuffer() and the
  // encoder holds them.
  size_t output_buffers_in_encoder_count_{0};

  // proxy to pass weak reference to webrtc which could be invalidated when
  // frame size changes and new output buffers are allocated.
  std::unique_ptr<EncodedBufferReferenceHolder>
      encoded_buffer_reference_holder_;

  // The buffer ids that are not sent to a hardware video encoder and this holds
  // them. UseOutputBitstreamBuffer() is called for them on the next Encode().
  Vector<int32_t> pending_output_buffers_;

  // Whether to send the frames to VEA as native buffer. Native buffer allows
  // VEA to pass the buffer to the encoder directly without further processing.
  bool use_native_input_{false};

  // A black frame used when the video track is disabled.
  scoped_refptr<media::VideoFrame> black_frame_;

  // The video codec type, as reported to WebRTC.
  const webrtc::VideoCodecType video_codec_type_;

  // The scalability mode, as reported to WebRTC.
  const std::optional<webrtc::ScalabilityMode> scalability_mode_;

  // The content type, as reported to WebRTC (screenshare vs realtime video).
  const webrtc::VideoContentType video_content_type_;

  // This has the same information as |encoder_info_.preferred_pixel_formats|
  // but can be used on |sequence_checker_| without acquiring the lock.
  absl::InlinedVector<webrtc::VideoFrameBuffer::Type,
                      webrtc::kMaxPreferredPixelFormats>
      preferred_pixel_formats_;

  UpdateEncoderInfoCallback update_encoder_info_callback_;

  // Calling this causes a software encoder fallback.
  base::RepeatingClosure execute_software_fallback_;

  // The spatial layer resolutions configured in VEA::Initialize(). This is set
  // only in CreateAndInitializeVEA().
  WTF::Vector<gfx::Size> init_spatial_layer_resolutions_;

  // The current active spatial layer range. This is set in
  // CreateAndInitializeVEA() and updated in RequestEncodingParametersChange().
  ActiveSpatialLayers active_spatial_layers_;

#if BUILDFLAG(RTC_USE_H265)
  // Parameter sets(VPS/SPS/PPS) tracker used for H.265, to ensure parameter
  // sets are always included in IRAP pictures.
  std::unique_ptr<H265ParameterSetsTracker> ps_tracker_;
#endif  // BUILDFLAG(RTC_USE_H265)

  // We cannot immediately return error conditions to the WebRTC user of this
  // class, as there is no error callback in the webrtc::VideoEncoder interface.
  // Instead, we cache an error status here and return it the next time an
  // interface entry point is called.
  int32_t status_ GUARDED_BY_CONTEXT(sequence_checker_){
      WEBRTC_VIDEO_CODEC_UNINITIALIZED};

  // Protect |encoded_image_callback_|. |encoded_image_callback_| is read on
  // media thread and written in webrtc encoder thread.
  mutable base::Lock lock_;

  // webrtc::VideoEncoder encode complete callback.
  // TODO(b/257021675): Don't guard this by |lock_|
  raw_ptr<webrtc::EncodedImageCallback> encoded_image_callback_
      GUARDED_BY(lock_){nullptr};

  // Used to rewrite the encoded image metadata to look like simulcast
  // instead of SVC. Set only when simulcat config is emulated by SVC one.
  std::optional<webrtc::SimulcastToSvcConverter> simulcast_to_svc_converter_;

  // They are bound to |gpu_task_runner_|, which is sequence checked by
  // |sequence_checker|.
  base::WeakPtr<Impl> weak_this_;
  base::WeakPtrFactory<Impl> weak_this_factory_{this};
};

RTCVideoEncoder::Impl::Impl(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    scoped_refptr<media::MojoVideoEncoderMetricsProviderFactory>
        encoder_metrics_provider_factory,
    webrtc::VideoCodecType video_codec_type,
    std::optional<webrtc::ScalabilityMode> scalability_mode,
    webrtc::VideoContentType video_content_type,
    UpdateEncoderInfoCallback update_encoder_info_callback,
    base::RepeatingClosure execute_software_fallback,
    base::WeakPtr<Impl>& weak_this_for_client)
    : gpu_factories_(gpu_factories),
      encoder_metrics_provider_factory_(
          std::move(encoder_metrics_provider_factory)),
      video_codec_type_(video_codec_type),
      scalability_mode_(scalability_mode),
      video_content_type_(video_content_type),
      update_encoder_info_callback_(std::move(update_encoder_info_callback)),
      execute_software_fallback_(std::move(execute_software_fallback)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  CHECK(encoder_metrics_provider_factory_);
  preferred_pixel_formats_ = {webrtc::VideoFrameBuffer::Type::kI420};
  weak_this_ = weak_this_factory_.GetWeakPtr();
  encoded_buffer_reference_holder_ =
      std::make_unique<EncodedBufferReferenceHolder>(weak_this_);
  weak_this_for_client = weak_this_;
}

void RTCVideoEncoder::Impl::CreateAndInitializeVEA(
    const media::VideoEncodeAccelerator::Config& vea_config,
    SignaledValue init_event) {
  TRACE_EVENT0("webrtc", "RTCVideoEncoder::Impl::CreateAndInitializeVEA");
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  status_ = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  async_init_event_ = ScopedSignaledValue(std::move(init_event));

  video_encoder_ = gpu_factories_->CreateVideoEncodeAccelerator();
  if (!video_encoder_) {
    NotifyErrorStatus({media::EncoderStatus::Codes::kEncoderInitializationError,
                       "Failed to create VideoEncodeAccelerato"});
    return;
  }

  input_visible_size_ = vea_config.input_visible_size;
  // The valid config is NV12+kGpuMemoryBuffer and I420+kShmem.
  CHECK_EQ(
      vea_config.input_format == media::PIXEL_FORMAT_NV12,
      vea_config.storage_type ==
          media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer);
  if (vea_config.storage_type ==
      media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer) {
    use_native_input_ = true;
    preferred_pixel_formats_ = {webrtc::VideoFrameBuffer::Type::kNV12};
  }

  encoder_metrics_provider_ =
      encoder_metrics_provider_factory_->CreateVideoEncoderMetricsProvider();
  encoder_metrics_provider_->Initialize(
      vea_config.output_profile, vea_config.input_visible_size,
      /*is_hardware_encoder=*/true,
      ToSVCScalabilityMode(vea_config.spatial_layers,
                           vea_config.inter_layer_pred));
  if (!video_encoder_->Initialize(vea_config, this,
                                  std::make_unique<media::NullMediaLog>())) {
    NotifyErrorStatus({media::EncoderStatus::Codes::kEncoderInitializationError,
                       "Failed to initialize VideoEncodeAccelerator"});
    return;
  }

  init_spatial_layer_resolutions_.clear();
  for (const auto& layer : vea_config.spatial_layers) {
    init_spatial_layer_resolutions_.emplace_back(layer.width, layer.height);
  }

  active_spatial_layers_.begin_index = 0;
  active_spatial_layers_.end_index = vea_config.spatial_layers.size();

#if BUILDFLAG(RTC_USE_H265)
  if (video_codec_type_ == webrtc::kVideoCodecH265 && !ps_tracker_) {
    ps_tracker_ = std::make_unique<H265ParameterSetsTracker>();
  }
#endif  // BUILDFLAG(RTC_USE_H265)

  // RequireBitstreamBuffers or NotifyError will be called and the waiter will
  // be signaled.
}

void RTCVideoEncoder::Impl::NotifyEncoderInfoChange(
    const media::VideoEncoderInfo& info) {
  update_encoder_info_callback_.Run(
      info,
      std::vector<webrtc::VideoFrameBuffer::Type>(
          preferred_pixel_formats_.begin(), preferred_pixel_formats_.end()));
}

void RTCVideoEncoder::Impl::Enqueue(FrameChunk frame_chunk) {
  TRACE_EVENT1("webrtc", "RTCVideoEncoder::Impl::Enqueue", "timestamp",
               frame_chunk.timestamp_us);
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status_ != WEBRTC_VIDEO_CODEC_OK) {
    // When |status_| is already not OK, the error has been notified.
    return;
  }

  // Avoid large latencies to build up by dropping frames when the number of
  // frames that are sent to a hardware video encoder reaches a certain limit.
  // `frames_in_encoder_count_` is reduced by `BitstreamBufferReady` when
  // the first spatial layer of a frame has been encoded.
  // Killswitch: blink::features::VideoEncoderLimitsFramesInEncoder.
  if (base::FeatureList::IsEnabled(
          features::kVideoEncoderLimitsFramesInEncoder) &&
      frames_in_encoder_count_ >= kMaxFramesInEncoder) {
    DVLOG(1) << "VAE drops the input frame to reduce latency";
    base::AutoLock lock(lock_);
    if (encoded_image_callback_) {
      encoded_image_callback_->OnDroppedFrame(
          webrtc::EncodedImageCallback::DropReason::kDroppedByEncoder);
    }
    return;
  }

// On Windows it is possible that RtcVideoEncoder is configured to only accept
// native inputs, but the incoming frame is not backed by GpuMemoryBuffer and
// is not a black frame.
#if BUILDFLAG(IS_WIN)
  {
    // Check if the incoming frame is backed by unowned memory. This could
    // happen when: 1. Zero-copy capture feature is turned on but device does
    // not support MediaFoundation; 2. The video track gets disabled so black
    // frames are sent.
    scoped_refptr<media::VideoFrame> frame;
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer =
        frame_chunk.video_frame_buffer;
    // For black frames their handling will depend on the current
    // |use_native_input_| state. As a result we don't toggle
    // |use_native_input_| flag here for them.
    if (frame_buffer->type() == webrtc::VideoFrameBuffer::Type::kNative) {
      frame = static_cast<WebRtcVideoFrameAdapter*>(frame_buffer.get())
                  ->getMediaVideoFrame();
      if (frame->storage_type() == media::VideoFrame::STORAGE_UNOWNED_MEMORY) {
        if (use_native_input_) {
          use_native_input_ = false;
          // VEA previously worked with imported frames. Now they need input
          // buffers when handling non-imported frames.
          if (input_buffers_.empty()) {
            input_buffers_free_.resize(input_buffers_requested_count_);
            input_buffers_.resize(input_buffers_requested_count_);
            for (wtf_size_t i = 0; i < input_buffers_requested_count_; i++) {
              input_buffers_free_[i] = i;
              input_buffers_[i] = nullptr;
            }
          }
        }
      } else if (frame->storage_type() ==
                 media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
        if (!use_native_input_) {
          use_native_input_ = true;
          // VEA previously worked with input buffers. Now they need imported
          // frames, so get rid of those buffers.
          input_buffers_free_.clear();
          input_buffers_.clear();
        }
      }
    }
  }
#endif

  if (use_native_input_) {
    DCHECK(pending_frames_.empty());
    EncodeOneFrameWithNativeInput(std::move(frame_chunk));
    return;
  }

  pending_frames_.push_back(std::move(frame_chunk));
  // When |input_buffers_free_| is empty, EncodeOneFrame() for the frame in
  // |pending_frames_| will be invoked from InputBufferReleased().
  while (!pending_frames_.empty() && !input_buffers_free_.empty()) {
    auto chunk = std::move(pending_frames_.front());
    pending_frames_.pop_front();
    EncodeOneFrame(std::move(chunk));
  }
}

void RTCVideoEncoder::Impl::BitstreamBufferAvailable(
    int32_t bitstream_buffer_id) {
  TRACE_EVENT0("webrtc", "RTCVideoEncoder::Impl::BitstreamBufferAvailable");
  DVLOG(3) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If there is no frame in a hardware video encoder,
  // UseOutputBitstreamBuffer() call for this buffer id is postponed in the next
  // Encode() call. This avoids unnecessary thread wake up in GPU process.
  if (frames_in_encoder_count_ == 0) {
    pending_output_buffers_.push_back(bitstream_buffer_id);
    return;
  }

  UseOutputBitstreamBuffer(bitstream_buffer_id);
}

void RTCVideoEncoder::Impl::Suspend(SignaledValue event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status_ == WEBRTC_VIDEO_CODEC_OK) {
    status_ = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  event.Set(status_);
  event.Signal();
}

void RTCVideoEncoder::Impl::Drain(SignaledValue event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status_ == WEBRTC_VIDEO_CODEC_OK ||
      status_ == WEBRTC_VIDEO_CODEC_UNINITIALIZED) {
    async_init_event_ = ScopedSignaledValue(std::move(event));
    video_encoder_->Flush(base::BindOnce(&RTCVideoEncoder::Impl::DrainCompleted,
                                         base::Unretained(this)));
  } else {
    event.Set(status_);
    event.Signal();
  }
}

void RTCVideoEncoder::Impl::DrainCompleted(bool success) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (success) {
    status_ = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    async_init_event_.SetAndReset(WEBRTC_VIDEO_CODEC_UNINITIALIZED);
  } else {
    NotifyErrorStatus({media::EncoderStatus::Codes::kEncoderInitializationError,
                       "Failed to flush VideoEncodeAccelerator"});
  }
}

void RTCVideoEncoder::Impl::SetSimulcastToSvcConverter(
    std::optional<webrtc::SimulcastToSvcConverter> simulcast_to_svc_converter) {
  simulcast_to_svc_converter_ = std::move(simulcast_to_svc_converter);
}

void RTCVideoEncoder::Impl::UseOutputBitstreamBuffer(
    int32_t bitstream_buffer_id) {
  TRACE_EVENT0("webrtc", "RTCVideoEncoder::Impl::UseOutputBitstreamBuffer");
  DVLOG(3) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (video_encoder_) {
    video_encoder_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
        bitstream_buffer_id,
        output_buffers_[bitstream_buffer_id].first.Duplicate(),
        output_buffers_[bitstream_buffer_id].first.GetSize()));
    output_buffers_in_encoder_count_++;
  }
}

void RTCVideoEncoder::Impl::RequestEncodingParametersChange(
    const webrtc::VideoEncoder::RateControlParameters& parameters) {
  DVLOG(3) << __func__ << " bitrate=" << parameters.bitrate.ToString()
           << ", framerate=" << parameters.framerate_fps;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status_ != WEBRTC_VIDEO_CODEC_OK)
    return;

  RequestEncodingParametersChangeInternal(parameters, std::nullopt);
}

void RTCVideoEncoder::Impl::RequestEncodingParametersChangeInternal(
    const webrtc::VideoEncoder::RateControlParameters& parameters,
    const std::optional<gfx::Size>& input_visible_size) {
  // NotfiyError() has been called. Don't proceed the change request.
  if (!video_encoder_)
    return;

  uint32_t framerate =
      std::max(1u, static_cast<uint32_t>(parameters.framerate_fps + 0.5));
  // This is a workaround to zero being temporarily provided, as part of the
  // initial setup, by WebRTC.
  media::VideoBitrateAllocation allocation;
  if (parameters.bitrate.get_sum_bps() == 0u) {
    allocation.SetBitrate(0, 0, 1u);
  } else {
    active_spatial_layers_.begin_index = 0;
    active_spatial_layers_.end_index = 0;
    for (size_t spatial_id = 0;
         spatial_id < media::VideoBitrateAllocation::kMaxSpatialLayers;
         ++spatial_id) {
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
        if (temporal_layer_bitrate > 0) {
          if (active_spatial_layers_.end_index == 0) {
            active_spatial_layers_.begin_index = spatial_id;
          }
          active_spatial_layers_.end_index = spatial_id + 1;
        }
      }
    }
    DCHECK_EQ(allocation.GetSumBps(), parameters.bitrate.get_sum_bps());
  }
  video_encoder_->RequestEncodingParametersChange(allocation, framerate,
                                                  input_visible_size);
}

void RTCVideoEncoder::Impl::RequestEncodingParametersChangeWithSizeChange(
    const webrtc::VideoEncoder::RateControlParameters& parameters,
    const gfx::Size& input_visible_size,
    const media::VideoCodecProfile& profile,
    const media::SVCInterLayerPredMode& inter_layer_pred,
    const std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>&
        spatial_layers,
    SignaledValue event) {
  DVLOG(3) << __func__ << " bitrate=" << parameters.bitrate.ToString()
           << ", framerate=" << parameters.framerate_fps
           << ", resolution=" << input_visible_size.ToString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(status_, WEBRTC_VIDEO_CODEC_UNINITIALIZED);

  async_init_event_ = ScopedSignaledValue(std::move(event));
  if (input_visible_size == input_visible_size_) {
    // If the input visible size is the same, we expect all the resolution of
    // spatial layers should be the same.
    CHECK_EQ(init_spatial_layer_resolutions_.size(), spatial_layers.size());
    for (size_t i = 0; i < spatial_layers.size(); ++i) {
      wtf_size_t wtf_i = base::checked_cast<wtf_size_t>(i);
      CHECK_EQ(init_spatial_layer_resolutions_[wtf_i].width(),
               spatial_layers[i].width);
      CHECK_EQ(init_spatial_layer_resolutions_[wtf_i].height(),
               spatial_layers[i].height);
    }
    RequestEncodingParametersChangeInternal(parameters, std::nullopt);
    status_ = WEBRTC_VIDEO_CODEC_OK;
    async_init_event_.SetAndReset(WEBRTC_VIDEO_CODEC_OK);
    return;
  }

  DVLOG(3) << __func__ << " expecting new buffers, old size "
           << input_visible_size_.ToString();
  init_spatial_layer_resolutions_.clear();
  for (const auto& layer : spatial_layers) {
    init_spatial_layer_resolutions_.emplace_back(layer.width, layer.height);
  }
  encoder_metrics_provider_->Initialize(
      profile, input_visible_size,
      /*is_hardware_encoder=*/true,
      ToSVCScalabilityMode(spatial_layers, inter_layer_pred));

  RequestEncodingParametersChangeInternal(parameters, input_visible_size);

  input_visible_size_ = input_visible_size;
}

void RTCVideoEncoder::Impl::RecordTimestampMatchUMA() const {
  base::UmaHistogramBoolean("Media.RTCVideoEncoderTimestampMatchSuccess",
                            !failed_timestamp_match_);
}

ActiveSpatialLayers RTCVideoEncoder::Impl::GetActiveSpatialLayers() const {
  if (init_spatial_layer_resolutions_.empty()) {
    return ActiveSpatialLayers();
  }
  return active_spatial_layers_;
}

void RTCVideoEncoder::Impl::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  TRACE_EVENT0("webrtc", "RTCVideoEncoder::Impl::RequireBitstreamBuffers");
  DVLOG(3) << __func__ << " input_count=" << input_count
           << ", input_coded_size=" << input_coded_size.ToString()
           << ", output_buffer_size=" << output_buffer_size;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto scoped_event = std::move(async_init_event_);
  if (!video_encoder_)
    return;

  input_frame_coded_size_ = input_coded_size;
  input_buffers_requested_count_ = input_count + kInputBufferExtraCount;

  // |input_buffers_| is only needed in non import mode.
  if (!use_native_input_) {
    input_buffers_free_.resize(input_buffers_requested_count_);
    input_buffers_.resize(input_buffers_requested_count_);
    for (wtf_size_t i = 0; i < input_buffers_requested_count_; i++) {
      input_buffers_free_[i] = i;
      input_buffers_[i] = nullptr;
    }
  }

  output_buffers_.clear();
  for (int i = 0; i < kOutputBufferCount; ++i) {
    base::UnsafeSharedMemoryRegion region =
        gpu_factories_->CreateSharedMemoryRegion(output_buffer_size);
    base::WritableSharedMemoryMapping mapping = region.Map();
    if (!mapping.IsValid()) {
      NotifyErrorStatus({media::EncoderStatus::Codes::kSystemAPICallError,
                         "failed to create output buffer"});
      return;
    }
    output_buffers_.push_back(std::make_pair(
        std::move(region),
        base::MakeRefCounted<RefCountedWritableSharedMemoryMapping>(
            std::move(mapping))));
  }
  encoded_buffer_reference_holder_ =
      std::make_unique<EncodedBufferReferenceHolder>(weak_this_);

  // Immediately provide all output buffers to the VEA.
  for (wtf_size_t i = 0; i < output_buffers_.size(); ++i) {
    UseOutputBitstreamBuffer(i);
  }

  pending_output_buffers_.clear();
  pending_output_buffers_.reserve(output_buffers_.size());

  DCHECK_EQ(status_, WEBRTC_VIDEO_CODEC_UNINITIALIZED);
  status_ = WEBRTC_VIDEO_CODEC_OK;

  scoped_event.SetAndReset(WEBRTC_VIDEO_CODEC_OK);
}

void RTCVideoEncoder::Impl::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const media::BitstreamBufferMetadata& metadata) {
  TRACE_EVENT2("webrtc", "RTCVideoEncoder::Impl::BitstreamBufferReady",
               "timestamp", metadata.timestamp.InMicroseconds(),
               "bitstream_buffer_id", bitstream_buffer_id);
  DVLOG(3) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id
           << ", payload_size=" << metadata.payload_size_bytes
           << ", end_of_picture=" << metadata.end_of_picture()
           << ", key_frame=" << metadata.key_frame
           << ", timestamp ms=" << metadata.timestamp.InMicroseconds();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (bitstream_buffer_id < 0 ||
      bitstream_buffer_id >= static_cast<int>(output_buffers_.size())) {
    NotifyErrorStatus({media::EncoderStatus::Codes::kInvalidOutputBuffer,
                       "invalid bitstream_buffer_id: " +
                           base::NumberToString(bitstream_buffer_id)});
    return;
  }

  DCHECK_NE(output_buffers_in_encoder_count_, 0u);
  output_buffers_in_encoder_count_--;

  // Decrease |frames_in_encoder_count_| on the first frame so that
  // UseOutputBitstreamBuffer() is not called until next frame if no frame but
  // the current frame is in VideoEncodeAccelerator.
  if (metadata.spatial_idx().value_or(0) == 0) {
    CHECK_NE(0u, frames_in_encoder_count_);
    frames_in_encoder_count_--;
  }

  if (status_ == WEBRTC_VIDEO_CODEC_UNINITIALIZED) {
    // The encoder has been suspended, drain remaining frames.
    BitstreamBufferAvailable(bitstream_buffer_id);
    return;
  }

  // An encoder drops a frame.
  if (metadata.dropped_frame()) {
    BitstreamBufferAvailable(bitstream_buffer_id);
    // Invoke OnDroppedFrame() only in the end of picture. How to call
    // OnDroppedFrame() in spatial layers is not defined in the webrtc encoder
    // API. We call once in spatial layers. This point will be fixed in a
    // new WebRTC encoder API.
    if (metadata.end_of_picture()) {
      base::AutoLock lock(lock_);
      if (!encoded_image_callback_) {
        return;
      }
      encoded_image_callback_->OnDroppedFrame(
          webrtc::EncodedImageCallback::DropReason::kDroppedByEncoder);
    }
    return;
  }

  scoped_refptr<RefCountedWritableSharedMemoryMapping> output_mapping =
      output_buffers_[bitstream_buffer_id].second;
  if (metadata.payload_size_bytes >
      output_buffers_[bitstream_buffer_id].second->size()) {
    NotifyErrorStatus({media::EncoderStatus::Codes::kInvalidOutputBuffer,
                       "invalid payload_size: " +
                           base::NumberToString(metadata.payload_size_bytes)});
    return;
  }

  if (metadata.end_of_picture()) {
    CHECK(encoder_metrics_provider_);
    encoder_metrics_provider_->IncrementEncodedFrameCount();
  }

  // Find RTP and capture timestamps by going through |pending_timestamps_|.
  // Derive it from current time otherwise.
  std::optional<uint32_t> rtp_timestamp;
  std::optional<int64_t> capture_timestamp_ms;
  std::optional<ActiveSpatialLayers> expected_active_spatial_layers;
  if (!failed_timestamp_match_) {
    // Pop timestamps until we have a match.
    while (!submitted_frames_.empty()) {
      auto& front_frame = submitted_frames_.front();
      const bool end_of_picture = metadata.end_of_picture();
      if (front_frame.media_timestamp_ == metadata.timestamp) {
        rtp_timestamp = front_frame.rtp_timestamp_;
        capture_timestamp_ms = front_frame.capture_time_ms_;
        expected_active_spatial_layers = front_frame.active_spatial_layers_;
        const size_t num_spatial_layers =
            std::max(front_frame.active_spatial_layers_.size(), size_t{1});
        ++front_frame.produced_frames_;

        if (front_frame.produced_frames_ == num_spatial_layers &&
            !end_of_picture) {
          // The top layer must always have the end-of-picture indicator.
          NotifyErrorStatus({media::EncoderStatus::Codes::kEncoderFailedEncode,
                             "missing end-of-picture"});
          return;
        }
        if (end_of_picture) {
          // Remove pending timestamp at the top spatial layer in the case of
          // SVC encoding.
          if (front_frame.produced_frames_ != num_spatial_layers) {
            // At least one resolution was not produced.
            NotifyErrorStatus(
                {media::EncoderStatus::Codes::kEncoderFailedEncode,
                 "missing resolution"});
            return;
          }
          submitted_frames_.pop_front();
        }
        break;
      }
      submitted_frames_.pop_front();
    }
    DCHECK(rtp_timestamp.has_value());
  }

  if (!rtp_timestamp.has_value() || !capture_timestamp_ms.has_value()) {
    failed_timestamp_match_ = true;
    submitted_frames_.clear();
    const int64_t current_time_ms =
        rtc::TimeMicros() / base::Time::kMicrosecondsPerMillisecond;
    // RTP timestamp can wrap around. Get the lower 32 bits.
    rtp_timestamp = static_cast<uint32_t>(current_time_ms * 90);
    capture_timestamp_ms = current_time_ms;
  }

  // Only H.265 bitstream may need a fix. If a fixed bitstream is available, the
  // original bitstream buffer can be released immediately.
  bool fixed_bitstream = false;
  webrtc::EncodedImage image;
#if BUILDFLAG(RTC_USE_H265)
  if (ps_tracker_.get()) {
    H265ParameterSetsTracker::FixedBitstream fixed =
        ps_tracker_->MaybeFixBitstream(rtc::MakeArrayView(
            output_mapping->front(), metadata.payload_size_bytes));
    if (fixed.action == H265ParameterSetsTracker::PacketAction::kInsert) {
      image.SetEncodedData(fixed.bitstream);
      BitstreamBufferAvailable(bitstream_buffer_id);
      fixed_bitstream = true;
    }
  }
#endif  // BUILDFLAG(RTC_USE_H265)
  if (!fixed_bitstream) {
    image.SetEncodedData(rtc::make_ref_counted<EncodedDataWrapper>(
        std::move(output_mapping), metadata.payload_size_bytes,
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            &EncodedBufferReferenceHolder::BitstreamBufferAvailable,
            encoded_buffer_reference_holder_->GetWeakPtr(),
            bitstream_buffer_id))));
  }

  auto encoded_size = metadata.encoded_size.value_or(input_visible_size_);

  image._encodedWidth = encoded_size.width();
  image._encodedHeight = encoded_size.height();
  image.SetRtpTimestamp(rtp_timestamp.value());
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
  if (scalability_mode_.has_value()) {
    info.scalability_mode = scalability_mode_;
  }
  switch (video_codec_type_) {
    case webrtc::kVideoCodecH264: {
      webrtc::CodecSpecificInfoH264& h264 = info.codecSpecific.H264;
      h264.packetization_mode = webrtc::H264PacketizationMode::NonInterleaved;
      h264.idr_frame = metadata.key_frame;
      if (metadata.h264) {
        h264.temporal_idx = metadata.h264->temporal_idx;
        h264.base_layer_sync = metadata.h264->layer_sync;
        image.SetTemporalIndex(metadata.h264->temporal_idx);
      } else {
        h264.temporal_idx = webrtc::kNoTemporalIdx;
        h264.base_layer_sync = false;
      }
    } break;
    case webrtc::kVideoCodecVP8:
      info.codecSpecific.VP8.keyIdx = -1;
      if (metadata.vp8) {
        image.SetTemporalIndex(metadata.vp8->temporal_idx);
      }
      break;
    case webrtc::kVideoCodecVP9: {
      webrtc::CodecSpecificInfoVP9& vp9 = info.codecSpecific.VP9;
      if (metadata.vp9) {
        // Temporal and/or spatial layer stream.
        CHECK(expected_active_spatial_layers);
        if (metadata.key_frame) {
          if (metadata.vp9->spatial_layer_resolutions.empty()) {
            NotifyErrorStatus(
                {media::EncoderStatus::Codes::kEncoderFailedEncode,
                 "SVC resolution metadata is not filled on keyframe"});
            return;
          }

          CHECK_NE(expected_active_spatial_layers->end_index, 0u);
          const size_t expected_begin_index =
              expected_active_spatial_layers->begin_index;
          const size_t expected_end_index =
              expected_active_spatial_layers->end_index;
          const size_t begin_index =
              metadata.vp9->begin_active_spatial_layer_index;
          const size_t end_index = metadata.vp9->end_active_spatial_layer_index;
          if (begin_index != expected_begin_index ||
              end_index != expected_end_index) {
            NotifyErrorStatus(
                {media::EncoderStatus::Codes::kEncoderFailedEncode,
                 base::StrCat({"SVC active layer indices don't match "
                               "request: expected [",
                               base::NumberToString(expected_begin_index), ", ",
                               base::NumberToString(expected_end_index),
                               "), but got [",
                               base::NumberToString(begin_index), ", ",
                               base::NumberToString(end_index), ")"})});
            return;
          }

          const std::vector<gfx::Size> expected_resolutions(
              init_spatial_layer_resolutions_.begin() + begin_index,
              init_spatial_layer_resolutions_.begin() + end_index);
          if (metadata.vp9->spatial_layer_resolutions != expected_resolutions) {
            NotifyErrorStatus(
                {media::EncoderStatus::Codes::kEncoderFailedEncode,
                 "Encoded SVC resolution set does not match request"});
            return;
          }
        }
        const ActiveSpatialLayers& vea_active_spatial_layers =
            *expected_active_spatial_layers;
        CHECK_NE(vea_active_spatial_layers.end_index, 0u);
        const uint8_t spatial_index =
            metadata.vp9->spatial_idx + vea_active_spatial_layers.begin_index;
        if (spatial_index >= init_spatial_layer_resolutions_.size()) {
          NotifyErrorStatus(
              {media::EncoderStatus::Codes::kInvalidOutputBuffer,
               base::StrCat(
                   {"spatial_idx=", base::NumberToString(spatial_index),
                    " is not less than init_spatial_layer_resolutions_.size()=",
                    base::NumberToString(
                        init_spatial_layer_resolutions_.size())})});
          return;
        }
        if (spatial_index >= vea_active_spatial_layers.end_index) {
          NotifyErrorStatus(
              {media::EncoderStatus::Codes::kInvalidOutputBuffer,
               base::StrCat(
                   {"spatial_idx=", base::NumberToString(spatial_index),
                    " is not less than vea_active_spatial_layers.end_index=",
                    base::NumberToString(
                        vea_active_spatial_layers.end_index)})});
          return;
        }
        image._encodedWidth =
            init_spatial_layer_resolutions_[spatial_index].width();
        image._encodedHeight =
            init_spatial_layer_resolutions_[spatial_index].height();
        image.SetSpatialIndex(spatial_index);
        image.SetTemporalIndex(metadata.vp9->temporal_idx);

        vp9.first_frame_in_picture =
            spatial_index == vea_active_spatial_layers.begin_index;
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

        // |num_spatial_layers| is not the number of active spatial layers,
        // but the highest spatial layer + 1.
        vp9.first_active_layer = vea_active_spatial_layers.begin_index;
        vp9.num_spatial_layers = vea_active_spatial_layers.end_index;

        if (vp9.ss_data_available) {
          vp9.spatial_layer_resolution_present = true;
          vp9.gof.num_frames_in_gof = 0;
          for (size_t i = 0; i < vea_active_spatial_layers.begin_index; ++i) {
            // Signal disabled layers.
            vp9.width[i] = 0;
            vp9.height[i] = 0;
          }
          for (size_t i = vea_active_spatial_layers.begin_index;
               i < vea_active_spatial_layers.end_index; ++i) {
            wtf_size_t wtf_i = base::checked_cast<wtf_size_t>(i);
            vp9.width[i] = init_spatial_layer_resolutions_[wtf_i].width();
            vp9.height[i] = init_spatial_layer_resolutions_[wtf_i].height();
          }
        }
        vp9.flexible_mode = true;
        vp9.gof_idx = 0;
        info.end_of_picture = metadata.end_of_picture();
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
      // TODO(bugs.webrtc.org/11999): Fill `info.generic_frame_info` to
      // provide more accurate description of used layering than webrtc can
      // simulate based on the codec specific info.
    } break;
    default:
      break;
  }

  if (simulcast_to_svc_converter_) {
    simulcast_to_svc_converter_->ConvertFrame(image, info);
  }

  base::AutoLock lock(lock_);
  if (!encoded_image_callback_)
    return;

  const auto result = encoded_image_callback_->OnEncodedImage(image, &info);
  if (result.error != webrtc::EncodedImageCallback::Result::OK) {
    DVLOG(2)
        << "ReturnEncodedImage(): webrtc::EncodedImageCallback::Result.error = "
        << result.error;
  }
}

void RTCVideoEncoder::Impl::NotifyErrorStatus(
    const media::EncoderStatus& status) {
  TRACE_EVENT0("webrtc", "RTCVideoEncoder::Impl::NotifyErrorStatus");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!status.is_ok());
  LOG(ERROR) << "NotifyErrorStatus is called with code="
             << static_cast<int>(status.code())
             << ", message=" << status.message();
  if (encoder_metrics_provider_) {
    // |encoder_metrics_provider_| is nullptr if NotifyErrorStatus() is called
    // before it is created in CreateAndInitializeVEA().
    encoder_metrics_provider_->SetError(status);
  }
  // Don't count the error multiple times.
  if (status_ != WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE) {
    RecordEncoderStatusUMA(status, video_codec_type_);
  }

  input_visible_size_ = gfx::Size();

  video_encoder_.reset();
  status_ = WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;

  async_init_event_.SetAndReset(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);

  execute_software_fallback_.Run();
}

RTCVideoEncoder::Impl::~Impl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RecordTimestampMatchUMA();
  if (video_encoder_) {
    video_encoder_.reset();
    status_ = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    RecordEncoderStatusUMA(media::EncoderStatus::Codes::kOk, video_codec_type_);
  }

  async_init_event_.reset();

  encoded_buffer_reference_holder_.reset();

  // weak_this_ must be invalidated in |gpu_task_runner_|.
  weak_this_factory_.InvalidateWeakPtrs();
}

void RTCVideoEncoder::Impl::EncodeOneFrame(FrameChunk frame_chunk) {
  DVLOG(3) << "Impl::EncodeOneFrame()";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!input_buffers_free_.empty());
  TRACE_EVENT1("webrtc", "RTCVideoEncoder::Impl::EncodeOneFrame", "timestamp",
               frame_chunk.timestamp_us);

  if (!video_encoder_) {
    return;
  }

  const base::TimeDelta timestamp =
      base::Microseconds(frame_chunk.timestamp_us);

  scoped_refptr<media::VideoFrame> frame;
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer =
      frame_chunk.video_frame_buffer;

  // All non-native frames require a copy because we can't tell if non-copy
  // conditions are met.
  bool requires_copy_or_scale =
      frame_buffer->type() != webrtc::VideoFrameBuffer::Type::kNative;
  if (!requires_copy_or_scale) {
    const WebRtcVideoFrameAdapter* frame_adapter =
        static_cast<WebRtcVideoFrameAdapter*>(frame_buffer.get());
    frame = frame_adapter->getMediaVideoFrame();
    frame->set_timestamp(timestamp);
    const media::VideoFrame::StorageType storage = frame->storage_type();
    const bool is_memory_based_frame =
        storage == media::VideoFrame::STORAGE_UNOWNED_MEMORY ||
        storage == media::VideoFrame::STORAGE_OWNED_MEMORY ||
        storage == media::VideoFrame::STORAGE_SHMEM;
    const bool is_right_format = frame->format() == media::PIXEL_FORMAT_I420 ||
                                 frame->format() == media::PIXEL_FORMAT_NV12;
    requires_copy_or_scale =
        !is_right_format || RequiresSizeChange(*frame) ||
        !(is_memory_based_frame || frame->HasMappableGpuBuffer());
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
        frame_buffer->type() == webrtc::VideoFrameBuffer::Type::kNative &&
        input_frame_coded_size_ == input_visible_size_;
#else
        // TODO(https://crbug.com/1307206): Android (e.g. android-pie-arm64-rel)
        // and CrOS does not support the native buffer scaling path. Investigate
        // why and find a way to enable it, if possible.
        false;
#endif
    if (native_buffer_scaling) {
      DCHECK_EQ(frame_buffer->type(), webrtc::VideoFrameBuffer::Type::kNative);
      auto scaled_buffer = frame_buffer->Scale(input_visible_size_.width(),
                                               input_visible_size_.height());
      auto mapped_buffer =
          scaled_buffer->GetMappedFrameBuffer(preferred_pixel_formats_);
      if (!mapped_buffer) {
        mapped_buffer = scaled_buffer->ToI420();
      }
      if (!mapped_buffer) {
        NotifyErrorStatus({media::EncoderStatus::Codes::kSystemAPICallError,
                           "Failed to map buffer"});
        return;
      }

      DCHECK_NE(mapped_buffer->type(), webrtc::VideoFrameBuffer::Type::kNative);
      frame = ConvertFromMappedWebRtcVideoFrameBuffer(mapped_buffer, timestamp);
      if (!frame) {
        NotifyErrorStatus(
            {media::EncoderStatus::Codes::kFormatConversionError,
             "Failed to convert WebRTC mapped buffer to media::VideoFrame"});
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
        if (!input_buffers_[index]->IsValid()) {
          NotifyErrorStatus({media::EncoderStatus::Codes::kSystemAPICallError,
                             "Failed to create input buffer"});
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
        NotifyErrorStatus({media::EncoderStatus::Codes::kEncoderFailedEncode,
                           "Failed to create input buffer"});
        return;
      }

      // |frame| is STORAGE_UNOWNED_MEMORY at this point. Writing the data is
      // allowed.
      // Do a strided copy and scale (if necessary) the input frame to match
      // the input requirements for the encoder.
      // TODO(magjed): Downscale with an image pyramid instead.
      rtc::scoped_refptr<webrtc::I420BufferInterface> i420_buffer =
          frame_buffer->ToI420();
      if (libyuv::I420Scale(
              i420_buffer->DataY(), i420_buffer->StrideY(),
              i420_buffer->DataU(), i420_buffer->StrideU(),
              i420_buffer->DataV(), i420_buffer->StrideV(),
              i420_buffer->width(), i420_buffer->height(),
              frame->GetWritableVisibleData(media::VideoFrame::Plane::kY),
              frame->stride(media::VideoFrame::Plane::kY),
              frame->GetWritableVisibleData(media::VideoFrame::Plane::kU),
              frame->stride(media::VideoFrame::Plane::kU),
              frame->GetWritableVisibleData(media::VideoFrame::Plane::kV),
              frame->stride(media::VideoFrame::Plane::kV),
              frame->visible_rect().width(), frame->visible_rect().height(),
              libyuv::kFilterBox)) {
        NotifyErrorStatus({media::EncoderStatus::Codes::kFormatConversionError,
                           "Failed to copy buffer"});
        return;
      }

      // |frame| becomes STORAGE_SHMEM. Writing the buffer is not permitted
      // after here.
      frame->BackWithSharedMemory(&region);

      input_buffers_free_.pop_back();
      frame->AddDestructionObserver(
          base::BindPostTaskToCurrentDefault(WTF::BindOnce(
              &RTCVideoEncoder::Impl::InputBufferReleased, weak_this_, index)));
    }
  }

  if (!failed_timestamp_match_) {
    DCHECK(!base::Contains(submitted_frames_, timestamp,
                           &FrameInfo::media_timestamp_));
    submitted_frames_.emplace_back(timestamp, frame_chunk.timestamp,
                                   frame_chunk.render_time_ms,
                                   GetActiveSpatialLayers());
  }

  // Call UseOutputBitstreamBuffer() for pending output buffers.
  for (const auto& bitstream_buffer_id : pending_output_buffers_) {
    UseOutputBitstreamBuffer(bitstream_buffer_id);
  }
  pending_output_buffers_.clear();

  if (simulcast_to_svc_converter_) {
    simulcast_to_svc_converter_->EncodeStarted(frame_chunk.force_keyframe);
  }

  frames_in_encoder_count_++;
  DVLOG(3) << "frames_in_encoder_count=" << frames_in_encoder_count_;
  video_encoder_->Encode(frame, frame_chunk.force_keyframe);
}

void RTCVideoEncoder::Impl::EncodeOneFrameWithNativeInput(
    FrameChunk frame_chunk) {
  DVLOG(3) << "Impl::EncodeOneFrameWithNativeInput()";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(input_buffers_.empty() && input_buffers_free_.empty());
  TRACE_EVENT1("webrtc", "RTCVideoEncoder::Impl::EncodeOneFrameWithNativeInput",
               "timestamp", frame_chunk.timestamp_us);

  if (!video_encoder_) {
    return;
  }

  scoped_refptr<media::VideoFrame> frame;
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer =
      frame_chunk.video_frame_buffer;
  if (frame_buffer->type() != webrtc::VideoFrameBuffer::Type::kNative) {
    // If we get a non-native frame it's because the video track is disabled and
    // WebRTC VideoBroadcaster replaces the camera frame with a black YUV frame.
    if (!black_frame_) {
      gfx::Size natural_size(frame_buffer->width(), frame_buffer->height());
      if (!CreateBlackMappableSIFrame(natural_size)) {
        NotifyErrorStatus({media::EncoderStatus::Codes::kSystemAPICallError,
                           "Failed to allocate native buffer for black frame"});
        return;
      }
    }
    frame = media::VideoFrame::WrapVideoFrame(
        black_frame_, black_frame_->format(), black_frame_->visible_rect(),
        black_frame_->natural_size());
  } else {
    frame = static_cast<WebRtcVideoFrameAdapter*>(frame_buffer.get())
                ->getMediaVideoFrame();
  }
  frame->set_timestamp(base::Microseconds(frame_chunk.timestamp_us));

  if (frame->storage_type() != media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    NotifyErrorStatus({media::EncoderStatus::Codes::kInvalidInputFrame,
                       "frame isn't mappable shared image based VideoFrame"});
    return;
  }

  if (!failed_timestamp_match_) {
    DCHECK(!base::Contains(submitted_frames_, frame->timestamp(),
                           &FrameInfo::media_timestamp_));
    submitted_frames_.emplace_back(frame->timestamp(), frame_chunk.timestamp,
                                   frame_chunk.render_time_ms,
                                   GetActiveSpatialLayers());
  }

  // Call UseOutputBitstreamBuffer() for pending output buffers.
  for (const auto& bitstream_buffer_id : pending_output_buffers_) {
    UseOutputBitstreamBuffer(bitstream_buffer_id);
  }
  pending_output_buffers_.clear();

  frames_in_encoder_count_++;
  DVLOG(3) << "frames_in_encoder_count=" << frames_in_encoder_count_;

  video_encoder_->Encode(frame, frame_chunk.force_keyframe);
}

bool RTCVideoEncoder::Impl::CreateBlackMappableSIFrame(
    const gfx::Size& natural_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto buffer_format = gfx::BufferFormat::YUV_420_BIPLANAR;
  const auto si_format = viz::GetSharedImageFormat(buffer_format);
  const auto buffer_usage =
      gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE;

  // Setting some default usage in order to get a mappable shared image.
  const auto si_usage =
      gpu::SHARED_IMAGE_USAGE_CPU_WRITE | gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;

  auto* sii = gpu_factories_->SharedImageInterface();
  if (!sii) {
    return false;
  }

  auto shared_image = sii->CreateSharedImage(
      {si_format, natural_size, gfx::ColorSpace(),
       gpu::SharedImageUsageSet(si_usage), "RTCVideoEncoder"},
      gpu::kNullSurfaceHandle, buffer_usage);
  if (!shared_image) {
    LOG(ERROR) << "Unable to create a mappable shared image.";
    return false;
  }

  // Map in order to write to it.
  auto mapping = shared_image->Map();
  if (!mapping) {
    LOG(ERROR) << "Mapping shared image failed.";
    sii->DestroySharedImage(gpu::SyncToken(), std::move(shared_image));
    return false;
  }

  // Fills the NV12 frame with YUV black (0x00, 0x80, 0x80).
  std::ranges::fill(mapping->GetMemoryForPlane(0), 0x0);
  std::ranges::fill(mapping->GetMemoryForPlane(1), 0x80);

  gpu::SyncToken sync_token = sii->GenVerifiedSyncToken();
  black_frame_ = media::VideoFrame::WrapMappableSharedImage(
      std::move(shared_image), sync_token, base::NullCallback(),
      gfx::Rect(mapping->Size()), natural_size, base::TimeDelta());
  return true;
}

void RTCVideoEncoder::Impl::InputBufferReleased(int index) {
  TRACE_EVENT0("webrtc", "RTCVideoEncoder::Impl::InputBufferReleased");
  DVLOG(3) << "Impl::InputBufferReleased(): index=" << index;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!use_native_input_);

  // NotfiyError() has been called. Don't proceed the frame completion.
  if (!video_encoder_)
    return;

  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(input_buffers_.size()));
  input_buffers_free_.push_back(index);

  while (!pending_frames_.empty() && !input_buffers_free_.empty()) {
    auto chunk = std::move(pending_frames_.front());
    pending_frames_.pop_front();
    EncodeOneFrame(std::move(chunk));
  }
}

bool RTCVideoEncoder::Impl::RequiresSizeChange(
    const media::VideoFrame& frame) const {
  return (frame.coded_size() != input_frame_coded_size_ ||
          frame.visible_rect() != gfx::Rect(input_visible_size_));
}

void RTCVideoEncoder::Impl::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  DVLOG(3) << __func__;
  base::AutoLock lock(lock_);
  encoded_image_callback_ = callback;
}

#if BUILDFLAG(RTC_USE_H265)
void RTCVideoEncoder::Impl::SetH265ParameterSetsTrackerForTesting(
    std::unique_ptr<H265ParameterSetsTracker> tracker) {
  ps_tracker_ = std::move(tracker);
}
#endif

RTCVideoEncoder::RTCVideoEncoder(
    media::VideoCodecProfile profile,
    bool is_constrained_h264,
    media::GpuVideoAcceleratorFactories* gpu_factories,
    scoped_refptr<media::MojoVideoEncoderMetricsProviderFactory>
        encoder_metrics_provider_factory)
    : profile_(profile),
      is_constrained_h264_(is_constrained_h264),
      gpu_factories_(gpu_factories),
      encoder_metrics_provider_factory_(
          std::move(encoder_metrics_provider_factory)),
      gpu_task_runner_(gpu_factories->GetTaskRunner()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  CHECK(encoder_metrics_provider_factory_);
  DVLOG(1) << "RTCVideoEncoder(): profile=" << GetProfileName(profile);

  // The default values of EncoderInfo.
  encoder_info_.scaling_settings = webrtc::VideoEncoder::ScalingSettings::kOff;
  encoder_info_.requested_resolution_alignment = 1;
  encoder_info_.apply_alignment_to_all_simulcast_layers = false;
  encoder_info_.supports_native_handle = true;
  encoder_info_.implementation_name = "ExternalEncoder";
  encoder_info_.has_trusted_rate_controller = false;
  encoder_info_.is_hardware_accelerated = true;
  encoder_info_.is_qp_trusted = true;
  encoder_info_.fps_allocation[0] = {
      webrtc::VideoEncoder::EncoderInfo::kMaxFramerateFraction};
  DCHECK(encoder_info_.resolution_bitrate_limits.empty());
  // Simulcast is supported for VP9 codec if svc is supported.
  // Since this encoder is used for all codecs, need to always
  // report true.
  encoder_info_.supports_simulcast =
      media::IsVp9kSVCHWEncodingEnabled() &&
      base::FeatureList::IsEnabled(
          features::kRtcVideoEncoderConvertSimulcastToSvc);
  encoder_info_.preferred_pixel_formats = {
      webrtc::VideoFrameBuffer::Type::kI420};

  impl_initialized_ = false;
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

RTCVideoEncoder::~RTCVideoEncoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  DVLOG(3) << __func__;

  // |weak_this_| must be invalidated on |webrtc_sequence_checker_|.
  weak_this_factory_.InvalidateWeakPtrs();

  ReleaseImpl();

  DCHECK(!impl_);

  // |encoder_metrics_provider_factory_| needs to be destroyed on the same
  // sequence as one that destroys the VideoEncoderMetricsProviders created by
  // it. It is gpu task runner in this case.
  gpu_task_runner_->ReleaseSoon(FROM_HERE,
                                std::move(encoder_metrics_provider_factory_));
}

int32_t RTCVideoEncoder::DrainEncoderAndUpdateFrameSize(
    const gfx::Size& input_visible_size,
    const webrtc::VideoEncoder::RateControlParameters& params,
    const media::SVCInterLayerPredMode& inter_layer_pred,
    const std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>&
        spatial_layers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);

  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::WaitableEvent initialization_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  int32_t initialization_retval = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  {
    int32_t drain_result;
    base::WaitableEvent drain_waiter(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    PostCrossThreadTask(
        *gpu_task_runner_.get(), FROM_HERE,
        CrossThreadBindOnce(&RTCVideoEncoder::Impl::Drain, weak_impl_,
                            SignaledValue(&drain_waiter, &drain_result)));
    drain_waiter.Wait();
    DVLOG(3) << __func__ << " Drain complete, status " << drain_result;

    if (drain_result != WEBRTC_VIDEO_CODEC_OK &&
        drain_result != WEBRTC_VIDEO_CODEC_UNINITIALIZED) {
      return drain_result;
    }
  }

  DVLOG(3) << __func__ << ": updating frame size on existing instance";
  PostCrossThreadTask(
      *gpu_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          &RTCVideoEncoder::Impl::RequestEncodingParametersChangeWithSizeChange,
          weak_impl_, params, input_visible_size, profile_, inter_layer_pred,
          spatial_layers,
          SignaledValue(&initialization_waiter, &initialization_retval)));
  initialization_waiter.Wait();
  return initialization_retval;
}

int32_t RTCVideoEncoder::InitializeEncoder(
    const media::VideoEncodeAccelerator::Config& vea_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  TRACE_EVENT1("webrtc", "RTCVideoEncoder::InitEncode", "config",
               vea_config.AsHumanReadableString());
  DVLOG(1) << __func__ << ": config=" << vea_config.AsHumanReadableString();
  auto init_start = base::TimeTicks::Now();
  // This wait is necessary because this task is completed in GPU process
  // asynchronously but WebRTC API is synchronous.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::WaitableEvent initialization_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  int32_t initialization_retval = WEBRTC_VIDEO_CODEC_UNINITIALIZED;

  if (!impl_initialized_) {
    DVLOG(3) << __func__ << ": CreateAndInitializeVEA";
    PostCrossThreadTask(
        *gpu_task_runner_.get(), FROM_HERE,
        CrossThreadBindOnce(
            &RTCVideoEncoder::Impl::CreateAndInitializeVEA, weak_impl_,
            vea_config,
            SignaledValue(&initialization_waiter, &initialization_retval)));
    // webrtc::VideoEncoder expects this call to be synchronous.
    initialization_waiter.Wait();
    if (initialization_retval == WEBRTC_VIDEO_CODEC_OK) {
      UMA_HISTOGRAM_TIMES("WebRTC.RTCVideoEncoder.Initialize",
                          base::TimeTicks::Now() - init_start);
      impl_initialized_ = true;
    }
    RecordInitEncodeUMA(initialization_retval, profile_);
  } else {
    DCHECK(frame_size_change_supported_);
    webrtc::VideoEncoder::RateControlParameters params(
        AllocateBitrateForVEAConfig(vea_config), vea_config.framerate);
    initialization_retval = DrainEncoderAndUpdateFrameSize(
        vea_config.input_visible_size, params, vea_config.inter_layer_pred,
        vea_config.spatial_layers);
  }
  return initialization_retval;
}

bool RTCVideoEncoder::CodecSettingsUsableForFrameSizeChange(
    const webrtc::VideoCodec& codec_settings) const {
  if (codec_settings.codecType != codec_settings_.codecType) {
    return false;
  }
  if (codec_settings.GetScalabilityMode() !=
      codec_settings_.GetScalabilityMode()) {
    return false;
  }
  if (codec_settings.GetFrameDropEnabled() !=
      codec_settings_.GetFrameDropEnabled()) {
    return false;
  }
  if (codec_settings.mode != codec_settings_.mode) {
    return false;
  }

  if (codec_settings.codecType == webrtc::kVideoCodecVP9) {
    const auto vp9 = codec_settings_.VP9();
    const auto new_vp9 = codec_settings.VP9();
    if (vp9.numberOfTemporalLayers != new_vp9.numberOfTemporalLayers ||
        vp9.numberOfSpatialLayers != new_vp9.numberOfSpatialLayers ||
        vp9.interLayerPred != new_vp9.interLayerPred) {
      return false;
    }
  }
  return true;
}

int32_t RTCVideoEncoder::InitEncode(
    const webrtc::VideoCodec* codec_settings,
    const webrtc::VideoEncoder::Settings& settings) {
  TRACE_EVENT0("webrtc", "RTCVideoEncoder::InitEncode");
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  DVLOG(1) << __func__ << " codecType=" << codec_settings->codecType
           << ", width=" << codec_settings->width
           << ", height=" << codec_settings->height
           << ", startBitrate=" << codec_settings->startBitrate;

  // Try to rewrite the simulcast config as SVC one.
  webrtc::VideoCodec converted_settings;
  std::optional<webrtc::SimulcastToSvcConverter> simulcast_to_svc_converter;

  int32_t initialization_error_message = WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;

  if (codec_settings->numberOfSimulcastStreams > 1) {
    // No VEA currently supports simulcast. It, however, can be
    // emulated with SVC VP9 if the streams have the same temporal
    // settings and 4:2:1 scaling.
    if (codec_settings->codecType != webrtc::kVideoCodecVP9 ||
        !base::FeatureList::IsEnabled(
            features::kRtcVideoEncoderConvertSimulcastToSvc) ||
        !webrtc::SimulcastUtility::ValidSimulcastParameters(
            *codec_settings, codec_settings->numberOfSimulcastStreams)) {
      return WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;
    }
    simulcast_to_svc_converter.emplace(*codec_settings);
    converted_settings = simulcast_to_svc_converter->GetConfig();
    // If we've rewritten config, never report software fallback on errors.
    // Let the WebRTC try to initialize each simulcast stream separately.
    initialization_error_message =
        WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;
  } else {
    converted_settings = *codec_settings;
  }

  if (impl_) {
    if (!impl_initialized_ || has_error_ || !frame_size_change_supported_ ||
        !CodecSettingsUsableForFrameSizeChange(converted_settings)) {
      DVLOG(3) << __func__ << " ReleaseImpl";
      ReleaseImpl();
    }
  }

  codec_settings_ = converted_settings;

  // Several HW encoders are known to yield worse quality compared to SW
  // encoders for smaller resolutions such as 180p. (270p should also be a
  // problem but some HW encoders already fallback for resolutions not divisible
  // by 4.) At 360p, manual testing suggests HW and SW are roughly on par in
  // terms of quality.
  //
  // By default, Android is excluded from this logic because there are
  // situations where a codec like H264 is available in HW but not SW in which
  // case SW fallback would result in a change of codec, see
  // https://crbug.com/1469318.
  //
  // H.265 does not support SW fallback, so it is excluded from low resoloution
  // fallback.
  if (codec_settings_.codecType != webrtc::kVideoCodecH265 &&
      base::FeatureList::IsEnabled(features::kForceSoftwareForLowResolutions)) {
    uint16_t force_sw_height = 359;
    if (base::FeatureList::IsEnabled(features::kForcingSoftwareIncludes360)) {
      force_sw_height = 360;
    }
    if (codec_settings_.height <= force_sw_height) {
      LOG(WARNING)
          << "Fallback to SW due to low resolution being less than 360p ("
          << codec_settings_.width << "x" << codec_settings_.height << ")";
      return initialization_error_message;
    }
  }

  if (codec_settings_.codecType == webrtc::kVideoCodecH264 &&
      (codec_settings_.width % 2 != 0 || codec_settings_.height % 2 != 0)) {
    LOG(ERROR) << "Input video size is " << codec_settings_.width << "x"
               << codec_settings_.height << ", "
               << "but hardware H.264 encoder only supports even sized frames.";
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }

  has_error_ = false;

  uint32_t bitrate_bps = 0;
  // Check for overflow converting bitrate (kilobits/sec) to bits/sec.
  if (!ConvertKbpsToBps(codec_settings_.startBitrate, &bitrate_bps)) {
    LOG(ERROR) << "Overflow converting bitrate from kbps to bps: bps="
               << codec_settings_.startBitrate;
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  gfx::Size input_visible_size;
  std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>
      spatial_layers;
  auto inter_layer_pred = media::SVCInterLayerPredMode::kOff;
  if (!CreateSpatialLayersConfig(codec_settings_, &spatial_layers,
                                 &inter_layer_pred, &input_visible_size)) {
    return initialization_error_message;
  }

  // Fallback to SW if VEA does not support VP9 SVC encoding.
  if (codec_settings_.codecType == webrtc::kVideoCodecVP9 &&
      (codec_settings_.VP9()->numberOfTemporalLayers > 1 ||
       codec_settings_.VP9()->numberOfSpatialLayers > 1)) {
    const auto vea_supported_profiles =
        gpu_factories_->GetVideoEncodeAcceleratorSupportedProfiles().value_or(
            media::VideoEncodeAccelerator::SupportedProfiles());
    auto support_profile = base::ranges::find_if(
        vea_supported_profiles,
        [this](const media::VideoEncodeAccelerator::SupportedProfile&
                   support_profile) {
          return this->profile_ == support_profile.profile &&
                 support_profile.scalability_modes.size() > 0;
        });
    if (vea_supported_profiles.end() != support_profile) {
      media::SVCScalabilityMode scalability_mode =
          ToSVCScalabilityMode(spatial_layers, inter_layer_pred);
      if (support_profile->scalability_modes.end() ==
          base::ranges::find_if(
              support_profile->scalability_modes,
              [scalability_mode](const media::SVCScalabilityMode& value) {
                return value == scalability_mode;
              })) {
        return initialization_error_message;
      }
    }
  }

  // Check that |profile| supports |input_visible_size|.
  if (base::FeatureList::IsEnabled(features::kWebRtcUseMinMaxVEADimensions)) {
    const auto vea_supported_profiles =
        gpu_factories_->GetVideoEncodeAcceleratorSupportedProfiles().value_or(
            media::VideoEncodeAccelerator::SupportedProfiles());

    for (const auto& vea_profile : vea_supported_profiles) {
      if (vea_profile.profile == profile_ &&
          (input_visible_size.width() > vea_profile.max_resolution.width() ||
           input_visible_size.height() > vea_profile.max_resolution.height() ||
           input_visible_size.width() < vea_profile.min_resolution.width() ||
           input_visible_size.height() < vea_profile.min_resolution.height())) {
        LOG(ERROR) << "Requested dimensions (" << input_visible_size.ToString()
                   << ") beyond accelerator limits ("
                   << vea_profile.min_resolution.ToString() << " - "
                   << vea_profile.max_resolution.ToString() << ")";
        return initialization_error_message;
      }
    }
  }

  auto webrtc_content_type = webrtc::VideoContentType::UNSPECIFIED;
  auto vea_content_type =
      media::VideoEncodeAccelerator::Config::ContentType::kCamera;
  if (codec_settings_.mode == webrtc::VideoCodecMode::kScreensharing) {
    webrtc_content_type = webrtc::VideoContentType::SCREENSHARE;
    vea_content_type =
        media::VideoEncodeAccelerator::Config::ContentType::kDisplay;
  }

  if (!impl_) {
    // base::Unretained(this) is safe because |impl_| is synchronously destroyed
    // in Release() so that |impl_| does not call UpdateEncoderInfo() after this
    // is destructed.
    Impl::UpdateEncoderInfoCallback update_encoder_info_callback =
        base::BindRepeating(&RTCVideoEncoder::UpdateEncoderInfo,
                            base::Unretained(this));
    base::RepeatingClosure execute_software_fallback =
        base::BindPostTaskToCurrentDefault(base::BindRepeating(
            &RTCVideoEncoder::SetError, weak_this_, ++impl_id_));

    impl_ = std::make_unique<Impl>(
        gpu_factories_, encoder_metrics_provider_factory_,
        ProfileToWebRtcVideoCodecType(profile_),
        codec_settings_.GetScalabilityMode(), webrtc_content_type,
        update_encoder_info_callback, execute_software_fallback, weak_impl_);
  }

  media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_I420;
  auto storage_type =
      media::VideoEncodeAccelerator::Config::StorageType::kShmem;
  if (IsZeroCopyEnabled(webrtc_content_type)) {
    pixel_format = media::PIXEL_FORMAT_NV12;
    storage_type =
        media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;
  }

  media::VideoEncodeAccelerator::Config vea_config(
      pixel_format, input_visible_size, profile_,
      media::Bitrate::ConstantBitrate(bitrate_bps),
      codec_settings_.maxFramerate, storage_type, vea_content_type);
  vea_config.is_constrained_h264 = is_constrained_h264_;
  vea_config.spatial_layers = spatial_layers;
  vea_config.inter_layer_pred = inter_layer_pred;
  vea_config.drop_frame_thresh_percentage =
      GetDropFrameThreshold(codec_settings_);
  // When we don't have built in H264 software encoding, allow usage of any
  // software encoders provided by the platform.
#if !BUILDFLAG(ENABLE_OPENH264) && BUILDFLAG(RTC_USE_H264)
  if (profile_ >= media::H264PROFILE_MIN &&
      profile_ <= media::H264PROFILE_MAX) {
    vea_config.required_encoder_type =
        media::VideoEncodeAccelerator::Config::EncoderType::kNoPreference;
  }
#endif

  int32_t initialization_ret = InitializeEncoder(vea_config);
  if (initialization_ret != WEBRTC_VIDEO_CODEC_OK) {
    ReleaseImpl();
    CHECK(!impl_);
  } else {
    impl_->SetSimulcastToSvcConverter(std::move(simulcast_to_svc_converter));
  }
  return initialization_ret;
}

int32_t RTCVideoEncoder::Encode(
    const webrtc::VideoFrame& input_image,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  TRACE_EVENT1("webrtc", "RTCVideoEncoder::Encode", "timestamp",
               input_image.timestamp_us());
  DVLOG(3) << __func__;
  if (!impl_) {
    DVLOG(3) << "Encoder is not initialized";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  if (has_error_)
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;

  const bool want_key_frame =
      frame_types && frame_types->size() &&
      frame_types->front() == webrtc::VideoFrameType::kVideoFrameKey;
  PostCrossThreadTask(
      *gpu_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&RTCVideoEncoder::Impl::Enqueue, weak_impl_,
                          FrameChunk(input_image, want_key_frame)));
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  DVLOG(3) << __func__;
  if (!impl_) {
    if (!callback)
      return WEBRTC_VIDEO_CODEC_OK;
    DVLOG(3) << "Encoder is not initialized";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  // TOD(b/257021675): RegisterEncodeCompleteCallback() should be called twice,
  // with a valid pointer after InitEncode() and with a nullptr after Release().
  // Setting callback in |impl_| should be done asynchronously by posting the
  // task to |media_task_runner_|.
  // However, RegisterEncodeCompleteCallback() are actually called multiple
  // times with valid pointers, this may be a bug. To workaround this problem,
  // a mutex is used so that it is guaranteed that the previous callback is not
  // executed after RegisterEncodeCompleteCallback().
  impl_->RegisterEncodeCompleteCallback(callback);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoEncoder::Release() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  DVLOG(3) << __func__;
  if (!impl_)
    return WEBRTC_VIDEO_CODEC_OK;

  if (!frame_size_change_supported_ || !impl_initialized_ || has_error_) {
    DVLOG(3) << __func__ << " ReleaseImpl";
    ReleaseImpl();
  } else {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    int32_t suspend_result;
    base::WaitableEvent suspend_waiter(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    PostCrossThreadTask(
        *gpu_task_runner_.get(), FROM_HERE,
        CrossThreadBindOnce(&RTCVideoEncoder::Impl::Suspend, weak_impl_,
                            SignaledValue(&suspend_waiter, &suspend_result)));
    suspend_waiter.Wait();
    if (suspend_result != WEBRTC_VIDEO_CODEC_UNINITIALIZED) {
      ReleaseImpl();
    }
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

void RTCVideoEncoder::ReleaseImpl() {
  if (!impl_) {
    return;
  }

  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::WaitableEvent release_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  PostCrossThreadTask(
      *gpu_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          [](std::unique_ptr<Impl> impl, base::WaitableEvent* waiter) {
            impl.reset();
            waiter->Signal();
          },
          std::move(impl_), CrossThreadUnretained(&release_waiter)));

  release_waiter.Wait();

  // The object pointed by |weak_impl_| has been invalidated in Impl destructor.
  // Calling reset() is optional, but it's good to invalidate the value of
  // |weak_impl_| too
  weak_impl_.reset();
  impl_initialized_ = false;
}

void RTCVideoEncoder::SetRates(
    const webrtc::VideoEncoder::RateControlParameters& parameters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  TRACE_EVENT1("webrtc", "SetRates", "parameters",
               parameters.bitrate.ToString());
  DVLOG(3) << __func__ << " new_bit_rate=" << parameters.bitrate.ToString()
           << ", frame_rate=" << parameters.framerate_fps;
  if (!impl_) {
    DVLOG(3) << "Encoder is not initialized";
    return;
  }

  if (has_error_)
    return;

  PostCrossThreadTask(
      *gpu_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          &RTCVideoEncoder::Impl::RequestEncodingParametersChange, weak_impl_,
          parameters));
  return;
}

webrtc::VideoEncoder::EncoderInfo RTCVideoEncoder::GetEncoderInfo() const {
  base::AutoLock auto_lock(lock_);
  return encoder_info_;
}

void RTCVideoEncoder::UpdateEncoderInfo(
    media::VideoEncoderInfo media_enc_info,
    std::vector<webrtc::VideoFrameBuffer::Type> preferred_pixel_formats) {
  // See b/261437029#comment7 why this needs to be done in |gpu_task_runner_|.
  DCHECK(gpu_task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock auto_lock(lock_);

  frame_size_change_supported_ =
      base::FeatureList::IsEnabled(features::kKeepEncoderInstanceOnRelease) &&
      media_enc_info.supports_frame_size_change;
  encoder_info_.implementation_name = media_enc_info.implementation_name;
  encoder_info_.supports_native_handle = media_enc_info.supports_native_handle;
  encoder_info_.has_trusted_rate_controller =
      media_enc_info.has_trusted_rate_controller;
  encoder_info_.is_hardware_accelerated =
      media_enc_info.is_hardware_accelerated;
  // Simulcast is supported via VP9 SVC
  encoder_info_.supports_simulcast =
      media_enc_info.supports_simulcast ||
      (media::IsVp9kSVCHWEncodingEnabled() &&
       base::FeatureList::IsEnabled(
           features::kRtcVideoEncoderConvertSimulcastToSvc));
  encoder_info_.is_qp_trusted = media_enc_info.reports_average_qp;
  encoder_info_.requested_resolution_alignment =
      media_enc_info.requested_resolution_alignment;
  encoder_info_.apply_alignment_to_all_simulcast_layers =
      media_enc_info.apply_alignment_to_all_simulcast_layers;
  static_assert(
      webrtc::kMaxSpatialLayers >= media::VideoEncoderInfo::kMaxSpatialLayers,
      "webrtc::kMaxSpatiallayers is less than "
      "media::VideoEncoderInfo::kMaxSpatialLayers");
  for (size_t i = 0; i < std::size(media_enc_info.fps_allocation); ++i) {
    if (media_enc_info.fps_allocation[i].empty())
      continue;
    encoder_info_.fps_allocation[i] =
        absl::InlinedVector<uint8_t, webrtc::kMaxTemporalStreams>(
            media_enc_info.fps_allocation[i].begin(),
            media_enc_info.fps_allocation[i].end());
  }
  for (const auto& limit : media_enc_info.resolution_bitrate_limits) {
    encoder_info_.resolution_bitrate_limits.emplace_back(
        limit.frame_size.GetArea(), limit.min_start_bitrate_bps,
        limit.min_bitrate_bps, limit.max_bitrate_bps);
  }
  encoder_info_.preferred_pixel_formats.assign(preferred_pixel_formats.begin(),
                                               preferred_pixel_formats.end());
}

void RTCVideoEncoder::SetError(uint32_t impl_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(webrtc_sequence_checker_);
  //  RTCVideoEncoder should reject to set error if the impl_id is not equal to
  //  current impl_id_, which means it's requested by a released impl_.
  if (impl_id == impl_id_) {
    has_error_ = true;
    impl_initialized_ = false;
  }

  if (error_callback_for_testing_)
    std::move(error_callback_for_testing_).Run();
}

#if BUILDFLAG(RTC_USE_H265)
void RTCVideoEncoder::SetH265ParameterSetsTrackerForTesting(
    std::unique_ptr<H265ParameterSetsTracker> tracker) {
  if (!impl_) {
    DVLOG(1) << "Encoder is not initialized";
    return;
  }
  impl_->SetH265ParameterSetsTrackerForTesting(std::move(tracker));
}
#endif

}  // namespace blink
