// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_encoder.h"

#include <algorithm>
#include <string>

#include "base/containers/contains.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/clamped_math.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/async_destroy_video_encoder.h"
#include "media/base/limits.h"
#include "media/base/mime_util.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_encoder.h"
#include "media/base/video_util.h"
#include "media/media_buildflags.h"
#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/h264_level_limits.h"
#include "media/video/offloading_video_encoder.h"
#include "media/video/video_encode_accelerator_adapter.h"
#include "media/video/video_encoder_fallback.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_avc_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk_metadata.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_hevc_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_svc_output_metadata.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_color_space_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_encode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_encode_options_for_av_1.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_encode_options_for_avc.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_encode_options_for_vp_9.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_support.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_pixel_format.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/webcodecs/allow_shared_buffer_source_util.h"
#include "third_party/blink/renderer/modules/webcodecs/background_readback.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_state_helper.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/gpu_factories_retriever.h"
#include "third_party/blink/renderer/modules/webcodecs/video_color_space.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_video_frame_pool.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

#if BUILDFLAG(ENABLE_LIBAOM)
#include "media/video/av1_video_encoder.h"
#endif

#if BUILDFLAG(ENABLE_OPENH264)
#include "media/video/openh264_video_encoder.h"
#endif

#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/video/vpx_video_encoder.h"
#endif

