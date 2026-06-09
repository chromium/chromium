// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/digital_identity_credential.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"
#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_v8_value_converter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_create_request.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_get_request.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_request_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/scoped_abort_state.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"  // IWYU pragma: keep
#include "third_party/blink/renderer/modules/credentialmanagement/credential_utils.h"
#include "third_party/blink/renderer/modules/credentialmanagement/digital_credential.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

namespace {

using mojom::blink::RequestDigitalIdentityStatus;

enum class DigitalIdentityRequestType {
  kGet,
  kCreate,
};

// Abort an ongoing WebIdentityDigitalCredential request. This will only be
// called before the request finishes due to `scoped_abort_state`.
void AbortRequest(ScriptState* script_state) {
  if (!script_state->ContextIsValid()) {
    return;
  }

  CredentialManagerProxy::From(script_state)->DigitalIdentityRequest()->Abort();
}

// Converts a base::Value to a ScriptObject.
// Returns an empty ScriptObject on failure.
ScriptObject ValueToScriptObject(ScriptState* script_state,
                                 base::Value response) {
  std::unique_ptr<WebV8ValueConverter> converter =
      Platform::Current()->CreateWebV8ValueConverter();
  ScriptState::Scope script_state_scope(script_state);
  v8::Local<v8::Value> v8_response =
      converter->ToV8Value(response, script_state->GetContext());
  if (v8_response.IsEmpty() || !v8_response->IsObject()) {
    // Parsed value is not an object.
    return ScriptObject();
  }
  return ScriptObject(script_state->GetIsolate(), v8_response);
}

void OnCompleteRequest(ScriptPromiseResolver<IDLNullable<Credential>>* resolver,
                       std::unique_ptr<ScopedAbortState> scoped_abort_state,
                       DigitalIdentityRequestType request_type,
                       RequestDigitalIdentityStatus status,
                       const String& protocol,
                       base::Value token) {
  TRACE_EVENT("content.digitalcredentials", "OnCompleteRequest", "status",
              status, "request_type", request_type, "protocol", protocol);

  switch (status) {
    case RequestDigitalIdentityStatus::kErrorTooManyRequests: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "Only one navigator.credentials.get/create request may be "
          "outstanding at one time."));
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
          UNSAFE_TODO(
              String::Format("The '%s' feature requires transient activation.",
                             request_type == DigitalIdentityRequestType::kCreate
                                 ? "digital-credentials-create"
                                 : "digital-credentials-get"))));
      return;

    case RequestDigitalIdentityStatus::kErrorUserDeclined: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError, "Request is cancelled."));
      return;
    }
    case RequestDigitalIdentityStatus::kError: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNetworkError, "Error retrieving a token."));
      return;
    }
    case RequestDigitalIdentityStatus::kSuccess: {
      switch (request_type) {
        case DigitalIdentityRequestType::kGet:
          UseCounter::Count(resolver->GetExecutionContext(),
                            WebFeature::kIdentityDigitalCredentialsSuccess);
          break;
        case DigitalIdentityRequestType::kCreate:
          UseCounter::Count(
              resolver->GetExecutionContext(),
              WebFeature::kIdentityDigitalCredentialsCreationSuccess);
          break;
      }

      DigitalCredential* credential = DigitalCredential::Create(
          protocol,
          ValueToScriptObject(resolver->GetScriptState(), std::move(token)));
      resolver->Resolve(credential);
      return;
    }
  }
}
bool IsSerializable(ScriptState* script_state, ScriptObject data) {
  v8::Local<v8::String> data_string;
  v8::TryCatch try_catch(script_state->GetIsolate());
  return v8::JSON::Stringify(script_state->GetContext(), data.V8Object())
             .ToLocal(&data_string) &&
         !try_catch.HasCaught();
}

}  // anonymous namespace

bool CheckSupportedProtocol(ExecutionContext* execution_context,
                            const String& protocol,
                            DigitalCredentialExchangeType type) {
  struct ProtocolEntry {
    const char* protocol;
    WebFeature feature;
  };

  static constexpr ProtocolEntry kPresentationProtocols[] = {
      {"openid4vp-v1-unsigned",
       WebFeature::kDigitalCredentialsProtocolOpenId4VpUnsigned},
      {"openid4vp-v1-signed",
       WebFeature::kDigitalCredentialsProtocolOpenId4VpSigned},
      {"openid4vp-v1-multisigned",
       WebFeature::kDigitalCredentialsProtocolOpenId4VpMultisigned},
      {"org-iso-mdoc", WebFeature::kDigitalCredentialsProtocolOrgIsoMdoc},
  };

  static constexpr ProtocolEntry kIssuanceProtocols[] = {
      // TODO(crbug.com/521181022): Ensure this aligns to the specification
      {"openid4vci", WebFeature::kDigitalCredentialsProtocolOpenId4Vci},
      {"openid4vci-v1", WebFeature::kDigitalCredentialsProtocolOpenId4VciV1},
  };

  bool is_supported = false;
  WebFeature feature = WebFeature::kDigitalCredentialsProtocolUnknown;

  if (type == DigitalCredentialExchangeType::kPresentation ||
      type == DigitalCredentialExchangeType::kAny) {
    for (const auto& entry : kPresentationProtocols) {
      if (protocol == entry.protocol) {
        is_supported = true;
        feature = entry.feature;
        break;
      }
    }
  }

  if (!is_supported && (type == DigitalCredentialExchangeType::kIssuance ||
                        type == DigitalCredentialExchangeType::kAny)) {
    for (const auto& entry : kIssuanceProtocols) {
      if (protocol == entry.protocol) {
        is_supported = true;
        feature = entry.feature;
        break;
      }
    }
  }

  UseCounter::Count(execution_context, feature);

  if (!RuntimeEnabledFeatures::DigitalCredentialsProtocolFilterEnabled(
          execution_context)) {
    return true;
  }
  return is_supported;
}

