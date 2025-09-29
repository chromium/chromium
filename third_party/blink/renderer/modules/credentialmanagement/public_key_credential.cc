// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/public_key_credential.h"

#include <utility>
#include <variant>

#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_all_accepted_credentials_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_response_js_on.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_current_user_details_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_registration_response_js_on.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_unknown_credential_options.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/credentialmanagement/authentication_credentials_container.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/json.h"
#include "third_party/blink/renderer/modules/credentialmanagement/scoped_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-value.h"

namespace blink {

namespace {

// https://www.w3.org/TR/webauthn/#dom-publickeycredential-type-slot:
constexpr char kPublicKeyCredentialType[] = "public-key";

void OnIsUserVerifyingComplete(ScriptPromiseResolver<IDLBoolean>* resolver,
                               bool available) {
  resolver->Resolve(available);
}

String AuthenticatorAttachmentToString(
    mojom::blink::AuthenticatorAttachment authenticator_attachment) {
  switch (authenticator_attachment) {
    case mojom::blink::AuthenticatorAttachment::PLATFORM:
      return "platform";
    case mojom::blink::AuthenticatorAttachment::CROSS_PLATFORM:
      return "cross-platform";
    case mojom::blink::AuthenticatorAttachment::NO_PREFERENCE:
      return g_null_atom;
  }
}

void OnGetClientCapabilitiesComplete(
    ScriptPromiseResolver<IDLRecord<IDLString, IDLBoolean>>* resolver,
    const Vector<mojom::blink::WebAuthnClientCapabilityPtr> capabilities) {
  Vector<std::pair<String, bool>> results;
  for (const auto& capability : capabilities) {
    results.emplace_back(std::move(capability->name), capability->supported);
  }

  // Extensions are added from the AuthenticationExtensionsClientInputs
  // dictionary defined in authentication_extensions_client_inputs.idl.
  // According to the specification, we should include a key for each
  // extension implemented by the client, formed by prefixing "extension:"
  // to the extension identifier.
  //
  // Excluded extensions: cableAuthentication, uvm, remoteDesktopClientOverride,
  // and supplementalPubKeys.
  results.emplace_back("extension:appid", true);
  results.emplace_back("extension:appidExclude", true);
  results.emplace_back("extension:hmacCreateSecret", true);
  results.emplace_back("extension:credentialProtectionPolicy", true);
  results.emplace_back("extension:enforceCredentialProtectionPolicy", true);
  results.emplace_back("extension:minPinLength", true);
  results.emplace_back("extension:credProps", true);
  results.emplace_back("extension:largeBlob", true);
  results.emplace_back("extension:credBlob", true);
  results.emplace_back("extension:getCredBlob", true);
  results.emplace_back(
      "extension:payment",
      RuntimeEnabledFeatures::SecurePaymentConfirmationEnabled());
  results.emplace_back("extension:prf", true);

  // Results should be sorted lexicographically based on the keys.
  std::sort(
      results.begin(), results.end(),
      [](const std::pair<String, bool>& a, const std::pair<String, bool>& b) {
        return CodeUnitCompare(a.first, b.first) < 0;
      });

  // TODO(crbug.com/393055190): Remove this when the feature is graduated from
  // origin trials.
  if (!RuntimeEnabledFeatures::WebAuthenticationImmediateGetEnabled(
          resolver->GetExecutionContext())) {
    for (wtf_size_t i = 0; i < results.size(); ++i) {
      if (results[i].first == "immediateGet") {
        results.EraseAt(i);
        break;
      }
    }
  }
  resolver->Resolve(std::move(results));
}

void OnSignalReportComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
    mojom::AuthenticatorStatus status,
    mojom::blink::WebAuthnDOMExceptionDetailsPtr dom_exception_details) {
  auto* resolver = scoped_resolver->Release()->DowncastTo<IDLUndefined>();
  if (status != mojom::blink::AuthenticatorStatus::SUCCESS) {
    resolver->Reject(
        AuthenticatorStatusToDOMException(status, dom_exception_details));
    return;
  }
  resolver->Resolve();
}

}  // namespace

PublicKeyCredential::PublicKeyCredential(
    const String& id,
    DOMArrayBuffer* raw_id,
    AuthenticatorResponse* response,
    mojom::blink::AuthenticatorAttachment authenticator_attachment,
    const AuthenticationExtensionsClientOutputs* extension_outputs,
    const String& type)
    : Credential(id, type.empty() ? kPublicKeyCredentialType : type),
      raw_id_(raw_id),
      response_(response),
      authenticator_attachment_(
          AuthenticatorAttachmentToString(authenticator_attachment)),
      extension_outputs_(extension_outputs) {}

