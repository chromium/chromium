// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_decoder.h"

#include <utility>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_util.h"
#include "media/base/mime_util.h"
#include "media/base/supported_types.h"
#include "media/base/video_decoder.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_support.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_region.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_config_eval.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/video_decoder_broker.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/filters/h264_to_annex_b_bitstream_converter.h"
#include "media/formats/mp4/box_definitions.h"
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

namespace blink {

namespace {

media::GpuVideoAcceleratorFactories* GetGpuFactoriesOnMainThread() {
  DCHECK(IsMainThread());
  return Platform::Current()->GetGpuFactories();
}

void DecoderSupport_OnKnown(
    VideoDecoderSupport* support,
    std::unique_ptr<VideoDecoder::MediaConfigType> media_config,
    ScriptPromiseResolver* resolver,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  DCHECK(gpu_factories->IsDecoderSupportKnown());
  support->setSupported(
      gpu_factories->IsDecoderConfigSupported(*media_config) ==
      media::GpuVideoAcceleratorFactories::Supported::kTrue);
  resolver->Resolve(support);
}

void DecoderSupport_OnGpuFactories(
    VideoDecoderSupport* support,
    std::unique_ptr<VideoDecoder::MediaConfigType> media_config,
    ScriptPromiseResolver* resolver,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  if (!gpu_factories || !gpu_factories->IsGpuVideoAcceleratorEnabled()) {
    support->setSupported(false);
    resolver->Resolve(support);
    return;
  }

  if (gpu_factories->IsDecoderSupportKnown()) {
    DecoderSupport_OnKnown(support, std::move(media_config), resolver,
                           gpu_factories);
    return;
  }

  gpu_factories->NotifyDecoderSupportKnown(
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          &DecoderSupport_OnKnown, WrapCrossThreadPersistent(support),
          std::move(media_config), WrapCrossThreadPersistent(resolver),
          CrossThreadUnretained(gpu_factories))));
}

bool ParseCodecString(const String& codec_string,
                      media::VideoType& out_video_type,
                      String& out_console_message) {
  bool is_codec_ambiguous = true;
  media::VideoCodec codec = media::kUnknownVideoCodec;
  media::VideoCodecProfile profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;
  media::VideoColorSpace color_space = media::VideoColorSpace::REC709();
  uint8_t level = 0;
  bool parse_succeeded =
      media::ParseVideoCodecString("", codec_string.Utf8(), &is_codec_ambiguous,
                                   &codec, &profile, &level, &color_space);

  if (!parse_succeeded) {
    out_console_message = "Failed to parse codec string.";
    return false;
  }

  if (is_codec_ambiguous) {
    out_console_message = "Codec string is ambiguous.";
    return false;
  }

  out_video_type = {codec, profile, level, color_space};
  return true;
}