namespace WTF {

template <>
struct CrossThreadCopier<media::EncoderStatus>
    : public CrossThreadCopierPassThrough<media::EncoderStatus> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

using EncoderType = media::VideoEncodeAccelerator::Config::EncoderType;

namespace {

constexpr const char kCategory[] = "media";

int ComputeMaxActiveEncodes(
    absl::optional<int> frame_delay = absl::nullopt,
    absl::optional<int> input_capacity = absl::nullopt) {
  constexpr int kDefaultEncoderFrameDelay = 0;

  // The maximum number of input frames above the encoder frame delay that we
  // want to be able to enqueue in |media_encoder_|.
  constexpr int kDefaultEncoderExtraInputCapacity = 5;

  const int preferred_capacity =
      frame_delay.value_or(kDefaultEncoderFrameDelay) +
      kDefaultEncoderExtraInputCapacity;
  return input_capacity.has_value()
             ? std::min(preferred_capacity, input_capacity.value())
             : preferred_capacity;
}

media::VideoEncodeAccelerator::SupportedRateControlMode BitrateToSupportedMode(
    const media::Bitrate& bitrate) {
  switch (bitrate.mode()) {
    case media::Bitrate::Mode::kConstant:
      return media::VideoEncodeAccelerator::kConstantMode;
    case media::Bitrate::Mode::kVariable:
      return media::VideoEncodeAccelerator::kVariableMode
#if BUILDFLAG(IS_ANDROID)
             // On Android we allow CBR-only encoders to be used for VBR because
             // most devices don't properly advertise support for VBR encoding.
             // In most cases they will initialize successfully when configured
             // for VBR.
             | media::VideoEncodeAccelerator::kConstantMode
#endif  // BUILDFLAG(IS_ANDROID)
          ;

    case media::Bitrate::Mode::kExternal:
      return media::VideoEncodeAccelerator::kExternalMode;
  }
}

bool IsAcceleratedConfigurationSupported(
    media::VideoCodecProfile profile,
    const media::VideoEncoder::Options& options,
    media::GpuVideoAcceleratorFactories* gpu_factories,
    EncoderType required_encoder_type) {
  if (!gpu_factories || !gpu_factories->IsGpuVideoEncodeAcceleratorEnabled()) {
    return false;
  }

  auto supported_profiles =
      gpu_factories->GetVideoEncodeAcceleratorSupportedProfiles().value_or(
          media::VideoEncodeAccelerator::SupportedProfiles());

  bool found_supported_profile = false;
  for (auto& supported_profile : supported_profiles) {
    if (supported_profile.profile != profile) {
      continue;
    }

    if (supported_profile.is_software_codec) {
      if (required_encoder_type == EncoderType::kHardware) {
        continue;
      }
    } else if (required_encoder_type == EncoderType::kSoftware) {
      continue;
    }

    if (supported_profile.min_resolution.width() > options.frame_size.width() ||
        supported_profile.min_resolution.height() >
            options.frame_size.height()) {
      continue;
    }

    if (supported_profile.max_resolution.width() < options.frame_size.width() ||
        supported_profile.max_resolution.height() <
            options.frame_size.height()) {
      continue;
    }

    double max_supported_framerate =
        static_cast<double>(supported_profile.max_framerate_numerator) /
        supported_profile.max_framerate_denominator;
    if (options.framerate.has_value() &&
        options.framerate.value() > max_supported_framerate) {
      continue;
    }

    if (options.scalability_mode.has_value() &&
        !base::Contains(supported_profile.scalability_modes,
                        options.scalability_mode.value())) {
      continue;
    }

    if (options.bitrate.has_value()) {
      auto mode = BitrateToSupportedMode(options.bitrate.value());
      if (!(mode & supported_profile.rate_control_modes)) {
        continue;
      }
    }

    found_supported_profile = true;
    break;
  }
  return found_supported_profile;
}

VideoEncoderTraits::ParsedConfig* ParseConfigStatic(
    const VideoEncoderConfig* config,
    ExceptionState& exception_state) {
  auto* result = MakeGarbageCollected<VideoEncoderTraits::ParsedConfig>();

  if (config->codec().LengthWithStrippedWhiteSpace() == 0) {
    exception_state.ThrowTypeError("Invalid codec; codec is required.");
    return nullptr;
  }

  if (config->height() == 0 || config->width() == 0) {
    exception_state.ThrowTypeError(
        "Invalid size; height and width must be greater than zero.");
    return nullptr;
  }
  result->options.frame_size.SetSize(config->width(), config->height());

  if (config->alpha() == "keep") {
    result->not_supported_error_message =
        "Alpha encoding is not currently supported.";
    return result;
  }

  result->options.latency_mode =
      (config->latencyMode() == "quality")
          ? media::VideoEncoder::LatencyMode::Quality
          : media::VideoEncoder::LatencyMode::Realtime;

  if (config->hasBitrateMode() && config->bitrateMode() == "quantizer") {
    result->options.bitrate = media::Bitrate::ExternalRateControl();
  } else if (config->hasBitrate()) {
    uint32_t bps = base::saturated_cast<uint32_t>(config->bitrate());
    if (bps == 0) {
      result->not_supported_error_message =
          String::Format("Unsupported bitrate: %u", bps);
      return result;
    }
    if (config->hasBitrateMode() && config->bitrateMode() == "constant") {
      result->options.bitrate = media::Bitrate::ConstantBitrate(bps);
    } else {
      // VBR in media:Bitrate supports both target and peak bitrate.
      // Currently webcodecs doesn't expose peak bitrate
      // (assuming unconstrained VBR), here we just set peak as 10 times
      // target as a good enough way of expressing unconstrained VBR.
      result->options.bitrate = media::Bitrate::VariableBitrate(
          bps, base::ClampMul(bps, 10u).RawValue());
    }
  }

  if (config->hasDisplayWidth() && config->hasDisplayHeight()) {
    if (config->displayHeight() == 0 || config->displayWidth() == 0) {
      exception_state.ThrowTypeError(
          "Invalid display size; height and width must be greater than zero.");
      return nullptr;
    }
    result->display_size.emplace(config->displayWidth(),
                                 config->displayHeight());
  } else if (config->hasDisplayWidth() || config->hasDisplayHeight()) {
    exception_state.ThrowTypeError(
        "Invalid display size; both height and width must be set together.");
    return nullptr;
  }

  if (config->hasFramerate()) {
    constexpr double kMinFramerate = .0001;
    constexpr double kMaxFramerate = 1'000'000'000;
    if (std::isnan(config->framerate()) ||
        config->framerate() < kMinFramerate ||
        config->framerate() > kMaxFramerate) {
      result->not_supported_error_message = String::Format(
          "Unsupported framerate; expected range from %f to %f, received %f.",
          kMinFramerate, kMaxFramerate, config->framerate());
      return result;
    }
    result->options.framerate = config->framerate();
  } else {
    result->options.framerate =
        media::VideoEncodeAccelerator::kDefaultFramerate;
  }

  // https://w3c.github.io/webrtc-svc/
  if (config->hasScalabilityMode()) {
    if (config->scalabilityMode() == "L1T1") {
      result->options.scalability_mode = media::SVCScalabilityMode::kL1T1;
    } else if (config->scalabilityMode() == "L1T2") {
      result->options.scalability_mode = media::SVCScalabilityMode::kL1T2;
    } else if (config->scalabilityMode() == "L1T3") {
      result->options.scalability_mode = media::SVCScalabilityMode::kL1T3;
    } else {
      result->not_supported_error_message =
          String::Format("Unsupported scalabilityMode: %s",
                         config->scalabilityMode().Utf8().c_str());
      return result;
    }
  }

  // The IDL defines a default value of "no-preference".
  DCHECK(config->hasHardwareAcceleration());

  result->hw_pref = StringToHardwarePreference(
      IDLEnumAsString(config->hardwareAcceleration()));

  bool is_codec_ambiguous = true;
  result->codec = media::VideoCodec::kUnknown;
  result->profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;
  result->level = 0;
  result->codec_string = config->codec();

  // Some codec strings provide color space info, but for WebCodecs this is
  // ignored. Instead, the VideoFrames given to encode() are the source of truth
  // for input color space. Note also that the output color space is up to the
  // underlying codec impl. See https://github.com/w3c/webcodecs/issues/345.
  media::VideoColorSpace codec_string_color_space;

  bool parse_succeeded = media::ParseVideoCodecString(
      "", config->codec().Utf8(), &is_codec_ambiguous, &result->codec,
      &result->profile, &result->level, &codec_string_color_space);

  if (!parse_succeeded || is_codec_ambiguous) {
    result->codec = media::VideoCodec::kUnknown;
    return result;
  }

  // We are done with the parsing.
  if (!config->hasAvc() && !config->hasHevc())
    return result;

  switch (result->codec) {
    case media::VideoCodec::kH264: {
      std::string avc_format = IDLEnumAsString(config->avc()->format()).Utf8();
      if (avc_format == "avc") {
        result->options.avc.produce_annexb = false;
      } else if (avc_format == "annexb") {
        result->options.avc.produce_annexb = true;
      } else {
        NOTREACHED();
      }
      break;
    }
    case media::VideoCodec::kHEVC: {
      std::string hevc_format =
          IDLEnumAsString(config->hevc()->format()).Utf8();
      if (hevc_format == "hevc") {
        result->options.hevc.produce_annexb = false;
      } else if (hevc_format == "annexb") {
        result->options.hevc.produce_annexb = true;
      } else {
        NOTREACHED();
      }
      break;
    }
    default:
      break;
  }

  return result;
}

bool VerifyCodecSupportStatic(VideoEncoderTraits::ParsedConfig* config,
                              String* js_error_message) {
  if (config->not_supported_error_message) {
    *js_error_message = *config->not_supported_error_message;
    return false;
  }

  const auto& frame_size = config->options.frame_size;
  if (frame_size.height() > media::limits::kMaxDimension) {
    *js_error_message = String::Format(
        "Invalid height; expected range from %d to %d, received %d.", 1,
        media::limits::kMaxDimension, frame_size.height());
    return false;
  }
  if (frame_size.width() > media::limits::kMaxDimension) {
    *js_error_message = String::Format(
        "Invalid width; expected range from %d to %d, received %d.", 1,
        media::limits::kMaxDimension, frame_size.width());
    return false;
  }
  if (frame_size.Area64() > media::limits::kMaxCanvas) {
    *js_error_message = String::Format(
        "Invalid resolution; expected range from %d to %d, "
        "received %" PRIu64
        " (%d * "
        "%d).",
        1, media::limits::kMaxCanvas, frame_size.Area64(), frame_size.width(),
        frame_size.height());
    return false;
  }

  switch (config->codec) {
    case media::VideoCodec::kAV1:
    case media::VideoCodec::kVP8:
    case media::VideoCodec::kVP9:
      break;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case media::VideoCodec::kHEVC:
      if (config->profile != media::VideoCodecProfile::HEVCPROFILE_MAIN) {
        *js_error_message = "Unsupported hevc profile.";
        return false;
      }
      break;
#endif

    case media::VideoCodec::kH264: {
      if (config->options.frame_size.width() % 2 != 0 ||
          config->options.frame_size.height() % 2 != 0) {
        *js_error_message = "H264 only supports even sized frames.";
        return false;
      }

      // Note: This calculation is incorrect for interlaced or MBAFF encoding;
      // but we don't support those and likely never will.
      gfx::Size coded_size(
          base::bits::AlignUp(config->options.frame_size.width(), 16),
          base::bits::AlignUp(config->options.frame_size.height(), 16));
      uint64_t coded_area = coded_size.Area64();
      uint64_t max_coded_area =
          media::H264LevelToMaxFS(config->level) * 16ull * 16ull;
      if (coded_area > max_coded_area) {
        *js_error_message = String::Format(
            "The provided resolution (%s) has a coded area "
            "(%d*%d=%" PRIu64 ") which exceeds the maximum coded area (%" PRIu64
            ") supported by the AVC level (%1.1f) indicated "
            "by the codec string (0x%02X). You must either "
            "specify a lower resolution or higher AVC level.",
            config->options.frame_size.ToString().c_str(), coded_size.width(),
            coded_size.height(), coded_area, max_coded_area,
            config->level / 10.0f, config->level);
        return false;
      }
      break;
    }

    default:
      *js_error_message = "Unsupported codec type.";
      return false;
  }

  return true;
}

VideoEncoderConfig* CopyConfig(
    const VideoEncoderConfig& config,
    const VideoEncoderTraits::ParsedConfig& parsed_config) {
  auto* result = VideoEncoderConfig::Create();
  result->setCodec(config.codec());
  result->setWidth(config.width());
  result->setHeight(config.height());

  if (config.hasDisplayWidth())
    result->setDisplayWidth(config.displayWidth());

  if (config.hasDisplayHeight())
    result->setDisplayHeight(config.displayHeight());

  if (config.hasFramerate())
    result->setFramerate(config.framerate());

  if (config.hasBitrate())
    result->setBitrate(config.bitrate());

  if (config.hasScalabilityMode())
    result->setScalabilityMode(config.scalabilityMode());

  if (config.hasHardwareAcceleration())
    result->setHardwareAcceleration(config.hardwareAcceleration());

  if (config.hasAlpha())
    result->setAlpha(config.alpha());

  if (config.hasBitrateMode())
    result->setBitrateMode(config.bitrateMode());

  if (config.hasLatencyMode())
    result->setLatencyMode(config.latencyMode());

  if (config.hasAvc() && config.avc()->hasFormat()) {
    auto* avc = AvcEncoderConfig::Create();
    avc->setFormat(config.avc()->format());
    result->setAvc(avc);
  }

  if (config.hasHevc() && config.hevc()->hasFormat()) {
    auto* hevc = HevcEncoderConfig::Create();
    hevc->setFormat(config.hevc()->format());
    result->setHevc(hevc);
  }

  return result;
}

bool CanUseGpuMemoryBufferReadback(media::VideoPixelFormat format,
                                   bool force_opaque) {
  // GMB readback only works with NV12, so only opaque buffers can be used.
  return (format == media::PIXEL_FORMAT_XBGR ||
          format == media::PIXEL_FORMAT_XRGB ||
          (force_opaque && (format == media::PIXEL_FORMAT_ABGR ||
                            format == media::PIXEL_FORMAT_ARGB))) &&
         WebGraphicsContext3DVideoFramePool::
             IsGpuMemoryBufferReadbackFromTextureEnabled();
}

bool MayHaveOSSoftwareEncoder(media::VideoCodecProfile profile) {
  // Allow OS software encoding when we don't have an equivalent
  // software encoder.
  //
  // Note: Since we don't enumerate OS software encoders this may still fail and
  // trigger fallback to our bundled software encoder (if any).
  //
  // Note 2: It's not ideal to have this logic live here, but otherwise we need
  // to always wait for GpuFactories enumeration.
  //
  // TODO(crbug.com/1383643): Add IS_WIN here once we can force
  // selection of a software encoder there.
#if (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)) && !BUILDFLAG(ENABLE_OPENH264)
  return media::VideoCodecProfileToVideoCodec(profile) ==
         media::VideoCodec::kH264;
#else
  return false;
#endif  // (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)) &&
        // !BUILDFLAG(ENABLE_OPENH264)
}

EncoderType GetRequiredEncoderType(media::VideoCodecProfile profile,
                                   HardwarePreference hw_pref) {
  if (hw_pref != HardwarePreference::kPreferHardware &&
      MayHaveOSSoftwareEncoder(profile)) {
    return hw_pref == HardwarePreference::kPreferSoftware
               ? EncoderType::kSoftware
               : EncoderType::kNoPreference;
  }
  return EncoderType::kHardware;
}

}  // namespace

// static
const char* VideoEncoderTraits::GetName() {
  return "VideoEncoder";
}

// static
VideoEncoder* VideoEncoder::Create(ScriptState* script_state,
                                   const VideoEncoderInit* init,
                                   ExceptionState& exception_state) {
  auto* result =
      MakeGarbageCollected<VideoEncoder>(script_state, init, exception_state);
  return exception_state.HadException() ? nullptr : result;
}

VideoEncoder::VideoEncoder(ScriptState* script_state,
                           const VideoEncoderInit* init,
                           ExceptionState& exception_state)
    : Base(script_state, init, exception_state),
      max_active_encodes_(ComputeMaxActiveEncodes()) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);
}