// static
ScriptPromise<IDLRecord<IDLString, IDLBoolean>>
PublicKeyCredential::getClientCapabilities(ScriptState* script_state) {
  // Ignore calls if the current realm execution context is no longer valid,
  // e.g., because the responsible document was detached.
  if (!script_state->ContextIsValid()) {
    return ScriptPromise<IDLRecord<IDLString, IDLBoolean>>::
        RejectWithDOMException(
            script_state,
            MakeGarbageCollected<DOMException>(
                DOMExceptionCode::kInvalidStateError, "Context is detached"));
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLRecord<IDLString, IDLBoolean>>>(script_state);
  ScriptPromise promise = resolver->Promise();

  UseCounter::Count(resolver->GetExecutionContext(),
                    WebFeature::kWebAuthnGetClientCapabilities);

  auto* authenticator =
      CredentialManagerProxy::From(script_state)->Authenticator();
  authenticator->GetClientCapabilities(
      BindOnce(&OnGetClientCapabilitiesComplete, WrapPersistent(resolver)));
  return promise;
}

// static
ScriptPromise<IDLBoolean>
PublicKeyCredential::isUserVerifyingPlatformAuthenticatorAvailable(
    ScriptState* script_state) {
  // Ignore calls if the current realm execution context is no longer valid,
  // e.g., because the responsible document was detached.
  if (!script_state->ContextIsValid()) {
    return ScriptPromise<IDLBoolean>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "Context is detached"));
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(script_state);
  auto promise = resolver->Promise();

  UseCounter::Count(
      resolver->GetExecutionContext(),
      WebFeature::
          kCredentialManagerIsUserVerifyingPlatformAuthenticatorAvailable);

  auto* authenticator =
      CredentialManagerProxy::From(script_state)->Authenticator();
  authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(
      BindOnce(&OnIsUserVerifyingComplete, WrapPersistent(resolver)));
  return promise;
}

AuthenticationExtensionsClientOutputs*
PublicKeyCredential::getClientExtensionResults() const {
  return const_cast<AuthenticationExtensionsClientOutputs*>(
      extension_outputs_.Get());
}

// static
ScriptPromise<IDLBoolean> PublicKeyCredential::isConditionalMediationAvailable(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(script_state);
  auto promise = resolver->Promise();

  // Ignore calls if the current realm execution context is no longer valid,
  // e.g., because the responsible document was detached.
  DCHECK(resolver->GetExecutionContext());
  if (resolver->GetExecutionContext()->IsContextDestroyed()) {
    resolver->Reject();
    return promise;
  }
  UseCounter::Count(
      resolver->GetExecutionContext(),
      WebFeature::kCredentialManagerIsConditionalMediationAvailable);
  auto* authenticator =
      CredentialManagerProxy::From(script_state)->Authenticator();
  authenticator->IsConditionalMediationAvailable(
      BindOnce([](ScriptPromiseResolver<IDLBoolean>* resolver,
                  bool available) { resolver->Resolve(available); },
               WrapPersistent(resolver)));
  return promise;
}

v8::Local<v8::Object> PublicKeyCredential::toJSON(
    ScriptState* script_state) const {
  // PublicKeyCredential.response holds an AuthenticatorAttestationResponse, if
  // it was returned from a create call, or an AuthenticatorAssertionResponse
  // if returned from a get() call. In the former case, the spec wants us to
  // return a RegistrationResponseJSON, and in the latter an
  // AuthenticationResponseJSON.  We can't reflect the type of `response_`
  // though, so we serialize it to JSON first and branch on the result type.
  std::variant<AuthenticatorAssertionResponseJSON*,
               AuthenticatorAttestationResponseJSON*>
      response_json = response_->toJSON();

  v8::Local<v8::Value> result;
  std::visit(
      absl::Overload{
          [&](AuthenticatorAttestationResponseJSON* attestation_response) {
            auto* registration_response = RegistrationResponseJSON::Create();
            registration_response->setId(id());
            registration_response->setRawId(WebAuthnBase64UrlEncode(rawId()));
            registration_response->setResponse(attestation_response);
            if (!authenticator_attachment_.IsNull()) {
              registration_response->setAuthenticatorAttachment(
                  authenticator_attachment_);
            }
            registration_response->setClientExtensionResults(
                AuthenticationExtensionsClientOutputsToJSON(
                    script_state, *extension_outputs_));
            registration_response->setType(type());
            result = registration_response->ToV8(script_state);
          },
          [&](AuthenticatorAssertionResponseJSON* assertion_response) {
            auto* authentication_response =
                AuthenticationResponseJSON::Create();
            authentication_response->setId(id());
            authentication_response->setRawId(WebAuthnBase64UrlEncode(rawId()));
            authentication_response->setResponse(assertion_response);
            if (!authenticator_attachment_.IsNull()) {
              authentication_response->setAuthenticatorAttachment(
                  authenticator_attachment_);
            }
            authentication_response->setClientExtensionResults(
                AuthenticationExtensionsClientOutputsToJSON(
                    script_state, *extension_outputs_));
            authentication_response->setType(type());
            result = authentication_response->ToV8(script_state);
          }},
      response_json);
  CHECK(result->IsObject());
  return result.As<v8::Object>();
}