// TODO(crbug.com/1179970): rename out_console_message.
// TODO(crbug.com/1181443): Make this a pure virtual in DecoderTemplate, and
// refactor its uses.
// TODO(crbug.com/1198324): Merge shared logic with VideoFramePlaneInit.
bool IsValidConfig(const VideoDecoderConfig& config,
                   media::VideoType& out_video_type,
                   String& out_console_message) {
  if (!ParseCodecString(config.codec(), out_video_type, out_console_message))
    return false;

  if (config.hasCodedWidth() || config.hasCodedHeight()) {
    if (!config.hasCodedWidth()) {
      out_console_message =
          "Invalid config, codedHeight specified without codedWidth.";
      return false;
    }
    if (!config.hasCodedHeight()) {
      out_console_message =
          "Invalid config, codedWidth specified without codedHeight.";
      return false;
    }

    const uint32_t coded_width = config.codedWidth();
    const uint32_t coded_height = config.codedHeight();
    if (coded_width == 0 || coded_width > media::limits::kMaxDimension ||
        coded_height == 0 || coded_height > media::limits::kMaxDimension) {
      out_console_message = String::Format("Invalid coded size (%u, %u).",
                                           coded_width, coded_height);
      return false;
    }

    // Validate visible region.
    uint32_t visible_left = 0;
    uint32_t visible_top = 0;
    uint32_t visible_width = coded_width;
    uint32_t visible_height = coded_height;
    if (config.hasVisibleRegion()) {
      visible_left = config.visibleRegion()->left();
      visible_top = config.visibleRegion()->top();
      visible_width = config.visibleRegion()->width();
      visible_height = config.visibleRegion()->height();
    } else {
      // TODO(sandersd): Plumb |execution_context| so we can log a deprecation
      // notice.
      if (config.hasCropLeft()) {
        visible_left = config.cropLeft();
        if (visible_left >= coded_width) {
          out_console_message =
              String::Format("Invalid cropLeft %u for codedWidth %u.",
                             visible_left, coded_width);
          return false;
        }
        visible_width = coded_width - visible_left;
      }
      if (config.hasCropTop()) {
        visible_top = config.cropTop();
        if (visible_top >= coded_height) {
          out_console_message =
              String::Format("Invalid cropTop %u for codedHeight %u.",
                             visible_top, coded_height);
          return false;
        }
        visible_width = coded_width - visible_left;
      }
      if (config.hasCropWidth())
        visible_width = config.cropWidth();
      if (config.hasCropHeight())
        visible_height = config.cropHeight();
    }
    if (visible_left >= coded_width || visible_top >= coded_height ||
        visible_width == 0 || visible_width > media::limits::kMaxDimension ||
        visible_height == 0 || visible_height > media::limits::kMaxDimension ||
        visible_left + visible_width > coded_width ||
        visible_top + visible_height > coded_height) {
      out_console_message = String::Format(
          "Invalid visible region {left: %u, top: %u, width: %u, height: %u} "
          "for coded size (%u, %u).",
          visible_left, visible_top, visible_width, visible_height, coded_width,
          coded_height);
      return false;
    }
  } else {
    if (config.hasVisibleRegion()) {
      out_console_message =
          "Invalid config, visibleRegion specified without coded size.";
      return false;
    }
    if (config.hasCropLeft()) {
      out_console_message =
          "Invalid config, cropLeft specified without coded size.";
      return false;
    }
    if (config.hasCropTop()) {
      out_console_message =
          "Invalid config, cropTop specified without coded size.";
      return false;
    }
    if (config.hasCropWidth()) {
      out_console_message =
          "Invalid config, cropWidth specified without coded size.";
      return false;
    }
    if (config.hasCropHeight()) {
      out_console_message =
          "Invalid config, cropHeight specified without coded size.";
      return false;
    }
  }

  if (config.hasDisplayWidth() || config.hasDisplayHeight()) {
    if (!config.hasDisplayWidth()) {
      out_console_message =
          "Invalid config, displayHeight specified without displayWidth.";
      return false;
    }
    if (!config.hasDisplayHeight()) {
      out_console_message =
          "Invalid config, displayWidth specified without displayHeight.";
      return false;
    }

    uint32_t display_width = config.displayWidth();
    uint32_t display_height = config.displayHeight();
    if (display_width == 0 || display_width > media::limits::kMaxDimension ||
        display_height == 0 || display_height > media::limits::kMaxDimension) {
      out_console_message = String::Format("Invalid display size (%u, %u).",
                                           display_width, display_height);
      return false;
    }
  }

  return true;
}

VideoDecoderConfig* CopyConfig(const VideoDecoderConfig& config) {
  VideoDecoderConfig* copy = VideoDecoderConfig::Create();
  copy->setCodec(config.codec());

  if (config.hasDescription()) {
    DOMArrayPiece buffer(config.description());
    DOMArrayBuffer* buffer_copy =
        DOMArrayBuffer::Create(buffer.Data(), buffer.ByteLength());
    copy->setDescription(
        ArrayBufferOrArrayBufferView::FromArrayBuffer(buffer_copy));
  }

  if (config.hasCodedWidth())
    copy->setCodedWidth(config.codedWidth());

  if (config.hasCodedHeight())
    copy->setCodedHeight(config.codedHeight());

  if (config.hasVisibleRegion()) {
    auto* region = MakeGarbageCollected<VideoFrameRegion>();
    region->setLeft(config.visibleRegion()->left());
    region->setTop(config.visibleRegion()->top());
    region->setWidth(config.visibleRegion()->width());
    region->setHeight(config.visibleRegion()->height());
    copy->setVisibleRegion(region);
  }

  if (config.hasCropLeft())
    copy->setCropLeft(config.cropLeft());

  if (config.hasCropTop())
    copy->setCropTop(config.cropTop());

  if (config.hasCropWidth())
    copy->setCropWidth(config.cropWidth());

  if (config.hasCropHeight())
    copy->setCropHeight(config.cropHeight());

  if (config.hasDisplayWidth())
    copy->setDisplayWidth(config.displayWidth());

  if (config.hasDisplayHeight())
    copy->setDisplayHeight(config.displayHeight());

  if (config.hasHardwareAcceleration())
    copy->setHardwareAcceleration(config.hardwareAcceleration());

  if (config.hasOptimizeForLatency())
    copy->setOptimizeForLatency(config.optimizeForLatency());

  return copy;
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
  media_log->SetProperty<media::MediaLogProperty::kFrameTitle>(
      std::string("VideoDecoder(WebCodecs)"));
  media_log->SetProperty<media::MediaLogProperty::kVideoDecoderName>(
      decoder.GetDecoderType());
  media_log->SetProperty<media::MediaLogProperty::kIsPlatformVideoDecoder>(
      decoder.IsPlatformDecoder());
  media_log->SetProperty<media::MediaLogProperty::kVideoTracks>(
      std::vector<MediaConfigType>{media_config});
  MEDIA_LOG(INFO, media_log)
      << "Initialized VideoDecoder: " << media_config.AsHumanReadableString();
  UMA_HISTOGRAM_ENUMERATION("Blink.WebCodecs.VideoDecoder.Codec",
                            media_config.codec(), media::kVideoCodecMax + 1);
}

