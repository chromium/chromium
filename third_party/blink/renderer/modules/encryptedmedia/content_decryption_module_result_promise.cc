// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encryptedmedia/content_decryption_module_result_promise.h"

#include "media/base/key_systems.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/platform/web_content_decryption_module.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

void WebCdmExceptionToPromiseRejection(
    ScriptPromiseResolverBase* resolver,
    WebContentDecryptionModuleException cdm_exception,
    const String& message) {
  switch (cdm_exception) {
    case kWebContentDecryptionModuleExceptionTypeError:
      resolver->RejectWithTypeError(message);
      return;
    case kWebContentDecryptionModuleExceptionNotSupportedError:
      resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                       message);
      return;
    case kWebContentDecryptionModuleExceptionInvalidStateError:
      resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                       message);
      return;
    case kWebContentDecryptionModuleExceptionQuotaExceededError:
      resolver->RejectWithDOMException(DOMExceptionCode::kQuotaExceededError,
                                       message);
      return;
  }

  NOTREACHED_IN_MIGRATION();
}

ContentDecryptionModuleResultPromise::ContentDecryptionModuleResultPromise(
    ScriptPromiseResolverBase* resolver,
    const MediaKeysConfig& config,
    EmeApiType api_type)
    : resolver_(resolver), config_(config), api_type_(api_type) {}

ContentDecryptionModuleResultPromise::~ContentDecryptionModuleResultPromise() =
    default;

void ContentDecryptionModuleResultPromise::Complete() {
  NOTREACHED_IN_MIGRATION();
  if (!IsValidToFulfillPromise())
    return;
  resolver_->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                    "Unexpected completion.");
  resolver_.Clear();
}

void ContentDecryptionModuleResultPromise::CompleteWithContentDecryptionModule(
    std::unique_ptr<WebContentDecryptionModule> cdm) {
  NOTREACHED_IN_MIGRATION();
  if (!IsValidToFulfillPromise())
    return;
  resolver_->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                    "Unexpected completion.");
  resolver_.Clear();
}

void ContentDecryptionModuleResultPromise::CompleteWithSession(
    WebContentDecryptionModuleResult::SessionStatus status) {
  NOTREACHED_IN_MIGRATION();
  if (!IsValidToFulfillPromise())
    return;
  resolver_->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                    "Unexpected completion.");
  resolver_.Clear();
}

void ContentDecryptionModuleResultPromise::CompleteWithKeyStatus(
    WebEncryptedMediaKeyInformation::KeyStatus) {
  if (!IsValidToFulfillPromise())
    return;
  resolver_->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                    "Unexpected completion.");
  resolver_.Clear();
}

void ContentDecryptionModuleResultPromise::CompleteWithError(
    WebContentDecryptionModuleException exception_code,
    uint32_t system_code,
    const WebString& error_message) {
  if (!IsValidToFulfillPromise())
    return;

  // Report Media.EME.ApiPromiseRejection UKM.
  auto* execution_context = GetExecutionContext();
  if (IsA<LocalDOMWindow>(execution_context)) {
    Document* document = To<LocalDOMWindow>(execution_context)->document();
    if (document) {
      ukm::builders::Media_EME_ApiPromiseRejection builder(
          document->UkmSourceID());
      builder.SetKeySystem(
          media::GetKeySystemIntForUKM(config_.key_system.Ascii()));
      builder.SetUseHardwareSecureCodecs(
          static_cast<int>(config_.use_hardware_secure_codecs));
      builder.SetApi(static_cast<int>(api_type_));
      builder.SetSystemCode(system_code);
      builder.Record(document->UkmRecorder());
    }
  }

  // Non-zero |system_code| is appended to the |error_message|. If the
  // |error_message| is empty, we'll report "Rejected with system code
  // (|system_code|)".
  StringBuilder result;
  result.Append(error_message);
  if (system_code != 0) {
    if (result.empty())
      result.Append("Rejected with system code");
    result.Append(" (");
    result.AppendNumber(system_code);
    result.Append(')');
  }

  WebCdmExceptionToPromiseRejection(resolver_, exception_code,
                                    result.ToString());
  resolver_.Clear();
}

ExecutionContext* ContentDecryptionModuleResultPromise::GetExecutionContext()
    const {
  return resolver_->GetExecutionContext();
}

bool ContentDecryptionModuleResultPromise::IsValidToFulfillPromise() {
  // getExecutionContext() is no longer valid once the context is destroyed.
  // isContextDestroyed() is called to see if the context is in the
  // process of being destroyed. If it is, there is no need to fulfill this
  // promise which is about to go away anyway.
  return GetExecutionContext() && !GetExecutionContext()->IsContextDestroyed();
}

MediaKeysConfig ContentDecryptionModuleResultPromise::GetMediaKeysConfig() {
  return config_;
}

void ContentDecryptionModuleResultPromise::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
  ContentDecryptionModuleResult::Trace(visitor);
}

}  // namespace blink
