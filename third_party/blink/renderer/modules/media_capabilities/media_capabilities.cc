// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities.h"

#include <memory>
#include <sstream>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/mime_util.h"
#include "media/base/supported_types.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/stream_parser_factory.h"
#include "media/learning/common/media_learning_tasks.h"
#include "media/learning/common/target_histogram.h"
#include "media/learning/mojo/public/mojom/learning_task_controller.mojom-blink.h"
#include "media/mojo/mojom/media_metrics_provider.mojom-blink.h"
#include "media/mojo/mojom/media_types.mojom-blink.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/supported_video_decoder_config.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_encrypted_media_client.h"
#include "third_party/blink/public/platform/web_encrypted_media_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
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
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/encryptedmedia/encrypted_media_utils.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_system_access.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_system_access_initializer_base.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_keys_controller.h"
#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities_identifiability_metrics.h"
#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_handler.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/media_capabilities/web_media_capabilities_info.h"
#include "third_party/blink/renderer/platform/media_capabilities/web_media_configuration.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/peerconnection/transmission_encoding_info_handler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

const double kLearningBadWindowThresholdDefault = 2;
const double kLearningNnrThresholdDefault = 3;

constexpr const char* kApplicationMimeTypePrefix = "application/";
constexpr const char* kAudioMimeTypePrefix = "audio/";
constexpr const char* kVideoMimeTypePrefix = "video/";
constexpr const char* kCodecsMimeTypeParam = "codecs";
constexpr const char* kSmpteSt2086HdrMetadataType = "smpteSt2086";
constexpr const char* kSmpteSt209410HdrMetadataType = "smpteSt2094-10";
constexpr const char* kSmpteSt209440HdrMetadataType = "smpteSt2094-40";
constexpr const char* kSrgbColorGamut = "srgb";
constexpr const char* kP3ColorGamut = "p3";
constexpr const char* kRec2020ColorGamut = "rec2020";
constexpr const char* kSrgbTransferFunction = "srgb";
constexpr const char* kPqTransferFunction = "pq";
constexpr const char* kHlgTransferFunction = "hlg";

// Gets parameters for kMediaLearningSmoothnessExperiment field trial. Will
// provide sane defaults when field trial not enabled. Values of -1 indicate
// predictions from a given task should be ignored.

// static
double GetLearningBadWindowThreshold() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      media::kMediaLearningSmoothnessExperiment,
      MediaCapabilities::kLearningBadWindowThresholdParamName,
      kLearningBadWindowThresholdDefault);
}

// static
double GetLearningNnrThreshold() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      media::kMediaLearningSmoothnessExperiment,
      MediaCapabilities::kLearningNnrThresholdParamName,
      kLearningNnrThresholdDefault);
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

ScriptPromise CreateResolvedPromiseToDecodingInfoWith(
    bool value,
    ScriptState* script_state,
    const MediaDecodingConfiguration* config) {
  MediaCapabilitiesDecodingInfo* info = CreateDecodingInfoWith(value);
  media_capabilities_identifiability_metrics::ReportDecodingInfoResult(
      ExecutionContext::From(script_state), config, info);
  return ScriptPromise::Cast(script_state, ToV8(info, script_state));
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
  using GetPerfCallback =
      base::OnceCallback<void(ScriptPromiseResolver*, MediaKeySystemAccess*)>;

  MediaCapabilitiesKeySystemAccessInitializer(
      ScriptState* script_state,
      const String& key_system,
      const HeapVector<Member<MediaKeySystemConfiguration>>&
          supported_configurations,
      GetPerfCallback get_perf_callback)
      : MediaKeySystemAccessInitializerBase(script_state,
                                            key_system,
                                            supported_configurations),
        get_perf_callback_(std::move(get_perf_callback)) {}

  ~MediaCapabilitiesKeySystemAccessInitializer() override = default;

  void RequestSucceeded(
      std::unique_ptr<WebContentDecryptionModuleAccess> access) override {
    DVLOG(3) << __func__;

    if (!IsExecutionContextValid())
      return;

    // Query the client for smoothness and power efficiency of the video. It
    // will resolve the promise.
    std::move(get_perf_callback_)
        .Run(resolver_.Get(), MakeGarbageCollected<MediaKeySystemAccess>(
                                  KeySystem(), std::move(access)));
  }

  void RequestNotSupported(const WebString& error_message) override {
    DVLOG(3) << __func__ << " error: " << error_message.Ascii();

    if (!IsExecutionContextValid())
      return;

    MediaCapabilitiesDecodingInfo* info =
        CreateEncryptedDecodingInfoWith(false, nullptr);

    resolver_->Resolve(info);
  }

  void Trace(Visitor* visitor) const override {
    MediaKeySystemAccessInitializerBase::Trace(visitor);
  }

 private:
  GetPerfCallback get_perf_callback_;

  DISALLOW_COPY_AND_ASSIGN(MediaCapabilitiesKeySystemAccessInitializer);
};