VideoEncoder::~VideoEncoder() = default;

VideoEncoder::ParsedConfig* VideoEncoder::ParseConfig(
    const VideoEncoderConfig* config,
    ExceptionState& exception_state) {
  return ParseConfigStatic(config, exception_state);
}

bool VideoEncoder::VerifyCodecSupport(ParsedConfig* config,
                                      String* js_error_message) {
  return VerifyCodecSupportStatic(config, js_error_message);
}

std::unique_ptr<media::VideoEncoder>
VideoEncoder::CreateAcceleratedVideoEncoder(
    media::VideoCodecProfile profile,
    const media::VideoEncoder::Options& options,
    media::GpuVideoAcceleratorFactories* gpu_factories,
    HardwarePreference hw_pref) {
  auto required_encoder_type = GetRequiredEncoderType(profile, hw_pref);
  if (!IsAcceleratedConfigurationSupported(profile, options, gpu_factories,
                                           required_encoder_type)) {
    return nullptr;
  }

  return std::make_unique<
      media::AsyncDestroyVideoEncoder<media::VideoEncodeAcceleratorAdapter>>(
      std::make_unique<media::VideoEncodeAcceleratorAdapter>(
          gpu_factories, logger_->log()->Clone(), callback_runner_,
          required_encoder_type));
}

std::unique_ptr<media::VideoEncoder> CreateAv1VideoEncoder() {
#if BUILDFLAG(ENABLE_LIBAOM)
  return std::make_unique<media::Av1VideoEncoder>();
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_LIBAOM)
}

std::unique_ptr<media::VideoEncoder> CreateVpxVideoEncoder() {
#if BUILDFLAG(ENABLE_LIBVPX)
  return std::make_unique<media::VpxVideoEncoder>();
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_LIBVPX)
}

std::unique_ptr<media::VideoEncoder> CreateOpenH264VideoEncoder() {
#if BUILDFLAG(ENABLE_OPENH264)
  return std::make_unique<media::OpenH264VideoEncoder>();
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_OPENH264)
}

