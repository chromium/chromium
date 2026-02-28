// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_login_element.h"

#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_v8_value_converter.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_credential_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

bool IsEnterKeyKeydownEvent(Event& event) {
  auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
  if (!keyboard_event) {
    return false;
  }
  return keyboard_event->key() == "Enter";
}

bool IsLoginClick(Event& event) {
  auto* mouse_event = DynamicTo<MouseEvent>(event);
  if (!mouse_event || event.type() != event_type_names::kClick) {
    // TODO(crbug.com/477699742): should we handle touch and pointer events too?
    return false;
  }
  return mouse_event->button() == 0;
}

}  // namespace

HTMLLoginElement::HTMLLoginElement(Document& document)
    : HTMLElement(html_names::kLoginTag, document),
      federated_auth_request_(document.GetExecutionContext()) {}

ScriptValue HTMLLoginElement::credential(ScriptState* script_state) const {
  if (!credential_) {
    return ScriptValue();
  }

  // TODO(crbug.com/477699742): consider storing a v8::Local<v8::Value> as
  // member rather than a base::Value so that we don't have to recompute this
  // every time on demand.
  std::unique_ptr<WebV8ValueConverter> converter =
      Platform::Current()->CreateWebV8ValueConverter();
  v8::Local<v8::Value> v8_value =
      converter->ToV8Value(*credential_, script_state->GetContext());
  return ScriptValue(script_state->GetIsolate(), v8_value);
}

void HTMLLoginElement::NotifyCredentialReceived(base::Value token) {
  DCHECK(isConnected());
  credential_ = std::move(token);
  DispatchEvent(*Event::Create(event_type_names::kComplete));
}

Vector<mojom::blink::IdentityProviderRequestOptionsPtr>
HTMLLoginElement::GetFederatedRequestOptions() const {
  Vector<mojom::blink::IdentityProviderRequestOptionsPtr> options_list;
  for (HTMLCredentialElement& credential :
       Traversal<HTMLCredentialElement>::ChildrenOf(*this)) {
    if (auto options = credential.GetFederatedRequestOptions()) {
      options_list.push_back(std::move(options));
    }
  }
  return options_list;
}

FocusableState HTMLLoginElement::IsFocusableState(
    UpdateBehavior update_behavior) const {
  // TODO(crbug.com/477699742): if you tab through the document, will the login
  // element be focused even if empty?
  if (!GetFederatedRequestOptions().empty()) {
    return FocusableState::kFocusable;
  }
  return HTMLElement::IsFocusableState(update_behavior);
}

bool HTMLLoginElement::ShouldHaveFocusAppearance() const {
  return (IsFocused() && !GetFederatedRequestOptions().empty()) ||
         HTMLElement::ShouldHaveFocusAppearance();
}

bool HTMLLoginElement::WillRespondToMouseClickEvents() {
  return !GetFederatedRequestOptions().empty();
}

bool HTMLLoginElement::IsInteractiveContent() const {
  return true;
}

Node::InsertionNotificationRequest HTMLLoginElement::InsertedInto(
    ContainerNode& insertion_point) {
  Node::InsertionNotificationRequest result =
      HTMLElement::InsertedInto(insertion_point);
  if (!insertion_point.isConnected()) {
    return result;
  }

  if (!GetExecutionContext()->IsSecureContext()) {
    GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kError,
            "The <login> element can only be used in a secure context."));
  }

  return result;
}

void HTMLLoginElement::DefaultEventHandler(Event& event) {
  if (!isConnected()) {
    HTMLElement::DefaultEventHandler(event);
    return;
  }

  bool is_click = IsLoginClick(event);
  bool is_enter = IsFocused() && IsEnterKeyKeydownEvent(event);

  if (!is_click && !is_enter) {
    HTMLElement::DefaultEventHandler(event);
    return;
  }

  if (!GetExecutionContext()->IsSecureContext()) {
    HTMLElement::DefaultEventHandler(event);
    return;
  }

  if (!GetExecutionContext()->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kIdentityCredentialsGet)) {
    GetExecutionContext()->AddConsoleMessage(MakeGarbageCollected<
                                             ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kError,
        "The 'identity-credentials-get' permissions policy is not enabled."));
    HTMLElement::DefaultEventHandler(event);
    return;
  }

  Vector<mojom::blink::IdentityProviderGetParametersPtr> idp_get_params;

  for (HTMLCredentialElement& credential :
       Traversal<HTMLCredentialElement>::ChildrenOf(*this)) {
    auto options = credential.GetFederatedRequestOptions();
    if (!options) {
      continue;
    }

    auto get_params = mojom::blink::IdentityProviderGetParameters::New();
    get_params->providers.push_back(std::move(options));
    get_params->mode = mojom::blink::RpMode::kActive;
    idp_get_params.push_back(std::move(get_params));
  }

  if (idp_get_params.empty()) {
    HTMLElement::DefaultEventHandler(event);
    return;
  }

  if (!federated_auth_request_.is_bound()) {
    GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
        federated_auth_request_.BindNewPipeAndPassReceiver(
            GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault)));
  }

  federated_auth_request_->RequestToken(
      std::move(idp_get_params),
      mojom::blink::CredentialMediationRequirement::kRequired,
      blink::BindOnce(&HTMLLoginElement::OnRequestTokenResponse,
                      WrapWeakPersistent(this)));
  event.SetDefaultHandled();

  HTMLElement::DefaultEventHandler(event);
}

void HTMLLoginElement::OnRequestTokenResponse(
    mojom::blink::RequestTokenStatus status,
    const std::optional<KURL>& selected_identity_provider_config_url,
    std::optional<base::Value> token,
    mojom::blink::TokenErrorPtr error,
    bool is_auto_selected) {
  if (status == mojom::blink::RequestTokenStatus::kSuccess && token) {
    NotifyCredentialReceived(std::move(*token));
  }
}

void HTMLLoginElement::Trace(Visitor* visitor) const {
  visitor->Trace(federated_auth_request_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
