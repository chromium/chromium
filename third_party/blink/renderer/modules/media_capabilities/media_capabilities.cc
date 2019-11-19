// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities.h"

#include <memory>
#include <utility>

#include "base/optional.h"
#include "media/base/mime_util.h"
#include "media/base/supported_types.h"
#include "media/filters/stream_parser_factory.h"
#include "media/mojo/mojom/media_types.mojom-blink.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_encrypted_media_client.h"
#include "third_party/blink/public/platform/web_encrypted_media_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/encryptedmedia/encrypted_media_utils.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_system_access.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_system_access_initializer_base.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_system_configuration.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_system_media_capability.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_keys_controller.h"
#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities_decoding_info.h"
#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities_info.h"
#include "third_party/blink/renderer/modules/media_capabilities/media_configuration.h"
#include "third_party/blink/renderer/modules/media_capabilities/media_decoding_configuration.h"
#include "third_party/blink/renderer/modules/media_capabilities/media_encoding_configuration.h"
#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_handler.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/media_capabilities/web_media_capabilities_info.h"
#include "third_party/blink/renderer/platform/media_capabilities/web_media_configuration.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/peerconnection/transmission_encoding_info_handler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include "third_party/blink/renderer/modules/encryptedmedia/media_key_system_access_initializer_base.h"

namespace blink {

namespace {

constexpr const char* kApplicationMimeTypePrefix = "application/";
constexpr const char* kAudioMimeTypePrefix = "audio/";
constexpr const char* kVideoMimeTypePrefix = "video/";
constexpr const char* kCodecsMimeTypeParam = "codecs";

// Utility function that will create a MediaCapabilitiesDecodingInfo object with
// all the values set to either true or false.
MediaCapabilitiesDecodingInfo* CreateDecodingInfoWith(bool value) {
  MediaCapabilitiesDecodingInfo* info = MediaCapabilitiesDecodingInfo::Create();
  info->setSupported(value);
  info->setSmooth(value);
  info->setPowerEfficient(value);
  return info;
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
        .Run(std::move(resolver_),
             MakeGarbageCollected<MediaKeySystemAccess>(std::move(access)));
  }

  void RequestNotSupported(const WebString& error_message) override {
    DVLOG(3) << __func__ << " error: " << error_message.Ascii();

    if (!IsExecutionContextValid())
      return;

    MediaCapabilitiesDecodingInfo* info =
        CreateEncryptedDecodingInfoWith(false, nullptr);

    resolver_->Resolve(info);
  }

  void Trace(blink::Visitor* visitor) override {
    MediaKeySystemAccessInitializerBase::Trace(visitor);
  }

 private:
  GetPerfCallback get_perf_callback_;