// This method is static and takes |self| in order to make it possible to use it
// with a weak |this|. It's needed in to avoid a persistent reference cycle.
std::unique_ptr<media::VideoEncoder> VideoEncoder::CreateSoftwareVideoEncoder(
    VideoEncoder* self,
    bool fallback,
    media::VideoCodec codec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
  if (!self)
    return nullptr;
  std::unique_ptr<media::VideoEncoder> result;
  switch (codec) {
    case media::VideoCodec::kAV1:
      result = CreateAv1VideoEncoder();
      break;
    case media::VideoCodec::kVP8:
    case media::VideoCodec::kVP9:
      result = CreateVpxVideoEncoder();
      break;
    case media::VideoCodec::kH264:
      result = CreateOpenH264VideoEncoder();
      break;
    default:
      break;
  }
  if (!result)
    return nullptr;
  if (fallback) {
    CHECK(self->encoder_metrics_provider_);
    self->encoder_metrics_provider_->Initialize(
        self->active_config_->profile, self->active_config_->options.frame_size,
        /*is_hardware_encoder=*/false,
        self->active_config_->options.scalability_mode.value_or(
            media::SVCScalabilityMode::kL1T1));
  }
  return std::make_unique<media::OffloadingVideoEncoder>(std::move(result));
}

std::unique_ptr<media::VideoEncoder> VideoEncoder::CreateMediaVideoEncoder(
    const ParsedConfig& config,
    media::GpuVideoAcceleratorFactories* gpu_factories,
    bool& is_platform_encoder) {
  is_platform_encoder = true;
  if (config.hw_pref == HardwarePreference::kPreferHardware ||
      config.hw_pref == HardwarePreference::kNoPreference ||
      MayHaveOSSoftwareEncoder(config.profile)) {
    auto result = CreateAcceleratedVideoEncoder(config.profile, config.options,
                                                gpu_factories, config.hw_pref);
    if (config.hw_pref == HardwarePreference::kPreferHardware) {
      return result;
    } else if (result) {
      // 'no-preference' or 'prefer-software' and we have OS software encoders.
      return std::make_unique<media::VideoEncoderFallback>(
          std::move(result), ConvertToBaseOnceCallback(CrossThreadBindOnce(
                                 &VideoEncoder::CreateSoftwareVideoEncoder,
                                 MakeUnwrappingCrossThreadWeakHandle(this),
                                 /*fallback=*/true, config.codec)));
    }
  }

  is_platform_encoder = false;
  return CreateSoftwareVideoEncoder(this, /*fallback=*/false, config.codec);
}

void VideoEncoder::ContinueConfigureWithGpuFactories(
    Request* request,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  DCHECK(active_config_);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool is_platform_encoder = false;
  media_encoder_ = CreateMediaVideoEncoder(*active_config_, gpu_factories,
                                           is_platform_encoder);
  if (!media_encoder_) {
    ReportError("Encoder creation error.",
                media::EncoderStatus(
                    media::EncoderStatus::Codes::kEncoderInitializationError,
                    "Unable to create encoder (most likely unsupported "
                    "codec/acceleration requirement combination)"));
    request->EndTracing();
    return;
  }

  auto info_cb = ConvertToBaseRepeatingCallback(
      CrossThreadBindRepeating(&VideoEncoder::OnMediaEncoderInfoChanged,
                               MakeUnwrappingCrossThreadWeakHandle(this)));

  auto output_cb = ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
      &VideoEncoder::CallOutputCallback,
      MakeUnwrappingCrossThreadWeakHandle(this),
      // We can't use |active_config_| from |this| because it can change by
      // the time the callback is executed.
      MakeUnwrappingCrossThreadHandle(active_config_.Get()), reset_count_));

  auto done_callback = [](VideoEncoder* self, Request* req,
                          media::VideoCodec codec,
                          media::EncoderStatus status) {
    if (!self || self->reset_count_ != req->reset_count) {
      req->EndTracing(/*aborted=*/true);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    DCHECK(self->active_config_);

    if (!status.is_ok()) {
      std::string error_message;
      switch (status.code()) {
        case media::EncoderStatus::Codes::kEncoderUnsupportedProfile:
          error_message = "Unsupported codec profile.";
          break;
        case media::EncoderStatus::Codes::kEncoderUnsupportedConfig:
          error_message = "Unsupported configuration parameters.";
          break;
        default:
          error_message = "Encoder initialization error.";
          break;
      }

      self->ReportError(error_message.c_str(), std::move(status));
    } else {
      UMA_HISTOGRAM_ENUMERATION("Blink.WebCodecs.VideoEncoder.Codec", codec,
                                media::VideoCodec::kMaxValue);
    }
    req->EndTracing();

    self->blocking_request_in_progress_ = nullptr;
    self->ProcessRequests();
  };
  if (!encoder_metrics_provider_) {
    encoder_metrics_provider_ = CreateVideoEncoderMetricsProvider();
  }
  encoder_metrics_provider_->Initialize(
      active_config_->profile, active_config_->options.frame_size,
      is_platform_encoder,
      active_config_->options.scalability_mode.value_or(
          media::SVCScalabilityMode::kL1T1));
  media_encoder_->Initialize(
      active_config_->profile, active_config_->options, std::move(info_cb),
      std::move(output_cb),
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          done_callback, MakeUnwrappingCrossThreadWeakHandle(this),
          MakeUnwrappingCrossThreadHandle(request), active_config_->codec)));
}

std::unique_ptr<media::VideoEncoderMetricsProvider>
VideoEncoder::CreateVideoEncoderMetricsProvider() const {
  mojo::PendingRemote<media::mojom::VideoEncoderMetricsProvider>
      video_encoder_metrics_provider;
  LocalDOMWindow* window = DomWindow();
  LocalFrame* local_frame = window ? window->GetFrame() : nullptr;
  // There is no DOM frame if WebCodecs runs in a service worker.
  if (local_frame) {
    local_frame->GetBrowserInterfaceBroker().GetInterface(
        video_encoder_metrics_provider.InitWithNewPipeAndPassReceiver());
  } else {
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        video_encoder_metrics_provider.InitWithNewPipeAndPassReceiver());
  }
  return base::MakeRefCounted<media::MojoVideoEncoderMetricsProviderFactory>(
             media::mojom::VideoEncoderUseCase::kWebCodecs,
             std::move(video_encoder_metrics_provider))
      ->CreateVideoEncoderMetricsProvider();
}

bool VideoEncoder::CanReconfigure(ParsedConfig& original_config,
                                  ParsedConfig& new_config) {
  // Reconfigure is intended for things that don't require changing underlying
  // codec implementation and can be changed on the fly.
  return original_config.codec == new_config.codec &&
         original_config.profile == new_config.profile &&
         original_config.level == new_config.level &&
         original_config.hw_pref == new_config.hw_pref;
}

const AtomicString& VideoEncoder::InterfaceName() const {
  return event_target_names::kVideoEncoder;
}

bool VideoEncoder::HasPendingActivity() const {
  return (active_encodes_ > 0) || Base::HasPendingActivity();
}