bool IsValidFrameRate(double framerate) {
  return std::isfinite(framerate) && framerate > 0;
}

bool IsValidMimeType(const String& content_type, const String& prefix) {
  ParsedContentType parsed_content_type(content_type);

  if (!parsed_content_type.IsValid())
    return false;

  // Valid ParsedContentType implies we have a mime type.
  DCHECK(parsed_content_type.MimeType());
  if (!parsed_content_type.MimeType().StartsWith(prefix) &&
      !parsed_content_type.MimeType().StartsWith(kApplicationMimeTypePrefix)) {
    return false;
  }
  const auto& parameters = parsed_content_type.GetParameters();

  if (parameters.ParameterCount() > 1)
    return false;

  if (parameters.ParameterCount() == 0)
    return true;

  return parameters.begin()->name.LowerASCII() == kCodecsMimeTypeParam;
}

bool IsValidMediaConfiguration(const MediaConfiguration* configuration) {
  return configuration->hasAudio() || configuration->hasVideo();
}

bool IsValidMediaDecodingConfiguration(
    const MediaDecodingConfiguration* configuration,
    String* message) {
  if (!IsValidMediaConfiguration(configuration)) {
    *message =
        "The configuration dictionary has neither |video| nor |audio| "
        "specified and needs at least one of them.";
    return false;
  }

  if (configuration->hasKeySystemConfiguration()) {
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

  return true;
}

bool IsValidVideoConfiguration(const VideoConfiguration* configuration) {
  DCHECK(configuration->hasContentType());

  if (!IsValidMimeType(configuration->contentType(), kVideoMimeTypePrefix))
    return false;

  DCHECK(configuration->hasFramerate());
  if (!IsValidFrameRate(configuration->framerate()))
    return false;

  return true;
}

bool IsValidAudioConfiguration(const AudioConfiguration* configuration) {
  DCHECK(configuration->hasContentType());

  if (!IsValidMimeType(configuration->contentType(), kAudioMimeTypePrefix))
    return false;

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

  DEFINE_THREAD_SAFE_STATIC_LOCAL(const String, codecs, ("codecs"));
  web_configuration.mime_type = parsed_content_type.MimeType().LowerASCII();
  web_configuration.codec = parsed_content_type.ParameterValueForName(codecs);

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

  DEFINE_THREAD_SAFE_STATIC_LOCAL(const String, codecs, ("codecs"));
  web_configuration.mime_type = parsed_content_type.MimeType().LowerASCII();
  web_configuration.codec = parsed_content_type.ParameterValueForName(codecs);

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
  if (configuration->type() == "record")
    web_configuration.type = MediaConfigurationType::kRecord;
  else if (configuration->type() == "transmission")
    web_configuration.type = MediaConfigurationType::kTransmission;
  else
    NOTREACHED();

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

bool CheckMseSupport(const String& mime_type, const String& codec) {
  // For MSE queries, we assume the queried audio and video streams will be
  // placed into separate source buffers.
  // TODO(chcunningham): Clarify this assumption in the spec.

  // Media MIME API expects a vector of codec strings. We query audio and video
  // separately, so |codec_string|.size() should always be 1 or 0 (when no
  // codecs parameter is required for the given mime type).
  std::vector<std::string> codec_vector;

  if (!codec.Ascii().empty())
    codec_vector.push_back(codec.Ascii());

  if (media::IsSupported != media::StreamParserFactory::IsTypeSupported(
                                mime_type.Ascii(), codec_vector)) {
    DVLOG(2) << __func__
             << " MSE does not support the content type: " << mime_type.Ascii()
             << " " << (codec_vector.empty() ? "" : codec_vector[1]);
    return false;
  }

  return true;
}

void ParseDynamicRangeConfigurations(
    const blink::VideoConfiguration* video_config,
    media::VideoColorSpace* color_space,
    gl::HdrMetadataType* hdr_metadata) {
  DCHECK(color_space);
  DCHECK(hdr_metadata);

  // TODO(1066628): Follow up on MediaCapabilities spec regarding reconciling
  // discrepancies between mime type and colorGamut/transferFunction; for now,
  // give precedence to the latter.

  if (video_config->hasHdrMetadataType()) {
    const auto& hdr_metadata_type = video_config->hdrMetadataType();
    // TODO(crbug.com/1092328): Switch by V8HdrMetadataType::Enum.
    if (hdr_metadata_type == kSmpteSt2086HdrMetadataType) {
      *hdr_metadata = gl::HdrMetadataType::kSmpteSt2086;
    } else if (hdr_metadata_type == kSmpteSt209410HdrMetadataType) {
      *hdr_metadata = gl::HdrMetadataType::kSmpteSt2094_10;
    } else if (hdr_metadata_type == kSmpteSt209440HdrMetadataType) {
      *hdr_metadata = gl::HdrMetadataType::kSmpteSt2094_40;
    } else {
      NOTREACHED();
    }
  } else {
    *hdr_metadata = gl::HdrMetadataType::kNone;
  }

  if (video_config->hasColorGamut()) {
    const auto& color_gamut = video_config->colorGamut();
    // TODO(crbug.com/1092328): Switch by V8ColorGamut::Enum.
    if (color_gamut == kSrgbColorGamut) {
      color_space->primaries = media::VideoColorSpace::PrimaryID::BT709;
    } else if (color_gamut == kP3ColorGamut) {
      color_space->primaries = media::VideoColorSpace::PrimaryID::SMPTEST431_2;
    } else if (color_gamut == kRec2020ColorGamut) {
      color_space->primaries = media::VideoColorSpace::PrimaryID::BT2020;
    } else {
      NOTREACHED();
    }
  }

  if (video_config->hasTransferFunction()) {
    const auto& transfer_function = video_config->transferFunction();
    // TODO(crbug.com/1092328): Switch by V8TransferFunction::Enum.
    if (transfer_function == kSrgbTransferFunction) {
      color_space->transfer = media::VideoColorSpace::TransferID::BT709;
    } else if (transfer_function == kPqTransferFunction) {
      color_space->transfer = media::VideoColorSpace::TransferID::SMPTEST2084;
    } else if (transfer_function == kHlgTransferFunction) {
      color_space->transfer = media::VideoColorSpace::TransferID::ARIB_STD_B67;
    } else {
      NOTREACHED();
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
  media::AudioCodec audio_codec = media::kUnknownAudioCodec;
  bool is_audio_codec_ambiguous = true;

  if (!media::ParseAudioCodecString(mime_type.Ascii(), codec.Ascii(),
                                    &is_audio_codec_ambiguous, &audio_codec)) {
    *console_warning = StringView("Failed to parse audio contentType: ") +
                       String{mime_type} + StringView("; codecs=") +
                       String{codec};

    return false;
  }

  if (is_audio_codec_ambiguous) {
    *console_warning = StringView("Invalid (ambiguous) audio codec string: ") +
                       String{mime_type} + StringView("; codecs=") +
                       String{codec};
    return false;
  }

  return true;
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
  uint8_t video_level = 0;
  media::VideoColorSpace video_color_space;
  bool is_video_codec_ambiguous = true;

  if (!media::ParseVideoCodecString(mime_type.Ascii(), codec.Ascii(),
                                    &is_video_codec_ambiguous, out_video_codec,
                                    out_video_profile, &video_level,
                                    &video_color_space)) {
    *console_warning = StringView("Failed to parse video contentType: ") +
                       String{mime_type} + StringView("; codecs=") +
                       String{codec};
    return false;
  }

  if (is_video_codec_ambiguous) {
    *console_warning = StringView("Invalid (ambiguous) video codec string: ") +
                       String{mime_type} + StringView("; codecs=") +
                       String{codec};
    return false;
  }

  return true;
}

// Returns whether the AudioConfiguration is supported.
// IsAudioCodecValid() MUST be called before.
bool IsAudioConfigurationSupported(
    const blink::AudioConfiguration* audio_config,
    const String& mime_type,
    const String& codec) {
  media::AudioCodec audio_codec = media::kUnknownAudioCodec;
  media::AudioCodecProfile audio_profile = media::AudioCodecProfile::kUnknown;
  bool is_audio_codec_ambiguous = true;
  bool is_spatial_rendering = false;

  // Must succeed as IsAudioCodecValid() should have been called before.
  bool parsed =
      media::ParseAudioCodecString(mime_type.Ascii(), codec.Ascii(),
                                   &is_audio_codec_ambiguous, &audio_codec);
  DCHECK(parsed && !is_audio_codec_ambiguous);

  if (audio_config->hasSpatialRendering())
    is_spatial_rendering = audio_config->spatialRendering();

  return media::IsSupportedAudioType(
      {audio_codec, audio_profile, is_spatial_rendering});
}

// Returns whether the VideoConfiguration is supported.
// IsVideoCodecValid() MUST be called before.
bool IsVideoConfigurationSupported(const String& mime_type,
                                   const String& codec,
                                   media::VideoColorSpace video_color_space,
                                   gl::HdrMetadataType hdr_metadata_type) {
  media::VideoCodec video_codec = media::kUnknownVideoCodec;
  media::VideoCodecProfile video_profile;
  uint8_t video_level = 0;
  bool is_video_codec_ambiguous = true;

  // Must succeed as IsVideoCodecValid() should have been called before.
  bool parsed = media::ParseVideoCodecString(
      mime_type.Ascii(), codec.Ascii(), &is_video_codec_ambiguous, &video_codec,
      &video_profile, &video_level, &video_color_space);
  DCHECK(parsed && !is_video_codec_ambiguous);

  return media::IsSupportedVideoType({video_codec, video_profile, video_level,
                                      video_color_space, hdr_metadata_type});
}

void OnMediaCapabilitiesEncodingInfo(
    ScriptPromiseResolver* resolver,
    std::unique_ptr<WebMediaCapabilitiesInfo> result) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  Persistent<MediaCapabilitiesInfo> info(MediaCapabilitiesInfo::Create());
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

  DEFINE_THREAD_SAFE_STATIC_LOCAL(const String, codecs, ("codecs"));
  *mime_type = parsed_content_type.MimeType().LowerASCII();
  *codec = parsed_content_type.ParameterValueForName(codecs);
  return true;
}

}  // anonymous namespace

const char MediaCapabilities::kLearningBadWindowThresholdParamName[] =
    "bad_window_threshold";

const char MediaCapabilities::kLearningNnrThresholdParamName[] =
    "nnr_threshold";

MediaCapabilities::MediaCapabilities(ExecutionContext* context)
    : decode_history_service_(context),
      bad_window_predictor_(context),
      nnr_predictor_(context) {}

void MediaCapabilities::Trace(blink::Visitor* visitor) const {
  visitor->Trace(decode_history_service_);
  visitor->Trace(bad_window_predictor_);
  visitor->Trace(nnr_predictor_);
  visitor->Trace(pending_cb_map_);
  ScriptWrappable::Trace(visitor);
}

MediaCapabilities::PendingCallbackState::PendingCallbackState(
    ScriptPromiseResolver* resolver,
    MediaKeySystemAccess* access,
    const base::TimeTicks& request_time,
    base::Optional<IdentifiableToken> input_token)
    : resolver(resolver),
      key_system_access(access),
      request_time(request_time),
      input_token(input_token) {}

void MediaCapabilities::PendingCallbackState::Trace(
    blink::Visitor* visitor) const {
  visitor->Trace(key_system_access);
  visitor->Trace(resolver);
}

ScriptPromise MediaCapabilities::decodingInfo(
    ScriptState* script_state,
    const MediaDecodingConfiguration* config,
    ExceptionState& exception_state) {
  const base::TimeTicks request_time = base::TimeTicks::Now();

  if (config->hasKeySystemConfiguration()) {
    UseCounter::Count(
        ExecutionContext::From(script_state),
        WebFeature::kMediaCapabilitiesDecodingInfoWithKeySystemConfig);
  }

  String message;
  if (!IsValidMediaDecodingConfiguration(config, &message)) {
    exception_state.ThrowTypeError(message);
    return ScriptPromise();
  }

  if (config->hasVideo() && !IsValidVideoConfiguration(config->video())) {
    exception_state.ThrowTypeError(
        "The video configuration dictionary is not valid.");
    return ScriptPromise();
  }

  if (config->hasAudio() && !IsValidAudioConfiguration(config->audio())) {
    exception_state.ThrowTypeError(
        "The audio configuration dictionary is not valid.");
    return ScriptPromise();
  }

  // Validation errors should return above.
  DCHECK(message.IsEmpty());

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
  if (config->type() == "media-source") {
    if ((config->hasAudio() &&
         !CheckMseSupport(audio_mime_str, audio_codec_str)) ||
        (config->hasVideo() &&
         !CheckMseSupport(video_mime_str, video_codec_str))) {
      // Unsupported EME queries should resolve with a null
      // MediaKeySystemAccess.
      MediaCapabilitiesDecodingInfo* info =
          CreateEncryptedDecodingInfoWith(false, nullptr);
      media_capabilities_identifiability_metrics::ReportDecodingInfoResult(
          ExecutionContext::From(script_state), config, info);
      return ScriptPromise::Cast(script_state, ToV8(info, script_state));
    }
  }

  media::VideoCodec video_codec = media::kUnknownVideoCodec;
  media::VideoCodecProfile video_profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;

  if ((config->hasAudio() &&
       !IsAudioCodecValid(audio_mime_str, audio_codec_str, &message)) ||
      (config->hasVideo() &&
       !IsVideoCodecValid(video_mime_str, video_codec_str, &video_codec,
                          &video_profile, &message))) {
    DCHECK(!message.IsEmpty());
    if (ExecutionContext* execution_context =
            ExecutionContext::From(script_state)) {
      execution_context->AddConsoleMessage(mojom::ConsoleMessageSource::kOther,
                                           mojom::ConsoleMessageLevel::kWarning,
                                           message);
    }

    return CreateResolvedPromiseToDecodingInfoWith(false, script_state, config);
  }

  // Validation errors should return above.
  DCHECK(message.IsEmpty());

  media::VideoColorSpace video_color_space;
  gl::HdrMetadataType hdr_metadata_type = gl::HdrMetadataType::kNone;
  if (config->hasVideo()) {
    ParseDynamicRangeConfigurations(config->video(), &video_color_space,
                                    &hdr_metadata_type);
  }

  if (config->hasKeySystemConfiguration()) {
    // GetEmeSupport() will call the VideoDecodePerfHistory service after
    // receiving info about support for the configuration for encrypted content.
    return GetEmeSupport(script_state, video_codec, video_profile,
                         video_color_space, config, request_time,
                         exception_state);
  }

  bool audio_supported = true;

  if (config->hasAudio()) {
    audio_supported = IsAudioConfigurationSupported(
        config->audio(), audio_mime_str, audio_codec_str);
  }

  // No need to check video capabilities if video not included in configuration
  // or when audio is already known to be unsupported.
  if (!audio_supported || !config->hasVideo()) {
    return CreateResolvedPromiseToDecodingInfoWith(audio_supported,
                                                   script_state, config);
  }

  DCHECK(message.IsEmpty());
  DCHECK(config->hasVideo());

  // Return early for unsupported configurations.
  if (!IsVideoConfigurationSupported(video_mime_str, video_codec_str,
                                     video_color_space, hdr_metadata_type)) {
    return CreateResolvedPromiseToDecodingInfoWith(false, script_state, config);
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  // IMPORTANT: Acquire the promise before potentially synchronously resolving
  // it in the code that follows. Otherwise the promise returned to JS will be
  // undefined. See comment above Promise() in script_promise_resolver.h
  ScriptPromise promise = resolver->Promise();

  GetPerfInfo(video_codec, video_profile, video_color_space, config,
              request_time, resolver, nullptr /* access */);

  return promise;
}

ScriptPromise MediaCapabilities::encodingInfo(
    ScriptState* script_state,
    const MediaEncodingConfiguration* configuration) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  // IMPORTANT: Acquire the promise before potentially synchronously resolving
  // it in the code that follows. Otherwise the promise returned to JS will be
  // undefined. See comment above Promise() in script_promise_resolver.h
  ScriptPromise promise = resolver->Promise();

  if (!IsValidMediaConfiguration(configuration)) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "The configuration dictionary has neither |video| nor |audio| "
        "specified and needs at least one of them."));
    return promise;
  }

  if (configuration->hasVideo() &&
      !IsValidVideoConfiguration(configuration->video())) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "The video configuration dictionary is not valid."));
    return promise;
  }

  if (configuration->hasAudio() &&
      !IsValidAudioConfiguration(configuration->audio())) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "The audio configuration dictionary is not valid."));
    return promise;
  }

  if (configuration->type() == "transmission") {
    if (auto* handler = TransmissionEncodingInfoHandler::Instance()) {
      handler->EncodingInfo(ToWebMediaConfiguration(configuration),
                            WTF::Bind(&OnMediaCapabilitiesEncodingInfo,
                                      WrapPersistent(resolver)));
      return promise;
    }
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Platform error: could not get EncodingInfoHandler."));
    return promise;
  }

  if (configuration->type() == "record") {
    if (auto* handler = MakeGarbageCollected<MediaRecorderHandler>(
            ExecutionContext::From(script_state)
                ->GetTaskRunner(TaskType::kInternalMediaRealTime))) {
      handler->EncodingInfo(ToWebMediaConfiguration(configuration),
                            WTF::Bind(&OnMediaCapabilitiesEncodingInfo,
                                      WrapPersistent(resolver)));
      return promise;
    }
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Platform error: could not create MediaRecorderHandler."));
    return promise;
  }

  resolver->Reject(V8ThrowException::CreateTypeError(
      script_state->GetIsolate(),
      "Valid configuration |type| should be either 'transmission' or "
      "'record'."));
  return promise;
}