// static
const PublicKeyCredentialCreationOptions*
PublicKeyCredential::parseCreationOptionsFromJSON(
    ScriptState* script_state,
    const PublicKeyCredentialCreationOptionsJSON* options,
    ExceptionState& exception_state) {
  return PublicKeyCredentialCreationOptionsFromJSON(options, exception_state);
}

// static
const PublicKeyCredentialRequestOptions*
PublicKeyCredential::parseRequestOptionsFromJSON(
    ScriptState* script_state,
    const PublicKeyCredentialRequestOptionsJSON* options,
    ExceptionState& exception_state) {
  return PublicKeyCredentialRequestOptionsFromJSON(options, exception_state);
}

// static
ScriptPromise<IDLUndefined> PublicKeyCredential::signalUnknownCredential(
    ScriptState* script_state,
    const UnknownCredentialOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "Context is detached"));
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  Vector<uint8_t> decoded_cred_id;
  if (!Base64UnpaddedURLDecode(options->credentialId(), decoded_cred_id)) {
    resolver->RejectWithTypeError("Invalid base64url string for credentialId.");
    return promise;
  }
  mojom::blink::PublicKeyCredentialReportOptionsPtr mojo_options =
      mojom::blink::PublicKeyCredentialReportOptions::From(*options);
  auto* authenticator =
      CredentialManagerProxy::From(script_state)->Authenticator();
  authenticator->Report(
      std::move(mojo_options),
      BindOnce(&OnSignalReportComplete,
               std::make_unique<ScopedPromiseResolver>(resolver)));
  return promise;
}

// static
ScriptPromise<IDLUndefined> PublicKeyCredential::signalAllAcceptedCredentials(
    ScriptState* script_state,
    const AllAcceptedCredentialsOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "Context is detached"));
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  for (String credential_id : options->allAcceptedCredentialIds()) {
    Vector<uint8_t> decoded_cred_id;
    if (!Base64UnpaddedURLDecode(credential_id, decoded_cred_id)) {
      resolver->RejectWithTypeError(
          "Invalid base64url string for allAcceptedCredentialIds.");
      return promise;
    }
  }
  Vector<uint8_t> decoded_user_id;
  if (!Base64UnpaddedURLDecode(options->userId(), decoded_user_id)) {
    resolver->RejectWithTypeError("Invalid base64url string for userId.");
    return promise;
  }
  mojom::blink::PublicKeyCredentialReportOptionsPtr mojo_options =
      mojom::blink::PublicKeyCredentialReportOptions::From(*options);
  auto* authenticator =
      CredentialManagerProxy::From(script_state)->Authenticator();
  authenticator->Report(
      std::move(mojo_options),
      BindOnce(&OnSignalReportComplete,
               std::make_unique<ScopedPromiseResolver>(resolver)));
  return promise;
}

// static
ScriptPromise<IDLUndefined> PublicKeyCredential::signalCurrentUserDetails(
    ScriptState* script_state,
    const CurrentUserDetailsOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "Context is detached"));
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  Vector<uint8_t> decoded_user_id;
  if (!Base64UnpaddedURLDecode(options->userId(), decoded_user_id)) {
    resolver->RejectWithTypeError("Invalid base64url string for userId.");
    return promise;
  }
  mojom::blink::PublicKeyCredentialReportOptionsPtr mojo_options =
      mojom::blink::PublicKeyCredentialReportOptions::From(*options);
  auto* authenticator =
      CredentialManagerProxy::From(script_state)->Authenticator();
  authenticator->Report(
      std::move(mojo_options),
      BindOnce(&OnSignalReportComplete,
               std::make_unique<ScopedPromiseResolver>(resolver)));
  return promise;
}

void PublicKeyCredential::Trace(Visitor* visitor) const {
  visitor->Trace(raw_id_);
  visitor->Trace(response_);
  visitor->Trace(extension_outputs_);
  Credential::Trace(visitor);
}

bool PublicKeyCredential::IsPublicKeyCredential() const {
  return true;
}

}  // namespace blink
