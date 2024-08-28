// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webcodecs/video_decoder.h"

#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_util.h"
#include "media/base/mime_util.h"
#include "media/base/supported_types.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_aspect_ratio.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
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
#include "third_party/blink/renderer/modules/webcodecs/array_buffer_util.h"
#include "third_party/blink/renderer/modules/webcodecs/decrypt_config_util.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/gpu_factories_retriever.h"
#include "third_party/blink/renderer/modules/webcodecs/video_color_space.h"
#include "third_party/blink/renderer/modules/webcodecs/video_decoder_broker.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/libgav1/src/src/buffer_pool.h"
#include "third_party/libgav1/src/src/decoder_state.h"
#include "third_party/libgav1/src/src/gav1/status_code.h"
#include "third_party/libgav1/src/src/obu_parser.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_LIBVPX)
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"        // nogncheck
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"  // nogncheck
#endif

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/filters/h264_to_annex_b_bitstream_converter.h"  // nogncheck
#include "media/formats/mp4/box_definitions.h"                  // nogncheck
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
#include "media/filters/h265_to_annex_b_bitstream_converter.h"  // nogncheck
#include "media/formats/mp4/hevc.h"                             // nogncheck
#endif
#endif

namespace blink {

namespace {

void DecoderSupport_OnKnown(
    VideoDecoderSupport* support,
    std::unique_ptr<VideoDecoder::MediaConfigType> media_config,
    ScriptPromiseResolver<VideoDecoderSupport>* resolver,
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
  if (codec_string.LengthWithStrippedWhiteSpace() == 0) {
    js_error_message = "Invalid codec; codec is required.";
    return false;
  }

  auto result = media::ParseVideoCodecString("", codec_string.Utf8(),
                                             /*allow_ambiguous_matches=*/false);

  if (!result) {
    js_error_message = "Unknown or ambiguous codec name.";
    out_video_type = {media::VideoCodec::kUnknown,
                      media::VIDEO_CODEC_PROFILE_UNKNOWN,
                      media::kNoVideoCodecLevel, media::VideoColorSpace()};
    return true;
  }

  out_video_type = {result->codec, result->profile, result->level,
                    result->color_space};
  return true;
}

VideoDecoderConfig* CopyConfig(const VideoDecoderConfig& config) {
  VideoDecoderConfig* copy = VideoDecoderConfig::Create();
  copy->setCodec(config.codec());

  if (config.hasDescription()) {
    auto desc_wrapper = AsSpan<const uint8_t>(config.description());
    if (!desc_wrapper.data()) {
      // Checked by IsValidVideoDecoderConfig.
      NOTREACHED_IN_MIGRATION();
      return nullptr;
    }
    DOMArrayBuffer* buffer_copy = DOMArrayBuffer::Create(desc_wrapper);
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

void ParseAv1KeyFrame(const media::DecoderBuffer& buffer,
                      libgav1::BufferPool* buffer_pool,
                      bool* is_key_frame) {
  libgav1::DecoderState decoder_state;
  libgav1::ObuParser parser(buffer.data(), buffer.size(),
                            /*operating_point=*/0, buffer_pool, &decoder_state);
  libgav1::RefCountedBufferPtr frame;
  libgav1::StatusCode status_code = parser.ParseOneFrame(&frame);
  *is_key_frame = status_code == libgav1::kStatusOk &&
                  parser.frame_header().frame_type == libgav1::kFrameKey;
}

void ParseVpxKeyFrame(const media::DecoderBuffer& buffer,
                      media::VideoCodec codec,
                      bool* is_key_frame) {
#if BUILDFLAG(ENABLE_LIBVPX)
  vpx_codec_stream_info_t stream_info = {0};
  stream_info.sz = sizeof(vpx_codec_stream_info_t);
  auto status = vpx_codec_peek_stream_info(
      codec == media::VideoCodec::kVP8 ? vpx_codec_vp8_dx()
                                       : vpx_codec_vp9_dx(),
      buffer.data(), static_cast<uint32_t>(buffer.size()), &stream_info);
  *is_key_frame = (status == VPX_CODEC_OK) && stream_info.is_kf;
#endif
}

void ParseH264KeyFrame(const media::DecoderBuffer& buffer, bool* is_key_frame) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  auto result = media::mp4::AVC::AnalyzeAnnexB(
      buffer.data(), buffer.size(), std::vector<media::SubsampleEntry>());
  *is_key_frame = result.is_keyframe.value_or(false);
#endif
}

void ParseH265KeyFrame(const media::DecoderBuffer& buffer, bool* is_key_frame) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  auto result = media::mp4::HEVC::AnalyzeAnnexB(
      buffer.data(), buffer.size(), std::vector<media::SubsampleEntry>());
  *is_key_frame = result.is_keyframe.value_or(false);
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
}

}  // namespace

struct VideoDecoder::DecoderSpecificData {
  void Reset() {
    decoder_helper.reset();
    av1_buffer_pool.reset();
  }