bool MediaCapabilities::EnsureLearningPredictors(
    ExecutionContext* execution_context) {
  DCHECK(execution_context && !execution_context->IsContextDestroyed());

  // One or both of these will have been bound in an earlier pass.
  if (bad_window_predictor_.is_bound() || nnr_predictor_.is_bound())
    return true;

  // MediaMetricsProvider currently only exposed via render frame.
  // TODO(chcunningham): Expose in worker contexts pending outcome of
  // media-learning experiments.
  if (execution_context->IsWorkerGlobalScope())
    return false;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      execution_context->GetTaskRunner(TaskType::kMediaElementEvent);

  mojo::Remote<media::mojom::blink::MediaMetricsProvider> metrics_provider;
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      metrics_provider.BindNewPipeAndPassReceiver(task_runner));

  if (!metrics_provider)
    return false;

  if (GetLearningBadWindowThreshold() != -1.0) {
    DCHECK_GE(GetLearningBadWindowThreshold(), 0);
    metrics_provider->AcquireLearningTaskController(
        media::learning::tasknames::kConsecutiveBadWindows,
        bad_window_predictor_.BindNewPipeAndPassReceiver(task_runner));
  }

  if (GetLearningNnrThreshold() != -1.0) {
    DCHECK_GE(GetLearningNnrThreshold(), 0);
    metrics_provider->AcquireLearningTaskController(
        media::learning::tasknames::kConsecutiveNNRs,
        nnr_predictor_.BindNewPipeAndPassReceiver(task_runner));
  }

  return bad_window_predictor_.is_bound() || nnr_predictor_.is_bound();
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

