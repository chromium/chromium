// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encryptedmedia/navigator_request_media_key_system_access.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/platform/web_encrypted_media_client.h"
#include "third_party/blink/public/platform/web_encrypted_media_request.h"
#include "third_party/blink/public/platform/web_media_key_system_configuration.h"
#include "third_party/blink/public/platform/web_media_key_system_media_capability.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/encryptedmedia/encrypted_media_utils.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_session.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_system_access.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_keys_controller.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/encrypted_media_request.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

const char kEncryptedMediaFeaturePolicyConsoleWarning[] =
    "Encrypted Media access has been blocked because of a Feature Policy "
    "applied to the current document. See https://goo.gl/EuHzyv for more "
    "details.";

static WebVector<WebEncryptedMediaInitDataType> ConvertInitDataTypes(
    const Vector<String>& init_data_types) {
  WebVector<WebEncryptedMediaInitDataType> result(init_data_types.size());
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

  NOTREACHED();
  return WebMediaKeySystemMediaCapability::EncryptionScheme::kNotSpecified;
}

static WebVector<WebMediaKeySystemMediaCapability> ConvertCapabilities(
    const HeapVector<MediaKeySystemMediaCapability>& capabilities) {
  WebVector<WebMediaKeySystemMediaCapability> result(capabilities.size());
  for (wtf_size_t i = 0; i < capabilities.size(); ++i) {
    const WebString& content_type = capabilities[i].contentType();
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
    result[i].robustness = capabilities[i].robustness();

    // From
    // https://github.com/WICG/encrypted-media-encryption-scheme/blob/master/explainer.md
    // "Asking for "any" encryption scheme is unrealistic. Defining null as
    // "any scheme" is convenient for backward compatibility, though.
    // Applications which ignore this feature by leaving encryptionScheme null
    // get the same user agent behavior they did before this feature existed."
    result[i].encryption_scheme =
        capabilities[i].hasEncryptionScheme()
            ? ConvertEncryptionScheme(capabilities[i].encryptionScheme())
            : WebMediaKeySystemMediaCapability::EncryptionScheme::kNotSpecified;
  }
  return result;
}

static WebMediaKeySystemConfiguration::Requirement ConvertMediaKeysRequirement(
    const String& requirement) {
  if (requirement == "required")
    return WebMediaKeySystemConfiguration::Requirement::kRequired;
  if (requirement == "optional")
    return WebMediaKeySystemConfiguration::Requirement::kOptional;
  if (requirement == "not-allowed")
    return WebMediaKeySystemConfiguration::Requirement::kNotAllowed;

  // Everything else gets the default value.
  NOTREACHED();
  return WebMediaKeySystemConfiguration::Requirement::kOptional;
}

static WebVector<WebEncryptedMediaSessionType> ConvertSessionTypes(
    const Vector<String>& session_types) {
  WebVector<WebEncryptedMediaSessionType> result(session_types.size());
  for (wtf_size_t i = 0; i < session_types.size(); ++i)
    result[i] = EncryptedMediaUtils::ConvertToSessionType(session_types[i]);
  return result;
}

// This class allows capabilities to be checked and a MediaKeySystemAccess
// object to be created asynchronously.
class MediaKeySystemAccessInitializer final : public EncryptedMediaRequest {
  WTF_MAKE_NONCOPYABLE(MediaKeySystemAccessInitializer);

 public:
  MediaKeySystemAccessInitializer(
      ScriptState*,
      const String& key_system,
      const HeapVector<MediaKeySystemConfiguration>& supported_configurations);
  ~MediaKeySystemAccessInitializer() override = default;

  // EncryptedMediaRequest implementation.
  WebString KeySystem() const override { return key_system_; }
  const WebVector<WebMediaKeySystemConfiguration>& SupportedConfigurations()
      const override {
    return supported_configurations_;
  }
  const SecurityOrigin* GetSecurityOrigin() const override;
  void RequestSucceeded(WebContentDecryptionModuleAccess*) override;
  void RequestNotSupported(const WebString& error_message) override;

  ScriptPromise Promise() { return resolver_->Promise(); }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(resolver_);
    EncryptedMediaRequest::Trace(visitor);
  }

 private:
  // Returns true if the ExecutionContext is valid, false otherwise.
  bool IsExecutionContextValid() const;

  // For widevine key system, generate warning and report to UMA if
  // |m_supportedConfigurations| contains any video capability with empty
  // robustness string.
  void CheckVideoCapabilityRobustness() const;

  Member<ScriptPromiseResolver> resolver_;
  const String key_system_;
  WebVector<WebMediaKeySystemConfiguration> supported_configurations_;
};

