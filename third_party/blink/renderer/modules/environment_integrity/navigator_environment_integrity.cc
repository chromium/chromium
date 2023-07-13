// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/environment_integrity/navigator_environment_integrity.h"

#include "build/build_config.h"
#include "third_party/blink/public/mojom/environment_integrity/environment_integrity_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/environment_integrity/environment_integrity.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/network/blink_schemeful_site.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

#if BUILDFLAG(IS_ANDROID)

namespace {

// Hash the script-provided content binding.
// Returns SHA256("<content_binding>;<eTLD+1>")
Vector<uint8_t> HashContentBinding(ExecutionContext* execution_context,
                                   const String& content_binding) {
  BlinkSchemefulSite schemeful_site =
      BlinkSchemefulSite(execution_context->GetSecurityOrigin());

  Digestor digestor(kHashAlgorithmSha256);
  digestor.UpdateUtf8(content_binding);
  if (!schemeful_site.IsOpaque()) {
    digestor.UpdateUtf8(";");
    digestor.UpdateUtf8(schemeful_site.Serialize());
  }

  DigestValue digest_result;
  digestor.Finish(digest_result);

  return Vector<uint8_t>(digest_result);
}

}  // namespace

NavigatorEnvironmentIntegrity::NavigatorEnvironmentIntegrity(
    Navigator& navigator)
    : Supplement(navigator),
      remote_environment_integrity_service_(navigator.GetExecutionContext()) {
  navigator.GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_environment_integrity_service_.BindNewPipeAndPassReceiver(
          navigator.GetExecutionContext()->GetTaskRunner(
              TaskType::kMiscPlatformAPI)));
}

#else

NavigatorEnvironmentIntegrity::NavigatorEnvironmentIntegrity(
    Navigator& navigator)
    : Supplement(navigator) {}

#endif

NavigatorEnvironmentIntegrity& NavigatorEnvironmentIntegrity::From(
    ExecutionContext* context,
    Navigator& navigator) {
  NavigatorEnvironmentIntegrity* supplement =
      Supplement<Navigator>::From<NavigatorEnvironmentIntegrity>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorEnvironmentIntegrity>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

const char NavigatorEnvironmentIntegrity::kSupplementName[] =
    "NavigatorEnvironmentIntegrity";

ScriptPromise NavigatorEnvironmentIntegrity::getEnvironmentIntegrity(
    ScriptState* script_state,
    const String& content_binding,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolverWithTracker<EnvironmentIntegrityResult>>(
      script_state, "Blink.EnvironmentIntegrity",
      /*timeout_interval=*/base::Seconds(10));
  ScriptPromise promise = resolver->Promise();

#if BUILDFLAG(IS_ANDROID)
  Vector<uint8_t> content_binding_bytes = HashContentBinding(
      GetSupplementable()->GetExecutionContext(), content_binding);
  remote_environment_integrity_service_->GetEnvironmentIntegrity(
      content_binding_bytes,
      WTF::BindOnce(&NavigatorEnvironmentIntegrity::ResolveEnvironmentIntegrity,
                    WrapPersistent(this), WrapPersistent(resolver)));
#else
  resolver->RecordAndThrowDOMException(
      exception_state, DOMExceptionCode::kUnknownError, "Unknown Error.",
      EnvironmentIntegrityResult::kNotSupported);
#endif

  return promise;
}

/* static */
ScriptPromise NavigatorEnvironmentIntegrity::getEnvironmentIntegrity(
    ScriptState* script_state,
    Navigator& navigator,
    const String& content_binding,
    ExceptionState& exception_state) {
  return From(ExecutionContext::From(script_state), navigator)
      .getEnvironmentIntegrity(script_state, content_binding, exception_state);
}

void NavigatorEnvironmentIntegrity::Trace(Visitor* visitor) const {
#if BUILDFLAG(IS_ANDROID)
  visitor->Trace(remote_environment_integrity_service_);
#endif
  Supplement<Navigator>::Trace(visitor);
}

#if BUILDFLAG(IS_ANDROID)
void NavigatorEnvironmentIntegrity::ResolveEnvironmentIntegrity(
    ScriptPromiseResolverWithTracker<EnvironmentIntegrityResult>* resolver,
    mojom::blink::EnvironmentIntegrityResponseCode response_code,
    const Vector<uint8_t>& token) {
  switch (response_code) {
    case mojom::blink::EnvironmentIntegrityResponseCode::kSuccess: {
      // Respond with the token data.
      DOMArrayBuffer* buffer =
          DOMArrayBuffer::Create(token.data(), token.size());
      EnvironmentIntegrity* environment_integrity =
          MakeGarbageCollected<EnvironmentIntegrity>(buffer);
      resolver->Resolve(environment_integrity, EnvironmentIntegrityResult::kOk);
      return;
    }

    case mojom::blink::EnvironmentIntegrityResponseCode::kTimeout: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
                           DOMExceptionCode::kTimeoutError, "Request Timeout."),
                       EnvironmentIntegrityResult::kTimedOut);
      return;
    }

    case mojom::blink::EnvironmentIntegrityResponseCode::kInternalError: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
                           DOMExceptionCode::kUnknownError, "Unknown Error."),
                       EnvironmentIntegrityResult::kUnknownError);
      return;
    }
  }
}
#endif

}  // namespace blink