ScriptPromise MediaCapabilities::GetEmeSupport(
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
    return ScriptPromise();
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);

  // See context here:
  // https://sites.google.com/a/chromium.org/dev/Home/chromium-security/deprecating-permissions-in-cross-origin-iframes
  if (!execution_context->IsFeatureEnabled(
          mojom::blink::FeaturePolicyFeature::kEncryptedMedia,
          ReportOptions::kReportOnFailure)) {
    UseCounter::Count(execution_context,
                      WebFeature::kEncryptedMediaDisabledByFeaturePolicy);
    execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kWarning,
        kEncryptedMediaFeaturePolicyConsoleWarning));
    exception_state.ThrowSecurityError(
        "decodingInfo(): Creating MediaKeySystemAccess is disabled by feature "
        "policy.");
    return ScriptPromise();
  }

  if (execution_context->IsWorkerGlobalScope()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Encrypted Media decoding info not available in Worker context.");
    return ScriptPromise();
  }

  if (!execution_context->IsSecureContext()) {
    exception_state.ThrowSecurityError(
        "Encrypted Media decoding info can only be queried in a secure"
        " context.");
    return ScriptPromise();
  }

  const MediaCapabilitiesKeySystemConfiguration* key_system_config =
      configuration->keySystemConfiguration();
  if (!key_system_config->hasKeySystem() ||
      key_system_config->keySystem().IsEmpty()) {
    exception_state.ThrowTypeError("The key system String is not valid.");
    return ScriptPromise();
  }

  MediaKeySystemConfiguration* eme_config =
      MediaKeySystemConfiguration::Create();

  // Set the initDataTypes attribute to a sequence containing
  // config.keySystemConfiguration.initDataType.
  // TODO(chcunningham): double check that this default is idiomatic. Here we
  // can't check hasInitDataType() because the default ("") makes that always
  // true. The default in EME is an empty list.
  if (!key_system_config->initDataType().IsEmpty()) {
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

  MediaCapabilitiesKeySystemAccessInitializer* initializer =
      MakeGarbageCollected<MediaCapabilitiesKeySystemAccessInitializer>(
          script_state, key_system_config->keySystem(), config_vector,
          WTF::Bind(&MediaCapabilities::GetPerfInfo, WrapPersistent(this),
                    video_codec, video_profile, video_color_space,
                    WrapPersistent(configuration), request_time));

  // IMPORTANT: Acquire the promise before potentially synchronously resolving
  // it in the code that follows. Otherwise the promise returned to JS will be
  // undefined. See comment above Promise() in script_promise_resolver.h
  ScriptPromise promise = initializer->Promise();

  Page* page = To<LocalDOMWindow>(execution_context)->GetFrame()->GetPage();
  MediaKeysController::From(page)
      ->EncryptedMediaClient(execution_context)
      ->RequestMediaKeySystemAccess(WebEncryptedMediaRequest(initializer));

  return promise;
}

