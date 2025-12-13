// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities.h"

#include <memory>
#include <optional>
#include <sstream>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/mime_util.h"
#include "media/base/supported_types.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/stream_parser_factory.h"
#include "media/mojo/mojom/media_metrics_provider.mojom-blink.h"
#include "media/mojo/mojom/media_types.mojom-blink.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_encrypted_media_client.h"
#include "third_party/blink/public/platform/web_encrypted_media_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_key_system_track_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_capabilities_decoding_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_capabilities_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_capabilities_key_system_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_decoding_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_encoding_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_key_system_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_key_system_media_capability.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/encryptedmedia/encrypted_media_utils.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_system_access.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_system_access_initializer_base.h"
#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_handler.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/media_capabilities/web_media_capabilities_info.h"
#include "third_party/blink/renderer/platform/media_capabilities/web_media_configuration.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_decoding_info_handler.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_encoding_info_handler.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_utils.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/audio_codecs/audio_format.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

const bool kWebrtcDecodeSmoothIfPowerEfficientDefault = true;
const bool kWebrtcEncodeSmoothIfPowerEfficientDefault = true;

constexpr const char* kApplicationMimeTypePrefix = "application/";
constexpr const char* kAudioMimeTypePrefix = "audio/";
constexpr const char* kVideoMimeTypePrefix = "video/";
constexpr const char* kCodecsMimeTypeParam = "codecs";

// static
bool WebrtcDecodeForceSmoothIfPowerEfficient() {
  return base::GetFieldTrialParamByFeatureAsBool(
      media::kWebrtcMediaCapabilitiesParameters,
      MediaCapabilities::kWebrtcDecodeSmoothIfPowerEfficientParamName,
      kWebrtcDecodeSmoothIfPowerEfficientDefault);
}

// static
bool WebrtcEncodeForceSmoothIfPowerEfficient() {
  return base::GetFieldTrialParamByFeatureAsBool(
      media::kWebrtcMediaCapabilitiesParameters,
      MediaCapabilities::kWebrtcEncodeSmoothIfPowerEfficientParamName,
      kWebrtcEncodeSmoothIfPowerEfficientDefault);
}

// static
bool UseGpuFactoriesForPowerEfficient(
    ExecutionContext* execution_context,
    const MediaKeySystemAccess* key_system_access) {
  // TODO(1105258): GpuFactories isn't available in worker scope yet.
  if (!execution_context || execution_context->IsWorkerGlobalScope())
    return false;

  // TODO(1105258): Decoding w/ EME often means we can't use the GPU accelerated
  // path. Add additional logic to detect when GPU acceleration is really
  // available.
  if (key_system_access)
    return false;

  return base::FeatureList::IsEnabled(
      media::kMediaCapabilitiesQueryGpuFactories);
}

// Utility function that will create a MediaCapabilitiesDecodingInfo object with
// all the values set to either true or false.
MediaCapabilitiesDecodingInfo* CreateDecodingInfoWith(bool value) {
  MediaCapabilitiesDecodingInfo* info = MediaCapabilitiesDecodingInfo::Create();
  info->setSupported(value);
  info->setSmooth(value);
  info->setPowerEfficient(value);
  return info;
}

// Utility function that will create a MediaCapabilitiesInfo object with
// all the values set to either true or false.
MediaCapabilitiesInfo* CreateEncodingInfoWith(bool value) {
  MediaCapabilitiesInfo* info = MediaCapabilitiesInfo::Create();
  info->setSupported(value);
  info->setSmooth(value);
  info->setPowerEfficient(value);
  return info;
}

ScriptPromise<MediaCapabilitiesDecodingInfo>
CreateResolvedPromiseToDecodingInfoWith(
    bool value,
    ScriptState* script_state,
    const MediaDecodingConfiguration* config) {
  MediaCapabilitiesDecodingInfo* info = CreateDecodingInfoWith(value);
  return ToResolvedPromise<MediaCapabilitiesDecodingInfo>(script_state, info);
}

MediaCapabilitiesDecodingInfo* CreateEncryptedDecodingInfoWith(
    bool value,
    MediaKeySystemAccess* access) {
  MediaCapabilitiesDecodingInfo* info = CreateDecodingInfoWith(value);
  info->setKeySystemAccess(access);
  return info;
}

class MediaCapabilitiesKeySystemAccessInitializer final
    : public MediaKeySystemAccessInitializerBase {
 public:
  using GetPerfCallback = base::OnceCallback<void(
      ScriptPromiseResolver<MediaCapabilitiesDecodingInfo>*,
      MediaKeySystemAccess*)>;

  MediaCapabilitiesKeySystemAccessInitializer(
      ExecutionContext* context,
      ScriptPromiseResolverBase* resolver,
      const String& key_system,
      const HeapVector<Member<MediaKeySystemConfiguration>>&
          supported_configurations,
      GetPerfCallback get_perf_callback)
      : MediaKeySystemAccessInitializerBase(
            context,
            resolver,
            key_system,
            supported_configurations,
            /*is_from_media_capabilities=*/true),
        get_perf_callback_(std::move(get_perf_callback)) {}

  MediaCapabilitiesKeySystemAccessInitializer(
      const MediaCapabilitiesKeySystemAccessInitializer&) = delete;
  MediaCapabilitiesKeySystemAccessInitializer& operator=(
      const MediaCapabilitiesKeySystemAccessInitializer&) = delete;

  ~MediaCapabilitiesKeySystemAccessInitializer() override = default;

  void RequestSucceeded(
      std::unique_ptr<WebContentDecryptionModuleAccess> access) override {
    DVLOG(3) << __func__;

    if (!IsExecutionContextValid())
      return;

    // Query the client for smoothness and power efficiency of the video. It
    // will resolve the promise.
    std::move(get_perf_callback_)
        .Run(resolver_->DowncastTo<MediaCapabilitiesDecodingInfo>(),
             MakeGarbageCollected<MediaKeySystemAccess>(std::move(access)));
  }

  void RequestNotSupported(const WebString& error_message) override {
    DVLOG(3) << __func__ << " error: " << error_message.Ascii();

    if (!IsExecutionContextValid())
      return;

    MediaCapabilitiesDecodingInfo* info =
        CreateEncryptedDecodingInfoWith(false, nullptr);

    resolver_->DowncastTo<MediaCapabilitiesDecodingInfo>()->Resolve(info);
  }

  void Trace(Visitor* visitor) const override {
    MediaKeySystemAccessInitializerBase::Trace(visitor);
  }

 private:
  GetPerfCallback get_perf_callback_;
};

bool IsValidFrameRate(double framerate) {
  return std::isfinite(framerate) && framerate > 0;
}

bool IsValidMimeType(const String& content_type,
                     const String& prefix,
                     bool is_webrtc) {
  ParsedContentType parsed_content_type(content_type);

  if (!parsed_content_type.IsValid())
    return false;

  // Valid ParsedContentType implies we have a mime type.
  DCHECK(parsed_content_type.MimeType());
  if (!parsed_content_type.MimeType().StartsWith(prefix) &&
      (is_webrtc ||
       !parsed_content_type.MimeType().StartsWith(kApplicationMimeTypePrefix)))
    return false;

  // No requirement on parameters for RTP MIME types.
  if (is_webrtc)
    return true;

  const auto& parameters = parsed_content_type.GetParameters();

  if (parameters.ParameterCount() > 1)
    return false;

  if (parameters.ParameterCount() == 0)
    return true;

  return EqualIgnoringASCIICase(parameters.begin()->name, kCodecsMimeTypeParam);
}