  DISALLOW_COPY_AND_ASSIGN(MediaCapabilitiesKeySystemAccessInitializer);
};

// Computes the effective framerate value based on the framerate field passed to
// the VideoConfiguration. It will return the parsed string as a double or
// compute the value in case of it is of the form "a/b".
// If the value is not valid, it will return NaN.
double ComputeFrameRate(const String& fps_str) {
  double result = ParseToDoubleForNumberType(fps_str);
  if (std::isfinite(result))
    return result > 0 ? result : std::numeric_limits<double>::quiet_NaN();

  wtf_size_t slash_position = fps_str.find('/');
  if (slash_position == kNotFound)
    return std::numeric_limits<double>::quiet_NaN();

  double numerator =
      ParseToDoubleForNumberType(fps_str.Substring(0, slash_position));
  double denominator = ParseToDoubleForNumberType(fps_str.Substring(
      slash_position + 1, fps_str.length() - slash_position - 1));
  if (std::isfinite(numerator) && std::isfinite(denominator) && numerator > 0 &&
      denominator > 0) {
    return numerator / denominator;
  }

  return std::numeric_limits<double>::quiet_NaN();
}

bool IsValidMimeType(const String& content_type, const String& prefix) {
  ParsedContentType parsed_content_type(content_type);

  if (!parsed_content_type.IsValid())
    return false;

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
    if (configuration->keySystemConfiguration()->hasAudioRobustness() &&
        !configuration->hasAudio()) {
      *message =
          "The keySystemConfiguration object contains an "
          "audioRobustness property but the root configuration has no "
          "audio configuration.";
      return false;
    }

    if (configuration->keySystemConfiguration()->hasVideoRobustness() &&
        !configuration->hasVideo()) {
      *message =
          "The keySystemConfiguration object contains an "
          "videoRobustness property but the root configuration has no "
          "video configuration.";
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
  if (std::isnan(ComputeFrameRate(configuration->framerate())))
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
  double computed_framerate = ComputeFrameRate(configuration->framerate());
  DCHECK(!std::isnan(computed_framerate));
  web_configuration.framerate = computed_framerate;

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
                       media::VideoCodecProfile* out_video_profile,
                       String* console_warning) {
  media::VideoCodec video_codec = media::kUnknownVideoCodec;
  uint8_t video_level = 0;
  media::VideoColorSpace video_color_space;
  bool is_video_codec_ambiguous = true;

  if (!media::ParseVideoCodecString(
          mime_type.Ascii(), codec.Ascii(), &is_video_codec_ambiguous,
          &video_codec, out_video_profile, &video_level, &video_color_space)) {
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
  bool is_audio_codec_ambiguous = true;

  // Must succeed as IsAudioCodecValid() should have been called before.
  bool parsed =
      media::ParseAudioCodecString(mime_type.Ascii(), codec.Ascii(),
                                   &is_audio_codec_ambiguous, &audio_codec);
  DCHECK(parsed && !is_audio_codec_ambiguous);

  return media::IsSupportedAudioType({audio_codec});
}

// Returns whether the VideoConfiguration is supported.
// IsVideoCodecValid() MUST be called before.
bool IsVideoConfigurationSupported(
    const blink::VideoConfiguration* video_config,
    const String& mime_type,
    const String& codec) {
  media::VideoCodec video_codec = media::kUnknownVideoCodec;
  media::VideoCodecProfile video_profile;
  uint8_t video_level = 0;
  media::VideoColorSpace video_color_space;
  bool is_video_codec_ambiguous = true;

  // Must succeed as IsVideoCodecValid() should have been called before.
  bool parsed = media::ParseVideoCodecString(
      mime_type.Ascii(), codec.Ascii(), &is_video_codec_ambiguous, &video_codec,
      &video_profile, &video_level, &video_color_space);
  DCHECK(parsed && !is_video_codec_ambiguous);

  return media::IsSupportedVideoType(
      {video_codec, video_profile, video_level, video_color_space});
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

MediaCapabilities::MediaCapabilities() = default;

ScriptPromise MediaCapabilities::decodingInfo(
    ScriptState* script_state,
    const MediaDecodingConfiguration* config) {
  if (config->hasKeySystemConfiguration()) {
    UseCounter::Count(
        ExecutionContext::From(script_state),
        WebFeature::kMediaCapabilitiesDecodingInfoWithKeySystemConfig);
  }
  if (config->hasVideo()) {
    DCHECK(config->video()->hasFramerate());
    if (!std::isnan(ComputeFrameRate(config->video()->framerate()))) {
      if (config->video()->framerate().find('/') != kNotFound) {
        UseCounter::Count(ExecutionContext::From(script_state),
                          WebFeature::kMediaCapabilitiesFramerateRatio);
      } else {
        UseCounter::Count(ExecutionContext::From(script_state),
                          WebFeature::kMediaCapabilitiesFramerateNumber);
      }
    }
  }

  String message;
  if (!IsValidMediaDecodingConfiguration(config, &message)) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(script_state->GetIsolate(), message));
  }

  if (config->hasVideo() && !IsValidVideoConfiguration(config->video())) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(),
                          "The video configuration dictionary is not valid."));
  }

  if (config->hasAudio() && !IsValidAudioConfiguration(config->audio())) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(),
                          "The audio configuration dictionary is not valid."));
  }

  // Validation errors should return above.
  DCHECK(message.IsEmpty());

  String audio_mime;
  String audio_codec;
  if (config->hasAudio()) {
    DCHECK(config->audio()->hasContentType());
    bool valid_content_type = ParseContentType(config->audio()->contentType(),
                                               &audio_mime, &audio_codec);
    DCHECK(valid_content_type);
  }

  String video_mime;
  String video_codec;
  if (config->hasVideo()) {
    DCHECK(config->video()->hasContentType());
    bool valid_content_type = ParseContentType(config->video()->contentType(),
                                               &video_mime, &video_codec);
    DCHECK(valid_content_type);
  }

  // MSE support is cheap to check (regex matching). Do it first. Also, note
  // that MSE support is not implied by EME support, so do it irrespective of
  // whether we have a KeySystem configuration.
  if (config->type() == "media-source") {
    if ((config->hasAudio() && !CheckMseSupport(audio_mime, audio_codec)) ||
        (config->hasVideo() && !CheckMseSupport(video_mime, video_codec))) {
      // Unsupported EME queries should resolve with a null
      // MediaKeySystemAccess.
      return ScriptPromise::Cast(
          script_state,
          ToV8(CreateEncryptedDecodingInfoWith(false, nullptr), script_state));
    }
  }

  media::VideoCodecProfile video_profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;

  if ((config->hasAudio() &&
       !IsAudioCodecValid(audio_mime, audio_codec, &message)) ||
      (config->hasVideo() &&
       !IsVideoCodecValid(video_mime, video_codec, &video_profile, &message))) {
    DCHECK(!message.IsEmpty());
    if (ExecutionContext* execution_context =
            ExecutionContext::From(script_state)) {
      execution_context->AddConsoleMessage(mojom::ConsoleMessageSource::kOther,
                                           mojom::ConsoleMessageLevel::kWarning,
                                           message);
    }

    return ScriptPromise::Cast(
        script_state, ToV8(CreateDecodingInfoWith(false), script_state));
  }

  // Validation errors should return above.
  DCHECK(message.IsEmpty());

  if (config->hasKeySystemConfiguration()) {
    // GetEmeSupport() will call the VideoDecodePerfHistory service after
    // receiving info about support for the configuration for encrypted content.
    return GetEmeSupport(script_state, video_profile, config);
  }

  bool audio_supported = true;

  if (config->hasAudio()) {
    audio_supported =
        IsAudioConfigurationSupported(config->audio(), audio_mime, audio_codec);
  }

  // No need to check video capabilities if video not included in configuration
  // or when audio is already known to be unsupported.
  if (!audio_supported || !config->hasVideo()) {
    return ScriptPromise::Cast(
        script_state,
        ToV8(CreateDecodingInfoWith(audio_supported), script_state));
  }

  DCHECK(message.IsEmpty());
  DCHECK(config->hasVideo());

  // Return early for unsupported configurations.
  if (!IsVideoConfigurationSupported(config->video(), video_mime,
                                     video_codec)) {
    return ScriptPromise::Cast(
        script_state, ToV8(CreateDecodingInfoWith(false), script_state));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  // IMPORTANT: Acquire the promise before potentially synchronously resolving
  // it in the code that follows. Otherwise the promise returned to JS will be
  // undefined. See comment above Promise() in script_promise_resolver.h
  ScriptPromise promise = resolver->Promise();

  GetPerfInfo(video_profile, config->video(), resolver, nullptr /* access */);

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
    if (auto* handler = MediaRecorderHandler::Create(
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

bool MediaCapabilities::EnsureService(ExecutionContext* execution_context) {
  if (decode_history_service_)
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
    media::VideoCodecProfile video_profile,
    const MediaDecodingConfiguration* configuration) {
  DVLOG(3) << __func__;
  DCHECK(configuration->hasKeySystemConfiguration());

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);
  Document* document = To<Document>(execution_context);

  // See context here:
  // https://sites.google.com/a/chromium.org/dev/Home/chromium-security/deprecating-permissions-in-cross-origin-iframes
  if (!document->IsFeatureEnabled(mojom::FeaturePolicyFeature::kEncryptedMedia,
                                  ReportOptions::kReportOnFailure)) {
    UseCounter::Count(document,
                      WebFeature::kEncryptedMediaDisabledByFeaturePolicy);
    document->AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                               mojom::ConsoleMessageLevel::kWarning,
                               kEncryptedMediaFeaturePolicyConsoleWarning));
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kSecurityError,
                          "decodingInfo(): Creating MediaKeySystemAccess is "
                          "disabled by feature policy."));
  }

  // Calling context must have a real Document bound to a Page. This check is
  // ported from rMKSA (see http://crbug.com/456720).
  if (!document->GetPage()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kInvalidStateError,
            "The context provided is not associated with a page."));
  }

  if (execution_context->IsWorkerGlobalScope()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kInvalidStateError,
            "Encrypted Media decoding info not available in Worker context."));
  }

  if (!execution_context->IsSecureContext()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kSecurityError,
                          "Encrypted Media decoding info can only be "
                          "queried in a secure context."));
  }

  MediaCapabilitiesKeySystemConfiguration* key_system_config =
      configuration->keySystemConfiguration();
  if (!key_system_config->hasKeySystem() ||
      key_system_config->keySystem().IsEmpty()) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(
            script_state->GetIsolate(), "The key system String is not valid."));
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
    // Set the robustness attribute to
    // config.keySystemConfiguration.audioRobustness.
    audio_capability->setRobustness(key_system_config->audioRobustness());

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
    // Set the robustness attribute to
    // config.keySystemConfiguration.videoRobustness.
    video_capability->setRobustness(key_system_config->videoRobustness());

    eme_config->setVideoCapabilities(
        HeapVector<Member<MediaKeySystemMediaCapability>>(1, video_capability));
  }

  HeapVector<Member<MediaKeySystemConfiguration>> config_vector(1, eme_config);

  MediaCapabilitiesKeySystemAccessInitializer* initializer =
      MakeGarbageCollected<MediaCapabilitiesKeySystemAccessInitializer>(
          script_state, key_system_config->keySystem(), config_vector,
          WTF::Bind(&MediaCapabilities::GetPerfInfo, WrapPersistent(this),
                    video_profile, WrapPersistent(configuration->video())));

  // IMPORTANT: Acquire the promise before potentially synchronously resolving
  // it in the code that follows. Otherwise the promise returned to JS will be
  // undefined. See comment above Promise() in script_promise_resolver.h
  ScriptPromise promise = initializer->Promise();

  MediaKeysController::From(document->GetPage())
      ->EncryptedMediaClient(execution_context)
      ->RequestMediaKeySystemAccess(WebEncryptedMediaRequest(initializer));

  return promise;
}

