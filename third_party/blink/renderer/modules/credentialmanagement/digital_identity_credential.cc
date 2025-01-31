// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/digital_identity_credential.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/notreached.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"
#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_object_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_request.h"
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
#include "third_party/blink/renderer/platform/bindings/callback_method_retriever.h"
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

String ValidateAndStringifyObject(
    ScriptPromiseResolver<IDLNullable<Credential>>* resolver,
    const ScriptValue& input) {
  v8::Local<v8::String> value;
  if (input.IsEmpty() || !input.V8Value()->IsObject() ||
      !v8::JSON::Stringify(resolver->GetScriptState()->GetContext(),
                           input.V8Value().As<v8::Object>())
           .ToLocal(&value)) {
    resolver->RejectWithTypeError(
        "IdentityRequestProvider request objects should either by strings or "
        "JSON-Serializable objects.");
    return String();
  }

  String output = ToBlinkString<String>(
      resolver->GetScriptState()->GetIsolate(), value, kDoNotExternalize);

  // Implementation defined constant controlling the allowed JSON length.
  static constexpr size_t kMaxJSONStringLength = 1024 * 1024;

  if (output.length() > kMaxJSONStringLength) {
    resolver->RejectWithTypeError(
        String::Format("JSON serialization of IdentityRequestProvider request "
                       "objects should be no longer than %zu characters",
                       kMaxJSONStringLength));
    return String();
  }

  return output;
}

void OnCompleteRequest(ScriptPromiseResolver<IDLNullable<Credential>>* resolver,
                       std::unique_ptr<ScopedAbortState> scoped_abort_state,
                       RequestDigitalIdentityStatus status,
                       const WTF::String& protocol,
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
    case RequestDigitalIdentityStatus::kErrorNoRequests:
      resolver->RejectWithTypeError(
          "Digital identity API needs at least one request.");
      return;
    case RequestDigitalIdentityStatus::kErrorInvalidJson:
      resolver->RejectWithTypeError(
          "Digital identity API requires valid JSON in the request.");
      return;

    case RequestDigitalIdentityStatus::kErrorNoTransientUserActivation:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "The 'digital-credentials-get' feature requires transient "
          "activation."));
      return;

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

bool IsDigitalIdentityCredentialType(const CredentialCreationOptions& options) {
  return options.hasDigital();
}

void DiscoverDigitalIdentityCredentialFromExternalSource(
    ScriptPromiseResolver<IDLNullable<Credential>>* resolver,
    const CredentialRequestOptions& options,
    ExceptionState& exception_state) {
  CHECK(IsDigitalIdentityCredentialType(options));
  CHECK(RuntimeEnabledFeatures::WebIdentityDigitalCredentialsEnabled(
      resolver->GetExecutionContext()));

  if (!CheckGenericSecurityRequirementsForCredentialsContainerRequest(
          resolver)) {
    return;
  }

  if (!resolver->GetExecutionContext()->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kDigitalCredentialsGet)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "The 'digital-credentials-get' feature is not enabled in this "
        "document. Permissions Policy may be used to delegate digital "
        "credential API capabilities to cross-origin child frames."));
    return;
  }

  auto process_request =
      [resolver](
          V8UnionObjectOrString* request_object_or_string,
          const String& protocol,
          Vector<blink::mojom::blink::DigitalCredentialRequestPtr>& requests) {
        String stringified_request;
        if (request_object_or_string->IsString()) {
          stringified_request = request_object_or_string->GetAsString();
        } else {
          stringified_request = ValidateAndStringifyObject(
              resolver, request_object_or_string->GetAsObject());
          if (stringified_request.IsNull()) {
            return;
          }
        }

        blink::mojom::blink::DigitalCredentialRequestPtr
            digital_credential_request =
                blink::mojom::blink::DigitalCredentialRequest::New();
        digital_credential_request->protocol = protocol;
        digital_credential_request->data = stringified_request;
        requests.push_back(std::move(digital_credential_request));
      };

  mojom::blink::GetRequestFormat format =
      mojom::blink::GetRequestFormat::kModern;
  // When the new format is available (i.e. contains requests()), consider it,
  // otherwise use the old format (i.e. contains providers).
  Vector<blink::mojom::blink::DigitalCredentialRequestPtr> requests;
  if (options.digital()->hasRequests()) {
    format = mojom::blink::GetRequestFormat::kModern;
    for (const auto& request : options.digital()->requests()) {
      process_request(request->data(), request->protocol(), requests);
    }
  } else if (
      options.digital()->hasProviders() &&
      !RuntimeEnabledFeatures::
          WebIdentityDigitalCredentialsStopSupportingLegacyRequestFormatEnabled()) {
    format = mojom::blink::GetRequestFormat::kLegacy;
    for (const auto& provider : options.digital()->providers()) {
      process_request(provider->request(), provider->protocol(), requests);
    }
  }

  if (requests.empty()) {
    resolver->RejectWithTypeError(
        "Digital identity API needs at least one request.");
    return;
  }

  UseCounter::Count(resolver->GetExecutionContext(),
                    WebFeature::kIdentityDigitalCredentials);

  ScriptState* script_state = resolver->GetScriptState();
  std::unique_ptr<ScopedAbortState> scoped_abort_state;
  if (auto* signal = options.getSignalOr(nullptr)) {
    auto callback = WTF::BindOnce(&AbortRequest, WrapPersistent(script_state));
    auto* handle = signal->AddAlgorithm(std::move(callback));
    scoped_abort_state = std::make_unique<ScopedAbortState>(signal, handle);
  }

  auto* request =
      CredentialManagerProxy::From(script_state)->DigitalIdentityRequest();
  request->Get(std::move(requests), format,
               WTF::BindOnce(&OnCompleteRequest, WrapPersistent(resolver),
                             std::move(scoped_abort_state)));
}

void CreateDigitalIdentityCredentialInExternalSource(
    ScriptPromiseResolver<IDLNullable<Credential>>* resolver,
    const CredentialCreationOptions& options,
    ExceptionState& exception_state) {
  CHECK(IsDigitalIdentityCredentialType(options));
  CHECK(RuntimeEnabledFeatures::WebIdentityDigitalCredentialsCreationEnabled(
      resolver->GetExecutionContext()));

  if (!CheckGenericSecurityRequirementsForCredentialsContainerRequest(
          resolver)) {
    return;
  }

  DigitalCredentialCreationOptions* request = options.digital();
  blink::mojom::blink::DigitalCredentialRequestPtr digital_credential_request =
      blink::mojom::blink::DigitalCredentialRequest::New();
  digital_credential_request->protocol = request->protocol();
  String stringified_request_data =
      ValidateAndStringifyObject(resolver, request->data());
  if (stringified_request_data.IsNull()) {
    return;
  }
  digital_credential_request->data = stringified_request_data;

  UseCounter::Count(resolver->GetExecutionContext(),
                    WebFeature::kIdentityDigitalCredentials);

  ScriptState* script_state = resolver->GetScriptState();
  std::unique_ptr<ScopedAbortState> scoped_abort_state;
  if (auto* signal = options.getSignalOr(nullptr)) {
    auto callback = WTF::BindOnce(&AbortRequest, WrapPersistent(script_state));
    auto* handle = signal->AddAlgorithm(std::move(callback));
    scoped_abort_state = std::make_unique<ScopedAbortState>(signal, handle);
  }

  CredentialManagerProxy::From(script_state)
      ->DigitalIdentityRequest()
      ->Create(std::move(digital_credential_request),
               WTF::BindOnce(&OnCompleteRequest, WrapPersistent(resolver),
                             std::move(scoped_abort_state)));
}

}  // namespace blink