  // Bitstream converter to annex B for AVC/HEVC.
  std::unique_ptr<VideoDecoderHelper> decoder_helper;

  // Buffer pool for use with libgav1::ObuParser.
  std::unique_ptr<libgav1::BufferPool> av1_buffer_pool;
};

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
ScriptPromise<VideoDecoderSupport> VideoDecoder::isConfigSupported(
    ScriptState* script_state,
    const VideoDecoderConfig* config,
    ExceptionState& exception_state) {
  // Run the "check if a config is a valid VideoDecoderConfig" algorithm.
  String js_error_message;
  std::optional<media::VideoType> video_type =
      IsValidVideoDecoderConfig(*config, &js_error_message /* out */);
  if (!video_type) {
    exception_state.ThrowTypeError(js_error_message);
    return EmptyPromise();
  }

  // Run the "Clone Configuration" algorithm.
  auto* config_copy = CopyConfig(*config);

  // Run the "Check Configuration Support" algorithm.
  HardwarePreference hw_pref = GetHardwareAccelerationPreference(*config_copy);
  VideoDecoderSupport* support = VideoDecoderSupport::Create();
  support->setConfig(config_copy);

  if ((hw_pref == HardwarePreference::kPreferSoftware &&
       !media::IsBuiltInVideoCodec(video_type->codec)) ||
      !media::IsSupportedVideoType(*video_type)) {
    support->setSupported(false);
    return ToResolvedPromise<VideoDecoderSupport>(script_state, support);
  }

  // Check that we can make a media::VideoDecoderConfig. The |js_error_message|
  // is ignored, we report only via |support.supported|.
  std::optional<MediaConfigType> media_config;
  media_config = MakeMediaVideoDecoderConfig(*config_copy, &js_error_message);
  if (!media_config) {
    support->setSupported(false);
    return ToResolvedPromise<VideoDecoderSupport>(script_state, support);
  }

  // If hardware is preferred, asynchronously check for a hardware decoder.
  if (hw_pref == HardwarePreference::kPreferHardware) {
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<VideoDecoderSupport>>(
            script_state, exception_state.GetContext());
    auto promise = resolver->Promise();
    RetrieveGpuFactoriesWithKnownDecoderSupport(CrossThreadBindOnce(
        &DecoderSupport_OnKnown, MakeUnwrappingCrossThreadHandle(support),
        std::make_unique<MediaConfigType>(*media_config),
        MakeUnwrappingCrossThreadHandle(resolver)));
    return promise;
  }

  // Otherwise, the config is supported.
  support->setSupported(true);
  return ToResolvedPromise<VideoDecoderSupport>(script_state, support);
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
std::optional<media::VideoType> VideoDecoder::IsValidVideoDecoderConfig(
    const VideoDecoderConfig& config,
    String* js_error_message) {
  media::VideoType video_type;
  if (!ParseCodecString(config.codec(), video_type, *js_error_message))
    return std::nullopt;

  if (config.hasDescription()) {
    auto desc_wrapper = AsSpan<const uint8_t>(config.description());
    if (!desc_wrapper.data()) {
      *js_error_message = "Invalid config, description is detached.";
      return std::nullopt;
    }
  }

  if (config.hasCodedWidth() || config.hasCodedHeight()) {
    if (!config.hasCodedWidth()) {
      *js_error_message =
          "Invalid config, codedHeight specified without codedWidth.";
      return std::nullopt;
    }
    if (!config.hasCodedHeight()) {
      *js_error_message =
          "Invalid config, codedWidth specified without codedHeight.";
      return std::nullopt;
    }

    const uint32_t coded_width = config.codedWidth();
    const uint32_t coded_height = config.codedHeight();
    if (!coded_width || !coded_height) {
      *js_error_message = String::Format("Invalid coded size (%u, %u).",
                                         coded_width, coded_height);
      return std::nullopt;
    }
  }

  if (config.hasDisplayAspectWidth() || config.hasDisplayAspectHeight()) {
    if (!config.hasDisplayAspectWidth()) {
      *js_error_message =
          "Invalid config, displayAspectHeight specified without "
          "displayAspectWidth.";
      return std::nullopt;
    }
    if (!config.hasDisplayAspectHeight()) {
      *js_error_message =
          "Invalid config, displayAspectWidth specified without "
          "displayAspectHeight.";
      return std::nullopt;
    }

    uint32_t display_aspect_width = config.displayAspectWidth();
    uint32_t display_aspect_height = config.displayAspectHeight();
    if (display_aspect_width == 0 || display_aspect_height == 0) {
      *js_error_message =
          String::Format("Invalid display aspect (%u, %u).",
                         display_aspect_width, display_aspect_height);
      return std::nullopt;
    }
  }

  return video_type;
}

// static
std::optional<media::VideoDecoderConfig>
VideoDecoder::MakeMediaVideoDecoderConfig(const ConfigType& config,
                                          String* js_error_message,
                                          bool* needs_converter_out) {
  std::unique_ptr<VideoDecoderHelper> decoder_helper;
  VideoDecoder::DecoderSpecificData decoder_specific_data;
  return MakeMediaVideoDecoderConfigInternal(
      config, decoder_specific_data, js_error_message, needs_converter_out);
}

// static
std::optional<media::VideoDecoderConfig>
VideoDecoder::MakeMediaVideoDecoderConfigInternal(
    const ConfigType& config,
    DecoderSpecificData& decoder_specific_data,
    String* js_error_message,
    bool* needs_converter_out) {
  decoder_specific_data.Reset();
  media::VideoType video_type;
  if (!ParseCodecString(config.codec(), video_type, *js_error_message)) {
    // Checked by IsValidVideoDecoderConfig().
    NOTREACHED_IN_MIGRATION();
    return std::nullopt;
  }
  if (video_type.codec == media::VideoCodec::kUnknown) {
    return std::nullopt;
  }

  std::vector<uint8_t> extra_data;
  if (config.hasDescription()) {
    auto desc_wrapper = AsSpan<const uint8_t>(config.description());
    if (!desc_wrapper.data()) {
      // Checked by IsValidVideoDecoderConfig().
      NOTREACHED_IN_MIGRATION();
      return std::nullopt;
    }
    if (!desc_wrapper.empty()) {
      const uint8_t* start = desc_wrapper.data();
      const size_t size = desc_wrapper.size();
      extra_data.assign(start, start + size);
    }
  }
  if (needs_converter_out) {
    *needs_converter_out = (extra_data.size() > 0);
  }

  if ((extra_data.size() > 0) &&
      (video_type.codec == media::VideoCodec::kH264 ||
       video_type.codec == media::VideoCodec::kHEVC)) {
    VideoDecoderHelper::Status status;
    decoder_specific_data.decoder_helper = VideoDecoderHelper::Create(
        video_type, extra_data.data(), static_cast<int>(extra_data.size()),
        &status);
    if (status != VideoDecoderHelper::Status::kSucceed) {
      if (video_type.codec == media::VideoCodec::kH264) {
        if (status == VideoDecoderHelper::Status::kDescriptionParseFailed) {
          *js_error_message = "Failed to parse avcC.";
        } else if (status == VideoDecoderHelper::Status::kUnsupportedCodec) {
          *js_error_message = "H.264 decoding is not supported.";
        }
      } else if (video_type.codec == media::VideoCodec::kHEVC) {
        if (status == VideoDecoderHelper::Status::kDescriptionParseFailed) {
          *js_error_message = "Failed to parse hvcC.";
        } else if (status == VideoDecoderHelper::Status::kUnsupportedCodec) {
          *js_error_message = "HEVC decoding is not supported.";
        }
      }
      return std::nullopt;
    }
    // The description should not be provided to the decoder because the stream
    // will be converted to Annex B format.
    extra_data.clear();
  }

  if (video_type.codec == media::VideoCodec::kAV1) {
    decoder_specific_data.av1_buffer_pool =
        std::make_unique<libgav1::BufferPool>(
            /*on_frame_buffer_size_changed=*/nullptr,
            /*get_frame_buffer=*/nullptr,
            /*release_frame_buffer=*/nullptr,
            /*callback_private_data=*/nullptr);
  }

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
  media::VideoColorSpace media_color_space = video_type.color_space;
  if (config.hasColorSpace()) {
    VideoColorSpace* color_space =
        MakeGarbageCollected<VideoColorSpace>(config.colorSpace());
    media_color_space = color_space->ToMediaColorSpace();
  }

  auto encryption_scheme = media::EncryptionScheme::kUnencrypted;
  if (config.hasEncryptionScheme()) {
    auto scheme = ToMediaEncryptionScheme(config.encryptionScheme());
    if (!scheme) {
      *js_error_message = "Unsupported encryption scheme";
      return std::nullopt;
    }
    encryption_scheme = scheme.value();
  }

  media::VideoDecoderConfig media_config;
  media_config.Initialize(video_type.codec, video_type.profile,
                          media::VideoDecoderConfig::AlphaMode::kIsOpaque,
                          media_color_space, media::kNoTransformation,
                          coded_size, visible_rect, natural_size, extra_data,
                          encryption_scheme);
  media_config.set_aspect_ratio(aspect_ratio);
  if (!media_config.IsValidConfig()) {
    *js_error_message = "Unsupported config.";
    return std::nullopt;
  }

  return media_config;
}

VideoDecoder::VideoDecoder(ScriptState* script_state,
                           const VideoDecoderInit* init,
                           ExceptionState& exception_state)
    : DecoderTemplate<VideoDecoderTraits>(script_state, init, exception_state),
      decoder_specific_data_(std::make_unique<DecoderSpecificData>()) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);
}

