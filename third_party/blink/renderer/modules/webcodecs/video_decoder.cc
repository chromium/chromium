// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_decoder.h"

#include <utility>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_util.h"
#include "media/base/mime_util.h"
#include "media/base/supported_types.h"
#include "media/base/video_aspect_ratio.h"
#include "media/base/video_decoder.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_color_space_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_support.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/allow_shared_buffer_source_util.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/gpu_factories_retriever.h"
#include "third_party/blink/renderer/modules/webcodecs/video_color_space.h"
#include "third_party/blink/renderer/modules/webcodecs/video_decoder_broker.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/libaom/libaom_buildflags.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_LIBAOM)
#include "third_party/libaom/source/libaom/aom/aom_decoder.h"  // nogncheck
#include "third_party/libaom/source/libaom/aom/aomdx.h"        // nogncheck
#endif

#if BUILDFLAG(ENABLE_LIBVPX)
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"        // nogncheck
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"  // nogncheck
#endif

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/filters/h264_to_annex_b_bitstream_converter.h"  // nogncheck
#include "media/formats/mp4/box_definitions.h"                  // nogncheck
#endif

namespace blink {

namespace {

void DecoderSupport_OnKnown(
    VideoDecoderSupport* support,
    std::unique_ptr<VideoDecoder::MediaConfigType> media_config,
    ScriptPromiseResolver* resolver,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  if (!gpu_factories) {
    support->setSupported(false);
    resolver->Resolve(support);
    return;
  }

  DCHECK(gpu_factories->IsDecoderSupportKnown());
  support->setSupported(
      gpu_factories->IsDecoderConfigSupportedOrUnknown(*media_config) ==
      media::GpuVideoAcceleratorFactories::Supported::kTrue);
  resolver->Resolve(support);
}

bool ParseCodecString(const String& codec_string,
                      media::VideoType& out_video_type,
                      String& js_error_message) {
  bool is_codec_ambiguous = true;
  media::VideoCodec codec = media::VideoCodec::kUnknown;
  media::VideoCodecProfile profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;
  media::VideoColorSpace color_space = media::VideoColorSpace::REC709();
  uint8_t level = 0;
  bool parse_succeeded =
      media::ParseVideoCodecString("", codec_string.Utf8(), &is_codec_ambiguous,
                                   &codec, &profile, &level, &color_space);

  if (!parse_succeeded) {
    js_error_message = "Failed to parse codec string.";
    return false;
  }

  if (is_codec_ambiguous) {
    js_error_message = "Codec string is ambiguous.";
    return false;
  }

  out_video_type = {codec, profile, level, color_space};
  return true;
}

VideoDecoderConfig* CopyConfig(const VideoDecoderConfig& config,
                               ExceptionState& exception_state) {
  VideoDecoderConfig* copy = VideoDecoderConfig::Create();
  copy->setCodec(config.codec());

  if (config.hasDescription()) {
    auto desc_wrapper = AsSpan<const uint8_t>(config.description());
    if (!desc_wrapper.data()) {
      exception_state.ThrowTypeError("description is detached.");
      return nullptr;
    }
    DOMArrayBuffer* buffer_copy =
        DOMArrayBuffer::Create(desc_wrapper.data(), desc_wrapper.size());
    copy->setDescription(
        MakeGarbageCollected<AllowSharedBufferSource>(buffer_copy));
  }

  if (config.hasCodedWidth())
    copy->setCodedWidth(config.codedWidth());

  if (config.hasCodedHeight())
    copy->setCodedHeight(config.codedHeight());

  if (config.hasDisplayAspectWidth())
    copy->setDisplayAspectWidth(config.displayAspectWidth());

  if (config.hasDisplayAspectHeight())
    copy->setDisplayAspectHeight(config.displayAspectHeight());

  if (config.hasColorSpace()) {
    VideoColorSpace* color_space =
        MakeGarbageCollected<VideoColorSpace>(config.colorSpace());
    copy->setColorSpace(color_space->toJSON());
  }

  if (config.hasHardwareAcceleration())
    copy->setHardwareAcceleration(config.hardwareAcceleration());

  if (config.hasOptimizeForLatency())
    copy->setOptimizeForLatency(config.optimizeForLatency());

  return copy;
}

void ParseAv1KeyFrame(const media::DecoderBuffer& buffer, bool* is_key_frame) {
#if BUILDFLAG(ENABLE_LIBAOM)
  aom_codec_stream_info_t stream_info = {0};
  auto status = aom_codec_peek_stream_info(
      &aom_codec_av1_dx_algo, buffer.data(), buffer.data_size(), &stream_info);
  *is_key_frame = (status == AOM_CODEC_OK) && stream_info.is_kf;
#endif
}

void ParseVpxKeyFrame(const media::DecoderBuffer& buffer,
                      media::VideoCodec codec,
                      bool* is_key_frame) {
#if BUILDFLAG(ENABLE_LIBVPX)
  vpx_codec_stream_info_t stream_info = {0};
  stream_info.sz = sizeof(vpx_codec_stream_info_t);
  auto status = vpx_codec_peek_stream_info(
      codec == media::VideoCodec::kVP8 ? &vpx_codec_vp8_dx_algo
                                       : &vpx_codec_vp9_dx_algo,
      buffer.data(), static_cast<uint32_t>(buffer.data_size()), &stream_info);
  *is_key_frame = (status == VPX_CODEC_OK) && stream_info.is_kf;
#endif
}

void ParseH264KeyFrame(const media::DecoderBuffer& buffer, bool* is_key_frame) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  auto result = media::mp4::AVC::AnalyzeAnnexB(
      buffer.data(), buffer.data_size(), std::vector<media::SubsampleEntry>());
  *is_key_frame = result.is_keyframe.value_or(false);
#endif
}

}  // namespace