void MediaCapabilities::GetPerfInfo(
    media::VideoCodec video_codec,
    media::VideoCodecProfile video_profile,
    media::VideoColorSpace video_color_space,
    const MediaDecodingConfiguration* decoding_config,
    const base::TimeTicks& request_time,
    ScriptPromiseResolver* resolver,
    MediaKeySystemAccess* access) {
  ExecutionContext* execution_context = resolver->GetExecutionContext();
  if (!execution_context || execution_context->IsContextDestroyed())
    return;

  if (!decoding_config->hasVideo()) {
    // Audio-only is always smooth and power efficient.
    MediaCapabilitiesDecodingInfo* info = CreateDecodingInfoWith(true);
    info->setKeySystemAccess(access);
    media_capabilities_identifiability_metrics::ReportDecodingInfoResult(
        execution_context, decoding_config, info);
    resolver->Resolve(info);
    return;
  }

  const VideoConfiguration* video_config = decoding_config->video();
  String key_system = "";
  bool use_hw_secure_codecs = false;

  if (access) {
    key_system = access->keySystem();
    use_hw_secure_codecs = access->UseHardwareSecureCodecs();
  }

  if (!EnsurePerfHistoryService(execution_context)) {
    MediaCapabilitiesDecodingInfo* info = CreateDecodingInfoWith(true);
    media_capabilities_identifiability_metrics::ReportDecodingInfoResult(
        execution_context, decoding_config, info);
    resolver->Resolve(WrapPersistent(info));
    return;
  }

  const int callback_id = CreateCallbackId();
  pending_cb_map_.insert(
      callback_id,
      MakeGarbageCollected<MediaCapabilities::PendingCallbackState>(
          resolver, access, request_time,
          media_capabilities_identifiability_metrics::
              ComputeDecodingInfoInputToken(decoding_config)));

  if (base::FeatureList::IsEnabled(media::kMediaLearningSmoothnessExperiment)) {
    GetPerfInfo_ML(execution_context, callback_id, video_codec, video_profile,
                   video_config->width(), video_config->framerate());
  }

  media::mojom::blink::PredictionFeaturesPtr features =
      media::mojom::blink::PredictionFeatures::New(
          static_cast<media::mojom::blink::VideoCodecProfile>(video_profile),
          gfx::Size(video_config->width(), video_config->height()),
          video_config->framerate(), key_system, use_hw_secure_codecs);

  decode_history_service_->GetPerfInfo(
      std::move(features), WTF::Bind(&MediaCapabilities::OnPerfHistoryInfo,
                                     WrapPersistent(this), callback_id));

  if (UseGpuFactoriesForPowerEfficient(execution_context, access)) {
    GetGpuFactoriesSupport(callback_id, video_codec, video_profile,
                           video_color_space, decoding_config);
  }
}