bool IsValidMediaConfiguration(const MediaConfiguration* configuration) {
  return configuration->hasAudio() || configuration->hasVideo();
}

bool IsValidVideoConfiguration(const VideoConfiguration* configuration,
                               bool is_decode,
                               bool is_webrtc) {
  DCHECK(configuration->hasContentType());
  if (!IsValidMimeType(configuration->contentType(), kVideoMimeTypePrefix,
                       is_webrtc))
    return false;

  DCHECK(configuration->hasFramerate());
  if (!IsValidFrameRate(configuration->framerate()))
    return false;

  // scalabilityMode only valid for WebRTC encode configuration.
  if ((!is_webrtc || is_decode) && configuration->hasScalabilityMode())
    return false;
  // spatialScalability only valid for WebRTC decode configuration.
  if ((!is_webrtc || !is_decode) && configuration->hasSpatialScalability())
    return false;

  return true;
}

bool IsValidAudioConfiguration(const AudioConfiguration* configuration,
                               bool is_webrtc) {
  DCHECK(configuration->hasContentType());

  if (!IsValidMimeType(configuration->contentType(), kAudioMimeTypePrefix,
                       is_webrtc))
    return false;

  return true;
}

bool IsValidMediaDecodingConfiguration(
    const MediaDecodingConfiguration* configuration,
    bool is_webrtc,
    String* message) {
  if (!IsValidMediaConfiguration(configuration)) {
    *message =
        "The configuration dictionary has neither |video| nor |audio| "
        "specified and needs at least one of them.";
    return false;
  }

  if (configuration->hasKeySystemConfiguration()) {
    if (is_webrtc) {
      *message =
          "The keySystemConfiguration object cannot be set for webrtc "
          "MediaDecodingType.";
      return false;
    }

    if (configuration->keySystemConfiguration()->hasAudio() &&
        !configuration->hasAudio()) {
      *message =
          "The keySystemConfiguration object contains an audio property but "
          "the root configuration has no audio configuration.";
      return false;
    }

    if (configuration->keySystemConfiguration()->hasVideo() &&
        !configuration->hasVideo()) {
      *message =
          "The keySystemConfiguration object contains a video property but the "
          "root configuration has no video configuration.";
      return false;
    }
  }

  if (configuration->hasVideo() &&
      !IsValidVideoConfiguration(configuration->video(), /*is_decode=*/true,
                                 is_webrtc)) {
    *message = "The video configuration dictionary is not valid.";
    return false;
  }

  if (configuration->hasAudio() &&
      !IsValidAudioConfiguration(configuration->audio(), is_webrtc)) {
    *message = "The audio configuration dictionary is not valid.";
    return false;
  }

  return true;
}

bool IsValidMediaEncodingConfiguration(
    const MediaEncodingConfiguration* configuration,
    bool is_webrtc,
    String* message) {
  if (!IsValidMediaConfiguration(configuration)) {
    *message =
        "The configuration dictionary has neither |video| nor |audio| "
        "specified and needs at least one of them.";
    return false;
  }

  if (configuration->hasVideo() &&
      !IsValidVideoConfiguration(configuration->video(), /*is_decode=*/false,
                                 is_webrtc)) {
    *message = "The video configuration dictionary is not valid.";
    return false;
  }

  if (configuration->hasAudio() &&
      !IsValidAudioConfiguration(configuration->audio(), is_webrtc)) {
    *message = "The audio configuration dictionary is not valid.";
    return false;
  }

  return true;
}

WebAudioConfiguration ToWebAudioConfiguration(
    const AudioConfiguration* configuration) {
  WebAudioConfiguration web_configuration;

  // |contentType| is mandatory.
  DCHECK(configuration->hasContentType());
  ParsedContentType parsed_content_type(configuration->contentType());
  DCHECK(parsed_content_type.IsValid());
  DCHECK(!parsed_content_type.GetParameters().HasDuplicatedNames());

  web_configuration.mime_type = parsed_content_type.MimeType().LowerASCII();
  web_configuration.codec =
      parsed_content_type.ParameterValueForName(kCodecsMimeTypeParam);

  // |channels| is optional and will be set to a null WebString if not present.
  web_configuration.channels = configuration->hasChannels()
                                   ? WebString(configuration->channels())
                                   : WebString();

  if (configuration->hasBitrate())
    web_configuration.bitrate = configuration->bitrate();

  if (configuration->hasSamplerate())
    web_configuration.samplerate = configuration->samplerate();

  return web_configuration;
}

WebVideoConfiguration ToWebVideoConfiguration(
    const VideoConfiguration* configuration) {
  WebVideoConfiguration web_configuration;

  // All the properties are mandatory.
  DCHECK(configuration->hasContentType());
  ParsedContentType parsed_content_type(configuration->contentType());
  DCHECK(parsed_content_type.IsValid());
  DCHECK(!parsed_content_type.GetParameters().HasDuplicatedNames());
  web_configuration.mime_type = parsed_content_type.MimeType().LowerASCII();
  web_configuration.codec =
      parsed_content_type.ParameterValueForName(kCodecsMimeTypeParam);

  DCHECK(configuration->hasWidth());
  web_configuration.width = configuration->width();

  DCHECK(configuration->hasHeight());
  web_configuration.height = configuration->height();

  DCHECK(configuration->hasBitrate());
  web_configuration.bitrate = configuration->bitrate();

  DCHECK(configuration->hasFramerate());
  web_configuration.framerate = configuration->framerate();

  return web_configuration;
}

WebMediaConfiguration ToWebMediaConfiguration(
    const MediaEncodingConfiguration* configuration) {
  WebMediaConfiguration web_configuration;

  // |type| is required.
  DCHECK(configuration->hasType());
  switch (configuration->type().AsEnum()) {
    case V8MediaEncodingType::Enum::kRecord:
      web_configuration.type = MediaConfigurationType::kRecord;
      break;
    case V8MediaEncodingType::Enum::kWebrtc:
      NOTREACHED();
  }

  if (configuration->hasAudio()) {
    web_configuration.audio_configuration =
        ToWebAudioConfiguration(configuration->audio());
  }

  if (configuration->hasVideo()) {
    web_configuration.video_configuration =
        ToWebVideoConfiguration(configuration->video());
  }

  return web_configuration;
}

webrtc::SdpAudioFormat ToSdpAudioFormat(
    const AudioConfiguration* configuration) {
  DCHECK(configuration->hasContentType());
  // Convert audio_configuration to SdpAudioFormat.
  ParsedContentType parsed_content_type(configuration->contentType());
  DCHECK(parsed_content_type.IsValid());
  const String codec_name =
      WebrtcCodecNameFromMimeType(parsed_content_type.MimeType(), "audio");
  // TODO(https://crbug.com/1187565): Deal with the special case where the clock
  // rate is not the same as the sample rate.
  const int clockrate_hz =
      configuration->hasSamplerate() ? configuration->samplerate() : 0;
  const size_t channels = configuration->hasChannels()
                              ? configuration->channels().ToUIntStrict()
                              : 0;
  return {codec_name.Utf8(), clockrate_hz, channels};
}

