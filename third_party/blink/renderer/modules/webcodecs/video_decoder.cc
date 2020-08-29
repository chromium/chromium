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
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_config_eval.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/video_decoder_broker.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
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

// static
std::unique_ptr<VideoDecoderTraits::MediaDecoderType>
VideoDecoderTraits::CreateDecoder(ExecutionContext& execution_context,
                                  media::MediaLog* media_log) {
  return std::make_unique<VideoDecoderBroker>(
      execution_context, Platform::Current()->GetGpuFactories());
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
int VideoDecoderTraits::GetMaxDecodeRequests(const MediaDecoderType& decoder) {
  return decoder.GetMaxDecodeRequests();
}

// static
VideoDecoder* VideoDecoder::Create(ScriptState* script_state,
                                   const VideoDecoderInit* init,
                                   ExceptionState& exception_state) {
  return MakeGarbageCollected<VideoDecoder>(script_state, init,
                                            exception_state);
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

  bool is_codec_ambiguous = true;
  media::VideoCodec codec = media::kUnknownVideoCodec;
  media::VideoCodecProfile profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;
  media::VideoColorSpace color_space = media::VideoColorSpace::REC709();
  uint8_t level = 0;
  bool parse_succeeded = media::ParseVideoCodecString(
      "", config.codec().Utf8(), &is_codec_ambiguous, &codec, &profile, &level,
      &color_space);

  if (!parse_succeeded) {
    *out_console_message = "Failed to parse codec string.";
    return CodecConfigEval::kInvalid;
  }

  if (is_codec_ambiguous) {
    *out_console_message = "Codec string is ambiguous.";
    return CodecConfigEval::kInvalid;
  }

  if (!media::IsSupportedVideoType({codec, profile, level, color_space})) {
    *out_console_message = "Configuration is not supported.";
    return CodecConfigEval::kUnsupported;
  }

  // TODO(sandersd): Can we allow shared ArrayBuffers?
  std::vector<uint8_t> extra_data;
  if (config.hasDescription()) {
    if (config.description().IsArrayBuffer()) {
      DOMArrayBuffer* buffer = config.description().GetAsArrayBuffer();
      uint8_t* start = static_cast<uint8_t*>(buffer->Data());
      size_t size = buffer->ByteLengthAsSizeT();
      extra_data.assign(start, start + size);
    } else {
      DCHECK(config.description().IsArrayBufferView());
      DOMArrayBufferView* view =
          config.description().GetAsArrayBufferView().Get();
      uint8_t* start = static_cast<uint8_t*>(view->BaseAddress());
      size_t size = view->byteLengthAsSizeT();
      extra_data.assign(start, start + size);
    }
  }

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (codec == media::kCodecH264) {
    if (extra_data.empty()) {
      *out_console_message =
          "H.264 configuration must include an avcC description.";
      return CodecConfigEval::kInvalid;
    }

    h264_avcc_ = std::make_unique<media::mp4::AVCDecoderConfigurationRecord>();
    h264_converter_ = std::make_unique<media::H264ToAnnexBBitstreamConverter>();
    if (!h264_converter_->ParseConfiguration(
            extra_data.data(), static_cast<uint32_t>(extra_data.size()),
            h264_avcc_.get())) {
      *out_console_message = "Failed to parse avcC.";
      return CodecConfigEval::kInvalid;
    }
  } else {
    h264_avcc_.reset();
    h264_converter_.reset();
  }
#else
  if (codec == media::kCodecH264) {
    *out_console_message = "H.264 decoding is not supported.";
    return CodecConfigEval::kUnsupported;
  }
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  // TODO(sandersd): Use size information from the VideoDecoderConfig when it is
  // provided, and figure out how to combine it with the avcC. Update fuzzer to
  // match.
  gfx::Size size = gfx::Size(1280, 720);

  out_media_config->Initialize(codec, profile,
                               media::VideoDecoderConfig::AlphaMode::kIsOpaque,
                               color_space, media::kNoTransformation, size,
                               gfx::Rect(gfx::Point(), size), size, extra_data,
                               media::EncryptionScheme::kUnencrypted);

  return CodecConfigEval::kSupported;
}

scoped_refptr<media::DecoderBuffer> VideoDecoder::MakeDecoderBuffer(
    const InputType& chunk) {
  uint8_t* src = static_cast<uint8_t*>(chunk.data()->Data());
  size_t src_size = chunk.data()->ByteLengthAsSizeT();

  scoped_refptr<media::DecoderBuffer> decoder_buffer;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (h264_converter_) {
    // Note: this may not be safe if support for SharedArrayBuffers is added.
    uint32_t output_size = h264_converter_->CalculateNeededOutputBufferSize(
        src, static_cast<uint32_t>(src_size), h264_avcc_.get());
    if (!output_size) {
      // TODO(sandersd): Provide an error message.
      return nullptr;
    }

    std::vector<uint8_t> buf(output_size);
    if (!h264_converter_->ConvertNalUnitStreamToByteStream(
            src, static_cast<uint32_t>(src_size), h264_avcc_.get(), buf.data(),
            &output_size)) {
      // TODO(sandersd): Provide an error message.
      return nullptr;
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