// static
media::StatusOr<VideoDecoderTraits::OutputType*> VideoDecoderTraits::MakeOutput(
    scoped_refptr<MediaOutputType> output,
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
  HardwarePreference hw_pref = GetHardwareAccelerationPreference(*config);

  if (hw_pref == HardwarePreference::kRequire)
    return IsAcceleratedConfigSupported(script_state, config, exception_state);

  media::VideoType video_type;
  String console_message;

  if (!IsValidConfig(*config, video_type, console_message)) {
    exception_state.ThrowTypeError(console_message);
    return ScriptPromise();
  }

  // Accept all supported configs if we are not requiring hardware only.
  VideoDecoderSupport* support = VideoDecoderSupport::Create();
  support->setSupported(media::IsSupportedVideoType(video_type));
  support->setConfig(CopyConfig(*config));
  return ScriptPromise::Cast(script_state, ToV8(support, script_state));
}

ScriptPromise VideoDecoder::IsAcceleratedConfigSupported(
    ScriptState* script_state,
    const VideoDecoderConfig* config,
    ExceptionState& exception_state) {
  String console_message;
  auto media_config = std::make_unique<MediaConfigType>();
  CodecConfigEval config_eval;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  std::unique_ptr<media::H264ToAnnexBBitstreamConverter> h264_converter;
  std::unique_ptr<media::mp4::AVCDecoderConfigurationRecord> h264_avcc;
  config_eval = MakeMediaVideoDecoderConfig(
      *config, *media_config, h264_converter, h264_avcc, console_message);
#else
  config_eval =
      MakeMediaVideoDecoderConfig(*config, *media_config, console_message);
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  if (config_eval != CodecConfigEval::kSupported) {
    exception_state.ThrowTypeError(console_message);
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  VideoDecoderSupport* support = VideoDecoderSupport::Create();
  support->setConfig(CopyConfig(*config));

  if (IsMainThread()) {
    media::GpuVideoAcceleratorFactories* gpu_factories =
        Platform::Current()->GetGpuFactories();
    DecoderSupport_OnGpuFactories(support, std::move(media_config), resolver,
                                  gpu_factories);
  } else {
    auto on_gpu_factories_cb = CrossThreadBindOnce(
        &DecoderSupport_OnGpuFactories, WrapCrossThreadPersistent(support),
        std::move(media_config), WrapCrossThreadPersistent(resolver));

    Thread::MainThread()->GetTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        ConvertToBaseOnceCallback(
            CrossThreadBindOnce(&GetGpuFactoriesOnMainThread)),
        ConvertToBaseOnceCallback(std::move(on_gpu_factories_cb)));
  }

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
// TODO(crbug.com/1179970): rename out_console_message.
CodecConfigEval VideoDecoder::MakeMediaVideoDecoderConfig(
    const ConfigType& config,
    MediaConfigType& out_media_config,
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    std::unique_ptr<media::H264ToAnnexBBitstreamConverter>& out_h264_converter,
    std::unique_ptr<media::mp4::AVCDecoderConfigurationRecord>& out_h264_avcc,
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
    String& out_console_message) {
  media::VideoType video_type;

  if (!IsValidConfig(config, video_type, out_console_message))
    return CodecConfigEval::kInvalid;

  // TODO(sandersd): Can we allow shared ArrayBuffers?
  std::vector<uint8_t> extra_data;
  if (config.hasDescription()) {
    DOMArrayPiece buffer(config.description());
    uint8_t* start = static_cast<uint8_t*>(buffer.Data());
    size_t size = buffer.ByteLength();
    extra_data.assign(start, start + size);
  }

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (video_type.codec == media::kCodecH264 && !extra_data.empty()) {
    out_h264_avcc =
        std::make_unique<media::mp4::AVCDecoderConfigurationRecord>();
    out_h264_converter =
        std::make_unique<media::H264ToAnnexBBitstreamConverter>();
    if (!out_h264_converter->ParseConfiguration(
            extra_data.data(), static_cast<uint32_t>(extra_data.size()),
            out_h264_avcc.get())) {
      out_console_message = "Failed to parse avcC.";
      return CodecConfigEval::kInvalid;
    }
  } else {
    out_h264_avcc.reset();
    out_h264_converter.reset();
  }
#else
  if (video_type.codec == media::kCodecH264) {
    out_console_message = "H.264 decoding is not supported.";
    return CodecConfigEval::kUnsupported;
  }
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  // TODO(sandersd): Use size information from the VideoDecoderConfig when it is
  // provided, and figure out how to combine it with the avcC. Update fuzzer to
  // match.
  gfx::Size size = gfx::Size(1280, 720);

  out_media_config.Initialize(
      video_type.codec, video_type.profile,
      media::VideoDecoderConfig::AlphaMode::kIsOpaque, video_type.color_space,
      media::kNoTransformation, size, gfx::Rect(gfx::Point(), size), size,
      extra_data, media::EncryptionScheme::kUnencrypted);

  return CodecConfigEval::kSupported;
}

VideoDecoder::VideoDecoder(ScriptState* script_state,
                           const VideoDecoderInit* init,
                           ExceptionState& exception_state)
    : DecoderTemplate<VideoDecoderTraits>(script_state, init, exception_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);
}