webrtc::SdpVideoFormat ToSdpVideoFormat(
    const VideoConfiguration* configuration) {
  DCHECK(configuration->hasContentType());
  // Convert video_configuration to SdpVideoFormat.
  ParsedContentType parsed_content_type(configuration->contentType());
  DCHECK(parsed_content_type.IsValid());
  const String codec_name =
      WebrtcCodecNameFromMimeType(parsed_content_type.MimeType(), "video");
  const std::map<std::string, std::string> parameters =
      ConvertToSdpVideoFormatParameters(parsed_content_type.GetParameters());
  return {codec_name.Utf8(), parameters};
}

bool CheckMseSupport(const String& mime_type, const String& codec) {
  // For MSE queries, we assume the queried audio and video streams will be
  // placed into separate source buffers.
  // TODO(chcunningham): Clarify this assumption in the spec.

  // Media MIME API expects a vector of codec strings. We query audio and video
  // separately, so |codec_string|.size() should always be 1 or 0 (when no
  // codecs parameter is required for the given mime type).
  base::span<const std::string> codecs;

  const std::string codec_ascii = codec.Ascii();
  if (!codec.Ascii().empty())
    codecs = base::span_from_ref(codec_ascii);

  if (media::SupportsType::kSupported !=
      media::StreamParserFactory::IsTypeSupported(mime_type.Ascii(), codecs)) {
    DVLOG(2) << __func__
             << " MSE does not support the content type: " << mime_type.Ascii()
             << " " << (codecs.empty() ? "" : codecs.front());
    return false;
  }

  return true;
}

void ParseDynamicRangeConfigurations(
    const blink::VideoConfiguration* video_config,
    media::VideoColorSpace* color_space,
    gfx::HdrMetadataType* hdr_metadata) {
  DCHECK(color_space);
  DCHECK(hdr_metadata);

  // TODO(1066628): Follow up on MediaCapabilities spec regarding reconciling
  // discrepancies between mime type and colorGamut/transferFunction; for now,
  // give precedence to the latter.

  if (video_config->hasHdrMetadataType()) {
    switch (video_config->hdrMetadataType().AsEnum()) {
      case V8HdrMetadataType::Enum::kSmpteSt2086:
        *hdr_metadata = gfx::HdrMetadataType::kSmpteSt2086;
        break;
      case V8HdrMetadataType::Enum::kSmpteSt209410:
        *hdr_metadata = gfx::HdrMetadataType::kSmpteSt2094_10;
        break;
      case V8HdrMetadataType::Enum::kSmpteSt209440:
        *hdr_metadata = gfx::HdrMetadataType::kSmpteSt2094_40;
        break;
    }
  } else {
    *hdr_metadata = gfx::HdrMetadataType::kNone;
  }

  if (video_config->hasColorGamut()) {
    switch (video_config->colorGamut().AsEnum()) {
      case V8ColorGamut::Enum::kSRGB:
        color_space->primaries = media::VideoColorSpace::PrimaryID::BT709;
        break;
      case V8ColorGamut::Enum::kP3:
        color_space->primaries =
            media::VideoColorSpace::PrimaryID::SMPTEST431_2;
        break;
      case V8ColorGamut::Enum::kRec2020:
        color_space->primaries = media::VideoColorSpace::PrimaryID::BT2020;
        break;
    }
  }

  if (video_config->hasTransferFunction()) {
    switch (video_config->transferFunction().AsEnum()) {
      case V8TransferFunction::Enum::kSRGB:
        color_space->transfer = media::VideoColorSpace::TransferID::BT709;
        break;
      case V8TransferFunction::Enum::kPq:
        color_space->transfer = media::VideoColorSpace::TransferID::SMPTEST2084;
        break;
      case V8TransferFunction::Enum::kHlg:
        color_space->transfer =
            media::VideoColorSpace::TransferID::ARIB_STD_B67;
        break;
    }
  }
}

// Returns whether the audio codec associated with the audio configuration is
// valid and non-ambiguous.
// |console_warning| is an out param containing a message to be printed in the
//                   console.
bool IsAudioCodecValid(const String& mime_type,
                       const String& codec,
                       String* console_warning) {
  std::optional<media::AudioType> result =
      media::ParseAudioCodecString(mime_type.Ascii(), codec.Ascii(),
                                   /*allow_ambiguous_matches=*/false);
  if (result) {
    return true;
  }

  if (media::ParseAudioCodecString(mime_type.Ascii(), codec.Ascii(),
                                   /*allow_ambiguous_matches=*/true)) {
    *console_warning =
        StrCat({"Invalid (ambiguous) audio codec string: ", mime_type,
                "; codecs=", codec});
    return false;
  }

  *console_warning = StrCat(
      {"Failed to parse audio contentType: ", mime_type, "; codecs=", codec});
  return false;
}

// Returns whether the video codec associated with the video configuration is
// valid and non-ambiguous.
// |out_video_profile| is an out param containing the video codec profile if the
//                     codec is valid.
// |console_warning| is an out param containing a message to be printed in the
//                   console.
bool IsVideoCodecValid(const String& mime_type,
                       const String& codec,
                       media::VideoCodec* out_video_codec,
                       media::VideoCodecProfile* out_video_profile,
                       String* console_warning) {
  std::optional<media::VideoType> result =
      media::ParseVideoCodecString(mime_type.Ascii(), codec.Ascii(),
                                   /*allow_ambiguous_matches=*/false);
  if (result) {
    *out_video_codec = result->codec;
    *out_video_profile = result->profile;
    return true;
  }

  if (media::ParseVideoCodecString(mime_type.Ascii(), codec.Ascii(),
                                   /*allow_ambiguous_matches=*/true)) {
    *console_warning =
        StrCat({"Invalid (ambiguous) video codec string: ", mime_type,
                "; codecs=", codec});
    return false;
  }

  *console_warning = StrCat(
      {"Failed to parse video contentType: ", mime_type, "; codecs=", codec});
  return false;
}

// Returns whether the AudioConfiguration is supported.
// IsAudioCodecValid() MUST be called before.
bool IsAudioConfigurationSupported(
    const blink::AudioConfiguration* audio_config,
    const String& mime_type,
    const String& codec) {
  // Must succeed as IsAudioCodecValid() should have been called before.
  std::optional<media::AudioType> audio_type =
      media::ParseAudioCodecString(mime_type.Ascii(), codec.Ascii(),
                                   /*allow_ambiguous_matches=*/false);
  DCHECK(audio_type);

  if (audio_config->hasSpatialRendering())
    audio_type->spatial_rendering = audio_config->spatialRendering();

  return media::IsDecoderSupportedAudioType(*audio_type);
}

// Returns whether the VideoConfiguration is supported.
// IsVideoCodecValid() MUST be called before.
bool IsVideoConfigurationSupported(const String& mime_type,
                                   const String& codec,
                                   media::VideoColorSpace video_color_space,
                                   gfx::HdrMetadataType hdr_metadata_type) {
  // Must succeed as IsVideoCodecValid() should have been called before.
  std::optional<media::VideoType> result =
      media::ParseVideoCodecString(mime_type.Ascii(), codec.Ascii(),
                                   /*allow_ambiguous_matches=*/false);
  DCHECK(result);

  // ParseVideoCodecString will fill in a default of REC709 for every codec, but
  // only some codecs actually have color space information that we can use
  // to validate against provided colorGamut and transferFunction fields.
  const bool codec_string_has_non_default_color_space =
      result->color_space.IsSpecified() &&
      (result->codec == media::VideoCodec::kVP9 ||
       result->codec == media::VideoCodec::kAV1);

  if (video_color_space.IsSpecified() &&
      codec_string_has_non_default_color_space) {
    // Per spec, report unsupported if color space information is mismatched.
    if (video_color_space.transfer != result->color_space.transfer ||
        video_color_space.primaries != result->color_space.primaries) {
      DLOG(ERROR) << "Mismatched color spaces between config and codec string.";
      return false;
    }
    // Prefer color space from codec string since it'll be more specified.
    video_color_space = result->color_space;
  } else if (video_color_space.IsSpecified()) {
    // Prefer color space from the config.
  } else {
    // There's no color space in the config and only a default one from codec.
    video_color_space = result->color_space;
  }

  return media::IsDecoderSupportedVideoType({result->codec, result->profile,
                                             result->level, video_color_space,
                                             hdr_metadata_type});
}

