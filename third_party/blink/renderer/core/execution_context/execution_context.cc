/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2012 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/execution_context/execution_context.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/csp/execution_context_csp_delegate.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/fetch_client_settings_object_impl.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

ExecutionContext::ExecutionContext(v8::Isolate* isolate,
                                   Agent* agent,
                                   OriginTrialContext* origin_trial_context)
    : isolate_(isolate),
      circular_sequential_id_(0),
      in_dispatch_error_event_(false),
      lifecycle_state_(mojom::FrameLifecycleState::kRunning),
      is_context_destroyed_(false),
      csp_delegate_(MakeGarbageCollected<ExecutionContextCSPDelegate>(*this)),
      agent_(agent),
      origin_trial_context_(origin_trial_context),
      window_interaction_tokens_(0),
      referrer_policy_(network::mojom::ReferrerPolicy::kDefault) {
  if (origin_trial_context_)
    origin_trial_context_->BindExecutionContext(this);
}

ExecutionContext::~ExecutionContext() = default;

// static
ExecutionContext* ExecutionContext::From(const ScriptState* script_state) {
  v8::HandleScope scope(script_state->GetIsolate());
  return ToExecutionContext(script_state->GetContext());
}

// static
ExecutionContext* ExecutionContext::ForCurrentRealm(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  return ToExecutionContext(info.GetIsolate()->GetCurrentContext());
}

// static
ExecutionContext* ExecutionContext::ForRelevantRealm(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  return ToExecutionContext(info.Holder()->CreationContext());
}

void ExecutionContext::SetLifecycleState(mojom::FrameLifecycleState state) {
  bool was_paused = lifecycle_state_ != mojom::FrameLifecycleState::kRunning;
  lifecycle_state_ = state;
  NotifyContextLifecycleStateChanged(state);
  bool paused = lifecycle_state_ != mojom::FrameLifecycleState::kRunning;

  if (was_paused == paused)
    return;

  if (paused)
    TasksWerePaused();
  else
    TasksWereUnpaused();
}

void ExecutionContext::NotifyContextDestroyed() {
  is_context_destroyed_ = true;
  ContextLifecycleNotifier::NotifyContextDestroyed();
}

void ExecutionContext::AddConsoleMessageImpl(mojom::ConsoleMessageSource source,
                                             mojom::ConsoleMessageLevel level,
                                             const String& message,
                                             bool discard_duplicates) {
  AddConsoleMessage(ConsoleMessage::Create(source, level, message),
                    discard_duplicates);
}

void ExecutionContext::DispatchErrorEvent(
    ErrorEvent* error_event,
    SanitizeScriptErrors sanitize_script_errors) {
  if (in_dispatch_error_event_) {
    pending_exceptions_.push_back(error_event);
    return;
  }

  // First report the original exception and only then all the nested ones.
  if (!DispatchErrorEventInternal(error_event, sanitize_script_errors))
    ExceptionThrown(error_event);

  if (pending_exceptions_.IsEmpty())
    return;
  for (ErrorEvent* e : pending_exceptions_)
    ExceptionThrown(e);
  pending_exceptions_.clear();
}

bool ExecutionContext::DispatchErrorEventInternal(
    ErrorEvent* error_event,
    SanitizeScriptErrors sanitize_script_errors) {
  EventTarget* target = ErrorEventTarget();
  if (!target)
    return false;

  if (sanitize_script_errors == SanitizeScriptErrors::kSanitize) {
    error_event = ErrorEvent::CreateSanitizedError(
        ToScriptState(this, *error_event->World()));
  }

  DCHECK(!in_dispatch_error_event_);
  in_dispatch_error_event_ = true;
  target->DispatchEvent(*error_event);
  in_dispatch_error_event_ = false;
  return error_event->defaultPrevented();
}

bool ExecutionContext::IsContextPaused() const {
  return lifecycle_state_ != mojom::FrameLifecycleState::kRunning;
}

int ExecutionContext::CircularSequentialID() {
  ++circular_sequential_id_;
  if (circular_sequential_id_ > ((1U << 31) - 1U))
    circular_sequential_id_ = 1;

  return circular_sequential_id_;
}

PublicURLManager& ExecutionContext::GetPublicURLManager() {
  if (!public_url_manager_)
    public_url_manager_ = MakeGarbageCollected<PublicURLManager>(this);
  return *public_url_manager_;
}

ContentSecurityPolicyDelegate&
ExecutionContext::GetContentSecurityPolicyDelegate() {
  return *csp_delegate_;
}

ContentSecurityPolicy* ExecutionContext::GetContentSecurityPolicyForWorld() {
  // Isolated worlds are only relevant for Documents. Hence just return the main
  // world's content security policy by default.
  return GetContentSecurityPolicy();
}

const SecurityOrigin* ExecutionContext::GetSecurityOrigin() {
  return GetSecurityContext().GetSecurityOrigin();
}

SecurityOrigin* ExecutionContext::GetMutableSecurityOrigin() {
  return GetSecurityContext().GetMutableSecurityOrigin();
}

