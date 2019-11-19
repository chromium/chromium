// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encryptedmedia/navigator_request_media_key_system_access.h"

#include <algorithm>

#include "base/macros.h"
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
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_system_access_initializer_base.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_keys_controller.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/encrypted_media_request.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// This class allows capabilities to be checked and a MediaKeySystemAccess
// object to be created asynchronously.
class MediaKeySystemAccessInitializer final
    : public MediaKeySystemAccessInitializerBase {
 public:
  MediaKeySystemAccessInitializer(
      ScriptState*,
      const String& key_system,
      const HeapVector<Member<MediaKeySystemConfiguration>>&
          supported_configurations);
  ~MediaKeySystemAccessInitializer() override = default;

  // EncryptedMediaRequest implementation.
  void RequestSucceeded(
      std::unique_ptr<WebContentDecryptionModuleAccess>) override;
  void RequestNotSupported(const WebString& error_message) override;

  void Trace(blink::Visitor* visitor) override {
    MediaKeySystemAccessInitializerBase::Trace(visitor);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaKeySystemAccessInitializer);
};

MediaKeySystemAccessInitializer::MediaKeySystemAccessInitializer(
    ScriptState* script_state,
    const String& key_system,
    const HeapVector<Member<MediaKeySystemConfiguration>>&
        supported_configurations)
    : MediaKeySystemAccessInitializerBase(script_state,
                                          key_system,
                                          supported_configurations) {}

void MediaKeySystemAccessInitializer::RequestSucceeded(
    std::unique_ptr<WebContentDecryptionModuleAccess> access) {
  DVLOG(3) << __func__;

  if (!IsExecutionContextValid())
    return;

  resolver_->Resolve(
      MakeGarbageCollected<MediaKeySystemAccess>(std::move(access)));
  resolver_.Clear();
}

void MediaKeySystemAccessInitializer::RequestNotSupported(
    const WebString& error_message) {
  DVLOG(3) << __func__ << " error: " << error_message.Ascii();

  if (!IsExecutionContextValid())
    return;

  resolver_->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError, error_message));
  resolver_.Clear();
}

}  // namespace

ScriptPromise NavigatorRequestMediaKeySystemAccess::requestMediaKeySystemAccess(
    ScriptState* script_state,
    Navigator& navigator,
    const String& key_system,
    const HeapVector<Member<MediaKeySystemConfiguration>>&
        supported_configurations) {
  DVLOG(3) << __func__;

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  Document* document = To<Document>(execution_context);

  if (!document->IsFeatureEnabled(mojom::FeaturePolicyFeature::kEncryptedMedia,
                                  ReportOptions::kReportOnFailure)) {
    UseCounter::Count(document,
                      WebFeature::kEncryptedMediaDisabledByFeaturePolicy);
    document->AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                               mojom::ConsoleMessageLevel::kWarning,
                               kEncryptedMediaFeaturePolicyConsoleWarning));
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(
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
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kInvalidStateError,
            "The context provided is not associated with a page."));
  }

  UseCounter::Count(*document, WebFeature::kEncryptedMediaSecureOrigin);
  document->CountUseOnlyInCrossOriginIframe(
      WebFeature::kEncryptedMediaCrossOriginIframe);

  // 4. Let origin be the origin of document.
  //    (Passed with the execution context.)

  // 5. Let promise be a new promise.
  MediaKeySystemAccessInitializer* initializer =
      MakeGarbageCollected<MediaKeySystemAccessInitializer>(
          script_state, key_system, supported_configurations);
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