void VideoEncoder::Trace(Visitor* visitor) const {
  visitor->Trace(background_readback_);
  Base::Trace(visitor);
}

void VideoEncoder::ReportError(const char* error_message,
                               const media::EncoderStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!status.is_ok());
  // ReportError() can be called before |encoder_metrics_provider_| is created
  // in media::VideoEncoder::Initialize() (e.g. there is no available
  // media::VideoEncoder). Since the case is about webrtc::VideoEncoder failure,
  // we don't record it.
  if (encoder_metrics_provider_) {
    encoder_metrics_provider_->SetError(status);
  }
  HandleError(logger_->MakeOperationError(error_message, status));
}

bool VideoEncoder::ReadyToProcessNextRequest() {
  if (active_encodes_ >= max_active_encodes_)
    return false;

  return Base::ReadyToProcessNextRequest();
}

bool VideoEncoder::StartReadback(scoped_refptr<media::VideoFrame> frame,
                                 ReadbackDoneCallback result_cb) {
  // TODO(crbug.com/1195433): Once support for alpha channel encoding is
  // implemented, |force_opaque| must be set based on the
  // VideoEncoderConfig.
  const bool can_use_gmb =
      !disable_accelerated_frame_pool_ &&
      CanUseGpuMemoryBufferReadback(frame->format(), /*force_opaque=*/true);
  if (can_use_gmb && !accelerated_frame_pool_) {
    if (auto wrapper = SharedGpuContext::ContextProviderWrapper()) {
      accelerated_frame_pool_ =
          std::make_unique<WebGraphicsContext3DVideoFramePool>(wrapper);
    }
  }

  auto [pool_result_cb, background_result_cb] =
      base::SplitOnceCallback(std::move(result_cb));
  if (can_use_gmb && accelerated_frame_pool_) {
    auto origin = frame->metadata().texture_origin_is_top_left
                      ? kTopLeft_GrSurfaceOrigin
                      : kBottomLeft_GrSurfaceOrigin;

    // CopyRGBATextureToVideoFrame() operates on mailboxes and
    // not frames, so we must manually copy over properties relevant to
    // the encoder. We amend result callback to do exactly that.
    auto metadata_fix_lambda = [](scoped_refptr<media::VideoFrame> txt_frame,
                                  scoped_refptr<media::VideoFrame> result_frame)
        -> scoped_refptr<media::VideoFrame> {
      if (!result_frame)
        return result_frame;
      result_frame->set_timestamp(txt_frame->timestamp());
      result_frame->metadata().MergeMetadataFrom(txt_frame->metadata());
      result_frame->metadata().ClearTextureFrameMedatada();
      return result_frame;
    };

    auto callback_chain = ConvertToBaseOnceCallback(
                              CrossThreadBindOnce(metadata_fix_lambda, frame))
                              .Then(std::move(pool_result_cb));

    // TODO(crbug.com/1224279): This assumes that all frames are 8-bit sRGB.
    // Expose the color space and pixel format that is backing
    // `image->GetMailboxHolder()`, or, alternatively, expose an accelerated
    // SkImage.
    auto format = (frame->format() == media::PIXEL_FORMAT_XBGR ||
                   frame->format() == media::PIXEL_FORMAT_ABGR)
                      ? viz::SinglePlaneFormat::kRGBA_8888
                      : viz::SinglePlaneFormat::kBGRA_8888;

#if BUILDFLAG(IS_APPLE)
    // The Apple hardware encoder properly sets output color spaces, so we can
    // round trip through the encoder and decoder w/o downgrading to BT.601.
    constexpr auto kDstColorSpace = gfx::ColorSpace::CreateREC709();
#else
    // When doing RGBA to YUVA conversion using `accelerated_frame_pool_`, use
    // sRGB primaries and the 601 YUV matrix. Note that this is subtly
    // different from the 601 gfx::ColorSpace because the 601 gfx::ColorSpace
    // has different (non-sRGB) primaries.
    //
    // This is necessary for our tests to pass since encoders will default to
    // BT.601 when the color space information isn't told to the encoder. When
    // coming back through the decoder it pulls out the embedded color space
    // information instead of what's provided in the config.
    //
    // https://crbug.com/1258245, https://crbug.com/1377842
    constexpr gfx::ColorSpace kDstColorSpace(
        gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::SRGB,
        gfx::ColorSpace::MatrixID::SMPTE170M,
        gfx::ColorSpace::RangeID::LIMITED);
#endif

    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("media", "CopyRGBATextureToVideoFrame",
                                      this, "timestamp", frame->timestamp());
    if (accelerated_frame_pool_->CopyRGBATextureToVideoFrame(
            format, frame->coded_size(), frame->ColorSpace(), origin,
            frame->mailbox_holder(0), kDstColorSpace,
            std::move(callback_chain))) {
      return true;
    }

    TRACE_EVENT_NESTABLE_ASYNC_END0("media", "CopyRGBATextureToVideoFrame",
                                    this);

    // Error occurred, fall through to normal readback path below.
    disable_accelerated_frame_pool_ = true;
    accelerated_frame_pool_.reset();
  }

  if (!background_readback_)
    background_readback_ = BackgroundReadback::From(*GetExecutionContext());

  if (background_readback_) {
    background_readback_->ReadbackTextureBackedFrameToMemoryFrame(
        std::move(frame), std::move(background_result_cb));
    return true;
  }

  // Oh well, none of our readback mechanisms were able to succeed.
  return false;
}