void OnMediaCapabilitiesEncodingInfo(
    ScriptPromiseResolver<MediaCapabilitiesInfo>* resolver,
    std::unique_ptr<WebMediaCapabilitiesInfo> result) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  auto* info = MediaCapabilitiesInfo::Create();
  info->setSupported(result->supported);
  info->setSmooth(result->smooth);
  info->setPowerEfficient(result->power_efficient);

  resolver->Resolve(std::move(info));
}

bool ParseContentType(const String& content_type,
                      String* mime_type,
                      String* codec) {
  DCHECK(mime_type);
  DCHECK(codec);

  ParsedContentType parsed_content_type(content_type);
  if (!parsed_content_type.IsValid() ||
      parsed_content_type.GetParameters().HasDuplicatedNames()) {
    return false;
  }

  *mime_type = parsed_content_type.MimeType().LowerASCII();
  *codec = parsed_content_type.ParameterValueForName(kCodecsMimeTypeParam);
  return true;
}

#if BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)
bool IsDolbyVisionVideoCodec(const String& video_codec_str) {
  return video_codec_str.StartsWith("dvh1.", kTextCaseSensitive) ||
         video_codec_str.StartsWith("dvhe.", kTextCaseSensitive);
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)

}  // anonymous namespace

const char MediaCapabilities::kWebrtcDecodeSmoothIfPowerEfficientParamName[] =
    "webrtc_decode_smooth_if_power_efficient";

const char MediaCapabilities::kWebrtcEncodeSmoothIfPowerEfficientParamName[] =
    "webrtc_encode_smooth_if_power_efficient";

// static
const char MediaCapabilities::kSupplementName[] = "MediaCapabilities";

MediaCapabilities* MediaCapabilities::mediaCapabilities(
    NavigatorBase& navigator) {
  MediaCapabilities* supplement =
      Supplement<NavigatorBase>::From<MediaCapabilities>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<MediaCapabilities>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement;
}

MediaCapabilities::MediaCapabilities(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator),
      decode_history_service_(navigator.GetExecutionContext()),
      webrtc_history_service_(navigator.GetExecutionContext()) {}

void MediaCapabilities::Trace(blink::Visitor* visitor) const {
  visitor->Trace(decode_history_service_);
  visitor->Trace(webrtc_history_service_);
  visitor->Trace(pending_cb_map_);
  ScriptWrappable::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
}

MediaCapabilities::PendingCallbackState::PendingCallbackState(
    ScriptPromiseResolverBase* resolver,
    MediaKeySystemAccess* access,
    const base::TimeTicks& request_time)
    : resolver(resolver),
      key_system_access(access),
      request_time(request_time) {}

void MediaCapabilities::PendingCallbackState::Trace(
    blink::Visitor* visitor) const {
  visitor->Trace(key_system_access);
  visitor->Trace(resolver);
}

ScriptPromise<MediaCapabilitiesDecodingInfo> MediaCapabilities::decodingInfo(
    ScriptState* script_state,
    const MediaDecodingConfiguration* config,
    ExceptionState& exception_state) {
  const base::TimeTicks request_time = base::TimeTicks::Now();

  if (config->hasKeySystemConfiguration()) {
    UseCounter::Count(
        ExecutionContext::From(script_state),
        WebFeature::kMediaCapabilitiesDecodingInfoWithKeySystemConfig);
  }

  const bool is_webrtc = config->type() == V8MediaDecodingType::Enum::kWebrtc;
  String message;
  if (!IsValidMediaDecodingConfiguration(config, is_webrtc, &message)) {
    exception_state.ThrowTypeError(message);
    return EmptyPromise();
  }
  // Validation errors should return above.
  DCHECK(message.empty());

  if (is_webrtc) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kMediaCapabilitiesDecodingInfoWebrtc);

    auto* resolver = MakeGarbageCollected<
        ScriptPromiseResolver<MediaCapabilitiesDecodingInfo>>(
        script_state, exception_state.GetContext());

    // IMPORTANT: Acquire the promise before potentially synchronously resolving
    // it in the code that follows. Otherwise the promise returned to JS will be
    // undefined. See comment above Promise() in script_promise_resolver.h
    auto promise = resolver->Promise();

    if (auto* handler = webrtc_decoding_info_handler_for_test_
                            ? webrtc_decoding_info_handler_for_test_.get()
                            : WebrtcDecodingInfoHandler::Instance()) {
      const int callback_id = CreateCallbackId();
      pending_cb_map_.insert(
          callback_id,
          MakeGarbageCollected<MediaCapabilities::PendingCallbackState>(
              resolver, nullptr, request_time));

      std::optional<webrtc::SdpAudioFormat> sdp_audio_format =
          config->hasAudio()
              ? std::make_optional(ToSdpAudioFormat(config->audio()))
              : std::nullopt;

      std::optional<webrtc::SdpVideoFormat> sdp_video_format;
      bool spatial_scalability = false;
      media::VideoCodecProfile codec_profile =
          media::VIDEO_CODEC_PROFILE_UNKNOWN;
      int video_pixels = 0;
      int frames_per_second = 0;
      if (config->hasVideo()) {
        sdp_video_format =
            std::make_optional(ToSdpVideoFormat(config->video()));
        spatial_scalability = config->video()->hasSpatialScalability()
                                  ? config->video()->spatialScalability()
                                  : false;

        // Additional information needed for lookup in WebrtcVideoPerfHistory.
        codec_profile =
            WebRtcVideoFormatToMediaVideoCodecProfile(*sdp_video_format);
        video_pixels = config->video()->width() * config->video()->height();
        frames_per_second = base::ClampRound(config->video()->framerate());
      }
      media::mojom::blink::WebrtcPredictionFeaturesPtr features =
          media::mojom::blink::WebrtcPredictionFeatures::New(
              /*is_decode_stats=*/true, codec_profile, video_pixels,
              /*hardware_accelerated=*/false);

      handler->DecodingInfo(
          sdp_audio_format, sdp_video_format, spatial_scalability,
          BindOnce(&MediaCapabilities::OnWebrtcSupportInfo,
                   WrapPersistent(this), callback_id, std::move(features),
                   frames_per_second, OperationType::kDecoding));

      return promise;
    }
    // TODO(crbug.com/1187565): This should not happen unless we're out of
    // memory or something similar. Add UMA metric to count how often it
    // happens.
    DCHECK(false);
    DVLOG(2) << __func__ << " Could not get DecodingInfoHandler.";
    MediaCapabilitiesDecodingInfo* info = CreateDecodingInfoWith(false);
    resolver->Resolve(info);
    return promise;
  }

  String audio_mime_str;
  String audio_codec_str;
  if (config->hasAudio()) {
    DCHECK(config->audio()->hasContentType());
    bool valid_content_type = ParseContentType(
        config->audio()->contentType(), &audio_mime_str, &audio_codec_str);
    DCHECK(valid_content_type);
  }

  String video_mime_str;
  String video_codec_str;
  if (config->hasVideo()) {
    DCHECK(config->video()->hasContentType());
    bool valid_content_type = ParseContentType(
        config->video()->contentType(), &video_mime_str, &video_codec_str);
    DCHECK(valid_content_type);
  }

  // MSE support is cheap to check (regex matching). Do it first. Also, note
  // that MSE support is not implied by EME support, so do it irrespective of
  // whether we have a KeySystem configuration.
  if (config->type() == V8MediaDecodingType::Enum::kMediaSource) {
    if ((config->hasAudio() &&
         !CheckMseSupport(audio_mime_str, audio_codec_str)) ||
        (config->hasVideo() &&
         !CheckMseSupport(video_mime_str, video_codec_str))) {
      // Unsupported EME queries should resolve with a null
      // MediaKeySystemAccess.
      MediaCapabilitiesDecodingInfo* info =
          CreateEncryptedDecodingInfoWith(false, nullptr);
      return ToResolvedPromise<MediaCapabilitiesDecodingInfo>(script_state,
                                                              info);
    }
  }

  media::VideoCodec video_codec = media::VideoCodec::kUnknown;
  media::VideoCodecProfile video_profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;

  if ((config->hasAudio() &&
       !IsAudioCodecValid(audio_mime_str, audio_codec_str, &message)) ||
      (config->hasVideo() &&
       !IsVideoCodecValid(video_mime_str, video_codec_str, &video_codec,
                          &video_profile, &message))) {
    DCHECK(!message.empty());
    if (ExecutionContext* execution_context =
            ExecutionContext::From(script_state)) {
      execution_context->AddConsoleMessage(mojom::ConsoleMessageSource::kOther,
                                           mojom::ConsoleMessageLevel::kWarning,
                                           message);
    }

    return CreateResolvedPromiseToDecodingInfoWith(false, script_state, config);
  }

  // Validation errors should return above.
  DCHECK(message.empty());

  // Fill in values for range, matrix since `VideoConfiguration` doesn't have
  // such concepts; these aren't used, but ensure VideoColorSpace.IsSpecified()
  // works as expected downstream.
  media::VideoColorSpace video_color_space;
  video_color_space.range = gfx::ColorSpace::RangeID::DERIVED;
  video_color_space.matrix = media::VideoColorSpace::MatrixID::BT709;

  gfx::HdrMetadataType hdr_metadata_type = gfx::HdrMetadataType::kNone;
  if (config->hasVideo()) {
    ParseDynamicRangeConfigurations(config->video(), &video_color_space,
                                    &hdr_metadata_type);
  }

  bool audio_supported = true;

  if (config->hasAudio()) {
    audio_supported = IsAudioConfigurationSupported(
        config->audio(), audio_mime_str, audio_codec_str);
  }

  if (!audio_supported) {
    return CreateResolvedPromiseToDecodingInfoWith(false, script_state, config);
  }

  // Audio-only
  if (!config->hasVideo()) {
    // Clear audio
    if (!config->hasKeySystemConfiguration()) {
      return CreateResolvedPromiseToDecodingInfoWith(audio_supported,
                                                     script_state, config);
    } else {
      return GetEmeSupport(script_state, video_codec, video_profile,
                           video_color_space, config, request_time,
                           exception_state);
    }
  }

  DCHECK(message.empty());
  DCHECK(config->hasVideo());

  const bool is_video_config_supported = IsVideoConfigurationSupported(
      video_mime_str, video_codec_str, video_color_space, hdr_metadata_type);

