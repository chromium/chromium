// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/credential_manager_proxy.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

CredentialManagerProxy::CredentialManagerProxy(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      authenticator_(window.GetExecutionContext()),
      credential_manager_(window.GetExecutionContext()),
      webotp_service_(window.GetExecutionContext()),
      payment_credential_(window.GetExecutionContext()) {
  LocalFrame* frame = window.GetFrame();
  DCHECK(frame);
  frame->GetBrowserInterfaceBroker().GetInterface(
      credential_manager_.BindNewPipeAndPassReceiver(
          frame->GetTaskRunner(TaskType::kUserInteraction)));
  frame->GetBrowserInterfaceBroker().GetInterface(
      authenticator_.BindNewPipeAndPassReceiver(
          frame->GetTaskRunner(TaskType::kUserInteraction)));
}

CredentialManagerProxy::~CredentialManagerProxy() = default;

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

// static
CredentialManagerProxy* CredentialManagerProxy::From(
    ScriptState* script_state) {
  DCHECK(script_state->ContextIsValid());
  LocalDOMWindow& window = *LocalDOMWindow::From(script_state);
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
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
const char CredentialManagerProxy::kSupplementName[] = "CredentialManagerProxy";

}  // namespace blink
