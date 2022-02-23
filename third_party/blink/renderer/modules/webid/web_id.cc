// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webid/web_id.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

namespace {

using mojom::blink::ProvideIdTokenStatus;
using mojom::blink::RequestIdTokenStatus;
using mojom::blink::RequestMode;

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
    : ExecutionContextClient(&context), auth_response_(&context) {}

ScriptPromise WebId::provide(ScriptState* script_state, String id_token) {
  BindRemote(auth_response_);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  auth_response_->ProvideIdToken(
      id_token, WTF::Bind(&OnProvideIdToken, WrapPersistent(resolver)));

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
  visitor->Trace(auth_response_);
}

void WebId::OnConnectionError() {
  auth_response_.reset();

  // TODO(kenrb): Cache the resolver and resolve the promise with an
  // appropriate error message.
}

}  // namespace blink