MediaKeySystemAccessInitializer::MediaKeySystemAccessInitializer(
    ScriptState* script_state,
    const String& key_system,
    const HeapVector<MediaKeySystemConfiguration>& supported_configurations)
    : resolver_(ScriptPromiseResolver::Create(script_state)),
      key_system_(key_system),
      supported_configurations_(supported_configurations.size()) {
  for (wtf_size_t i = 0; i < supported_configurations.size(); ++i) {
    const MediaKeySystemConfiguration& config = supported_configurations[i];
    WebMediaKeySystemConfiguration web_config;

    DCHECK(config.hasInitDataTypes());
    web_config.init_data_types = ConvertInitDataTypes(config.initDataTypes());

    DCHECK(config.hasAudioCapabilities());
    web_config.audio_capabilities =
        ConvertCapabilities(config.audioCapabilities());

    DCHECK(config.hasVideoCapabilities());
    web_config.video_capabilities =
        ConvertCapabilities(config.videoCapabilities());

    DCHECK(config.hasDistinctiveIdentifier());
    web_config.distinctive_identifier =
        ConvertMediaKeysRequirement(config.distinctiveIdentifier());

    DCHECK(config.hasPersistentState());
    web_config.persistent_state =
        ConvertMediaKeysRequirement(config.persistentState());

    if (config.hasSessionTypes()) {
      web_config.session_types = ConvertSessionTypes(config.sessionTypes());
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
    web_config.label = config.label();
    supported_configurations_[i] = web_config;
  }

  CheckVideoCapabilityRobustness();
}

const SecurityOrigin* MediaKeySystemAccessInitializer::GetSecurityOrigin()
    const {
  return IsExecutionContextValid()
             ? resolver_->GetExecutionContext()->GetSecurityOrigin()
             : nullptr;
}

void MediaKeySystemAccessInitializer::RequestSucceeded(
    WebContentDecryptionModuleAccess* access) {
  if (!IsExecutionContextValid())
    return;

  resolver_->Resolve(
      new MediaKeySystemAccess(key_system_, base::WrapUnique(access)));
  resolver_.Clear();
}

void MediaKeySystemAccessInitializer::RequestNotSupported(
    const WebString& error_message) {
  if (!IsExecutionContextValid())
    return;

  resolver_->Reject(DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                         error_message));
  resolver_.Clear();
}

bool MediaKeySystemAccessInitializer::IsExecutionContextValid() const {
  // isContextDestroyed() is called to see if the context is in the
  // process of being destroyed. If it is true, assume the context is no
  // longer valid as it is about to be destroyed anyway.
  ExecutionContext* context = resolver_->GetExecutionContext();
  return context && !context->IsContextDestroyed();
}

void MediaKeySystemAccessInitializer::CheckVideoCapabilityRobustness() const {
  // Only check for widevine key system.
  if (KeySystem() != "com.widevine.alpha")
    return;

  bool has_video_capabilities = false;
  bool has_empty_robustness = false;

  for (const auto& config : supported_configurations_) {
    for (const auto& capability : config.video_capabilities) {
      has_video_capabilities = true;
      if (capability.robustness.IsEmpty()) {
        has_empty_robustness = true;
        break;
      }
    }

    if (has_empty_robustness)
      break;
  }

  if (has_video_capabilities) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        EnumerationHistogram, empty_robustness_histogram,
        ("Media.EME.Widevine.VideoCapability.HasEmptyRobustness", 2));
    empty_robustness_histogram.Count(has_empty_robustness);
  }

  if (has_empty_robustness) {
    // TODO(xhwang): Write a best practice doc explaining details about risks of
    // using an empty robustness here, and provide the link to the doc in this
    // message. See http://crbug.com/720013
    resolver_->GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
        kJSMessageSource, kWarningMessageLevel,
        "It is recommended that a robustness level be specified. Not "
        "specifying the robustness level could result in unexpected "
        "behavior."));
  }
}

}  // namespace

ScriptPromise NavigatorRequestMediaKeySystemAccess::requestMediaKeySystemAccess(
    ScriptState* script_state,
    Navigator& navigator,
    const String& key_system,
    const HeapVector<MediaKeySystemConfiguration>& supported_configurations) {
  DVLOG(3) << __func__;

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  Document* document = To<Document>(execution_context);

  if (!document->IsFeatureEnabled(mojom::FeaturePolicyFeature::kEncryptedMedia,
                                  ReportOptions::kReportOnFailure)) {
    UseCounter::Count(document,
                      WebFeature::kEncryptedMediaDisabledByFeaturePolicy);
    document->AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel,
                               kEncryptedMediaFeaturePolicyConsoleWarning));
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            DOMExceptionCode::kSecurityError,
            "requestMediaKeySystemAccess is disabled by feature policy."));
  }

  // From https://w3c.github.io/encrypted-media/#requestMediaKeySystemAccess
  // When this method is invoked, the user agent must run the following steps:
  // 1. If keySystem is the empty string, return a promise rejected with a
  //    newly created TypeError.
  if (key_system.IsEmpty()) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(script_state->GetIsolate(),
                                          "The keySystem parameter is empty."));
  }

  // 2. If supportedConfigurations is empty, return a promise rejected with
  //    a newly created TypeError.
  if (!supported_configurations.size()) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(),
                          "The supportedConfigurations parameter is empty."));
  }

  // 3. Let document be the calling context's Document.
  //    (Done at the begining of this function.)
  if (!document->GetPage()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            DOMExceptionCode::kInvalidStateError,
            "The context provided is not associated with a page."));
  }

  UseCounter::Count(*document, WebFeature::kEncryptedMediaSecureOrigin);
  UseCounter::CountCrossOriginIframe(
      *document, WebFeature::kEncryptedMediaCrossOriginIframe);

  // 4. Let origin be the origin of document.
  //    (Passed with the execution context.)

  // 5. Let promise be a new promise.
  MediaKeySystemAccessInitializer* initializer =
      new MediaKeySystemAccessInitializer(script_state, key_system,
                                          supported_configurations);
  ScriptPromise promise = initializer->Promise();

  // 6. Asynchronously determine support, and if allowed, create and
  //    initialize the MediaKeySystemAccess object.
  MediaKeysController* controller =
      MediaKeysController::From(document->GetPage());
  WebEncryptedMediaClient* media_client =
      controller->EncryptedMediaClient(execution_context);
  media_client->RequestMediaKeySystemAccess(
      WebEncryptedMediaRequest(initializer));

  // 7. Return promise.
  return promise;
}

}  // namespace blink