void VideoEncoder::ProcessEncode(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);
  DCHECK(media_encoder_);
  DCHECK_EQ(request->type, Request::Type::kEncode);
  DCHECK_GT(requested_encodes_, 0u);

  auto frame = request->input->frame();
  auto encode_options = CreateEncodeOptions(request);
  active_encodes_++;
  request->StartTracingVideoEncode(encode_options.key_frame,
                                   frame->timestamp());

  auto encode_done_callback = ConvertToBaseOnceCallback(CrossThreadBindOnce(
      &VideoEncoder::OnEncodeDone, MakeUnwrappingCrossThreadWeakHandle(this),
      MakeUnwrappingCrossThreadHandle(request)));

  if (frame->metadata().frame_duration) {
    frame_metadata_[frame->timestamp()] =
        FrameMetadata{*frame->metadata().frame_duration};
  }

  // Currently underlying encoders can't handle frame backed by textures,
  // so let's readback pixel data to CPU memory.
  // TODO(crbug.com/1229845): We shouldn't be reading back frames here.
  if (frame->HasTextures() && !frame->HasGpuMemoryBuffer()) {
    // Stall request processing while we wait for the copy to complete. It'd
    // be nice to not have to do this, but currently the request processing
    // loop must execute synchronously or flush() will miss frames.
    //
    // Note: Set this before calling StartReadback() since callbacks could
    // resolve synchronously.
    blocking_request_in_progress_ = request;

    auto readback_done_callback = WTF::BindOnce(
        &VideoEncoder::OnReadbackDone, WrapWeakPersistent(this),
        WrapPersistent(request), frame, std::move(encode_done_callback));

    if (StartReadback(std::move(frame), std::move(readback_done_callback))) {
      request->input->close();
    } else {
      blocking_request_in_progress_ = nullptr;
      callback_runner_->PostTask(
          FROM_HERE, ConvertToBaseOnceCallback(CrossThreadBindOnce(
                         &VideoEncoder::OnEncodeDone,
                         MakeUnwrappingCrossThreadWeakHandle(this),
                         MakeUnwrappingCrossThreadHandle(request),
                         media::EncoderStatus(
                             media::EncoderStatus::Codes::kEncoderFailedEncode,
                             "Can't readback frame textures."))));
    }
    return;
  }

  // Currently underlying encoders can't handle alpha channel, so let's
  // wrap a frame with an alpha channel into a frame without it.
  // For example such frames can come from 2D canvas context with alpha = true.
  if (frame->storage_type() == media::VideoFrame::STORAGE_OWNED_MEMORY &&
      frame->format() == media::PIXEL_FORMAT_I420A) {
    frame = media::WrapAsI420VideoFrame(std::move(frame));
  }

  --requested_encodes_;
  ScheduleDequeueEvent();
  media_encoder_->Encode(frame, encode_options,
                         std::move(encode_done_callback));

  // We passed a copy of frame() above, so this should be safe to close here.
  request->input->close();
}

media::VideoEncoder::EncodeOptions VideoEncoder::CreateEncodeOptions(
    Request* request) {
  media::VideoEncoder::EncodeOptions result;
  result.key_frame = request->encodeOpts->keyFrame();
  switch (active_config_->codec) {
    case media::VideoCodec::kAV1: {
      if (!active_config_->options.bitrate.has_value() ||
          active_config_->options.bitrate->mode() !=
              media::Bitrate::Mode::kExternal) {
        break;
      }
      if (!request->encodeOpts->hasAv1() ||
          !request->encodeOpts->av1()->hasQuantizer()) {
        break;
      }
      result.quantizer = request->encodeOpts->av1()->quantizer();
      break;
    }
    case media::VideoCodec::kVP9: {
      if (!active_config_->options.bitrate.has_value() ||
          active_config_->options.bitrate->mode() !=
              media::Bitrate::Mode::kExternal) {
        break;
      }
      if (!request->encodeOpts->hasVp9() ||
          !request->encodeOpts->vp9()->hasQuantizer()) {
        break;
      }
      result.quantizer = request->encodeOpts->vp9()->quantizer();
      break;
    }
    case media::VideoCodec::kH264:
      if (!active_config_->options.bitrate.has_value() ||
          active_config_->options.bitrate->mode() !=
              media::Bitrate::Mode::kExternal) {
        break;
      }
      if (!request->encodeOpts->hasAvc() ||
          !request->encodeOpts->avc()->hasQuantizer()) {
        break;
      }
      result.quantizer = request->encodeOpts->avc()->quantizer();
      break;
    case media::VideoCodec::kVP8:
    default:
      break;
  }
  return result;
}

void VideoEncoder::OnReadbackDone(
    Request* request,
    scoped_refptr<media::VideoFrame> txt_frame,
    media::VideoEncoder::EncoderStatusCB done_callback,
    scoped_refptr<media::VideoFrame> result_frame) {
  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "CopyRGBATextureToVideoFrame", this);
  if (reset_count_ != request->reset_count) {
    return;
  }

  if (!result_frame) {
    callback_runner_->PostTask(
        FROM_HERE, ConvertToBaseOnceCallback(CrossThreadBindOnce(
                       std::move(done_callback),
                       media::EncoderStatus(
                           media::EncoderStatus::Codes::kEncoderFailedEncode,
                           "Can't readback frame textures."))));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto encode_options = CreateEncodeOptions(request);
  --requested_encodes_;
  ScheduleDequeueEvent();
  blocking_request_in_progress_ = nullptr;
  media_encoder_->Encode(std::move(result_frame), encode_options,
                         std::move(done_callback));
  ProcessRequests();
}