ContentSecurityPolicy* ExecutionContext::GetContentSecurityPolicy() {
  return GetSecurityContext().GetContentSecurityPolicy();
}

const base::UnguessableToken& ExecutionContext::GetAgentClusterID() const {
  return agent_->cluster_id();
}

void ExecutionContext::AllowWindowInteraction() {
  ++window_interaction_tokens_;
}

void ExecutionContext::ConsumeWindowInteraction() {
  if (window_interaction_tokens_ == 0)
    return;
  --window_interaction_tokens_;
}

bool ExecutionContext::IsWindowInteractionAllowed() const {
  return window_interaction_tokens_ > 0;
}

bool ExecutionContext::IsSecureContext() const {
  String unused_error_message;
  return IsSecureContext(unused_error_message);
}

// https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
String ExecutionContext::OutgoingReferrer() const {
  // Step 3.1: "If environment's global object is a Window object, then"
  // This case is implemented in Document::OutgoingReferrer().

  // Step 3.2: "Otherwise, let referrerSource be environment's creation URL."
  return Url().StrippedForUseAsReferrer();
}

void ExecutionContext::ParseAndSetReferrerPolicy(const String& policies,
                                                 bool support_legacy_keywords) {
  network::mojom::ReferrerPolicy referrer_policy;

  if (!SecurityPolicy::ReferrerPolicyFromHeaderValue(
          policies,
          support_legacy_keywords ? kSupportReferrerPolicyLegacyKeywords
                                  : kDoNotSupportReferrerPolicyLegacyKeywords,
          &referrer_policy)) {
    AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kRendering,
        mojom::ConsoleMessageLevel::kError,
        "Failed to set referrer policy: The value '" + policies +
            "' is not one of " +
            (support_legacy_keywords
                 ? "'always', 'default', 'never', 'origin-when-crossorigin', "
                 : "") +
            "'no-referrer', 'no-referrer-when-downgrade', 'origin', "
            "'origin-when-cross-origin', 'same-origin', 'strict-origin', "
            "'strict-origin-when-cross-origin', or 'unsafe-url'. The referrer "
            "policy "
            "has been left unchanged."));
    return;
  }

  SetReferrerPolicy(referrer_policy);
}

void ExecutionContext::SetReferrerPolicy(
    network::mojom::ReferrerPolicy referrer_policy) {
  // When a referrer policy has already been set, the latest value takes
  // precedence.
  UseCounter::Count(this, WebFeature::kSetReferrerPolicy);
  if (referrer_policy_ != network::mojom::ReferrerPolicy::kDefault)
    UseCounter::Count(this, WebFeature::kResetReferrerPolicy);

  referrer_policy_ = referrer_policy;
}

void ExecutionContext::RemoveURLFromMemoryCache(const KURL& url) {
  GetMemoryCache()->RemoveURLFromCache(url);
}

void ExecutionContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(public_url_manager_);
  visitor->Trace(pending_exceptions_);
  visitor->Trace(csp_delegate_);
  visitor->Trace(agent_);
  visitor->Trace(origin_trial_context_);
  ContextLifecycleNotifier::Trace(visitor);
  ConsoleLogger::Trace(visitor);
  Supplementable<ExecutionContext>::Trace(visitor);
}

bool ExecutionContext::IsSameAgentCluster(
    const base::UnguessableToken& other_id) const {
  base::UnguessableToken this_id = GetAgentClusterID();
  // If the AgentClusterID is empty then it should never be the same (e.g.
  // currently for worklets).
  if (this_id.is_empty() || other_id.is_empty())
    return false;
  return this_id == other_id;
}

v8::MicrotaskQueue* ExecutionContext::GetMicrotaskQueue() const {
  // TODO(keishi): Convert to DCHECK once we assign agents everywhere.
  if (!agent_)
    return nullptr;
  DCHECK(agent_->event_loop());
  return agent_->event_loop()->microtask_queue();
}

bool ExecutionContext::FeatureEnabled(OriginTrialFeature feature) const {
  return origin_trial_context_ &&
         origin_trial_context_->IsFeatureEnabled(feature);
}

void ExecutionContext::CountFeaturePolicyUsage(mojom::WebFeature feature) {
  UseCounter::Count(*this, feature);
}

bool ExecutionContext::FeaturePolicyFeatureObserved(
    mojom::FeaturePolicyFeature feature) {
  size_t feature_index = static_cast<size_t>(feature);
  if (parsed_feature_policies_.size() == 0) {
    parsed_feature_policies_.resize(
        static_cast<size_t>(mojom::FeaturePolicyFeature::kMaxValue) + 1);
  } else if (parsed_feature_policies_[feature_index]) {
    return true;
  }
  parsed_feature_policies_[feature_index] = true;
  return false;
}

bool ExecutionContext::RequireTrustedTypes() const {
  return GetSecurityContext().TrustedTypesRequiredByPolicy() &&
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled(this);
}

}  // namespace blink