// static
std::unique_ptr<VideoDecoderTraits::MediaDecoderType>
VideoDecoderTraits::CreateDecoder(
    ExecutionContext& execution_context,
    media::GpuVideoAcceleratorFactories* gpu_factories,
    media::MediaLog* media_log) {
  return std::make_unique<VideoDecoderBroker>(execution_context, gpu_factories,
                                              media_log);
}

// static
HardwarePreference VideoDecoder::GetHardwareAccelerationPreference(
    const ConfigType& config) {
  // The IDL defines a default value of "allow".
  DCHECK(config.hasHardwareAcceleration());
  return StringToHardwarePreference(
      IDLEnumAsString(config.hardwareAcceleration()));
}

// static
void VideoDecoderTraits::InitializeDecoder(
    MediaDecoderType& decoder,
    bool low_delay,
    const MediaConfigType& media_config,
    MediaDecoderType::InitCB init_cb,
    MediaDecoderType::OutputCB output_cb) {
  decoder.Initialize(media_config, low_delay, nullptr /* cdm_context */,
                     std::move(init_cb), output_cb, media::WaitingCB());
}

// static
void VideoDecoderTraits::UpdateDecoderLog(const MediaDecoderType& decoder,
                                          const MediaConfigType& media_config,
                                          media::MediaLog* media_log) {
  media_log->SetProperty<media::MediaLogProperty::kVideoDecoderName>(
      decoder.GetDecoderType());
  media_log->SetProperty<media::MediaLogProperty::kIsPlatformVideoDecoder>(
      decoder.IsPlatformDecoder());
  media_log->SetProperty<media::MediaLogProperty::kVideoTracks>(
      std::vector<MediaConfigType>{media_config});
  MEDIA_LOG(INFO, media_log)
      << "Initialized VideoDecoder: " << media_config.AsHumanReadableString();
  base::UmaHistogramEnumeration("Blink.WebCodecs.VideoDecoder.Codec",
                                media_config.codec());
}

// static
media::DecoderStatus::Or<VideoDecoderTraits::OutputType*>
VideoDecoderTraits::MakeOutput(scoped_refptr<MediaOutputType> output,
                               ExecutionContext* context) {
  return MakeGarbageCollected<VideoDecoderTraits::OutputType>(std::move(output),
                                                              context);
}

// static
int VideoDecoderTraits::GetMaxDecodeRequests(const MediaDecoderType& decoder) {
  return decoder.GetMaxDecodeRequests();
}

// static
const char* VideoDecoderTraits::GetName() {
  return "VideoDecoder";
}