void VideoEncoder::OnEncodeDone(Request* request, media::EncoderStatus status) {
  if (reset_count_ != request->reset_count) {
    request->EndTracing(/*aborted=*/true);
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  active_encodes_--;
  if (!status.is_ok()) {
    ReportError("Encoding error.", std::move(status));
  }
  request->EndTracing();
  ProcessRequests();
}

void VideoEncoder::ProcessConfigure(Request* request) {
  DCHECK_NE(state_.AsEnum(), V8CodecState::Enum::kClosed);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK(active_config_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  request->StartTracing();

  blocking_request_in_progress_ = request;

  String js_error_message;
  if (!VerifyCodecSupport(active_config_, &js_error_message)) {
    QueueHandleError(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError, js_error_message));
    request->EndTracing();
    return;
  }

  if (active_config_->hw_pref == HardwarePreference::kPreferSoftware &&
      !MayHaveOSSoftwareEncoder(active_config_->profile)) {
    ContinueConfigureWithGpuFactories(request, nullptr);
    return;
  }

  RetrieveGpuFactoriesWithKnownEncoderSupport(
      CrossThreadBindOnce(&VideoEncoder::ContinueConfigureWithGpuFactories,
                          MakeUnwrappingCrossThreadWeakHandle(this),
                          MakeUnwrappingCrossThreadHandle(request)));
}

void VideoEncoder::ProcessReconfigure(Request* request) {
  DCHECK_EQ(state_.AsEnum(), V8CodecState::Enum::kConfigured);
  DCHECK_EQ(request->type, Request::Type::kReconfigure);
  DCHECK(active_config_);
  DCHECK(media_encoder_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  request->StartTracing();

  auto reconf_done_callback = [](VideoEncoder* self, Request* req,
                                 media::EncoderStatus status) {
    if (!self || self->reset_count_ != req->reset_count) {
      req->EndTracing(/*aborted=*/true);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    DCHECK(self->active_config_);

    req->EndTracing();

    if (status.is_ok()) {
      self->blocking_request_in_progress_ = nullptr;
      self->ProcessRequests();
    } else {
      // Reconfiguration failed. Either encoder doesn't support changing options
      // or it didn't like this particular change. Let's try to configure it
      // from scratch.
      req->type = Request::Type::kConfigure;
      self->ProcessConfigure(req);
    }
  };

  auto flush_done_callback = [](VideoEncoder* self, Request* req,
                                decltype(reconf_done_callback) reconf_callback,
                                bool is_platform_encoder,
                                media::EncoderStatus status) {
    if (!self || self->reset_count_ != req->reset_count) {
      req->EndTracing(/*aborted=*/true);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (!status.is_ok()) {
      self->ReportError("Encoder initialization error.", std::move(status));
      self->blocking_request_in_progress_ = nullptr;
      req->EndTracing();
      return;
    }

    auto output_cb =
        ConvertToBaseRepeatingCallback(WTF::CrossThreadBindRepeating(
            &VideoEncoder::CallOutputCallback,
            MakeUnwrappingCrossThreadWeakHandle(self),
            // We can't use |active_config_| from |this| because it can change
            // by the time the callback is executed.
            MakeUnwrappingCrossThreadHandle(self->active_config_.Get()),
            self->reset_count_));

    if (!self->encoder_metrics_provider_) {
      self->encoder_metrics_provider_ =
          self->CreateVideoEncoderMetricsProvider();
    }
    self->encoder_metrics_provider_->Initialize(
        self->active_config_->profile, self->active_config_->options.frame_size,
        is_platform_encoder,
        self->active_config_->options.scalability_mode.value_or(
            media::SVCScalabilityMode::kL1T1));
    self->first_output_after_configure_ = true;
    self->media_encoder_->ChangeOptions(
        self->active_config_->options, std::move(output_cb),
        ConvertToBaseOnceCallback(CrossThreadBindOnce(
            reconf_callback, MakeUnwrappingCrossThreadWeakHandle(self),
            MakeUnwrappingCrossThreadHandle(req))));
  };

  blocking_request_in_progress_ = request;
  media_encoder_->Flush(WTF::BindOnce(
      flush_done_callback, MakeUnwrappingCrossThreadWeakHandle(this),
      MakeUnwrappingCrossThreadHandle(request), std::move(reconf_done_callback),
      is_platform_encoder_));
}

void VideoEncoder::OnMediaEncoderInfoChanged(
    const media::VideoEncoderInfo& encoder_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (encoder_info.is_hardware_accelerated)
    ApplyCodecPressure();
  else
    ReleaseCodecPressure();

  media::MediaLog* log = logger_->log();
  log->SetProperty<media::MediaLogProperty::kVideoEncoderName>(
      encoder_info.implementation_name);
  log->SetProperty<media::MediaLogProperty::kIsPlatformVideoEncoder>(
      encoder_info.is_hardware_accelerated);

  is_platform_encoder_ = encoder_info.is_hardware_accelerated;
  max_active_encodes_ = ComputeMaxActiveEncodes(encoder_info.frame_delay,
                                                encoder_info.input_capacity);
  // We may have increased our capacity for active encodes.
  ProcessRequests();
}

void VideoEncoder::CallOutputCallback(
    ParsedConfig* active_config,
    uint32_t reset_count,
    media::VideoEncoderOutput output,
    absl::optional<media::VideoEncoder::CodecDescription> codec_desc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(active_config);
  if (!script_state_->ContextIsValid() || !output_callback_ ||
      state_.AsEnum() != V8CodecState::Enum::kConfigured ||
      reset_count != reset_count_) {
    return;
  }

  MarkCodecActive();

  auto buffer =
      media::DecoderBuffer::FromArray(std::move(output.data), output.size);
  buffer->set_timestamp(output.timestamp);
  buffer->set_is_key_frame(output.key_frame);

  // Get duration from |frame_metadata_|.
  const auto it = frame_metadata_.find(output.timestamp);
  if (it != frame_metadata_.end()) {
    const auto duration = it->second.duration;
    if (!duration.is_zero() && duration != media::kNoTimestamp) {
      buffer->set_duration(duration);
    }

    // While encoding happens in presentation order, outputs may be out of order
    // for some codec configurations. The maximum number of reordered outputs is
    // 16, so we can clear everything before that.
    if (it - frame_metadata_.begin() > 16) {
      frame_metadata_.erase(frame_metadata_.begin(), it + 1);
    }
  }

  auto* chunk = MakeGarbageCollected<EncodedVideoChunk>(std::move(buffer));

  auto* metadata = EncodedVideoChunkMetadata::Create();
  if (active_config->options.scalability_mode.has_value()) {
    auto* svc_metadata = SvcOutputMetadata::Create();
    svc_metadata->setTemporalLayerId(output.temporal_id);
    metadata->setSvc(svc_metadata);
  }

  // TODO(https://crbug.com/1241448): All encoders should output color space.
  // For now, fallback to 601 since that is correct most often.
  gfx::ColorSpace output_color_space = output.color_space.IsValid()
                                           ? output.color_space
                                           : gfx::ColorSpace::CreateREC601();

  if (first_output_after_configure_ || codec_desc.has_value() ||
      output_color_space != last_output_color_space_) {
    first_output_after_configure_ = false;

    if (output_color_space != last_output_color_space_) {
// This should only fail when AndroidVideoEncodeAccelerator is used since it
// doesn't support color space changes. It's not worth plumbing a signal just
// for these DCHECKs, so disable them entirely.
#if !BUILDFLAG(IS_ANDROID)
      if (active_config->codec == media::VideoCodec::kH264) {
        DCHECK(active_config->options.avc.produce_annexb ||
               codec_desc.has_value());
      }
      DCHECK(output.key_frame) << "Encoders should generate a keyframe when "
                               << "changing color space";
#endif
      last_output_color_space_ = output_color_space;
    } else if (active_config->codec == media::VideoCodec::kH264) {
      DCHECK(active_config->options.avc.produce_annexb ||
             codec_desc.has_value());
    }

    auto encoded_size =
        output.encoded_size.value_or(active_config->options.frame_size);

    auto* decoder_config = VideoDecoderConfig::Create();
    decoder_config->setCodec(active_config->codec_string);
    decoder_config->setCodedHeight(encoded_size.height());
    decoder_config->setCodedWidth(encoded_size.width());

    if (active_config->display_size.has_value()) {
      decoder_config->setDisplayAspectHeight(
          active_config->display_size.value().height());
      decoder_config->setDisplayAspectWidth(
          active_config->display_size.value().width());
    }

    VideoColorSpace* color_space =
        MakeGarbageCollected<VideoColorSpace>(output_color_space);
    decoder_config->setColorSpace(color_space->toJSON());

    if (codec_desc.has_value()) {
      auto* desc_array_buf = DOMArrayBuffer::Create(codec_desc.value().data(),
                                                    codec_desc.value().size());
      decoder_config->setDescription(
          MakeGarbageCollected<AllowSharedBufferSource>(desc_array_buf));
    }
    metadata->setDecoderConfig(decoder_config);
  }

  encoder_metrics_provider_->IncrementEncodedFrameCount();

  TRACE_EVENT_BEGIN1(kCategory, GetTraceNames()->output.c_str(), "timestamp",
                     chunk->timestamp());

  ScriptState::Scope scope(script_state_);
  output_callback_->InvokeAndReportException(nullptr, chunk, metadata);

  TRACE_EVENT_END0(kCategory, GetTraceNames()->output.c_str());
}

void VideoEncoder::ResetInternal(DOMException* ex) {
  Base::ResetInternal(ex);
  active_encodes_ = 0;
}

static void isConfigSupportedWithSoftwareOnly(
    ScriptState* script_state,
    ScriptPromiseResolver* resolver,
    VideoEncoderSupport* support,
    VideoEncoderTraits::ParsedConfig* config) {
  std::unique_ptr<media::VideoEncoder> software_encoder;
  switch (config->codec) {
    case media::VideoCodec::kAV1:
      software_encoder = CreateAv1VideoEncoder();
      break;
    case media::VideoCodec::kVP8:
    case media::VideoCodec::kVP9:
      software_encoder = CreateVpxVideoEncoder();
      break;
    case media::VideoCodec::kH264:
      software_encoder = CreateOpenH264VideoEncoder();
      break;
    default:
      break;
  }
  if (!software_encoder) {
    support->setSupported(false);
    resolver->Resolve(support);
    return;
  }

  auto done_callback = [](std::unique_ptr<media::VideoEncoder> encoder,
                          ScriptPromiseResolver* resolver,
                          scoped_refptr<base::SingleThreadTaskRunner> runner,
                          VideoEncoderSupport* support,
                          media::EncoderStatus status) {
    support->setSupported(status.is_ok());
    resolver->Resolve(support);
    if (base::FeatureList::IsEnabled(
            features::kUseBlinkSchedulerTaskRunnerWithCustomDeleter)) {
      runner->DeleteSoon(FROM_HERE, std::move(encoder));
    } else {
      // This task runner may be destroyed without running tasks, so don't use
      // DeleteSoon() which can leak the codec. See https://crbug.com/1376851.
      runner->PostTask(FROM_HERE,
                       base::DoNothingWithBoundArgs(std::move(encoder)));
    }
  };

  auto* context = ExecutionContext::From(script_state);
  auto runner = context->GetTaskRunner(TaskType::kInternalDefault);
  auto* software_encoder_raw = software_encoder.get();
  software_encoder_raw->Initialize(
      config->profile, config->options, /*info_cb=*/base::DoNothing(),
      /*output_cb=*/base::DoNothing(),
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          done_callback, std::move(software_encoder),
          MakeUnwrappingCrossThreadHandle(resolver), std::move(runner),
          MakeUnwrappingCrossThreadHandle(support))));
}

static void isConfigSupportedWithHardwareOnly(
    ScriptPromiseResolver* resolver,
    VideoEncoderSupport* support,
    VideoEncoderTraits::ParsedConfig* config,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  auto required_encoder_type =
      GetRequiredEncoderType(config->profile, config->hw_pref);
  bool supported = IsAcceleratedConfigurationSupported(
      config->profile, config->options, gpu_factories, required_encoder_type);
  support->setSupported(supported);
  resolver->Resolve(support);
}

class FindAnySupported final : public ScriptFunction::Callable {
 public:
  ScriptValue Call(ScriptState* state, ScriptValue value) override {
    ExceptionContext context(ExceptionContextType::kConstructorOperationInvoke,
                             "VideoEncoderSupport");
    ExceptionState exception_state(state->GetIsolate(), context);
    HeapVector<Member<VideoEncoderSupport>> supports =
        NativeValueTraits<IDLSequence<VideoEncoderSupport>>::NativeValue(
            state->GetIsolate(), value.V8Value(), exception_state);

    VideoEncoderSupport* result = nullptr;
    // We don't really expect exceptions here, but if isConfigSupported() is
    // given a VideoEncoderConfig with uint64 values above max JS int (2^53 - 1)
    // creation of |supports| vector will fail. This can happen during fuzzing.
    if (!exception_state.HadException()) {
      for (auto& support : supports) {
        result = support;
        if (result->supported())
          break;
      }
    }
    return ScriptValue::From(state, result);
  }
};

// static
ScriptPromise VideoEncoder::isConfigSupported(ScriptState* script_state,
                                              const VideoEncoderConfig* config,
                                              ExceptionState& exception_state) {
  auto* parsed_config = ParseConfigStatic(config, exception_state);
  if (!parsed_config) {
    DCHECK(exception_state.HadException());
    return ScriptPromise();
  }
  auto* config_copy = CopyConfig(*config, *parsed_config);

  // Run very basic coarse synchronous validation
  String unused_js_error_message;
  if (!VerifyCodecSupportStatic(parsed_config, &unused_js_error_message)) {
    auto* support = VideoEncoderSupport::Create();
    support->setConfig(config_copy);
    support->setSupported(false);

    return ScriptPromise::Cast(
        script_state,
        ToV8Traits<VideoEncoderSupport>::ToV8(script_state, support)
            .ToLocalChecked());
  }

  // Create promises for resolving hardware and software encoding support and
  // put them into |promises|. Simultaneously run both versions of
  // isConfigSupported(), each version fulfills its own promise.
  HeapVector<ScriptPromise> promises;
  if (parsed_config->hw_pref != HardwarePreference::kPreferSoftware ||
      MayHaveOSSoftwareEncoder(parsed_config->profile)) {
    // Hardware support not denied, detect support by hardware encoders.
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
        script_state, exception_state.GetContext());
    promises.push_back(resolver->Promise());
    auto* support = VideoEncoderSupport::Create();
    support->setConfig(config_copy);
    auto gpu_retrieved_callback =
        CrossThreadBindOnce(isConfigSupportedWithHardwareOnly,
                            MakeUnwrappingCrossThreadHandle(resolver),
                            MakeUnwrappingCrossThreadHandle(support),
                            MakeUnwrappingCrossThreadHandle(parsed_config));
    RetrieveGpuFactoriesWithKnownEncoderSupport(
        std::move(gpu_retrieved_callback));
  }

  if (parsed_config->hw_pref != HardwarePreference::kPreferHardware) {
    // Hardware support not required, detect support by software encoders.
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
        script_state, exception_state.GetContext());
    promises.push_back(resolver->Promise());
    auto* support = VideoEncoderSupport::Create();
    support->setConfig(config_copy);
    isConfigSupportedWithSoftwareOnly(script_state, resolver, support,
                                      parsed_config);
  }

  // Wait for all |promises| to resolve and check if any of them have
  // support=true.
  auto* find_any_supported = MakeGarbageCollected<ScriptFunction>(
      script_state, MakeGarbageCollected<FindAnySupported>());

  return ScriptPromise::All(script_state, promises).Then(find_any_supported);
}

}  // namespace blink