VideoDecoder::~VideoDecoder() = default;

bool VideoDecoder::IsValidConfig(const ConfigType& config,
                                 String* js_error_message) {
  return IsValidVideoDecoderConfig(config, js_error_message /* out */)
      .has_value();
}

std::optional<media::VideoDecoderConfig> VideoDecoder::MakeMediaConfig(
    const ConfigType& config,
    String* js_error_message) {
  DCHECK(js_error_message);
  std::optional<media::VideoDecoderConfig> media_config =
      MakeMediaVideoDecoderConfigInternal(
          config, *decoder_specific_data_.get() /* out */,
          js_error_message /* out */);
  if (media_config)
    current_codec_ = media_config->codec();
  return media_config;
}

media::DecoderStatus::Or<scoped_refptr<media::DecoderBuffer>>
VideoDecoder::MakeInput(const InputType& chunk, bool verify_key_frame) {
  scoped_refptr<media::DecoderBuffer> decoder_buffer = chunk.buffer();
  if (decoder_specific_data_->decoder_helper) {
    const uint8_t* src = chunk.buffer()->data();
    size_t src_size = chunk.buffer()->size();

    // Note: this may not be safe if support for SharedArrayBuffers is added.
    uint32_t output_size =
        decoder_specific_data_->decoder_helper->CalculateNeededOutputBufferSize(
            src, static_cast<uint32_t>(src_size), verify_key_frame);
    if (!output_size) {
      return media::DecoderStatus(
          media::DecoderStatus::Codes::kMalformedBitstream,
          "Unable to determine size of bitstream buffer.");
    }

    std::vector<uint8_t> buf(output_size);
    if (decoder_specific_data_->decoder_helper
            ->ConvertNalUnitStreamToByteStream(
                src, static_cast<uint32_t>(src_size), buf.data(), &output_size,
                verify_key_frame) != VideoDecoderHelper::Status::kSucceed) {
      return media::DecoderStatus(
          media::DecoderStatus::Codes::kMalformedBitstream,
          "Unable to convert NALU to byte stream.");
    }

    decoder_buffer =
        media::DecoderBuffer::CopyFrom(base::span(buf).first(output_size));
    decoder_buffer->set_timestamp(chunk.buffer()->timestamp());
    decoder_buffer->set_duration(chunk.buffer()->duration());
  }

  bool is_key_frame = chunk.type() == "key";
  if (verify_key_frame) {
    if (current_codec_ == media::VideoCodec::kVP9 ||
        current_codec_ == media::VideoCodec::kVP8) {
      ParseVpxKeyFrame(*decoder_buffer, current_codec_, &is_key_frame);
    } else if (current_codec_ == media::VideoCodec::kAV1) {
      ParseAv1KeyFrame(*decoder_buffer,
                       decoder_specific_data_->av1_buffer_pool.get(),
                       &is_key_frame);
    } else if (current_codec_ == media::VideoCodec::kH264) {
      ParseH264KeyFrame(*decoder_buffer, &is_key_frame);

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      // Use a more helpful error message if we think the user may have forgot
      // to provide a description for AVC H.264. We could try to guess at the
      // NAL unit size and see if a NAL unit parses out, but this seems fine.
      if (!is_key_frame && !decoder_specific_data_->decoder_helper) {
        return media::DecoderStatus(
            media::DecoderStatus::Codes::kKeyFrameRequired,
            "A key frame is required after configure() or flush(). If you're "
            "using AVC formatted H.264 you must fill out the description field "
            "in the VideoDecoderConfig.");
      }
#endif
    } else if (current_codec_ == media::VideoCodec::kHEVC) {
      ParseH265KeyFrame(*decoder_buffer, &is_key_frame);

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
      if (!is_key_frame && !decoder_specific_data_->decoder_helper) {
        return media::DecoderStatus(
            media::DecoderStatus::Codes::kKeyFrameRequired,
            "A key frame is required after configure() or flush(). If you're "
            "using HEVC formatted H.265 you must fill out the description "
            "field in the VideoDecoderConfig.");
      }
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
    }

    if (!is_key_frame) {
      return media::DecoderStatus(
          media::DecoderStatus::Codes::kKeyFrameRequired,
          "A key frame is required after configure() or flush().");
    }
  }

  chunk_metadata_[chunk.buffer()->timestamp()] =
      ChunkMetadata{chunk.buffer()->duration()};

  return decoder_buffer;
}

media::DecoderStatus::Or<VideoDecoder::OutputType*> VideoDecoder::MakeOutput(
    scoped_refptr<MediaOutputType> output,
    ExecutionContext* context) {
  const auto it = chunk_metadata_.find(output->timestamp());
  if (it != chunk_metadata_.end()) {
    const auto duration = it->second.duration;
    if (!duration.is_zero() && duration != media::kNoTimestamp) {
      auto wrapped_output = media::VideoFrame::WrapVideoFrame(
          output, output->format(), output->visible_rect(),
          output->natural_size());
      wrapped_output->set_color_space(output->ColorSpace());
      wrapped_output->metadata().frame_duration = duration;
      output = wrapped_output;
    }

    // We erase from the beginning onward to our target frame since frames
    // should be returned in presentation order.
    chunk_metadata_.erase(chunk_metadata_.begin(), it + 1);
  }
  return MakeGarbageCollected<OutputType>(std::move(output), context);
}

const AtomicString& VideoDecoder::InterfaceName() const {
  return event_target_names::kVideoDecoder;
}

}  // namespace blink