void MediaCapabilities::GetPerfInfo(media::VideoCodecProfile video_profile,
                                    const VideoConfiguration* video_config,
                                    ScriptPromiseResolver* resolver,
                                    MediaKeySystemAccess* access) {
  ExecutionContext* execution_context = resolver->GetExecutionContext();
  if (!execution_context || execution_context->IsContextDestroyed())
    return;

  if (!video_config) {
    // Audio-only is always smooth and power efficient.
    MediaCapabilitiesDecodingInfo* info = CreateDecodingInfoWith(true);
    info->setKeySystemAccess(access);
    resolver->Resolve(info);
    return;
  }

  String key_system = "";
  bool use_hw_secure_codecs = false;

  if (access) {
    key_system = access->keySystem();
    use_hw_secure_codecs = access->UseHardwareSecureCodecs();
  }

  if (!EnsureService(execution_context)) {
    resolver->Resolve(WrapPersistent(CreateDecodingInfoWith(false)));
    return;
  }

  media::mojom::blink::PredictionFeaturesPtr features =
      media::mojom::blink::PredictionFeatures::New(
          static_cast<media::mojom::blink::VideoCodecProfile>(video_profile),
          WebSize(video_config->width(), video_config->height()),
          ComputeFrameRate(video_config->framerate()), key_system,
          use_hw_secure_codecs);

  decode_history_service_->GetPerfInfo(
      std::move(features),
      WTF::Bind(&MediaCapabilities::OnPerfInfo, WrapPersistent(this),
                WrapPersistent(resolver), WrapPersistent(access)));
}

void MediaCapabilities::OnPerfInfo(ScriptPromiseResolver* resolver,
                                   MediaKeySystemAccess* access,
                                   bool is_smooth,
                                   bool is_power_efficient) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  Persistent<MediaCapabilitiesDecodingInfo> info(
      MediaCapabilitiesDecodingInfo::Create());
  info->setSupported(true);
  info->setSmooth(is_smooth);
  info->setPowerEfficient(is_power_efficient);
  info->setKeySystemAccess(access);

  resolver->Resolve(std::move(info));
}

}  // namespace blink