#if BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)
  // Defer support check to EME for DV instead of asking from
  // SupplementalProfileCache.
  const bool should_defer_support_check_to_eme =
      IsDolbyVisionVideoCodec(video_codec_str) &&
      config->hasKeySystemConfiguration();
#else
  const bool should_defer_support_check_to_eme = false;
#endif  // BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)

  // Return early for unsupported configurations.
  if (!is_video_config_supported && !should_defer_support_check_to_eme) {
    return CreateResolvedPromiseToDecodingInfoWith(false, script_state, config);
  }

  if (config->hasKeySystemConfiguration()) {
    // GetEmeSupport() will call the VideoDecodePerfHistory service after
    // receiving info about support for the configuration for encrypted content.
    return GetEmeSupport(script_state, video_codec, video_profile,
                         video_color_space, config, request_time,
                         exception_state);
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<MediaCapabilitiesDecodingInfo>>(
      script_state, exception_state.GetContext());

  // IMPORTANT: Acquire the promise before potentially synchronously resolving
  // it in the code that follows. Otherwise the promise returned to JS will be
  // undefined. See comment above Promise() in script_promise_resolver.h
  auto promise = resolver->Promise();

  GetPerfInfo(video_codec, video_profile, video_color_space, config,
              request_time, resolver, nullptr /* access */);

  return promise;
}

ScriptPromise<MediaCapabilitiesInfo> MediaCapabilities::encodingInfo(
    ScriptState* script_state,
    const MediaEncodingConfiguration* config,
    ExceptionState& exception_state) {
  if (config->type() == V8MediaEncodingType::Enum::kRecord &&
      !RuntimeEnabledFeatures::MediaCapabilitiesEncodingInfoEnabled()) {
    exception_state.ThrowTypeError(
        "The provided value 'record' is not a valid enum value of type "
        "MediaEncodingType.");
    return EmptyPromise();
  }

  const base::TimeTicks request_time = base::TimeTicks::Now();

  const bool is_webrtc = config->type() == V8MediaEncodingType::Enum::kWebrtc;
  String message;
  if (!IsValidMediaEncodingConfiguration(config, is_webrtc, &message)) {
    exception_state.ThrowTypeError(message);
    return EmptyPromise();
  }
  // Validation errors should return above.
  DCHECK(message.empty());

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<MediaCapabilitiesInfo>>(
          script_state, exception_state.GetContext());

  // IMPORTANT: Acquire the promise before potentially synchronously resolving
  // it in the code that follows. Otherwise the promise returned to JS will be
  // undefined. See comment above Promise() in script_promise_resolver.h
  auto promise = resolver->Promise();

  if (is_webrtc) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kMediaCapabilitiesEncodingInfoWebrtc);

    if (auto* handler = webrtc_encoding_info_handler_for_test_
                            ? webrtc_encoding_info_handler_for_test_.get()
                            : WebrtcEncodingInfoHandler::Instance()) {
      const int callback_id = CreateCallbackId();
      pending_cb_map_.insert(
          callback_id,
          MakeGarbageCollected<MediaCapabilities::PendingCallbackState>(
              resolver, nullptr, request_time));

      std::optional<webrtc::SdpAudioFormat> sdp_audio_format =
          config->hasAudio()
              ? std::make_optional(ToSdpAudioFormat(config->audio()))
              : std::nullopt;

      std::optional<webrtc::SdpVideoFormat> sdp_video_format;
      String scalability_mode;
      media::VideoCodecProfile codec_profile =
          media::VIDEO_CODEC_PROFILE_UNKNOWN;
      int video_pixels = 0;
      int frames_per_second = 0;
      if (config->hasVideo()) {
        sdp_video_format =
            std::make_optional(ToSdpVideoFormat(config->video()));
        if (config->video()->hasScalabilityMode()) {
          scalability_mode = config->video()->scalabilityMode();
        }

        // Additional information needed for lookup in WebrtcVideoPerfHistory.
        codec_profile =
            WebRtcVideoFormatToMediaVideoCodecProfile(*sdp_video_format);
        video_pixels = config->video()->width() * config->video()->height();
        frames_per_second = base::ClampRound(config->video()->framerate());
      }
      media::mojom::blink::WebrtcPredictionFeaturesPtr features =
          media::mojom::blink::WebrtcPredictionFeatures::New(
              /*is_decode_stats=*/false, codec_profile, video_pixels,
              /*hardware_accelerated=*/false);

      handler->EncodingInfo(
          sdp_audio_format, sdp_video_format, scalability_mode,
          BindOnce(&MediaCapabilities::OnWebrtcSupportInfo,
                   WrapPersistent(this), callback_id, std::move(features),
                   frames_per_second, OperationType::kEncoding));

      return promise;
    }
    // TODO(crbug.com/1187565): This should not happen unless we're out of
    // memory or something similar. Add UMA metric to count how often it
    // happens.
    DCHECK(false);
    DVLOG(2) << __func__ << " Could not get EncodingInfoHandler.";
    MediaCapabilitiesInfo* info = CreateEncodingInfoWith(false);
    resolver->Resolve(info);
    return promise;
  }

  DCHECK_EQ(config->type(), V8MediaEncodingType::Enum::kRecord);
  DCHECK(RuntimeEnabledFeatures::MediaCapabilitiesEncodingInfoEnabled());

  auto task_runner = resolver->GetExecutionContext()->GetTaskRunner(
      TaskType::kInternalMediaRealTime);
  if (auto* handler = MakeGarbageCollected<MediaRecorderHandler>(
          task_runner, KeyFrameRequestProcessor::Configuration())) {
    task_runner->PostTask(
        FROM_HERE, blink::BindOnce(&MediaRecorderHandler::EncodingInfo,
                                   WrapPersistent(handler),
                                   ToWebMediaConfiguration(config),
                                   BindOnce(&OnMediaCapabilitiesEncodingInfo,
                                            WrapPersistent(resolver))));

    return promise;
  }

  DVLOG(2) << __func__ << " Could not get MediaRecorderHandler.";
  MediaCapabilitiesInfo* info = CreateEncodingInfoWith(false);
  resolver->Resolve(info);
  return promise;
}

