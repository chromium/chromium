// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/web_identity_requester.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/scoped_abort_state.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"
#include "third_party/blink/renderer/modules/credentialmanagement/identity_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/identity_credential_error.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

WebIdentityRequester::WebIdentityRequester(ExecutionContext* context,
                                           MediationRequirement requirement)
    : execution_context_(context), requirement_(requirement) {}

void WebIdentityRequester::OnRequestToken(
    mojom::blink::RequestTokenStatus status,
    const absl::optional<KURL>& selected_idp_config_url,
    const WTF::String& token,
    mojom::blink::TokenErrorPtr error,
    bool is_account_auto_selected) {
  for (const auto& provider_resolver_pair : provider_to_resolver_) {
    KURL provider = provider_resolver_pair.key;
    ScriptPromiseResolver* resolver = provider_resolver_pair.value;

    switch (status) {
      case mojom::blink::RequestTokenStatus::kErrorTooManyRequests: {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "Only one navigator.credentials.get request may be outstanding at "
            "one time."));
        continue;
      }
      case mojom::blink::RequestTokenStatus::kErrorCanceled: {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kAbortError, "The request has been aborted."));
        continue;
      }
      case mojom::blink::RequestTokenStatus::kError: {
        if (!RuntimeEnabledFeatures::FedCmErrorEnabled() || !error) {
          resolver->Reject(MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kNetworkError, "Error retrieving a token."));
          continue;
        }
        resolver->Reject(MakeGarbageCollected<IdentityCredentialError>(
            "Error retrieving a token.", error->code, error->url));
        continue;
      }
      case mojom::blink::RequestTokenStatus::kSuccess: {
        DCHECK(selected_idp_config_url);
        if (provider != selected_idp_config_url) {
          if (!RuntimeEnabledFeatures::FedCmErrorEnabled() || !error) {
            resolver->Reject(MakeGarbageCollected<DOMException>(
                DOMExceptionCode::kNetworkError, "Error retrieving a token."));
            continue;
          }
          resolver->Reject(MakeGarbageCollected<IdentityCredentialError>(
              "Error retrieving a token.", error->code, error->url));
          continue;
        }
        IdentityCredential* credential =
            IdentityCredential::Create(token, is_account_auto_selected);
        resolver->Resolve(credential);
        continue;
      }
      default:
        NOTREACHED();
    }
  }
  provider_to_resolver_.clear();
  scoped_abort_states_.clear();
  is_requesting_token_ = false;
}

void WebIdentityRequester::RequestToken() {
  auto* auth_request =
      CredentialManagerProxy::From(execution_context_)->FederatedAuthRequest();
  auth_request->RequestToken(
      std::move(idp_get_params_), requirement_,
      WTF::BindOnce(&WebIdentityRequester::OnRequestToken,
                    WrapPersistent(this)));
  window_onload_event_listener_.Clear();
  is_requesting_token_ = true;
  has_posted_task_ = false;
}

