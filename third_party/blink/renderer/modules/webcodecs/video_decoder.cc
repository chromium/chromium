// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_decoder.h"

#include <utility>
#include <vector>

#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
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
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/filters/h264_to_annex_b_bitstream_converter.h"
#include "media/formats/mp4/box_definitions.h"
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

namespace blink {

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

bool IsValidConfig(const VideoDecoderConfig& config,
                   media::VideoType& out_video_type,
                   String& out_console_message) {
  if (!ParseCodecString(config.codec(), out_video_type, out_console_message))
    return false;

  if (config.hasCodedWidth()) {
    if (config.codedWidth() == 0) {
      out_console_message =
          "Invalid codedWidth. Value must be greater than zero.";
      return false;
    }

    uint32_t crop_left = config.hasCropLeft() ? config.cropLeft() : 0;
    uint32_t crop_width =
        config.hasCropWidth() ? config.cropWidth() : config.codedWidth();

    if (crop_width == 0) {
      out_console_message =
          "Invalid cropWidth. Value must be greater than zero.";
      return false;
    }

    if (crop_left + crop_width > config.codedWidth()) {
      out_console_message =
          "Invalid cropLeft + cropWidth. Sum must not exceed codedWidth.";
      return false;
    }
  } else {  // !config.hasCodedWidth()
    if (config.hasCropLeft()) {
      out_console_message =
          "Invalid config. cropLeft specified without codedWidth.";
      return false;
    }

    if (config.hasCropWidth()) {
      out_console_message =
          "Invalid config. cropWidth specified without codedWidth.";
      return false;
    }
  }

  if (config.hasCodedHeight()) {
    if (config.codedHeight() == 0) {
      out_console_message =
          "Invalid codedHeight. Value must be greater than zero.";
      return false;
    }

    uint32_t crop_top = config.hasCropTop() ? config.cropTop() : 0;
    uint32_t crop_height =
        config.hasCropHeight() ? config.cropHeight() : config.codedHeight();

    if (crop_height == 0) {
      out_console_message =
          "Invalid cropHeight. Value must be greater than zero.";
      return false;
    }

    if (crop_top + crop_height > config.codedHeight()) {
      out_console_message =
          "Invalid cropTop + cropHeight. Sum must not exceed codedHeight.";
      return false;
    }
  } else {  // !config.hasCodedHeight()
    if (config.hasCropTop()) {
      out_console_message =
          "Invalid config. cropTop specified without codedHeight.";
      return false;
    }

    if (config.hasCropHeight()) {
      out_console_message =
          "Invalid config. cropHeight specified without codedHeight.";
      return false;
    }
  }

  if (config.hasDisplayWidth() && config.displayWidth() == 0) {
    out_console_message =
        "Invalid displayWidth. Value must be greater than zero.";
    return false;
  }

  if (config.hasDisplayHeight() && config.displayHeight() == 0) {
    out_console_message =
        "Invalid displayHeight. Value must be greater than zero.";
    return false;
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

  return copy;
}

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
void VideoDecoderTraits::InitializeDecoder(
    MediaDecoderType& decoder,
    const MediaConfigType& media_config,
    MediaDecoderType::InitCB init_cb,
    MediaDecoderType::OutputCB output_cb) {
  decoder.Initialize(media_config, false /* low_delay */,
                     nullptr /* cdm_context */, std::move(init_cb), output_cb,
                     media::WaitingCB());
}

// static
void VideoDecoderTraits::UpdateDecoderLog(const MediaDecoderType& decoder,
                                          const MediaConfigType& media_config,
                                          media::MediaLog* media_log) {
  media_log->SetProperty<media::MediaLogProperty::kFrameTitle>(
      std::string("VideoDecoder(WebCodecs)"));
  media_log->SetProperty<media::MediaLogProperty::kVideoDecoderName>(
      decoder.GetDisplayName());
  media_log->SetProperty<media::MediaLogProperty::kIsPlatformVideoDecoder>(
      decoder.IsPlatformDecoder());
  media_log->SetProperty<media::MediaLogProperty::kVideoTracks>(
      std::vector<MediaConfigType>{media_config});
}

// static
VideoDecoderTraits::OutputType* VideoDecoderTraits::MakeOutput(
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
  media::VideoType video_type;
  String console_message;

  if (!IsValidConfig(*config, video_type, console_message)) {
    exception_state.ThrowTypeError(console_message);
    return ScriptPromise();
  }

  // TODO(https://crbug.com/1164013): Add async checks for hardware support upon
  // adding "acceleration" options to the config.
  VideoDecoderSupport* support = VideoDecoderSupport::Create();
  support->setSupported(media::IsSupportedVideoType(video_type));
  support->setConfig(CopyConfig(*config));

  return ScriptPromise::Cast(script_state, ToV8(support, script_state));
}

// static
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
  // TODO(sandersd): Use kUnknownTimestamp instead of 0?
  decoder_buffer->set_duration(
      base::TimeDelta::FromMicroseconds(chunk.duration().value_or(0)));
  decoder_buffer->set_is_key_frame(chunk.type() == "key");

  return decoder_buffer;
}

}  // namespace blink
