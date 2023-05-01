// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_WEB_IDENTITY_REQUESTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_WEB_IDENTITY_REQUESTER_H_

#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom-blink.h"
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

using MediationRequirement = mojom::blink::CredentialMediationRequirement;

// Helper class to handle FedCM token requests.
class MODULES_EXPORT WebIdentityRequester final
    : public GarbageCollected<WebIdentityRequester> {
 public:
  WebIdentityRequester(ExecutionContext* context,
                       MediationRequirement requirement);

  void OnRequestToken(mojom::blink::RequestTokenStatus status,
                      const absl::optional<KURL>& selected_idp_config_url,
                      const WTF::String& token);

  // Invoked at most once per token request.
  void RequestToken();
  // Invoked at least once per token request, can be multiple times.
  void AppendGetCall(
      ScriptPromiseResolver* resolver,
      const HeapVector<Member<IdentityProviderConfig>>& providers,
      mojom::blink::RpContext rp_context);
  void InsertScopedAbortState(
      std::unique_ptr<ScopedAbortState> scoped_abort_state);

  // Starts the timer for recording the duration from when RequestToken is
  // called directly to when RequestToken would be called if invoked through
  // WebIdentityRequester.
  void StartDelayTimer(ScriptPromiseResolver* resolver);
  // Stops the timer for recording the duration from when RequestToken is
  // called directly to when RequestToken would be called if invoked through
  // WebIdentityRequester.
  void StopDelayTimer(bool timer_started_before_onload);

  void Trace(Visitor* visitor) const;

 private:
  void InitWindowOnloadEventListener(ScriptPromiseResolver* resolver);

  // A vector of pointers to mojom class objects. Each mojom class object
  // corresponds to parameters of a navigator.credentials.get call and contains
  // a vector of IDPs. This is to reduce storage of duplicate data such as
  // auto_reauthn values. We flatten these arrays of IDPs into a single
  // array of IDPs in FederatedAuthRequestImpl::RequestToken.
  Vector<mojom::blink::IdentityProviderGetParametersPtr> idp_get_params_;
  Member<ExecutionContext> execution_context_;
  HashSet<std::unique_ptr<ScopedAbortState>> scoped_abort_states_;
  Member<WebIdentityWindowOnloadEventListener> window_onload_event_listener_;
  HeapHashMap<KURL, Member<ScriptPromiseResolver>> provider_to_resolver_;
  MediationRequirement requirement_;
  bool is_requesting_token_{false};
  bool has_posted_task_{false};
  base::TimeTicks delay_start_time_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_WEB_IDENTITY_REQUESTER_H_