bool IsDigitalIdentityCredentialType(const CredentialRequestOptions& options) {
  return options.hasDigital();
}

bool IsDigitalIdentityCredentialType(const CredentialCreationOptions& options) {
  return options.hasDigital();
}

void DiscoverDigitalIdentityCredentialFromExternalSource(
    ScriptPromiseResolver<IDLNullable<Credential>>* resolver,
    const CredentialRequestOptions& options) {
  TRACE_EVENT("content.digitalcredentials",
              "DiscoverDigitalIdentityCredentialFromExternalSource");
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
  std::unique_ptr<WebV8ValueConverter> converter =
      Platform::Current()->CreateWebV8ValueConverter();

  Vector<blink::mojom::blink::DigitalCredentialGetRequestPtr> requests;
  ScriptState* script_state = resolver->GetScriptState();
  for (const auto& request : options.digital()->requests()) {
    if (!CheckSupportedProtocol(resolver->GetExecutionContext(),
                                request->protocol(),
                                DigitalCredentialExchangeType::kPresentation)) {
      continue;
    }
    if (!IsSerializable(script_state, request->data())) {
      resolver->RejectWithTypeError(
          "Digital identity API requires valid JSON in the request.");
      return;
    }
    blink::mojom::blink::DigitalCredentialGetRequestPtr
        digital_credential_request =
            blink::mojom::blink::DigitalCredentialGetRequest::New();
    digital_credential_request->protocol = request->protocol();
    std::unique_ptr<base::Value> digital_credential_request_data =
        converter->FromV8Value(request->data().V8Object(),
                               resolver->GetScriptState()->GetContext());
    // The `ExecutionContext` might have been destroyed by malicious getters
    // during the V8-to-C++ object conversion above. Bail out if it was.
    if (!resolver->GetScriptState()->ContextIsValid()) {
      return;
    }
    if (!digital_credential_request_data) {
      return;
    }
    digital_credential_request->data =
        std::move(*digital_credential_request_data);
    requests.push_back(std::move(digital_credential_request));
  }

  if (requests.empty()) {
    resolver->RejectWithTypeError(
        "Digital Credentials API call with no well-formed allowed protocol "
        "requests.");
    return;
  }

  UseCounter::Count(resolver->GetExecutionContext(),
                    WebFeature::kIdentityDigitalCredentials);

  std::unique_ptr<ScopedAbortState> scoped_abort_state;
  if (auto* signal = options.getSignalOr(nullptr)) {
    auto callback = BindOnce(&AbortRequest, WrapPersistent(script_state));
    auto* handle = signal->AddAlgorithm(std::move(callback));
    scoped_abort_state = std::make_unique<ScopedAbortState>(signal, handle);
  }

  LocalDOMWindow* window = To<LocalDOMWindow>(resolver->GetExecutionContext());
  LocalFrame* local_frame = window->GetFrame();

  bool has_transient_user_activation =
      LocalFrame::ConsumeTransientUserActivation(
          local_frame, UserActivationUpdateSource::kRenderer);
  bool is_delegated = false;
  // True if the digital-credentials-get operation is allowed based on either
  // transient user activation or delegated capability.
  bool digital_credentials_get_allowed = has_transient_user_activation;

  if (!digital_credentials_get_allowed &&
      RuntimeEnabledFeatures::CapabilityDelegationDigitalCredentialsEnabled(
          window)) {
    is_delegated = window->ConsumeDigitalCredentialsGetToken();
    digital_credentials_get_allowed = is_delegated;
  }

  base::UmaHistogramBoolean(
      "Blink.DigitalCredentials.Get.HasTransientUserActivation",
      has_transient_user_activation);

  base::UmaHistogramBoolean("Blink.DigitalCredentials.Get.IsDelegated",
                            is_delegated);

  if (!digital_credentials_get_allowed) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        RuntimeEnabledFeatures::CapabilityDelegationDigitalCredentialsEnabled(
            window)
            ? "The 'digital-credentials-get' feature requires transient "
              "activation or delegated capability."
            : "The 'digital-credentials-get' feature requires transient "
              "activation."));
    return;
  }

  auto* request =
      CredentialManagerProxy::From(script_state)->DigitalIdentityRequest();
  request->Get(std::move(requests),
               blink::BindOnce(&OnCompleteRequest, WrapPersistent(resolver),
                               std::move(scoped_abort_state),
                               DigitalIdentityRequestType::kGet));
}