bool MediaCapabilities::EnsurePerfHistoryService(
    ExecutionContext* execution_context) {
  if (decode_history_service_.is_bound())
    return true;

  if (!execution_context)
    return false;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      execution_context->GetTaskRunner(TaskType::kMediaElementEvent);

  execution_context->GetBrowserInterfaceBroker().GetInterface(
      decode_history_service_.BindNewPipeAndPassReceiver(task_runner));
  return true;
}

bool MediaCapabilities::EnsureWebrtcPerfHistoryService(
    ExecutionContext* execution_context) {
  if (webrtc_history_service_.is_bound())
    return true;

  if (!execution_context)
    return false;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      execution_context->GetTaskRunner(TaskType::kMediaElementEvent);

  execution_context->GetBrowserInterfaceBroker().GetInterface(
      webrtc_history_service_.BindNewPipeAndPassReceiver(task_runner));
  return true;
}

ScriptPromise<MediaCapabilitiesDecodingInfo> MediaCapabilities::GetEmeSupport(
    ScriptState* script_state,
    media::VideoCodec video_codec,
    media::VideoCodecProfile video_profile,
    media::VideoColorSpace video_color_space,
    const MediaDecodingConfiguration* configuration,
    const base::TimeTicks& request_time,
    ExceptionState& exception_state) {
  DVLOG(3) << __func__;
  DCHECK(configuration->hasKeySystemConfiguration());

  // Calling context must have a real window bound to a Page. This check is
  // ported from rMKSA (see http://crbug.com/456720).
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The context provided is not associated with a page.");
    return EmptyPromise();
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);

  // See context here:
  // https://sites.google.com/a/chromium.org/dev/Home/chromium-security/deprecating-permissions-in-cross-origin-iframes
  if (!execution_context->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kEncryptedMedia,
          ReportOptions::kReportOnFailure)) {
    UseCounter::Count(execution_context,
                      WebFeature::kEncryptedMediaDisabledByFeaturePolicy);
    execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kWarning,
        kEncryptedMediaPermissionsPolicyConsoleWarning));
    exception_state.ThrowSecurityError(
        "decodingInfo(): Creating MediaKeySystemAccess is disabled by feature "
        "policy.");
    return EmptyPromise();
  }

  if (execution_context->IsWorkerGlobalScope()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Encrypted Media decoding info not available in Worker context.");
    return EmptyPromise();
  }

  if (!execution_context->IsSecureContext()) {
    exception_state.ThrowSecurityError(
        "Encrypted Media decoding info can only be queried in a secure"
        " context.");
    return EmptyPromise();
  }

  const MediaCapabilitiesKeySystemConfiguration* key_system_config =
      configuration->keySystemConfiguration();
  if (!key_system_config->hasKeySystem() ||
      key_system_config->keySystem().empty()) {
    exception_state.ThrowTypeError("The key system String is not valid.");
    return EmptyPromise();
  }

  MediaKeySystemConfiguration* eme_config =
      MediaKeySystemConfiguration::Create();

  // Set the initDataTypes attribute to a sequence containing
  // config.keySystemConfiguration.initDataType.
  // TODO(chcunningham): double check that this default is idiomatic. Here we
  // can't check hasInitDataType() because the default ("") makes that always
  // true. The default in EME is an empty list.
  if (!key_system_config->initDataType().empty()) {
    eme_config->setInitDataTypes(
        Vector<String>(1, key_system_config->initDataType()));
  }

  // Set the distinctiveIdentifier attribute to
  // config.keySystemConfiguration.distinctiveIdentifier.
  eme_config->setDistinctiveIdentifier(
      key_system_config->distinctiveIdentifier());

  // Set the persistentState attribute to
  // config.keySystemConfiguration.persistentState.
  eme_config->setPersistentState(key_system_config->persistentState());

  // Set the sessionTypes attribute to
  // config.keySystemConfiguration.sessionTypes.
  if (key_system_config->hasSessionTypes())
    eme_config->setSessionTypes(key_system_config->sessionTypes());

  // If an audio is present in config...
  if (configuration->hasAudio()) {
    // set the audioCapabilities attribute to a sequence containing a single
    // MediaKeySystemMediaCapability, initialized as follows:
    MediaKeySystemMediaCapability* audio_capability =
        MediaKeySystemMediaCapability::Create();
    // Set the contentType attribute to config.audio.contentType.
    audio_capability->setContentType(configuration->audio()->contentType());
    // If config.keySystemConfiguration.audio is present, set the robustness
    // attribute to config.keySystemConfiguration.audio.robustness.
    if (key_system_config->hasAudio())
      audio_capability->setRobustness(key_system_config->audio()->robustness());

    eme_config->setAudioCapabilities(
        HeapVector<Member<MediaKeySystemMediaCapability>>(1, audio_capability));
  }

  // If a video is present in config...
  if (configuration->hasVideo()) {
    // set the videoCapabilities attribute to a sequence containing a single
    // MediaKeySystemMediaCapability, initialized as follows:
    MediaKeySystemMediaCapability* video_capability =
        MediaKeySystemMediaCapability::Create();
    // Set the contentType attribute to config.video.contentType.
    video_capability->setContentType(configuration->video()->contentType());
    // If config.keySystemConfiguration.video is present, set the robustness
    // attribute to config.keySystemConfiguration.video.robustness.
    if (key_system_config->hasVideo())
      video_capability->setRobustness(key_system_config->video()->robustness());

    eme_config->setVideoCapabilities(
        HeapVector<Member<MediaKeySystemMediaCapability>>(1, video_capability));
  }

  HeapVector<Member<MediaKeySystemConfiguration>> config_vector(1, eme_config);

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<MediaCapabilitiesDecodingInfo>>(script_state);
  MediaCapabilitiesKeySystemAccessInitializer* initializer =
      MakeGarbageCollected<MediaCapabilitiesKeySystemAccessInitializer>(
          execution_context, resolver, key_system_config->keySystem(),
          config_vector,
          blink::BindOnce(&MediaCapabilities::GetPerfInfo, WrapPersistent(this),
                          video_codec, video_profile, video_color_space,
                          WrapPersistent(configuration), request_time));

  // IMPORTANT: Acquire the promise before potentially synchronously resolving
  // it in the code that follows. Otherwise the promise returned to JS will be
  // undefined. See comment above Promise() in script_promise_resolver.h
  auto promise = resolver->Promise();

  EncryptedMediaUtils::GetEncryptedMediaClientFromLocalDOMWindow(
      To<LocalDOMWindow>(execution_context))
      ->RequestMediaKeySystemAccess(WebEncryptedMediaRequest(initializer));

  return promise;
}

