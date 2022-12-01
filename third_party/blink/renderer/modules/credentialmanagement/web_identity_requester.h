// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_WEB_IDENTITY_REQUESTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_WEB_IDENTITY_REQUESTER_H_

#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/scoped_abort_state.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/web_identity_window_onload_event_listener.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class IdentityProviderConfig;
class WebIdentityWindowOnloadEventListener;

// Helper class to handle FedCM token requests.
class WebIdentityRequester final
    : public GarbageCollected<WebIdentityRequester> {
 public:
  explicit WebIdentityRequester(
      ExecutionContext* context,
      std::unique_ptr<ScopedAbortState> scoped_abort_state);

  void OnRequestToken(mojom::blink::RequestTokenStatus status,
                      const absl::optional<KURL>& selected_idp_config_url,
                      const WTF::String& token);
  // Invoked at most once per token request.
  void RequestToken();
  // Invoked at least once per token request, can be multiple times.
  void AppendGetCall(
      ScriptPromiseResolver* resolver,
      const HeapVector<Member<IdentityProviderConfig>>& providers,
      bool prefer_auto_sign_in);
  void Trace(Visitor* visitor) const;

 private:
  // A vector of pointers to mojom class objects. Each mojom class object
  // corresponds to parameters of a navigator.credentials.get call and contains
  // a vector of IDPs. This is to reduce storage of duplicate data such as
  // prefer_auto_sign_in values. We flatten these arrays of IDPs into a single
  // array of IDPs in FederatedAuthRequestImpl::RequestToken.
  Vector<mojom::blink::IdentityProviderGetParametersPtr> idp_get_params_;
  Member<ExecutionContext> execution_context_;
  std::unique_ptr<ScopedAbortState> scoped_abort_state_;
  Member<WebIdentityWindowOnloadEventListener> window_onload_event_listener_;
  HeapHashMap<KURL, Member<ScriptPromiseResolver>> provider_to_resolver_;
  bool is_requesting_token_{false};
  bool has_posted_task_{false};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_WEB_IDENTITY_REQUESTER_H_
