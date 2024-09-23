// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

CredentialManagerProxy::CredentialManagerProxy(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      authenticator_(window.GetExecutionContext()),
      credential_manager_(window.GetExecutionContext()),
      webotp_service_(window.GetExecutionContext()),
      payment_credential_(window.GetExecutionContext()),
      federated_auth_request_(window.GetExecutionContext()),
      digital_identity_request_(window.GetExecutionContext()) {}

CredentialManagerProxy::~CredentialManagerProxy() = default;

mojom::blink::CredentialManager* CredentialManagerProxy::CredentialManager() {
  if (!credential_manager_.is_bound()) {
    LocalFrame* frame = GetSupplementable()->GetFrame();
    DCHECK(frame);
    frame->GetBrowserInterfaceBroker().GetInterface(
        credential_manager_.BindNewPipeAndPassReceiver(
            frame->GetTaskRunner(TaskType::kUserInteraction)));
  }
  return credential_manager_.get();
}

mojom::blink::Authenticator* CredentialManagerProxy::Authenticator() {
  if (!authenticator_.is_bound()) {
    LocalFrame* frame = GetSupplementable()->GetFrame();
    DCHECK(frame);
    frame->GetBrowserInterfaceBroker().GetInterface(
        authenticator_.BindNewPipeAndPassReceiver(
            frame->GetTaskRunner(TaskType::kUserInteraction)));
  }
  return authenticator_.get();
}

mojom::blink::WebOTPService* CredentialManagerProxy::WebOTPService() {
  if (!webotp_service_.is_bound()) {
    LocalFrame* frame = GetSupplementable()->GetFrame();
    DCHECK(frame);
    frame->GetBrowserInterfaceBroker().GetInterface(
        webotp_service_.BindNewPipeAndPassReceiver(
            frame->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return webotp_service_.get();
}

payments::mojom::blink::PaymentCredential*
CredentialManagerProxy::PaymentCredential() {
  if (!payment_credential_.is_bound()) {
    LocalFrame* frame = GetSupplementable()->GetFrame();
    DCHECK(frame);
    frame->GetBrowserInterfaceBroker().GetInterface(
        payment_credential_.BindNewPipeAndPassReceiver(
            frame->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return payment_credential_.get();
}

template <typename Interface>
void CredentialManagerProxy::BindRemoteForFedCm(
    HeapMojoRemote<Interface>& remote,
    base::OnceClosure disconnect_closure) {
  if (remote.is_bound())
    return;

  LocalFrame* frame = GetSupplementable()->GetFrame();
  // TODO(kenrb): Work out whether kUserInteraction is the best task type
  // here. It might be appropriate to create a new one.
  frame->GetBrowserInterfaceBroker().GetInterface(
      remote.BindNewPipeAndPassReceiver(
          frame->GetTaskRunner(TaskType::kUserInteraction)));
  remote.set_disconnect_handler(std::move(disconnect_closure));
}

mojom::blink::FederatedAuthRequest*
CredentialManagerProxy::FederatedAuthRequest() {
  BindRemoteForFedCm(
      federated_auth_request_,
      WTF::BindOnce(
          &CredentialManagerProxy::OnFederatedAuthRequestConnectionError,
          WrapWeakPersistent(this)));
  return federated_auth_request_.get();
}

void CredentialManagerProxy::OnFederatedAuthRequestConnectionError() {
  federated_auth_request_.reset();
  // TODO(crbug.com/1275769): Cache the resolver and resolve the promise with an
  // appropriate error message.
}

mojom::blink::DigitalIdentityRequest*
CredentialManagerProxy::DigitalIdentityRequest() {
  BindRemoteForFedCm(
      digital_identity_request_,
      WTF::BindOnce(
          &CredentialManagerProxy::OnDigitalIdentityRequestConnectionError,
          WrapWeakPersistent(this)));
  return digital_identity_request_.get();
}

void CredentialManagerProxy::OnDigitalIdentityRequestConnectionError() {
  digital_identity_request_.reset();
}

// TODO(crbug.com/1372275): Replace From(ScriptState*) with
// From(ExecutionContext*)
// static
CredentialManagerProxy* CredentialManagerProxy::From(
    ScriptState* script_state) {
  DCHECK(script_state->ContextIsValid());
  LocalDOMWindow& window = *LocalDOMWindow::From(script_state);
  return From(&window);
}

CredentialManagerProxy* CredentialManagerProxy::From(LocalDOMWindow* window) {
  auto* supplement =
      Supplement<LocalDOMWindow>::From<CredentialManagerProxy>(*window);
  if (!supplement) {
    supplement = MakeGarbageCollected<CredentialManagerProxy>(*window);
    ProvideTo(*window, supplement);
  }
  return supplement;
}

// static
CredentialManagerProxy* CredentialManagerProxy::From(
    ExecutionContext* execution_context) {
  // Since the FedCM API cannot be used by workers, the execution context is
  // always a window.
  LocalDOMWindow& window = *To<LocalDOMWindow>(execution_context);
  auto* supplement =
      Supplement<LocalDOMWindow>::From<CredentialManagerProxy>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<CredentialManagerProxy>(window);
    ProvideTo(window, supplement);
  }
  return supplement;
}

void CredentialManagerProxy::Trace(Visitor* visitor) const {
  visitor->Trace(authenticator_);
  visitor->Trace(credential_manager_);
  visitor->Trace(webotp_service_);
  visitor->Trace(payment_credential_);
  visitor->Trace(federated_auth_request_);
  visitor->Trace(digital_identity_request_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
const char CredentialManagerProxy::kSupplementName[] = "CredentialManagerProxy";

}  // namespace blink
