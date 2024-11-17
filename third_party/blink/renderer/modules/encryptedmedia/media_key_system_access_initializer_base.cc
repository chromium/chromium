// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encryptedmedia/media_key_system_access_initializer_base.h"

#include "base/metrics/histogram_functions.h"
#include "media/base/eme_constants.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_key_system_media_capability.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/encryptedmedia/encrypted_media_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace {

static WebVector<media::EmeInitDataType> ConvertInitDataTypes(
    const Vector<String>& init_data_types) {
  WebVector<media::EmeInitDataType> result(init_data_types.size());
  for (wtf_size_t i = 0; i < init_data_types.size(); ++i)
    result[i] = EncryptedMediaUtils::ConvertToInitDataType(init_data_types[i]);
  return result;
}

static WebMediaKeySystemMediaCapability::EncryptionScheme
ConvertEncryptionScheme(const String& encryption_scheme) {
  if (encryption_scheme == "cenc")
    return WebMediaKeySystemMediaCapability::EncryptionScheme::kCenc;
  if (encryption_scheme == "cbcs")
    return WebMediaKeySystemMediaCapability::EncryptionScheme::kCbcs;
  if (encryption_scheme == "cbcs-1-9")
    return WebMediaKeySystemMediaCapability::EncryptionScheme::kCbcs_1_9;

  // Any other strings are not recognized (and therefore not supported).
  return WebMediaKeySystemMediaCapability::EncryptionScheme::kUnrecognized;
}

static WebVector<WebMediaKeySystemMediaCapability> ConvertCapabilities(
    const HeapVector<Member<MediaKeySystemMediaCapability>>& capabilities) {
  WebVector<WebMediaKeySystemMediaCapability> result(capabilities.size());
  for (wtf_size_t i = 0; i < capabilities.size(); ++i) {
    const WebString& content_type = capabilities[i]->contentType();
    result[i].content_type = content_type;
    ParsedContentType type(content_type);
    if (type.IsValid() && !type.GetParameters().HasDuplicatedNames()) {
      // From
      // http://w3c.github.io/encrypted-media/#get-supported-capabilities-for-audio-video-type
      // "If the user agent does not recognize one or more parameters,
      // continue to the next iteration." There is no way to enumerate the
      // parameters, so only look up "codecs" if a single parameter is
      // present. Chromium expects "codecs" to be provided, so this capability
      // will be skipped if codecs is not the only parameter specified.
      result[i].mime_type = type.MimeType();
      if (type.GetParameters().ParameterCount() == 1u)
        result[i].codecs = type.ParameterValueForName("codecs");
    }

    result[i].robustness = capabilities[i]->robustness();
    result[i].encryption_scheme =
        (capabilities[i]->hasEncryptionScheme() &&
         !capabilities[i]->encryptionScheme().IsNull())
            ? ConvertEncryptionScheme(capabilities[i]->encryptionScheme())
            : WebMediaKeySystemMediaCapability::EncryptionScheme::kNotSpecified;
  }
  return result;
}

static WebVector<WebEncryptedMediaSessionType> ConvertSessionTypes(
    const Vector<String>& session_types) {
  WebVector<WebEncryptedMediaSessionType> result(session_types.size());
  for (wtf_size_t i = 0; i < session_types.size(); ++i)
    result[i] = EncryptedMediaUtils::ConvertToSessionType(session_types[i]);
  return result;
}

}  // namespace

