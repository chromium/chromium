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
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_provider.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_digital_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_request_options.h"
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

void OnCompleteRequest(ScriptPromiseResolver<IDLNullable<Credential>>* resolver,
                       std::unique_ptr<ScopedAbortState> scoped_abort_state,
                       const WTF::String& protocol,
                       bool should_return_digital_credential,
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

      if (should_return_digital_credential) {
        DigitalCredential* credential =
            DigitalCredential::Create(protocol, token);
        resolver->Resolve(credential);
        return;
      }
      IdentityCredential* credential =
          IdentityCredential::Create(token, /*is_auto_selected=*/false);
      resolver->Resolve(credential);
      return;
    }
  }
}

}  // anonymous namespace

bool IsDigitalIdentityCredentialType(const CredentialRequestOptions& options) {
  if (options.hasIdentity()) {
    return options.identity()->hasProviders() &&
           base::ranges::any_of(options.identity()->providers(),
                                &IdentityProviderConfig::hasHolder);
  }
  return options.hasDigital();
}

ScriptPromise<IDLNullable<Credential>>
DiscoverDigitalIdentityCredentialFromExternalSource(
    ScriptState* script_state,
    ScriptPromiseResolver<IDLNullable<Credential>>* resolver,
    const CredentialRequestOptions& options,
    ExceptionState& exception_state) {
  CHECK(IsDigitalIdentityCredentialType(options));
  CHECK(RuntimeEnabledFeatures::WebIdentityDigitalCredentialsEnabled(
      resolver->GetExecutionContext()));

  if (!CheckGenericSecurityRequirementsForCredentialsContainerRequest(
          resolver)) {
    return resolver->Promise();
  }

  size_t num_providers = 0u;
  if (options.hasIdentity()) {
    num_providers = options.identity()->hasProviders()
                        ? options.identity()->providers().size()
                        : 0u;
  } else {
    num_providers = options.digital()->hasProviders()
                        ? options.digital()->providers().size()
                        : 0u;
  }

  if (num_providers == 0) {
    exception_state.ThrowTypeError(
        "Digital identity API needs at least one provider.");
    resolver->Detach();
    return ScriptPromise<IDLNullable<Credential>>();
  }

  // TODO(https://crbug.com/1416939): make sure the Digital Credentials
  // API works well with the Multiple IdP API.
  if (num_providers > 1u) {
    exception_state.ThrowTypeError(
        "Digital identity API currently does not support multiple "
        "providers.");
    resolver->Detach();
    return ScriptPromise<IDLNullable<Credential>>();
  }

  if (!IsSameSecurityOriginWithAncestors(
          To<LocalDOMWindow>(resolver->GetExecutionContext())->GetFrame())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "The digital identity credential can only be requested in a "
        "document which is same-origin with all of its ancestors.");
    resolver->Detach();
    return ScriptPromise<IDLNullable<Credential>>();
  }

  UseCounter::Count(resolver->GetExecutionContext(),
                    WebFeature::kIdentityDigitalCredentials);

  auto* signal = options.getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "Request has been aborted"));
    return resolver->Promise();
  }

  std::unique_ptr<ScopedAbortState> scoped_abort_state;
  if (signal) {
    auto callback = WTF::BindOnce(&AbortRequest, WrapPersistent(script_state));
    auto* handle = signal->AddAlgorithm(std::move(callback));
    scoped_abort_state = std::make_unique<ScopedAbortState>(signal, handle);
  }

  WTF::String protocol;
  blink::mojom::blink::DigitalCredentialProviderPtr digital_credential_provider;
  if (options.hasIdentity()) {
    digital_credential_provider =
        blink::mojom::blink::DigitalCredentialProvider::From(
            *options.identity()->providers()[0]->holder());
  } else if (options.hasDigital()) {
    digital_credential_provider =
        blink::mojom::blink::DigitalCredentialProvider::New();
    auto provider = options.digital()->providers()[0];
    if (provider->hasProtocol()) {
      digital_credential_provider->protocol = provider->protocol();
    }
    if (provider->hasRequest()) {
      digital_credential_provider->request = provider->request();
    }
    protocol = provider->protocol();
  }

  auto* request =
      CredentialManagerProxy::From(script_state)->DigitalIdentityRequest();
  request->Request(
      std::move(digital_credential_provider),
      WTF::BindOnce(&OnCompleteRequest, WrapPersistent(resolver),
                    std::move(scoped_abort_state), protocol,
                    /*should_return_digital_credential=*/options.hasDigital()));
  return resolver->Promise();
}

}  // namespace blink