void MediaCapabilities::GetPerfInfo(
    media::VideoCodec video_codec,
    media::VideoCodecProfile video_profile,
    media::VideoColorSpace video_color_space,
    const MediaDecodingConfiguration* decoding_config,
    const base::TimeTicks& request_time,
    ScriptPromiseResolver<MediaCapabilitiesDecodingInfo>* resolver,
    MediaKeySystemAccess* access) {
  ExecutionContext* execution_context = resolver->GetExecutionContext();
  if (!execution_context || execution_context->IsContextDestroyed())
    return;

  if (!decoding_config->hasVideo()) {
    // Audio-only is always smooth and power efficient.
    MediaCapabilitiesDecodingInfo* info = CreateDecodingInfoWith(true);
    info->setKeySystemAccess(access);
    resolver->Resolve(info);
    return;
  }

  const VideoConfiguration* video_config = decoding_config->video();
  String key_system = "";
  bool use_hw_secure_codecs = false;

  if (access) {
    // Use the internal/base key system to keep the perf database simple, e.g.
    // different key system names may share the same internal/base key system
    // and the same CDM implementation, and hence the same performance.
    key_system = access->GetInternalKeySystem();
    use_hw_secure_codecs = access->UseHardwareSecureCodecs();
  }

  if (!EnsurePerfHistoryService(execution_context)) {
    MediaCapabilitiesDecodingInfo* info = CreateDecodingInfoWith(true);
    resolver->Resolve(WrapPersistent(info));
    return;
  }

  const int callback_id = CreateCallbackId();
  pending_cb_map_.insert(
      callback_id,
      MakeGarbageCollected<MediaCapabilities::PendingCallbackState>(
          resolver, access, request_time));

  media::mojom::blink::PredictionFeaturesPtr features =
      media::mojom::blink::PredictionFeatures::New(
          video_profile,
          gfx::Size(video_config->width(), video_config->height()),
          video_config->framerate(), key_system, use_hw_secure_codecs);

  decode_history_service_->GetPerfInfo(
      std::move(features), BindOnce(&MediaCapabilities::OnPerfHistoryInfo,
                                    WrapPersistent(this), callback_id));

  if (UseGpuFactoriesForPowerEfficient(execution_context, access)) {
    GetGpuFactoriesSupport(callback_id, video_codec, video_profile,
                           video_color_space, decoding_config);
  }
}

void MediaCapabilities::GetGpuFactoriesSupport(
    int callback_id,
    media::VideoCodec video_codec,
    media::VideoCodecProfile video_profile,
    media::VideoColorSpace video_color_space,
    const MediaDecodingConfiguration* decoding_config) {
  DCHECK(decoding_config->hasVideo());
  DCHECK(pending_cb_map_.Contains(callback_id));

  PendingCallbackState* pending_cb = pending_cb_map_.at(callback_id);
  if (!pending_cb) {
    // TODO(crbug.com/1125956): Determine how this can happen and prevent it.
    return;
  }

  ExecutionContext* execution_context =
      pending_cb->resolver->GetExecutionContext();

  // Frame may become detached in the time it takes us to get callback for
  // NotifyDecoderSupportKnown. In this case, report false as a means of clean
  // shutdown.
  if (!execution_context || execution_context->IsContextDestroyed()) {
    OnGpuFactoriesSupport(callback_id, false, video_codec);
    return;
  }

  DCHECK(UseGpuFactoriesForPowerEfficient(execution_context,
                                          pending_cb->key_system_access));

  media::GpuVideoAcceleratorFactories* gpu_factories =
      Platform::Current()->GetGpuFactories();
  if (!gpu_factories) {
    OnGpuFactoriesSupport(callback_id, false, video_codec);
    return;
  }

  if (!gpu_factories->IsDecoderSupportKnown()) {
    gpu_factories->NotifyDecoderSupportKnown(
        BindOnce(&MediaCapabilities::GetGpuFactoriesSupport,
                 WrapPersistent(this), callback_id, video_codec, video_profile,
                 video_color_space, WrapPersistent(decoding_config)));
    return;
  }

  // TODO(chcunningham): Get the actual scheme and alpha mode from
  // |decoding_config| once implemented (its already spec'ed).
  media::EncryptionScheme encryption_scheme =
      decoding_config->hasKeySystemConfiguration()
          ? media::EncryptionScheme::kCenc
          : media::EncryptionScheme::kUnencrypted;
  media::VideoDecoderConfig::AlphaMode alpha_mode =
      media::VideoDecoderConfig::AlphaMode::kIsOpaque;

  // A few things aren't known until demuxing time. These include: coded size,
  // visible rect, and extra data. Make reasonable guesses below. Ideally the
  // differences won't be make/break GPU acceleration support.
  const VideoConfiguration* video_config = decoding_config->video();
  gfx::Size natural_size(video_config->width(), video_config->height());
  media::VideoDecoderConfig config(
      video_codec, video_profile, alpha_mode, video_color_space,
      media::VideoTransformation(), natural_size /* coded_size */,
      gfx::Rect(natural_size) /* visible_rect */, natural_size,
      media::EmptyExtraData(), encryption_scheme);

  OnGpuFactoriesSupport(
      callback_id,
      gpu_factories->IsDecoderConfigSupportedOrUnknown(config) ==
          media::GpuVideoAcceleratorFactories::Supported::kTrue,
      video_codec);
}

