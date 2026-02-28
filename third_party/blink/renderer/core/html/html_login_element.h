// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_LOGIN_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_LOGIN_ELEMENT_H_

#include "base/values.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

// <login> element is used to trigger Federated Credential Management (FedCM)
// requests when clicked. It uses its <credential> children to determine the
// identity providers to use for the request.
// See https://github.com/fedidcg/login-element for the explainer.
class CORE_EXPORT HTMLLoginElement : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLLoginElement(Document&);

  ScriptValue credential(ScriptState*) const;

  Vector<mojom::blink::IdentityProviderRequestOptionsPtr>
  GetFederatedRequestOptions() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(complete, kComplete)

  void Trace(Visitor*) const override;

 private:
  FocusableState IsFocusableState(UpdateBehavior) const override;
  bool ShouldHaveFocusAppearance() const override;

  bool WillRespondToMouseClickEvents() override;
  bool IsInteractiveContent() const override;

  void DefaultEventHandler(Event&) override;

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;

  void NotifyCredentialReceived(base::Value token);

  void OnRequestTokenResponse(
      mojom::blink::RequestTokenStatus status,
      const std::optional<KURL>& selected_identity_provider_config_url,
      std::optional<base::Value> token,
      mojom::blink::TokenErrorPtr error,
      bool is_auto_selected);

  std::optional<base::Value> credential_;

  HeapMojoRemote<mojom::blink::FederatedAuthRequest> federated_auth_request_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_LOGIN_ELEMENT_H_