CodecConfigEval VideoDecoder::MakeMediaConfig(const ConfigType& config,
                                              MediaConfigType* out_media_config,
                                              String* out_console_message) {
  DCHECK(out_media_config);
  DCHECK(out_console_message);
  return MakeMediaVideoDecoderConfig(config, *out_media_config,
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
                                     h264_converter_ /* out */,
                                     h264_avcc_ /* out */,
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
                                     *out_console_message);
}

media::StatusOr<scoped_refptr<media::DecoderBuffer>>
VideoDecoder::MakeDecoderBuffer(const InputType& chunk) {
  uint8_t* src = static_cast<uint8_t*>(chunk.data()->Data());
  size_t src_size = chunk.data()->ByteLength();

  scoped_refptr<media::DecoderBuffer> decoder_buffer;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (h264_converter_) {
    // Note: this may not be safe if support for SharedArrayBuffers is added.
    uint32_t output_size = h264_converter_->CalculateNeededOutputBufferSize(
        src, static_cast<uint32_t>(src_size), h264_avcc_.get());
    if (!output_size) {
      return media::Status(media::StatusCode::kH264ParsingError,
                           "Unable to determine size of bitstream buffer.");
    }

    std::vector<uint8_t> buf(output_size);
    if (!h264_converter_->ConvertNalUnitStreamToByteStream(
            src, static_cast<uint32_t>(src_size), h264_avcc_.get(), buf.data(),
            &output_size)) {
      return media::Status(media::StatusCode::kH264ParsingError,
                           "Unable to convert NALU to byte stream.");
    }

    decoder_buffer = media::DecoderBuffer::CopyFrom(buf.data(), output_size);
  }
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (!decoder_buffer)
    decoder_buffer = media::DecoderBuffer::CopyFrom(src, src_size);

  decoder_buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(chunk.timestamp()));

  if (chunk.duration()) {
    // Clamp within bounds of our internal TimeDelta-based duration.
    // See media/base/timestamp_constants.h
    decoder_buffer->set_duration(base::TimeDelta::FromMicroseconds(
        std::min(base::saturated_cast<int64_t>(chunk.duration().value()),
                 std::numeric_limits<int64_t>::max() - 1)));
  }

  decoder_buffer->set_is_key_frame(chunk.type() == "key");

  return decoder_buffer;
}

}  // namespace blink