MediaKeySystemAccessInitializerBase::MediaKeySystemAccessInitializerBase(
    ExecutionContext* context,
    ScriptPromiseResolverBase* resolver,
    const String& key_system,
    const HeapVector<Member<MediaKeySystemConfiguration>>&
        supported_configurations,
    bool is_from_media_capabilities)
    : ExecutionContextClient(context),
      resolver_(resolver),
      key_system_(key_system),
      supported_configurations_(supported_configurations.size()),
      is_from_media_capabilities_(is_from_media_capabilities) {
  for (wtf_size_t i = 0; i < supported_configurations.size(); ++i) {
    const MediaKeySystemConfiguration* config = supported_configurations[i];
    WebMediaKeySystemConfiguration web_config;

    DCHECK(config->hasInitDataTypes());
    web_config.init_data_types = ConvertInitDataTypes(config->initDataTypes());

    DCHECK(config->hasAudioCapabilities());
    web_config.audio_capabilities =
        ConvertCapabilities(config->audioCapabilities());

    DCHECK(config->hasVideoCapabilities());
    web_config.video_capabilities =
        ConvertCapabilities(config->videoCapabilities());

    DCHECK(config->hasDistinctiveIdentifier());
    web_config.distinctive_identifier =
        EncryptedMediaUtils::ConvertToMediaKeysRequirement(
            config->distinctiveIdentifier().AsEnum());

    DCHECK(config->hasPersistentState());
    web_config.persistent_state =
        EncryptedMediaUtils::ConvertToMediaKeysRequirement(
            config->persistentState().AsEnum());

    if (config->hasSessionTypes()) {
      web_config.session_types = ConvertSessionTypes(config->sessionTypes());
    } else {
      // From the spec
      // (http://w3c.github.io/encrypted-media/#idl-def-mediakeysystemconfiguration):
      // If this member is not present when the dictionary is passed to
      // requestMediaKeySystemAccess(), the dictionary will be treated
      // as if this member is set to [ "temporary" ].
      WebVector<WebEncryptedMediaSessionType> session_types(
          static_cast<size_t>(1));
      session_types[0] = WebEncryptedMediaSessionType::kTemporary;
      web_config.session_types = session_types;
    }

    // If |label| is not present, it will be a null string.
    web_config.label = config->label();
    supported_configurations_[i] = web_config;
  }

  GenerateWarningAndReportMetrics();
}

const SecurityOrigin* MediaKeySystemAccessInitializerBase::GetSecurityOrigin()
    const {
  return IsExecutionContextValid() ? GetExecutionContext()->GetSecurityOrigin()
                                   : nullptr;
}

void MediaKeySystemAccessInitializerBase::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
  EncryptedMediaRequest::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

bool MediaKeySystemAccessInitializerBase::IsExecutionContextValid() const {
  // isContextDestroyed() is called to see if the context is in the
  // process of being destroyed. If it is true, assume the context is no
  // longer valid as it is about to be destroyed anyway.
  ExecutionContext* context = GetExecutionContext();
  return context && !context->IsContextDestroyed();
}

void MediaKeySystemAccessInitializerBase::GenerateWarningAndReportMetrics()
    const {
  const char kWidevineKeySystem[] = "com.widevine.alpha";
  const char kWidevineHwSecureAllRobustness[] = "HW_SECURE_ALL";

  // Only check for widevine key system for now.
  if (KeySystem() != kWidevineKeySystem)
    return;

  bool has_video_capabilities = false;
  bool has_empty_robustness = false;
  bool has_hw_secure_all = false;

  for (const auto& config : supported_configurations_) {
    for (const auto& capability : config.video_capabilities) {
      has_video_capabilities = true;
      if (capability.robustness.IsEmpty()) {
        has_empty_robustness = true;
      } else if (capability.robustness == kWidevineHwSecureAllRobustness) {
        has_hw_secure_all = true;
      }

      if (has_empty_robustness && has_hw_secure_all)
        break;
    }

    if (has_empty_robustness && has_hw_secure_all)
      break;
  }

  if (has_video_capabilities) {
    base::UmaHistogramBoolean(
        "Media.EME.Widevine.VideoCapability.HasEmptyRobustness",
        has_empty_robustness);
  }

  if (has_empty_robustness) {
    // TODO(xhwang): Write a best practice doc explaining details about risks of
    // using an empty robustness here, and provide the link to the doc in this
    // message. See http://crbug.com/720013
    GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kWarning,
            "It is recommended that a robustness level be specified. Not "
            "specifying the robustness level could result in unexpected "
            "behavior."));
  }

  if (!DomWindow())
    return;

  LocalFrame* frame = DomWindow()->GetFrame();
  ukm::builders::Media_EME_RequestMediaKeySystemAccess builder(
      DomWindow()->UkmSourceID());
  builder.SetKeySystem(KeySystemForUkmLegacy::kWidevine);
  builder.SetIsAdFrame(static_cast<int>(frame->IsAdFrame()));
  builder.SetIsCrossOrigin(
      static_cast<int>(frame->IsCrossOriginToOutermostMainFrame()));
  builder.SetIsTopFrame(static_cast<int>(frame->IsOutermostMainFrame()));
  builder.SetVideoCapabilities(static_cast<int>(has_video_capabilities));
  builder.SetVideoCapabilities_HasEmptyRobustness(
      static_cast<int>(has_empty_robustness));
  builder.SetVideoCapabilities_HasHwSecureAllRobustness(
      static_cast<int>(has_hw_secure_all));
  builder.SetIsFromMediaCapabilities(
      static_cast<int>(is_from_media_capabilities_));
  builder.Record(DomWindow()->UkmRecorder());
}

}  // namespace blink
