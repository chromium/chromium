// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webid/web_id.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_id_request_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

namespace {

using mojom::blink::LogoutStatus;
using mojom::blink::ProvideIdTokenStatus;
using mojom::blink::RequestIdTokenStatus;
using mojom::blink::RequestMode;

RequestMode ToRequestMode(const String& mode) {
  if (mode == "mediated") {
    return RequestMode::kMediated;
  } else {
    return RequestMode::kPermission;
  }
}

void OnRequestIdToken(ScriptPromiseResolver* resolver,
                      RequestIdTokenStatus status,
                      const WTF::String& id_token) {
  switch (status) {
    case RequestIdTokenStatus::kApprovalDeclined: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, "User declined the sign-in attempt."));
      return;
    }
    case RequestIdTokenStatus::kErrorTooManyRequests: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError,
          "Only one navigator.id.get request may be outstanding at one time."));
      return;
    }
    case RequestIdTokenStatus::kErrorWebIdNotSupportedByProvider: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNetworkError,
          "The indicated provider does not support WebID."));
      return;
    }
    case RequestIdTokenStatus::kErrorFetchingWellKnown: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNetworkError,
          "Error fetching the provider's .well-known configuration."));
      return;
    }
    case RequestIdTokenStatus::kErrorInvalidWellKnown: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNetworkError,
          "Provider's .well-known configuration is invalid."));
      return;
    }
    case RequestIdTokenStatus::kErrorFetchingSignin: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNetworkError,
          "Error attempting to reach the provider's sign-in endpoint."));
      return;
    }
    case RequestIdTokenStatus::kErrorInvalidSigninResponse: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNetworkError,
          "Provider's sign-in response is invalid"));
      return;
    }
    case RequestIdTokenStatus::kErrorInvalidAccountsResponse: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNetworkError,
          "Provider's accounts response is invalid"));
      return;
    }
    case RequestIdTokenStatus::kErrorInvalidTokenResponse: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNetworkError,
          "Provider's token response is invalid"));
      return;
    }
    case RequestIdTokenStatus::kError: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNetworkError, "Error retrieving an id token."));
      return;
    }
    case RequestIdTokenStatus::kSuccess: {
      resolver->Resolve(id_token);
      return;
    }
  }
}

void OnLogout(ScriptPromiseResolver* resolver, LogoutStatus status) {
  // TODO(kenrb); There should be more thought put into how this API works.
  // Returning success or failure doesn't have a lot of meaning. If some
  // logout attempts fail and others succeed, and even different attempts
  // fail for different reasons, how does that get conveyed to the caller?
  if (status != LogoutStatus::kSuccess) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError, "Error logging out endpoints."));
    return;
  }
  resolver->Resolve();
}

void OnProvideIdToken(ScriptPromiseResolver* resolver,
                      ProvideIdTokenStatus status) {
  // TODO(kenrb): Provide better messages for different error codes.
  if (status != ProvideIdTokenStatus::kSuccess) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError, "Error providing the id token."));
    return;
  }
  resolver->Resolve();
}

}  // namespace

WebId::WebId(ExecutionContext& context)
    : ExecutionContextClient(&context),
      auth_request_(&context),
      auth_response_(&context) {}

ScriptPromise WebId::get(ScriptState* script_state,
                         const WebIdRequestOptions* options,
                         ExceptionState& exception_state) {
  if (!options->hasProvider()) {
    exception_state.ThrowTypeError("Invalid parameters: provider required.");
    return ScriptPromise();
  }

  if (!options->hasRequest()) {
    exception_state.ThrowTypeError("Invalid parameters: request required.");
    return ScriptPromise();
  }

  DCHECK(options->hasMode());

  // TODO(kenrb): Add some renderer-side validation here, such as validating
  // |provider|, and making sure the calling context is legal. Some of this
  // has not been spec'd yet.
  KURL provider = KURL(NullURL(), options->provider());
  if (!provider.IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Invalid provider URL.");
    return ScriptPromise();
  }

  BindRemote(auth_request_);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  auth_request_->RequestIdToken(
      provider, options->request(), ToRequestMode(options->mode()),
      WTF::Bind(&OnRequestIdToken, WrapPersistent(resolver)));

  return promise;
}

ScriptPromise WebId::provide(ScriptState* script_state, String id_token) {
  BindRemote(auth_response_);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  auth_response_->ProvideIdToken(
      id_token, WTF::Bind(&OnProvideIdToken, WrapPersistent(resolver)));

  return promise;
}

ScriptPromise WebId::logout(ScriptState* script_state,
                            const Vector<String>& logout_endpoints) {
  if (logout_endpoints.IsEmpty()) {
    return ScriptPromise();
  }

  BindRemote(auth_request_);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  auth_request_->Logout(logout_endpoints,
                        WTF::Bind(&OnLogout, WrapPersistent(resolver)));

  return promise;
}

template <typename Interface>
void WebId::BindRemote(HeapMojoRemote<Interface>& remote) {
  auto* context = GetExecutionContext();

  if (remote.is_bound())
    return;

  // TODO(kenrb): Work out whether kUserInteraction is the best task type
  // here. It might be appropriate to create a new one.
  context->GetBrowserInterfaceBroker().GetInterface(
      remote.BindNewPipeAndPassReceiver(
          context->GetTaskRunner(TaskType::kUserInteraction)));
  remote.set_disconnect_handler(
      WTF::Bind(&WebId::OnConnectionError, WrapWeakPersistent(this)));
}

void WebId::Trace(blink::Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(auth_request_);
  visitor->Trace(auth_response_);
}

void WebId::OnConnectionError() {
  auth_request_.reset();
  // TODO(majidvp): We should handle connection errors for request and response
  // separately.
  auth_response_.reset();

  // TODO(kenrb): Cache the resolver and resolve the promise with an
  // appropriate error message.
}

}  // namespace blink
