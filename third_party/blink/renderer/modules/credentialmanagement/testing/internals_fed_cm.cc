// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/testing/internals_fed_cm.h"

#include <optional>
#include <string>

#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request_automation.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_dialog_button.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

mojo::Remote<test::mojom::blink::FederatedAuthRequestAutomation>
CreateFedAuthRequestAutomation(ScriptState* script_state) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  mojo::Remote<test::mojom::blink::FederatedAuthRequestAutomation>
      federated_auth_request_automation;
  window->GetBrowserInterfaceBroker().GetInterface(
      federated_auth_request_automation.BindNewPipeAndPassReceiver());
  return federated_auth_request_automation;
}

}  // namespace

// static
ScriptPromise<IDLString> InternalsFedCm::getFedCmDialogType(
    ScriptState* script_state,
    Internals&) {
  mojo::Remote<test::mojom::blink::FederatedAuthRequestAutomation>
      federated_auth_request_automation =
          CreateFedAuthRequestAutomation(script_state);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  auto promise = resolver->Promise();
  // Get the interface so `federated_auth_request_automation` can be moved
  // below.
  test::mojom::blink::FederatedAuthRequestAutomation*
      raw_federated_auth_request_automation =
          federated_auth_request_automation.get();
  raw_federated_auth_request_automation->GetDialogType(WTF::BindOnce(
      // While we only really need |resolver|, we also take the
      // mojo::Remote<> so that it remains alive after this function exits.
      [](ScriptPromiseResolver<IDLString>* resolver,
         mojo::Remote<test::mojom::blink::FederatedAuthRequestAutomation>,
         const WTF::String& type) {
        if (!type.empty()) {
          resolver->Resolve(type);
        } else {
          resolver->Reject();
        }
      },
      WrapPersistent(resolver), std::move(federated_auth_request_automation)));
  return promise;
}

// static
ScriptPromise<IDLString> InternalsFedCm::getFedCmTitle(
    ScriptState* script_state,
    Internals&) {
  mojo::Remote<test::mojom::blink::FederatedAuthRequestAutomation>
      federated_auth_request_automation =
          CreateFedAuthRequestAutomation(script_state);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  auto promise = resolver->Promise();
  // Get the interface so `federated_auth_request_automation` can be moved
  // below.
  test::mojom::blink::FederatedAuthRequestAutomation*
      raw_federated_auth_request_automation =
          federated_auth_request_automation.get();
  raw_federated_auth_request_automation->GetFedCmDialogTitle(WTF::BindOnce(
      // While we only really need |resolver|, we also take the
      // mojo::Remote<> so that it remains alive after this function exits.
      [](ScriptPromiseResolver<IDLString>* resolver,
         mojo::Remote<test::mojom::blink::FederatedAuthRequestAutomation>,
         const WTF::String& title) {
        if (!title.empty()) {
          resolver->Resolve(title);
        } else {
          resolver->Reject();
        }
      },
      WrapPersistent(resolver), std::move(federated_auth_request_automation)));
  return promise;
}

// static
ScriptPromise<IDLUndefined> InternalsFedCm::selectFedCmAccount(
    ScriptState* script_state,
    Internals&,
    int account_index,
    ExceptionState& exception_state) {
  mojo::Remote<test::mojom::blink::FederatedAuthRequestAutomation>
      federated_auth_request_automation =
          CreateFedAuthRequestAutomation(script_state);

  if (account_index < 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "A negative account index is not allowed");
    return EmptyPromise();
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  // Get the interface so `federated_auth_request_automation` can be moved
  // below.
  test::mojom::blink::FederatedAuthRequestAutomation*
      raw_federated_auth_request_automation =
          federated_auth_request_automation.get();
  raw_federated_auth_request_automation->SelectFedCmAccount(
      account_index,
      WTF::BindOnce(
          // While we only really need |resolver|, we also take the
          // mojo::Remote<> so that it remains alive after this function exits.
          [](ScriptPromiseResolver<IDLUndefined>* resolver,
             mojo::Remote<test::mojom::blink::FederatedAuthRequestAutomation>,
             bool success) {
            if (success) {
              resolver->Resolve();
            } else {
              resolver->Reject();
            }
          },
          WrapPersistent(resolver),
          std::move(federated_auth_request_automation)));
  return promise;
}

// static
ScriptPromise<IDLUndefined> InternalsFedCm::dismissFedCmDialog(
    ScriptState* script_state,
    Internals&) {
  mojo::Remote<test::mojom::blink::FederatedAuthRequestAutomation>
      federated_auth_request_automation =
          CreateFedAuthRequestAutomation(script_state);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  // Get the interface so `federated_auth_request_automation` can be moved
  // below.
  test::mojom::blink::FederatedAuthRequestAutomation*
      raw_federated_auth_request_automation =
          federated_auth_request_automation.get();
  raw_federated_auth_request_automation->DismissFedCmDialog(WTF::BindOnce(
      // While we only really need |resolver|, we also take the
      // mojo::Remote<> so that it remains alive after this function exits.
      [](ScriptPromiseResolver<IDLUndefined>* resolver,
         mojo::Remote<test::mojom::blink::FederatedAuthRequestAutomation>,
         bool success) {
        if (success) {
          resolver->Resolve();
        } else {
          resolver->Reject();
        }
      },
      WrapPersistent(resolver), std::move(federated_auth_request_automation)));
  return promise;
}

// static
ScriptPromise<IDLUndefined> InternalsFedCm::clickFedCmDialogButton(
    ScriptState* script_state,
    Internals&,
    const V8DialogButton& v8_button) {
  mojo::Remote<test::mojom::blink::FederatedAuthRequestAutomation>
      federated_auth_request_automation =
          CreateFedAuthRequestAutomation(script_state);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  test::mojom::blink::DialogButton button;
  switch (v8_button.AsEnum()) {
    case V8DialogButton::Enum::kConfirmIdpLoginContinue:
      button = test::mojom::blink::DialogButton::kConfirmIdpLoginContinue;
      break;
    case V8DialogButton::Enum::kErrorGotIt:
      button = test::mojom::blink::DialogButton::kErrorGotIt;
      break;
    case V8DialogButton::Enum::kErrorMoreDetails:
      button = test::mojom::blink::DialogButton::kErrorMoreDetails;
      break;
  }

  // Get the interface so `federated_auth_request_automation` can be moved
  // below.
  test::mojom::blink::FederatedAuthRequestAutomation*
      raw_federated_auth_request_automation =
          federated_auth_request_automation.get();
  raw_federated_auth_request_automation->ClickFedCmDialogButton(
      button,
      WTF::BindOnce(
          // While we only really need |resolver|, we also take the
          // mojo::Remote<> so that it remains alive after this function exits.
          [](ScriptPromiseResolver<IDLUndefined>* resolver,
             mojo::Remote<test::mojom::blink::FederatedAuthRequestAutomation>,
             bool success) {
            if (success) {
              resolver->Resolve();
            } else {
              resolver->Reject();
            }
          },
          WrapPersistent(resolver),
          std::move(federated_auth_request_automation)));
  return promise;
}

}  // namespace blink