void MediaCapabilities::GetPerfInfo_ML(ExecutionContext* execution_context,
                                       int callback_id,
                                       media::VideoCodec video_codec,
                                       media::VideoCodecProfile video_profile,
                                       int width,
                                       double framerate) {
  DCHECK(execution_context && !execution_context->IsContextDestroyed());
  DCHECK(pending_cb_map_.Contains(callback_id));

  if (!EnsureLearningPredictors(execution_context)) {
    return;
  }

  // FRAGILE: Order here MUST match order in
  // WebMediaPlayerImpl::UpdateSmoothnessHelper().
  // TODO(chcunningham): refactor into something more robust.
  Vector<media::learning::FeatureValue> ml_features(
      {media::learning::FeatureValue(video_codec),
       media::learning::FeatureValue(video_profile),
       media::learning::FeatureValue(width),
       media::learning::FeatureValue(framerate)});

  if (bad_window_predictor_.is_bound()) {
    bad_window_predictor_->PredictDistribution(
        ml_features, WTF::Bind(&MediaCapabilities::OnBadWindowPrediction,
                               WrapPersistent(this), callback_id));
  }

  if (nnr_predictor_.is_bound()) {
    nnr_predictor_->PredictDistribution(
        ml_features, WTF::Bind(&MediaCapabilities::OnNnrPrediction,
                               WrapPersistent(this), callback_id));
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

  DCHECK(UseGpuFactoriesForPowerEfficient(execution_context,
                                          pending_cb->key_system_access));

  // Frame may become detached in the time it takes us to get callback for
  // NotifyDecoderSupportKnown. In this case, report false as a means of clean
  // shutdown.
  if (!execution_context || execution_context->IsContextDestroyed()) {
    OnGpuFactoriesSupport(callback_id, false);
    return;
  }

  media::GpuVideoAcceleratorFactories* gpu_factories =
      Platform::Current()->GetGpuFactories();
  if (!gpu_factories) {
    OnGpuFactoriesSupport(callback_id, false);
    return;
  }

  if (!gpu_factories->IsDecoderSupportKnown()) {
    gpu_factories->NotifyDecoderSupportKnown(
        WTF::Bind(&MediaCapabilities::GetGpuFactoriesSupport,
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

  static_assert(media::VideoDecoderImplementation::kAlternate ==
                    media::VideoDecoderImplementation::kMaxValue,
                "Keep the array below in sync.");
  media::VideoDecoderImplementation decoder_impls[] = {
      media::VideoDecoderImplementation::kDefault,
      media::VideoDecoderImplementation::kAlternate};
  media::GpuVideoAcceleratorFactories::Supported supported =
      media::GpuVideoAcceleratorFactories::Supported::kUnknown;
  for (const auto& impl : decoder_impls) {
    supported = gpu_factories->IsDecoderConfigSupported(impl, config);
    DCHECK_NE(supported,
              media::GpuVideoAcceleratorFactories::Supported::kUnknown);
    if (supported == media::GpuVideoAcceleratorFactories::Supported::kTrue)
      break;
  }

  OnGpuFactoriesSupport(
      callback_id,
      supported == media::GpuVideoAcceleratorFactories::Supported::kTrue);
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

  if (nnr_predictor_.is_bound() &&
      !pending_cb->is_nnr_prediction_smooth.has_value())
    return;

  if (bad_window_predictor_.is_bound() &&
      !pending_cb->is_bad_window_prediction_smooth.has_value())
    return;

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

  Persistent<MediaCapabilitiesDecodingInfo> info(
      MediaCapabilitiesDecodingInfo::Create());
  info->setSupported(true);
  info->setKeySystemAccess(pending_cb->key_system_access);

  if (UseGpuFactoriesForPowerEfficient(execution_context,
                                       pending_cb->key_system_access)) {
    info->setPowerEfficient(*pending_cb->is_gpu_factories_supported);
  } else {
    info->setPowerEfficient(*pending_cb->db_is_power_efficient);
  }

  // If ML experiment is running: AND available ML signals.
  if (pending_cb->is_bad_window_prediction_smooth.has_value() ||
      pending_cb->is_nnr_prediction_smooth.has_value()) {
    info->setSmooth(
        pending_cb->is_bad_window_prediction_smooth.value_or(true) &&
        pending_cb->is_nnr_prediction_smooth.value_or(true));
  } else {
    // Use DB when ML experiment not running.
    info->setSmooth(*pending_cb->db_is_smooth);
  }

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

  media_capabilities_identifiability_metrics::ReportDecodingInfoResult(
      execution_context, pending_cb->input_token, info);
  pending_cb->resolver->Resolve(std::move(info));
  pending_cb_map_.erase(callback_id);
}

void MediaCapabilities::OnBadWindowPrediction(
    int callback_id,
    const base::Optional<::media::learning::TargetHistogram>& histogram) {
  DCHECK(pending_cb_map_.Contains(callback_id));
  PendingCallbackState* pending_cb = pending_cb_map_.at(callback_id);

  std::stringstream histogram_log;
  if (!histogram) {
    // No data, so optimistically assume zero bad windows.
    pending_cb->is_bad_window_prediction_smooth = true;
    histogram_log << "none";
  } else {
    double histogram_average = histogram->Average();
    pending_cb->is_bad_window_prediction_smooth =
        histogram_average < GetLearningBadWindowThreshold();
    histogram_log << histogram_average;
  }

  DVLOG(2) << __func__ << " bad_win_avg:" << histogram_log.str()
           << " smooth_threshold (<):" << GetLearningBadWindowThreshold();

  ResolveCallbackIfReady(callback_id);
}

void MediaCapabilities::OnNnrPrediction(
    int callback_id,
    const base::Optional<::media::learning::TargetHistogram>& histogram) {
  DCHECK(pending_cb_map_.Contains(callback_id));
  PendingCallbackState* pending_cb = pending_cb_map_.at(callback_id);

  std::stringstream histogram_log;
  if (!histogram) {
    // No data, so optimistically assume zero NNRs
    pending_cb->is_nnr_prediction_smooth = true;
    histogram_log << "none";
  } else {
    double histogram_average = histogram->Average();
    pending_cb->is_nnr_prediction_smooth =
        histogram_average < GetLearningNnrThreshold();
    histogram_log << histogram_average;
  }

  DVLOG(2) << __func__ << " nnr_avg:" << histogram_log.str()
           << " smooth_threshold (<):" << GetLearningNnrThreshold();

  ResolveCallbackIfReady(callback_id);
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
                                              bool is_supported) {
  DVLOG(2) << __func__ << " is_supported:" << is_supported;
  DCHECK(pending_cb_map_.Contains(callback_id));
  PendingCallbackState* pending_cb = pending_cb_map_.at(callback_id);

  pending_cb->is_gpu_factories_supported = is_supported;

  ResolveCallbackIfReady(callback_id);
}

int MediaCapabilities::CreateCallbackId() {
  // Search for the next available callback ID. 0 and -1 are reserved by
  // wtf::HashMap (meaning "empty" and "deleted").
  do {
    ++last_callback_id_;
  } while (last_callback_id_ == 0 || last_callback_id_ == -1 ||
           pending_cb_map_.Contains(last_callback_id_));

  return last_callback_id_;
}

}  // namespace blink
