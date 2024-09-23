// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/navigator_login.h"

#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_login_status.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

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
  // TODO(https://crbug.com/1382193): Determine if we should add an origin
  // parameter.
  auto* context = ExecutionContext::From(script_state);
  auto* request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();

  mojom::blink::IdpSigninStatus status;
  switch (v8_status.AsEnum()) {
    case V8LoginStatus::Enum::kLoggedIn:
      status = mojom::blink::IdpSigninStatus::kSignedIn;
      break;
    case V8LoginStatus::Enum::kLoggedOut:
      status = mojom::blink::IdpSigninStatus::kSignedOut;
      break;
  }
  request->SetIdpSigninStatus(context->GetSecurityOrigin(), status);
  return EmptyPromise();
}

void NavigatorLogin::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