void CreateDigitalIdentityCredentialInExternalSource(
    ScriptPromiseResolver<IDLNullable<Credential>>* resolver,
    const CredentialCreationOptions& options) {
  TRACE_EVENT("content.digitalcredentials",
              "CreateDigitalIdentityCredentialInExternalSource");
  CHECK(IsDigitalIdentityCredentialType(options));
  CHECK(RuntimeEnabledFeatures::WebIdentityDigitalCredentialsCreationEnabled(
      resolver->GetExecutionContext()));

  if (!CheckGenericSecurityRequirementsForCredentialsContainerRequest(
          resolver)) {
    return;
  }

  if (!resolver->GetExecutionContext()->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::
              kDigitalCredentialsCreate)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "The 'digital-credentials-create' feature is not enabled in this "
        "document. Permissions Policy may be used to delegate digital "
        "credential API capabilities to cross-origin child frames."));
    return;
  }

  std::unique_ptr<WebV8ValueConverter> converter =
      Platform::Current()->CreateWebV8ValueConverter();

  Vector<blink::mojom::blink::DigitalCredentialCreateRequestPtr> requests;
  ScriptState* script_state = resolver->GetScriptState();
  for (const auto& request : options.digital()->requests()) {
    if (!CheckSupportedProtocol(resolver->GetExecutionContext(),
                                request->protocol(),
                                DigitalCredentialExchangeType::kIssuance)) {
      continue;
    }
    if (!IsSerializable(script_state, request->data())) {
      resolver->RejectWithTypeError(
          "Digital identity API requires valid JSON in the request.");
      return;
    }
    blink::mojom::blink::DigitalCredentialCreateRequestPtr
        digital_credential_request =
            blink::mojom::blink::DigitalCredentialCreateRequest::New();
    digital_credential_request->protocol = request->protocol();
    std::unique_ptr<base::Value> digital_credential_request_data =
        converter->FromV8Value(request->data().V8Object(),
                               resolver->GetScriptState()->GetContext());
    // The `ExecutionContext` might have been destroyed by malicious getters
    // during the V8-to-C++ object conversion above. Bail out if it was.
    if (!resolver->GetScriptState()->ContextIsValid()) {
      return;
    }
    if (!digital_credential_request_data) {
      continue;
    }
    digital_credential_request->data =
        std::move(*digital_credential_request_data);

    requests.push_back(std::move(digital_credential_request));
  }

  if (requests.empty()) {
    resolver->RejectWithTypeError(
        "Digital Credentials API call with no well-formed allowed protocol "
        "requests.");
    return;
  }

  UseCounter::Count(resolver->GetExecutionContext(),
                    WebFeature::kIdentityDigitalCredentialsCreation);

  std::unique_ptr<ScopedAbortState> scoped_abort_state;
  if (auto* signal = options.getSignalOr(nullptr)) {
    auto callback = BindOnce(&AbortRequest, WrapPersistent(script_state));
    auto* handle = signal->AddAlgorithm(std::move(callback));
    scoped_abort_state = std::make_unique<ScopedAbortState>(signal, handle);
  }

  LocalDOMWindow* window = To<LocalDOMWindow>(resolver->GetExecutionContext());
  LocalFrame* local_frame = window->GetFrame();

  bool has_transient_user_activation =
      LocalFrame::ConsumeTransientUserActivation(
          local_frame, UserActivationUpdateSource::kRenderer);
  bool is_delegated = false;
  // True if the digital-credentials-create operation is allowed based on either
  // transient user activation or delegated capability.
  bool digital_credentials_create_allowed = has_transient_user_activation;

  if (!digital_credentials_create_allowed &&
      RuntimeEnabledFeatures::CapabilityDelegationDigitalCredentialsEnabled(
          window)) {
    is_delegated = window->ConsumeDigitalCredentialsCreateToken();
    digital_credentials_create_allowed = is_delegated;
  }

  base::UmaHistogramBoolean(
      "Blink.DigitalCredentials.Create.HasTransientUserActivation",
      has_transient_user_activation);

  base::UmaHistogramBoolean("Blink.DigitalCredentials.Create.IsDelegated",
                            is_delegated);

  if (!digital_credentials_create_allowed) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        RuntimeEnabledFeatures::CapabilityDelegationDigitalCredentialsEnabled(
            window)
            ? "The 'digital-credentials-create' feature requires transient "
              "activation or delegated capability."
            : "The 'digital-credentials-create' feature requires transient "
              "activation."));
    return;
  }

  CredentialManagerProxy::From(script_state)
      ->DigitalIdentityRequest()
      ->Create(std::move(requests),
               blink::BindOnce(&OnCompleteRequest, WrapPersistent(resolver),
                               std::move(scoped_abort_state),
                               DigitalIdentityRequestType::kCreate));
}

}  // namespace blink