void WebIdentityRequester::AppendGetCall(
    ScriptPromiseResolver* resolver,
    const HeapVector<Member<IdentityProviderConfig>>& providers,
    mojom::blink::RpContext rp_context,
    mojom::blink::RpMode rp_mode) {
  if (is_requesting_token_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "Only one token request may be outstanding at one time."));
    return;
  }

  Vector<mojom::blink::IdentityProviderPtr> idp_ptrs;
  for (const auto& provider : providers) {
    mojom::blink::IdentityProviderConfigPtr config =
        blink::mojom::blink::IdentityProviderConfig::From(*provider);
    if (provider_to_resolver_.Contains(KURL(config->config_url))) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "More than one navigator.credentials.get calls to the same "
          "provider."));
      return;
    }
    mojom::blink::IdentityProviderPtr idp =
        mojom::blink::IdentityProvider::NewFederated(std::move(config));
    idp_ptrs.push_back(std::move(idp));
  }

  for (const auto& idp_ptr : idp_ptrs) {
    provider_to_resolver_.insert(KURL(idp_ptr->get_federated()->config_url),
                                 WrapPersistent(resolver));
  }

  mojom::blink::IdentityProviderGetParametersPtr get_params =
      mojom::blink::IdentityProviderGetParameters::New(std::move(idp_ptrs),
                                                       rp_context, rp_mode);
  idp_get_params_.push_back(std::move(get_params));

  if (window_onload_event_listener_ || has_posted_task_)
    return;

  Document* document = resolver->DomWindow()->document();
  // Checking if document load is not completed is equivalent to checking if
  // this method was called before the window.onload event.
  if (!document->IsLoadCompleted()) {
    // Before window.onload event, we add a listener to the window onload event.
    // All get calls up until the window onload event is fired are collated into
    // a single token request. Once the window onload event is fired, we post a
    // task with all collated IDPs to RequestToken.
    InitWindowOnloadEventListener(resolver);
    return;
  }

  // During or after window.onload event, we immediately post a task to
  // RequestToken. All get calls up until the task in which RequestToken is
  // executed are collated into a single token request.
  document->GetTaskRunner(TaskType::kInternalDefault)
      ->PostTask(FROM_HERE, WTF::BindOnce(&WebIdentityRequester::RequestToken,
                                          WrapPersistent(this)));
  has_posted_task_ = true;
}

void WebIdentityRequester::InsertScopedAbortState(
    std::unique_ptr<ScopedAbortState> scoped_abort_state) {
  scoped_abort_states_.insert(std::move(scoped_abort_state));
}

void WebIdentityRequester::InitWindowOnloadEventListener(
    ScriptPromiseResolver* resolver) {
  window_onload_event_listener_ =
      MakeGarbageCollected<WebIdentityWindowOnloadEventListener>(
          resolver->DomWindow()->document(), WrapPersistent(this));
  resolver->DomWindow()->addEventListener(event_type_names::kLoad,
                                          window_onload_event_listener_);
}

void WebIdentityRequester::StartDelayTimer(ScriptPromiseResolver* resolver) {
  DCHECK(!RuntimeEnabledFeatures::FedCmMultipleIdentityProvidersEnabled(
      execution_context_));

  Document* document = resolver->DomWindow()->document();
  delay_start_time_ = base::TimeTicks::Now();
  bool timer_started_before_onload = !document->IsLoadCompleted();

  // Before window.onload event, we add a listener to the window onload event.
  // Once the window onload event is fired, we post a task to
  // StopDelayTimer.
  if (timer_started_before_onload) {
    InitWindowOnloadEventListener(resolver);
    return;
  }

  // During or after window.onload event, we post a task to StopDelayTimer.
  document->GetTaskRunner(TaskType::kInternalDefault)
      ->PostTask(FROM_HERE, WTF::BindOnce(&WebIdentityRequester::StopDelayTimer,
                                          WrapPersistent(this),
                                          timer_started_before_onload));
}

void WebIdentityRequester::StopDelayTimer(bool timer_started_before_onload) {
  DCHECK(!RuntimeEnabledFeatures::FedCmMultipleIdentityProvidersEnabled(
      execution_context_));

  UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.IsAfterWindowOnload",
                        !timer_started_before_onload);

  base::TimeDelta delay_duration = base::TimeTicks::Now() - delay_start_time_;
  if (timer_started_before_onload) {
    UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.WindowOnloadDelayDuration",
                               delay_duration);
    return;
  }
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.PostTaskDelayDuration",
                             delay_duration);
}

void WebIdentityRequester::AbortRequest(ScriptState* script_state) {
  if (!script_state->ContextIsValid()) {
    return;
  }

  if (!is_requesting_token_) {
    OnRequestToken(mojom::blink::RequestTokenStatus::kErrorCanceled,
                   absl::nullopt, "", nullptr,
                   /*is_account_auto_selected=*/false);
    return;
  }

  auto* auth_request =
      CredentialManagerProxy::From(script_state)->FederatedAuthRequest();
  auth_request->CancelTokenRequest();
}

void WebIdentityRequester::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
  visitor->Trace(window_onload_event_listener_);
  visitor->Trace(provider_to_resolver_);
}

}  // namespace blink