void MediaCapabilities::ResolveCallbackIfReady(int callback_id) {
  DCHECK(pending_cb_map_.Contains(callback_id));
  PendingCallbackState* pending_cb = pending_cb_map_.at(callback_id);
  ExecutionContext* execution_context =
      pending_cb_map_.at(callback_id)->resolver->GetExecutionContext();

  if (!pending_cb->db_is_power_efficient.has_value())
    return;

  // Both db_* fields should be set simultaneously by the DB callback.
  DCHECK(pending_cb->db_is_smooth.has_value());

  if (UseGpuFactoriesForPowerEfficient(execution_context,
                                       pending_cb->key_system_access) &&
      !pending_cb->is_gpu_factories_supported.has_value()) {
    return;
  }

  if (!pending_cb->resolver->GetExecutionContext() ||
      pending_cb->resolver->GetExecutionContext()->IsContextDestroyed()) {
    // We're too late! Now that all the callbacks have provided state, its safe
    // to erase the entry in the map.
    pending_cb_map_.erase(callback_id);
    return;
  }

  auto* info = MediaCapabilitiesDecodingInfo::Create();
  info->setSupported(true);
  info->setKeySystemAccess(pending_cb->key_system_access);

  if (UseGpuFactoriesForPowerEfficient(execution_context,
                                       pending_cb->key_system_access)) {
    info->setPowerEfficient(*pending_cb->is_gpu_factories_supported);
    // Builtin video codec guarantee a certain codec can be decoded under any
    // circumstances, and if the result is not powerEfficient and the video
    // codec is not builtin, that means the video will failed to play at the
    // given video config, so change the supported value to false here.
    if (!info->powerEfficient() &&
        !pending_cb->is_builtin_video_codec.value_or(true)) {
      info->setSupported(false);
    }
  } else {
    info->setPowerEfficient(*pending_cb->db_is_power_efficient);
  }

  info->setSmooth(*pending_cb->db_is_smooth);

  const base::TimeDelta process_time =
      base::TimeTicks::Now() - pending_cb->request_time;
  UMA_HISTOGRAM_TIMES("Media.Capabilities.DecodingInfo.Time.Video",
                      process_time);

  // Record another time in the appropriate subset, either clear or encrypted
  // content.
  if (pending_cb->key_system_access) {
    UMA_HISTOGRAM_TIMES("Media.Capabilities.DecodingInfo.Time.Video.Encrypted",
                        process_time);
  } else {
    UMA_HISTOGRAM_TIMES("Media.Capabilities.DecodingInfo.Time.Video.Clear",
                        process_time);
  }

  pending_cb->resolver->DowncastTo<MediaCapabilitiesDecodingInfo>()->Resolve(
      std::move(info));
  pending_cb_map_.erase(callback_id);
}

void MediaCapabilities::OnPerfHistoryInfo(int callback_id,
                                          bool is_smooth,
                                          bool is_power_efficient) {
  DCHECK(pending_cb_map_.Contains(callback_id));
  PendingCallbackState* pending_cb = pending_cb_map_.at(callback_id);

  pending_cb->db_is_smooth = is_smooth;
  pending_cb->db_is_power_efficient = is_power_efficient;

  ResolveCallbackIfReady(callback_id);
}

void MediaCapabilities::OnGpuFactoriesSupport(int callback_id,
                                              bool is_supported,
                                              media::VideoCodec video_codec) {
  DVLOG(2) << __func__ << " video_codec:" << video_codec
           << ", is_supported:" << is_supported;
  DCHECK(pending_cb_map_.Contains(callback_id));
  PendingCallbackState* pending_cb = pending_cb_map_.at(callback_id);

  pending_cb->is_gpu_factories_supported = is_supported;
  pending_cb->is_builtin_video_codec =
      media::IsDecoderBuiltInVideoCodec(video_codec);

  ResolveCallbackIfReady(callback_id);
}

void MediaCapabilities::OnWebrtcSupportInfo(
    int callback_id,
    media::mojom::blink::WebrtcPredictionFeaturesPtr features,
    float frames_per_second,
    OperationType type,
    bool is_supported,
    bool is_power_efficient) {
  DCHECK(pending_cb_map_.Contains(callback_id));
  PendingCallbackState* pending_cb = pending_cb_map_.at(callback_id);

  // Special treatment if the config is not supported, or if only audio was
  // specified which is indicated by the fact that `video_pixels` equals 0,
  // or if we fail to access the WebrtcPerfHistoryService.
  // If enabled through default setting or field trial, we also set
  // smooth=true if the configuration is power efficient.
  if (!is_supported || features->video_pixels == 0 ||
      !EnsureWebrtcPerfHistoryService(
          pending_cb->resolver->GetExecutionContext()) ||
      (is_power_efficient && features->is_decode_stats &&
       WebrtcDecodeForceSmoothIfPowerEfficient()) ||
      (is_power_efficient && !features->is_decode_stats &&
       WebrtcEncodeForceSmoothIfPowerEfficient())) {
    MediaCapabilitiesDecodingInfo* info =
        MediaCapabilitiesDecodingInfo::Create();
    info->setSupported(is_supported);
    info->setSmooth(is_supported);
    info->setPowerEfficient(is_power_efficient);
    if (type == OperationType::kEncoding) {
      pending_cb->resolver->DowncastTo<MediaCapabilitiesInfo>()->Resolve(info);
    } else {
      pending_cb->resolver->DowncastTo<MediaCapabilitiesDecodingInfo>()
          ->Resolve(info);
    }
    pending_cb_map_.erase(callback_id);
    return;
  }

  pending_cb->is_supported = is_supported;
  pending_cb->is_gpu_factories_supported = is_power_efficient;

  features->hardware_accelerated = is_power_efficient;

  webrtc_history_service_->GetPerfInfo(
      std::move(features), frames_per_second,
      BindOnce(&MediaCapabilities::OnWebrtcPerfHistoryInfo,
               WrapPersistent(this), callback_id, type));
}

void MediaCapabilities::OnWebrtcPerfHistoryInfo(int callback_id,
                                                OperationType type,
                                                bool is_smooth) {
  DCHECK(pending_cb_map_.Contains(callback_id));
  PendingCallbackState* pending_cb = pending_cb_map_.at(callback_id);

  // supported and gpu factories supported are set simultaneously.
  DCHECK(pending_cb->is_supported.has_value());
  DCHECK(pending_cb->is_gpu_factories_supported.has_value());

  if (!pending_cb->resolver->GetExecutionContext() ||
      pending_cb->resolver->GetExecutionContext()->IsContextDestroyed()) {
    // We're too late! Now that all the callbacks have provided state, its safe
    // to erase the entry in the map.
    pending_cb_map_.erase(callback_id);
    return;
  }

  auto* info = MediaCapabilitiesDecodingInfo::Create();
  info->setSupported(*pending_cb->is_supported);
  info->setPowerEfficient(*pending_cb->is_gpu_factories_supported);
  info->setSmooth(is_smooth);

  const base::TimeDelta process_time =
      base::TimeTicks::Now() - pending_cb->request_time;
  UMA_HISTOGRAM_TIMES("Media.Capabilities.DecodingInfo.Time.Webrtc",
                      process_time);

  if (type == OperationType::kEncoding) {
    pending_cb->resolver->DowncastTo<MediaCapabilitiesInfo>()->Resolve(info);
  } else {
    pending_cb->resolver->DowncastTo<MediaCapabilitiesDecodingInfo>()->Resolve(
        info);
  }
  pending_cb_map_.erase(callback_id);
}

int MediaCapabilities::CreateCallbackId() {
  // Search for the next available callback ID. 0 and -1 are reserved by
  // HashMap (meaning "empty" and "deleted").
  do {
    ++last_callback_id_;
  } while (last_callback_id_ == 0 || last_callback_id_ == -1 ||
           pending_cb_map_.Contains(last_callback_id_));

  return last_callback_id_;
}

}  // namespace blink
