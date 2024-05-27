// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/digital_identity_credential.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_object_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_provider_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_request_provider.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/scoped_abort_state.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"  // IWYU pragma: keep
#include "third_party/blink/renderer/modules/credentialmanagement/credential_utils.h"
#include "third_party/blink/renderer/modules/credentialmanagement/digital_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/identity_credential.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/to_blink_string.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace {

using mojom::blink::RequestDigitalIdentityStatus;

// Abort an ongoing WebIdentityDigitalCredential request. This will only be
// called before the request finishes due to `scoped_abort_state`.
void AbortRequest(ScriptState* script_state) {
  if (!script_state->ContextIsValid()) {
    return;
  }

  CredentialManagerProxy::From(script_state)->DigitalIdentityRequest()->Abort();
}

String ValidateAndStringifyObject(v8::Isolate* isolate,
                                  const ScriptValue& input,
                                  ExceptionState& exception_state) {
  v8::Local<v8::String> value;
  if (input.IsEmpty() || !input.V8Value()->IsObject() ||
      !v8::JSON::Stringify(isolate->GetCurrentContext(),
                           input.V8Value().As<v8::Object>())
           .ToLocal(&value)) {
    exception_state.ThrowTypeError(
        "IdentityRequestProvider request objects should either by strings or "
        "JSON-Serializable objects.");
    return String();
  }

  String output = ToBlinkString<String>(isolate, value, kDoNotExternalize);

  // Implementation defined constant controlling the allowed JSON length.
  static constexpr size_t kMaxJSONStringLength = 1024 * 1024;

  if (output.length() > kMaxJSONStringLength) {
    exception_state.ThrowTypeError(
        String::Format("JSON serialization of IdentityRequestProvider request "
                       "objects should be no longer than %zu characters",
                       kMaxJSONStringLength));
    return String();
  }

  return output;
}

void OnCompleteRequest(ScriptPromiseResolver<IDLNullable<Credential>>* resolver,
                       std::unique_ptr<ScopedAbortState> scoped_abort_state,
                       const WTF::String& protocol,
                       RequestDigitalIdentityStatus status,
                       const WTF::String& token) {
  switch (status) {
    case RequestDigitalIdentityStatus::kErrorTooManyRequests: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "Only one navigator.credentials.get request may be outstanding at "
          "one time."));
      return;
    }
    case RequestDigitalIdentityStatus::kErrorCanceled: {
      AbortSignal* signal =
          scoped_abort_state ? scoped_abort_state->Signal() : nullptr;
      if (signal && signal->aborted()) {
        auto* script_state = resolver->GetScriptState();
        ScriptState::Scope script_state_scope(script_state);
        resolver->Reject(signal->reason(script_state));
      } else {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kAbortError, "The request has been aborted."));
      }
      return;
    }
    case RequestDigitalIdentityStatus::kError: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNetworkError, "Error retrieving a token."));
      return;
    }
    case RequestDigitalIdentityStatus::kSuccess: {
      UseCounter::Count(resolver->GetExecutionContext(),
                        WebFeature::kIdentityDigitalCredentialsSuccess);

      DigitalCredential* credential =
          DigitalCredential::Create(protocol, token);
      resolver->Resolve(credential);
      return;
    }
  }
}

}  // anonymous namespace

bool IsDigitalIdentityCredentialType(const CredentialRequestOptions& options) {
  return options.hasDigital();
}

ScriptPromise<IDLNullable<Credential>>
DiscoverDigitalIdentityCredentialFromExternalSource(
    ScriptPromiseResolver<IDLNullable<Credential>>* resolver,
    ExceptionState& exception_state,
    const CredentialRequestOptions& options) {
  CHECK(IsDigitalIdentityCredentialType(options));
  CHECK(RuntimeEnabledFeatures::WebIdentityDigitalCredentialsEnabled(
      resolver->GetExecutionContext()));

  if (!CheckGenericSecurityRequirementsForCredentialsContainerRequest(
          resolver)) {
    return resolver->Promise();
  }

  size_t num_providers = options.digital()->hasProviders()
                             ? options.digital()->providers().size()
                             : 0u;

  if (num_providers == 0) {
    resolver->RejectWithTypeError(
        "Digital identity API needs at least one provider.");
    return resolver->Promise();
  }

  // TODO(https://crbug.com/1416939): make sure the Digital Credentials
  // API works well with the Multiple IdP API.
  if (num_providers > 1u) {
    resolver->RejectWithTypeError(
        "Digital identity API currently does not support multiple "
        "providers.");
    return resolver->Promise();
  }

  if (!IsSameSecurityOriginWithAncestors(
          To<LocalDOMWindow>(resolver->GetExecutionContext())->GetFrame())) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError,
        "The digital identity credential can only be requested in a "
        "document which is same-origin with all of its ancestors.");
    return resolver->Promise();
  }

  auto provider = options.digital()->providers()[0];
  V8UnionObjectOrString* request_object_or_string = provider->request();

  String stringified_request;
  if (request_object_or_string->IsString()) {
    stringified_request = request_object_or_string->GetAsString();
  } else {
    stringified_request = ValidateAndStringifyObject(
        resolver->GetExecutionContext()->GetIsolate(),
        request_object_or_string->GetAsObject(), exception_state);
  }

  if (exception_state.HadException()) {
    resolver->Reject(exception_state);
    return resolver->Promise();
  }

  UseCounter::Count(resolver->GetExecutionContext(),
                    WebFeature::kIdentityDigitalCredentials);

  auto* signal = options.getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "Request has been aborted"));
    return resolver->Promise();
  }

  ScriptState* script_state = resolver->GetScriptState();
  std::unique_ptr<ScopedAbortState> scoped_abort_state;
  if (signal) {
    auto callback = WTF::BindOnce(&AbortRequest, WrapPersistent(script_state));
    auto* handle = signal->AddAlgorithm(std::move(callback));
    scoped_abort_state = std::make_unique<ScopedAbortState>(signal, handle);
  }

  blink::mojom::blink::DigitalCredentialProviderPtr
      digital_credential_provider =
          blink::mojom::blink::DigitalCredentialProvider::New();
  digital_credential_provider->protocol = provider->protocol();
  digital_credential_provider->request = stringified_request;

  auto* request =
      CredentialManagerProxy::From(script_state)->DigitalIdentityRequest();
  request->Request(
      std::move(digital_credential_provider),
      WTF::BindOnce(&OnCompleteRequest, WrapPersistent(resolver),
                    std::move(scoped_abort_state), provider->protocol()));
  return resolver->Promise();
}

}  // namespace blink
