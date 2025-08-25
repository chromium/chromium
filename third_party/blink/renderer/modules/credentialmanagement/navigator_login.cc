// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/navigator_login.h"

#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/webid/login_status_options.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_provider_account.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_login_status.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_login_status_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

namespace {
void OnSetIdpSigninStatus(ScriptPromiseResolver<IDLUndefined>* resolver) {
  resolver->Resolve();
}
}  // namespace

const char NavigatorLogin::kSupplementName[] = "NavigatorLogin";

NavigatorLogin* NavigatorLogin::login(Navigator& navigator) {
  NavigatorLogin* supplement =
      Supplement<Navigator>::From<NavigatorLogin>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorLogin>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement;
}

NavigatorLogin::NavigatorLogin(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

ScriptPromise<IDLUndefined> NavigatorLogin::setStatus(
    ScriptState* script_state,
    const V8LoginStatus& v8_status) {
  auto* context = ExecutionContext::From(script_state);
  auto* request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  ScriptPromise<IDLUndefined> promise = resolver->Promise();

  mojom::blink::IdpSigninStatus status;
  switch (v8_status.AsEnum()) {
    case V8LoginStatus::Enum::kLoggedIn:
      status = mojom::blink::IdpSigninStatus::kSignedIn;
      break;
    case V8LoginStatus::Enum::kLoggedOut:
      status = mojom::blink::IdpSigninStatus::kSignedOut;
      break;
  }
  request->SetIdpSigninStatus(
      context->GetSecurityOrigin(), status, nullptr,
      BindOnce(&OnSetIdpSigninStatus, WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<IDLUndefined> NavigatorLogin::setStatus(
    ScriptState* script_state,
    const V8LoginStatus& v8_status,
    const LoginStatusOptions* options) {
  auto* context = ExecutionContext::From(script_state);
  auto* request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);

  ScriptPromise<IDLUndefined> promise = resolver->Promise();

  mojom::blink::IdpSigninStatus status;
  switch (v8_status.AsEnum()) {
    case V8LoginStatus::Enum::kLoggedIn:
      status = mojom::blink::IdpSigninStatus::kSignedIn;
      break;
    case V8LoginStatus::Enum::kLoggedOut:
      status = mojom::blink::IdpSigninStatus::kSignedOut;
      break;
  }
  if (options->hasAccounts()) {
    for (const auto& account : options->accounts()) {
      if (!account.Get()->hasPicture()) {
        continue;
      }

      KURL picture_url = context->CompleteURL(account.Get()->picture());
      if (!picture_url.IsValid()) {
        resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                         "Picture URL is invalid");
        return promise;
      }

      if (!network::IsUrlPotentiallyTrustworthy(GURL(picture_url))) {
        resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                         "Picture URL must be a secure URL");
        return promise;
      }

      account.Get()->setPicture(picture_url.GetString());
    }
  }

  request->SetIdpSigninStatus(
      context->GetSecurityOrigin(), status,
      mojo::ConvertTo<blink::mojom::blink::LoginStatusOptionsPtr>(*options),
      BindOnce(&OnSetIdpSigninStatus, WrapPersistent(resolver)));
  return promise;
}

void NavigatorLogin::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
