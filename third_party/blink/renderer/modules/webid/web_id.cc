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

void OnRequestIdToken(ScriptPromiseResolver* resolver,
                      mojom::blink::RequestIdTokenStatus status,
                      const WTF::String& id_token) {
  // TODO(kenrb): Provide better messages for different error codes.
  if (status != mojom::blink::RequestIdTokenStatus::kSuccess) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError,
        "Error loading the identity provider."));
    return;
  }
  resolver->Resolve(id_token);
}

void OnProvideIdToken(ScriptPromiseResolver* resolver,
                      mojom::blink::ProvideIdTokenStatus status) {
  // TODO(kenrb): Provide better messages for different error codes.
  if (status != mojom::blink::ProvideIdTokenStatus::kSuccess) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError, "Error providing the id token."));
    return;
  }
  resolver->Resolve();
}

}  // namespace

WebID::WebID(ExecutionContext& context)
    : ExecutionContextClient(&context), auth_request_(&context) {}

ScriptPromise WebID::get(ScriptState* script_state,
                         const WebIDRequestOptions* options,
                         ExceptionState& exception_state) {
  if (!options->hasProvider() || !options->hasRequest()) {
    exception_state.ThrowTypeError("Invalid WebIDRequestOptions");
    return ScriptPromise();
  }

  // TODO(kenrb): Add some renderer-side validation here, such as validating
  // |provider|, and making sure the calling context is legal. Some of this
  // has not been spec'd yet.
  KURL provider = KURL(NullURL(), options->provider());
  if (!provider.IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      "Invalid provider URL");
    return ScriptPromise();
  }

  BindAuthRequest();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  auth_request_->RequestIdToken(
      provider, options->request(),
      WTF::Bind(&OnRequestIdToken, WrapPersistent(resolver)));

  return promise;
}

ScriptPromise WebID::provide(ScriptState* script_state, String id_token) {
  BindAuthRequest();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  auth_request_->ProvideIdToken(
      id_token, WTF::Bind(&OnProvideIdToken, WrapPersistent(resolver)));

  return promise;
}

void WebID::BindAuthRequest() {
  auto* context = GetExecutionContext();

  if (auth_request_.is_bound())
    return;

  // TODO(kenrb): Work out whether kUserInteraction is the best task type
  // here. It might be appropriate to create a new one.
  context->GetBrowserInterfaceBroker().GetInterface(
      auth_request_.BindNewPipeAndPassReceiver(
          context->GetTaskRunner(TaskType::kUserInteraction)));
  auth_request_.set_disconnect_handler(
      WTF::Bind(&WebID::OnConnectionError, WrapWeakPersistent(this)));
}

void WebID::Trace(blink::Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(auth_request_);
}

void WebID::OnConnectionError() {
  auth_request_.reset();
  // TODO(kenrb): Cache the resolver and resolve the promise with an
  // appropriate error message.
}

}  // namespace blink