// static
VideoDecoder* VideoDecoder::Create(ScriptState* script_state,
                                   const VideoDecoderInit* init,
                                   ExceptionState& exception_state) {
  auto* result =
      MakeGarbageCollected<VideoDecoder>(script_state, init, exception_state);
  return exception_state.HadException() ? nullptr : result;
}

// static
ScriptPromise VideoDecoder::isConfigSupported(ScriptState* script_state,
                                              const VideoDecoderConfig* config,
                                              ExceptionState& exception_state) {
  String js_error_message;
  absl::optional<media::VideoType> video_type =
      IsValidVideoDecoderConfig(*config, &js_error_message /* out */);

  if (!video_type) {
    exception_state.ThrowTypeError(js_error_message);
    return ScriptPromise();
  }

  HardwarePreference hw_pref = GetHardwareAccelerationPreference(*config);

  if (hw_pref == HardwarePreference::kPreferHardware)
    return IsAcceleratedConfigSupported(script_state, config, exception_state);

  // Accept all supported configs if we are not requiring hardware only.
  VideoDecoderSupport* support = VideoDecoderSupport::Create();
  support->setSupported(media::IsSupportedVideoType(*video_type));

  auto* config_copy = CopyConfig(*config, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  support->setConfig(config_copy);

  return ScriptPromise::Cast(
      script_state, ToV8Traits<VideoDecoderSupport>::ToV8(script_state, support)
                        .ToLocalChecked());
}

ScriptPromise VideoDecoder::IsAcceleratedConfigSupported(
    ScriptState* script_state,
    const VideoDecoderConfig* config,
    ExceptionState& exception_state) {
  String js_error_message;
  DCHECK(IsValidVideoDecoderConfig(*config, &js_error_message));

  absl::optional<MediaConfigType> media_config;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  std::unique_ptr<media::H264ToAnnexBBitstreamConverter> h264_converter;
  std::unique_ptr<media::mp4::AVCDecoderConfigurationRecord> h264_avcc;
  media_config = MakeMediaVideoDecoderConfig(*config, h264_converter, h264_avcc,
                                             &js_error_message);
#else
  media_config = MakeMediaVideoDecoderConfig(*config, &js_error_message);
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  if (!media_config) {
    exception_state.ThrowTypeError(js_error_message);
    return ScriptPromise();
  }

  auto* config_copy = CopyConfig(*config, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  VideoDecoderSupport* support = VideoDecoderSupport::Create();
  support->setConfig(config_copy);

  RetrieveGpuFactoriesWithKnownDecoderSupport(CrossThreadBindOnce(
      &DecoderSupport_OnKnown, WrapCrossThreadPersistent(support),
      std::make_unique<MediaConfigType>(*media_config),
      WrapCrossThreadPersistent(resolver)));

  return promise;
}

HardwarePreference VideoDecoder::GetHardwarePreference(
    const ConfigType& config) {
  return GetHardwareAccelerationPreference(config);
}

bool VideoDecoder::GetLowDelayPreference(const ConfigType& config) {
  return config.hasOptimizeForLatency() && config.optimizeForLatency();
}

void VideoDecoder::SetHardwarePreference(HardwarePreference preference) {
  static_cast<VideoDecoderBroker*>(decoder())->SetHardwarePreference(
      preference);
}

// static
// TODO(crbug.com/1198324): Merge shared logic with VideoFramePlaneInit.
absl::optional<media::VideoType> VideoDecoder::IsValidVideoDecoderConfig(
    const VideoDecoderConfig& config,
    String* js_error_message) {
  media::VideoType video_type;
  if (!ParseCodecString(config.codec(), video_type, *js_error_message))
    return absl::nullopt;

  if (config.hasCodedWidth() || config.hasCodedHeight()) {
    if (!config.hasCodedWidth()) {
      *js_error_message =
          "Invalid config, codedHeight specified without codedWidth.";
      return absl::nullopt;
    }
    if (!config.hasCodedHeight()) {
      *js_error_message =
          "Invalid config, codedWidth specified without codedHeight.";
      return absl::nullopt;
    }

    const uint32_t coded_width = config.codedWidth();
    const uint32_t coded_height = config.codedHeight();
    if (coded_width == 0 || coded_width > media::limits::kMaxDimension ||
        coded_height == 0 || coded_height > media::limits::kMaxDimension) {
      // TODO(crbug.com/1212865): Exceeding implementation limits should not
      // throw in isConfigSupported() (the config is valid, just unsupported).
      *js_error_message = String::Format("Invalid coded size (%u, %u).",
                                         coded_width, coded_height);
      return absl::nullopt;
    }
  }

  if (config.hasDisplayAspectWidth() || config.hasDisplayAspectHeight()) {
    if (!config.hasDisplayAspectWidth()) {
      *js_error_message =
          "Invalid config, displayAspectHeight specified without "
          "displayAspectWidth.";
      return absl::nullopt;
    }
    if (!config.hasDisplayAspectHeight()) {
      *js_error_message =
          "Invalid config, displayAspectWidth specified without "
          "displayAspectHeight.";
      return absl::nullopt;
    }

    uint32_t display_aspect_width = config.displayAspectWidth();
    uint32_t display_aspect_height = config.displayAspectHeight();
    if (display_aspect_width == 0 || display_aspect_height == 0) {
      *js_error_message =
          String::Format("Invalid display aspect (%u, %u).",
                         display_aspect_width, display_aspect_height);
      return absl::nullopt;
    }
  }

  return video_type;
}

// static
absl::optional<media::VideoDecoderConfig>
VideoDecoder::MakeMediaVideoDecoderConfig(
    const ConfigType& config,
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    std::unique_ptr<media::H264ToAnnexBBitstreamConverter>& out_h264_converter,
    std::unique_ptr<media::mp4::AVCDecoderConfigurationRecord>& out_h264_avcc,
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
    String* js_error_message) {

  absl::optional<media::VideoType> video_type =
      IsValidVideoDecoderConfig(config, js_error_message /* out */);
  if (!video_type)
    return absl::nullopt;

  std::vector<uint8_t> extra_data;
  if (config.hasDescription()) {
    // TODO(crbug.com/1179970): This should throw if description is detached.
    auto desc_wrapper = AsSpan<const uint8_t>(config.description());
    if (!desc_wrapper.empty()) {
      const uint8_t* start = desc_wrapper.data();
      const size_t size = desc_wrapper.size();
      extra_data.assign(start, start + size);
    }
  }

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (video_type->codec == media::VideoCodec::kH264 && !extra_data.empty()) {
    out_h264_avcc =
        std::make_unique<media::mp4::AVCDecoderConfigurationRecord>();
    out_h264_converter =
        std::make_unique<media::H264ToAnnexBBitstreamConverter>();
    if (!out_h264_converter->ParseConfiguration(
            extra_data.data(), static_cast<uint32_t>(extra_data.size()),
            out_h264_avcc.get())) {
      *js_error_message = "Failed to parse avcC.";
      return absl::nullopt;
    }
  } else {
    out_h264_avcc.reset();
    out_h264_converter.reset();
  }
#else
  if (video_type->codec == media::VideoCodec::kH264) {
    *js_error_message = "H.264 decoding is not supported.";
    return absl::nullopt;
  }
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  // Guess 720p if no coded size hint is provided. This choice should result in
  // a preference for hardware decode.
  gfx::Size coded_size = gfx::Size(1280, 720);
  if (config.hasCodedWidth() && config.hasCodedHeight())
    coded_size = gfx::Size(config.codedWidth(), config.codedHeight());

  // These are meaningless.
  // TODO(crbug.com/1214061): Remove.
  gfx::Rect visible_rect(gfx::Point(), coded_size);
  gfx::Size natural_size = coded_size;

  // Note: Using a default-constructed VideoAspectRatio allows decoders to
  // override using in-band metadata.
  media::VideoAspectRatio aspect_ratio;
  if (config.hasDisplayAspectWidth() && config.hasDisplayAspectHeight()) {
    aspect_ratio = media::VideoAspectRatio::DAR(config.displayAspectWidth(),
                                                config.displayAspectHeight());
  }

  // TODO(crbug.com/1138680): Ensure that this default value is acceptable
  // under the WebCodecs spec. Should be BT.709 for YUV, sRGB for RGB, or
  // whatever was explicitly set for codec strings that include a color space.
  media::VideoColorSpace media_color_space = video_type->color_space;
  if (config.hasColorSpace()) {
    VideoColorSpace* color_space =
        MakeGarbageCollected<VideoColorSpace>(config.colorSpace());
    media_color_space = color_space->ToMediaColorSpace();
  }

  media::VideoDecoderConfig media_config;
  media_config.Initialize(video_type->codec, video_type->profile,
                          media::VideoDecoderConfig::AlphaMode::kIsOpaque,
                          media_color_space, media::kNoTransformation,
                          coded_size, visible_rect, natural_size, extra_data,
                          media::EncryptionScheme::kUnencrypted);
  media_config.set_aspect_ratio(aspect_ratio);

  return media_config;
}

VideoDecoder::VideoDecoder(ScriptState* script_state,
                           const VideoDecoderInit* init,
                           ExceptionState& exception_state)
    : DecoderTemplate<VideoDecoderTraits>(script_state, init, exception_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);
}

bool VideoDecoder::IsValidConfig(const ConfigType& config,
                                 String* js_error_message) {
  return IsValidVideoDecoderConfig(config, js_error_message /* out */)
      .has_value();
}

absl::optional<media::VideoDecoderConfig> VideoDecoder::MakeMediaConfig(
    const ConfigType& config,
    String* js_error_message) {
  DCHECK(js_error_message);
  absl::optional<media::VideoDecoderConfig> media_config =
      MakeMediaVideoDecoderConfig(config,
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
                                  h264_converter_ /* out */,
                                  h264_avcc_ /* out */,
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
                                  js_error_message /* out */);
  if (media_config)
    current_codec_ = media_config->codec();
  return media_config;
}

media::DecoderStatus::Or<scoped_refptr<media::DecoderBuffer>>
VideoDecoder::MakeDecoderBuffer(const InputType& chunk, bool verify_key_frame) {
  scoped_refptr<media::DecoderBuffer> decoder_buffer = chunk.buffer();
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (h264_converter_) {
    const uint8_t* src = chunk.buffer()->data();
    size_t src_size = chunk.buffer()->data_size();

    // Note: this may not be safe if support for SharedArrayBuffers is added.
    uint32_t output_size = h264_converter_->CalculateNeededOutputBufferSize(
        src, static_cast<uint32_t>(src_size), h264_avcc_.get());
    if (!output_size) {
      return media::DecoderStatus(
          media::DecoderStatus::Codes::kMalformedBitstream,
          "Unable to determine size of bitstream buffer.");
    }

    std::vector<uint8_t> buf(output_size);
    if (!h264_converter_->ConvertNalUnitStreamToByteStream(
            src, static_cast<uint32_t>(src_size), h264_avcc_.get(), buf.data(),
            &output_size)) {
      return media::DecoderStatus(
          media::DecoderStatus::Codes::kMalformedBitstream,
          "Unable to convert NALU to byte stream.");
    }

    decoder_buffer = media::DecoderBuffer::CopyFrom(buf.data(), output_size);
    decoder_buffer->set_timestamp(chunk.buffer()->timestamp());
    decoder_buffer->set_duration(chunk.buffer()->duration());
  }
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  bool is_key_frame = chunk.type() == "key";
  if (verify_key_frame) {
    if (current_codec_ == media::VideoCodec::kVP9 ||
        current_codec_ == media::VideoCodec::kVP8) {
      ParseVpxKeyFrame(*decoder_buffer, current_codec_, &is_key_frame);
    } else if (current_codec_ == media::VideoCodec::kAV1) {
      ParseAv1KeyFrame(*decoder_buffer, &is_key_frame);
    } else if (current_codec_ == media::VideoCodec::kH264) {
      ParseH264KeyFrame(*decoder_buffer, &is_key_frame);
    }

    if (!is_key_frame) {
      return media::DecoderStatus(
          media::DecoderStatus::Codes::kKeyFrameRequired,
          "A key frame is required after configure() or flush().");
    }
  }

  return decoder_buffer;
}

}  // namespace blink
