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

#include "base/metrics/histogram_functions.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/permissions_policy/document_policy_features.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/policy_disposition.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/policy_value.mojom-blink.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/execution_context_csp_delegate.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/runtime_feature_state/runtime_feature_state_override_context.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_proto.h"

namespace blink {

ExecutionContext::ExecutionContext(v8::Isolate* isolate,
                                   Agent* agent,
                                   bool is_window)
    : isolate_(isolate),
      security_context_(this),
      agent_(agent),
      circular_sequential_id_(0),
      in_dispatch_error_event_(false),
      lifecycle_state_(mojom::FrameLifecycleState::kRunning),
      csp_delegate_(MakeGarbageCollected<ExecutionContextCSPDelegate>(*this)),
      window_interaction_tokens_(0),
      origin_trial_context_(MakeGarbageCollected<OriginTrialContext>(this)),
      // RuntimeFeatureStateOverrideContext shouldn't attempt to communcate back
      // to browser for ExecutionContexts that aren't windows.
      // TODO(https://crbug.com/1410817): Add support for workers/non-frames.
      runtime_feature_state_override_context_(
          MakeGarbageCollected<RuntimeFeatureStateOverrideContext>(
              this,
              this,
              /*send_runtime_features_to_browser=*/is_window)) {
  DCHECK(agent_);
}

ExecutionContext::~ExecutionContext() = default;

// static
ExecutionContext* ExecutionContext::From(const ScriptState* script_state) {
  return ToExecutionContext(script_state);
}

// static
ExecutionContext* ExecutionContext::From(v8::Local<v8::Context> context) {
  return ToExecutionContext(context);
}

// static
CodeCacheHost* ExecutionContext::GetCodeCacheHostFromContext(
    ExecutionContext* execution_context) {
  DCHECK_NE(execution_context, nullptr);
  if (execution_context->IsWindow()) {
    auto* window = To<LocalDOMWindow>(execution_context);
    if (!window->GetFrame() ||
        !window->GetFrame()->Loader().GetDocumentLoader()) {
      return nullptr;
    }
    return window->GetFrame()->Loader().GetDocumentLoader()->GetCodeCacheHost();
  }

  if (execution_context->IsWorkerGlobalScope()) {
    auto* global_scope =
        DynamicTo<WorkerOrWorkletGlobalScope>(execution_context);
    return global_scope->GetCodeCacheHost();
  }

  DCHECK(execution_context->IsWorkletGlobalScope());

  if (execution_context->IsSharedStorageWorkletGlobalScope()) {
    auto* global_scope =
        DynamicTo<WorkerOrWorkletGlobalScope>(execution_context);
    return global_scope->GetCodeCacheHost();
  }

  return nullptr;
}

void ExecutionContext::SetIsInBackForwardCache(bool value) {
  if (!is_in_back_forward_cache_ && value) {
    ContextLifecycleNotifier::observers().ForEachObserver(
        [&](ContextLifecycleObserver* observer) {
          if (!observer->IsExecutionContextLifecycleObserver()) {
            return;
          }
          ExecutionContextLifecycleObserver* execution_context_observer =
              static_cast<ExecutionContextLifecycleObserver*>(observer);
          execution_context_observer->ContextEnteredBackForwardCache();
        });
  }
  is_in_back_forward_cache_ = value;
}

void ExecutionContext::SetLifecycleState(mojom::FrameLifecycleState state) {
  if (lifecycle_state_ == state)
    return;
  lifecycle_state_ = state;
  ContextLifecycleNotifier::observers().ForEachObserver(
      [&](ContextLifecycleObserver* observer) {
        if (!observer->IsExecutionContextLifecycleObserver())
          return;
        ExecutionContextLifecycleObserver* execution_context_observer =
            static_cast<ExecutionContextLifecycleObserver*>(observer);
        if (execution_context_observer->ObserverType() !=
            ExecutionContextLifecycleObserver::kStateObjectType)
          return;
        ExecutionContextLifecycleStateObserver* state_observer =
            static_cast<ExecutionContextLifecycleStateObserver*>(
                execution_context_observer);
#if DCHECK_IS_ON()
        DCHECK_EQ(state_observer->GetExecutionContext(), this);
        DCHECK(state_observer->UpdateStateIfNeededCalled());
#endif
        state_observer->ContextLifecycleStateChanged(state);
      });
}

void ExecutionContext::NotifyContextDestroyed() {
  ContextLifecycleNotifier::NotifyContextDestroyed();
}

void ExecutionContext::CountDeprecation(WebFeature feature) {
  Deprecation::CountDeprecation(this, feature);
}

HeapObserverList<ContextLifecycleObserver>&
ExecutionContext::ContextLifecycleObserverSet() {
  return ContextLifecycleNotifier::observers();
}

unsigned ExecutionContext::ContextLifecycleStateObserverCountForTesting()
    const {
  DCHECK(!ContextLifecycleNotifier::observers().IsIteratingOverObservers());
  unsigned lifecycle_state_observers = 0;
  ContextLifecycleNotifier::observers().ForEachObserver(
      [&](ContextLifecycleObserver* observer) {
        if (!observer->IsExecutionContextLifecycleObserver())
          return;
        if (static_cast<ExecutionContextLifecycleObserver*>(observer)
                ->ObserverType() !=
            ExecutionContextLifecycleObserver::kStateObjectType)
          return;
        lifecycle_state_observers++;
      });
  return lifecycle_state_observers;
}

bool ExecutionContext::SharedArrayBufferTransferAllowed() const {
  // Enable transfer if cross-origin isolated, or if the feature is enabled.
  if (CrossOriginIsolatedCapability() ||
      RuntimeEnabledFeatures::SharedArrayBufferEnabled()) {
    return true;
  }

  // TODO(crbug.com/1184892): Remove once fixed.
  if (SchemeRegistry::ShouldTreatURLSchemeAsAllowingSharedArrayBuffers(
          GetSecurityOrigin()->Protocol())) {
    return true;
  }

  // Check if the SharedArrayBuffer is always allowed for this origin. For
  // worklets use the origin of the main document (consistent with how origin is
  // verified in origin trials).
  const SecurityOrigin* origin;
  if (auto* worklet_scope = DynamicTo<WorkletGlobalScope>(this))
    origin = worklet_scope->DocumentSecurityOrigin();
  else
    origin = GetSecurityOrigin();

  CHECK(origin);

  if (SecurityPolicy::IsSharedArrayBufferAlwaysAllowedForOrigin(origin))
    return true;

#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  // On desktop, enable transfer for the reverse Origin Trial, or if the
  // Finch "kill switch" is on, or if enabled by Enterprise Policy.
  return RuntimeEnabledFeatures::UnrestrictedSharedArrayBufferEnabled(this) ||
         RuntimeEnabledFeatures::SharedArrayBufferOnDesktopEnabled() ||
         RuntimeEnabledFeatures::
             SharedArrayBufferUnrestrictedAccessAllowedEnabled();
#endif
}

bool ExecutionContext::CheckSharedArrayBufferTransferAllowedAndReport() {
  const bool allowed = SharedArrayBufferTransferAllowed();
  // File an issue if the transfer is prohibited, or if it will be prohibited
  // in the future, and the problem is encountered for the first time in this
  // execution context. This preserves postMessage performance during the
  // transition period.
  if (!allowed ||
      (!has_filed_shared_array_buffer_transfer_issue_ &&
       !CrossOriginIsolatedCapability() &&
       !SchemeRegistry::ShouldTreatURLSchemeAsAllowingSharedArrayBuffers(
           GetSecurityOrigin()->Protocol()))) {
    has_filed_shared_array_buffer_transfer_issue_ = true;
    AuditsIssue::ReportSharedArrayBufferIssue(
        this, allowed, SharedArrayBufferIssueType::kTransferIssue);
  }
  return allowed;
}

void ExecutionContext::FileSharedArrayBufferCreationIssue() {
  // This is performance critical, only do it once per context.
  if (has_filed_shared_array_buffer_creation_issue_)
    return;
  has_filed_shared_array_buffer_creation_issue_ = true;
  // In enforced mode, the SAB constructor isn't available.
  AuditsIssue::ReportSharedArrayBufferIssue(
      this, true, SharedArrayBufferIssueType::kCreationIssue);
}

void ExecutionContext::AddConsoleMessageImpl(
    mojom::blink::ConsoleMessageSource source,
    mojom::blink::ConsoleMessageLevel level,
    const String& message,
    bool discard_duplicates,
    std::optional<mojom::ConsoleMessageCategory> category) {
  auto* console_message =
      MakeGarbageCollected<ConsoleMessage>(source, level, message);
  if (category)
    console_message->SetCategory(*category);
  AddConsoleMessage(console_message, discard_duplicates);
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

  if (pending_exceptions_.empty())
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

// TODO(crbug.com/1406134): Review each usage and see if replacing with
// IsContextFrozenOrPaused() makes sense.
bool ExecutionContext::IsContextPaused() const {
  return lifecycle_state_ == mojom::blink::FrameLifecycleState::kPaused;
}

LoaderFreezeMode ExecutionContext::GetLoaderFreezeMode() const {
  if (is_in_back_forward_cache_) {
    DCHECK_EQ(lifecycle_state_, mojom::blink::FrameLifecycleState::kFrozen);
    return LoaderFreezeMode::kBufferIncoming;
  } else if (lifecycle_state_ == mojom::blink::FrameLifecycleState::kFrozen ||
             lifecycle_state_ == mojom::blink::FrameLifecycleState::kPaused) {
    return LoaderFreezeMode::kStrict;
  }
  return LoaderFreezeMode::kNone;
}

bool ExecutionContext::IsContextFrozenOrPaused() const {
  return lifecycle_state_ == mojom::blink::FrameLifecycleState::kPaused ||
         lifecycle_state_ == mojom::blink::FrameLifecycleState::kFrozen;
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

const DOMWrapperWorld* ExecutionContext::GetCurrentWorld() const {
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> v8_context = isolate->GetCurrentContext();

  // This can be called before we enter v8, hence the context might be empty.
  if (v8_context.IsEmpty())
    return nullptr;

  return &DOMWrapperWorld::Current(isolate);
}

ContentSecurityPolicy*
ExecutionContext::GetContentSecurityPolicyForCurrentWorld() {
  return GetContentSecurityPolicyForWorld(GetCurrentWorld());
}

ContentSecurityPolicy* ExecutionContext::GetContentSecurityPolicyForWorld(
    const DOMWrapperWorld* world) {
  // Only documents support isolated worlds and only isolated worlds can have
  // their own CSP distinct from the main world CSP. Hence just return the main
  // world's content security policy by default.
  return GetContentSecurityPolicy();
}

const SecurityOrigin* ExecutionContext::GetSecurityOrigin() const {
  return security_context_.GetSecurityOrigin();
}

SecurityOrigin* ExecutionContext::GetMutableSecurityOrigin() {
  return security_context_.GetMutableSecurityOrigin();
}

ContentSecurityPolicy* ExecutionContext::GetContentSecurityPolicy() const {
  return content_security_policy_.Get();
}

void ExecutionContext::SetContentSecurityPolicy(
    ContentSecurityPolicy* content_security_policy) {
  content_security_policy_ = content_security_policy;
}

void ExecutionContext::SetRequireTrustedTypes() {
  DCHECK(require_safe_types_ ||
         content_security_policy_->IsRequireTrustedTypes());
  require_safe_types_ = true;
}

void ExecutionContext::SetRequireTrustedTypesForTesting() {
  require_safe_types_ = true;
}

network::mojom::blink::WebSandboxFlags ExecutionContext::GetSandboxFlags()
    const {
  return security_context_.GetSandboxFlags();
}

bool ExecutionContext::IsSandboxed(
    network::mojom::blink::WebSandboxFlags mask) const {
  return security_context_.IsSandboxed(mask);
}

const base::UnguessableToken& ExecutionContext::GetAgentClusterID() const {
  return GetAgent()->cluster_id();
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

bool ExecutionContext::IsSecureContext(String& error_message) const {
  if (!IsSecureContext()) {
    error_message = SecurityOrigin::IsPotentiallyTrustworthyErrorMessage();
    return false;
  }
  return true;
}

// https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
String ExecutionContext::OutgoingReferrer() const {
  // Step 3.1: "If environment's global object is a Window object, then"
  // This case is implemented in Document::OutgoingReferrer().

  // Step 3.2: "Otherwise, let referrerSource be environment's creation URL."
  return Url().StrippedForUseAsReferrer();
}

void ExecutionContext::ParseAndSetReferrerPolicy(
    const String& policy,
    const ReferrerPolicySource source) {
  network::mojom::ReferrerPolicy referrer_policy;
  bool policy_is_valid = false;

  if (source == kPolicySourceHttpHeader) {
    policy_is_valid = SecurityPolicy::ReferrerPolicyFromHeaderValue(
        policy, kDoNotSupportReferrerPolicyLegacyKeywords, &referrer_policy);
  } else if (source == kPolicySourceMetaTag) {
    policy_is_valid = (SecurityPolicy::ReferrerPolicyFromString(
        policy, kSupportReferrerPolicyLegacyKeywords, &referrer_policy));
  } else {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  if (policy_is_valid) {
    SetReferrerPolicy(referrer_policy);
  } else {
    String error_reason;
    if (source == kPolicySourceMetaTag && policy.Contains(',')) {
      // Only a single token is permitted for Meta-specified policies
      // (https://crbug.com/1093914).
      error_reason =
          "A policy specified by a meta element must contain only one token.";
    } else {
      error_reason =
          "The value '" + policy + "' is not one of " +
          ((source == kPolicySourceMetaTag)
               ? "'always', 'default', 'never', 'origin-when-crossorigin', "
               : "") +
          "'no-referrer', 'no-referrer-when-downgrade', 'origin', "
          "'origin-when-cross-origin', 'same-origin', 'strict-origin', "
          "'strict-origin-when-cross-origin', or 'unsafe-url'.";
    }

    AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kRendering,
        mojom::ConsoleMessageLevel::kError,
        "Failed to set referrer policy: " + error_reason +
            " The referrer policy has been left unchanged."));
  }
}

network::mojom::ReferrerPolicy ExecutionContext::GetReferrerPolicy() const {
  return policy_container_->GetReferrerPolicy();
}

void ExecutionContext::SetReferrerPolicy(
    network::mojom::ReferrerPolicy referrer_policy) {
  // When a referrer policy has already been set, the latest value takes
  // precedence.
  UseCounter::Count(this, WebFeature::kSetReferrerPolicy);
  if (GetReferrerPolicy() != network::mojom::ReferrerPolicy::kDefault)
    UseCounter::Count(this, WebFeature::kResetReferrerPolicy);

  policy_container_->UpdateReferrerPolicy(referrer_policy);
}

void ExecutionContext::SetPolicyContainer(
    std::unique_ptr<PolicyContainer> container) {
  policy_container_ = std::move(container);
  security_context_.SetSandboxFlags(
      policy_container_->GetPolicies().sandbox_flags);
}

std::unique_ptr<PolicyContainer> ExecutionContext::TakePolicyContainer() {
  return std::move(policy_container_);
}

void ExecutionContext::RemoveURLFromMemoryCache(const KURL& url) {
  MemoryCache::Get()->RemoveURLFromCache(url);
}

void ExecutionContext::Trace(Visitor* visitor) const {
  visitor->Trace(security_context_);
  visitor->Trace(agent_);
  visitor->Trace(public_url_manager_);
  visitor->Trace(pending_exceptions_);
  visitor->Trace(csp_delegate_);
  visitor->Trace(origin_trial_context_);
  visitor->Trace(content_security_policy_);
  visitor->Trace(runtime_feature_state_override_context_);
  MojoBindingContext::Trace(visitor);
  ConsoleLogger::Trace(visitor);
  Supplementable<ExecutionContext>::Trace(visitor);
}

bool ExecutionContext::IsSameAgentCluster(
    const base::UnguessableToken& other_id) const {
  base::UnguessableToken this_id = GetAgentClusterID();
  // If the AgentClusterID is empty then it should never be the same (e.g.
  // NullExecutionContext).
  if (this_id.is_empty() || other_id.is_empty())
    return false;
  return this_id == other_id;
}

mojom::blink::V8CacheOptions ExecutionContext::GetV8CacheOptions() const {
  return mojom::blink::V8CacheOptions::kDefault;
}

v8::MicrotaskQueue* ExecutionContext::GetMicrotaskQueue() const {
  DCHECK(GetAgent());
  DCHECK(GetAgent()->event_loop());
  return GetAgent()->event_loop()->microtask_queue();
}

bool ExecutionContext::FeatureEnabled(
    mojom::blink::OriginTrialFeature feature) const {
  return origin_trial_context_->IsFeatureEnabled(feature);
}

bool ExecutionContext::IsFeatureEnabled(
    mojom::blink::PermissionsPolicyFeature feature,
    ReportOptions report_option,
    const String& message) {
  SecurityContext::FeatureStatus status =
      security_context_.IsFeatureEnabled(feature);

  if (status.should_report &&
      report_option == ReportOptions::kReportOnFailure) {
    mojom::blink::PolicyDisposition disposition =
        status.enabled ? mojom::blink::PolicyDisposition::kReport
                       : mojom::blink::PolicyDisposition::kEnforce;

    ReportPermissionsPolicyViolation(feature, disposition,
                                     status.reporting_endpoint, message);
  }
  return status.enabled;
}

bool ExecutionContext::IsFeatureEnabled(
    mojom::blink::PermissionsPolicyFeature feature) const {
  return security_context_.IsFeatureEnabled(feature).enabled;
}

bool ExecutionContext::IsFeatureEnabled(
    mojom::blink::DocumentPolicyFeature feature) const {
  DCHECK(GetDocumentPolicyFeatureInfoMap().at(feature).default_value.Type() ==
         mojom::blink::PolicyValueType::kBool);
  return IsFeatureEnabled(feature, PolicyValue::CreateBool(true));
}

bool ExecutionContext::IsFeatureEnabled(
    mojom::blink::DocumentPolicyFeature feature,
    PolicyValue threshold_value) const {
  return security_context_.IsFeatureEnabled(feature, threshold_value).enabled;
}

bool ExecutionContext::IsFeatureEnabled(
    mojom::blink::DocumentPolicyFeature feature,
    ReportOptions report_option,
    const String& message,
    const String& source_file) {
  DCHECK(GetDocumentPolicyFeatureInfoMap().at(feature).default_value.Type() ==
         mojom::blink::PolicyValueType::kBool);
  return IsFeatureEnabled(feature, PolicyValue::CreateBool(true), report_option,
                          message, source_file);
}

bool ExecutionContext::IsFeatureEnabled(
    mojom::blink::DocumentPolicyFeature feature,
    PolicyValue threshold_value,
    ReportOptions report_option,
    const String& message,
    const String& source_file) {
  SecurityContext::FeatureStatus status =
      security_context_.IsFeatureEnabled(feature, threshold_value);
  if (status.should_report &&
      report_option == ReportOptions::kReportOnFailure) {
    // If both |enabled| and |should_report| are true, the usage must have
    // violated the report-only policy, i.e. |disposition| ==
    // mojom::blink::PolicyDisposition::kReport.
    ReportDocumentPolicyViolation(
        feature,
        status.enabled ? mojom::blink::PolicyDisposition::kReport
                       : mojom::blink::PolicyDisposition::kEnforce,
        message, source_file);
  }
  return status.enabled;
}

bool ExecutionContext::RequireTrustedTypes() const {
  return require_safe_types_;
}

namespace {
using ContextType = ExecutionContext::Proto::ContextType;
ContextType GetContextType(const ExecutionContext& execution_context) {
  if (execution_context.IsWorkletGlobalScope()) {
    return ContextType::WORKLET;
  } else if (execution_context.IsDedicatedWorkerGlobalScope()) {
    return ContextType::DEDICATED_WORKER;
  } else if (execution_context.IsSharedWorkerGlobalScope()) {
    return ContextType::SHARED_WORKER;
  } else if (execution_context.IsServiceWorkerGlobalScope()) {
    return ContextType::SERVICE_WORKER;
  } else if (execution_context.IsWindow()) {
    return ContextType::WINDOW;
  }
  return ContextType::UNKNOWN_CONTEXT;
}

using WorldType = ExecutionContext::Proto::WorldType;
WorldType GetWorldType(const ExecutionContext& execution_context) {
  auto* current_world = execution_context.GetCurrentWorld();
  if (current_world == nullptr) {
    return WorldType::WORLD_UNKNOWN;
  }

  switch (current_world->GetWorldType()) {
    case DOMWrapperWorld::WorldType::kMain:
      return WorldType::WORLD_MAIN;
    case DOMWrapperWorld::WorldType::kIsolated:
      return WorldType::WORLD_ISOLATED;
    case DOMWrapperWorld::WorldType::kInspectorIsolated:
      return WorldType::WORLD_INSPECTOR_ISOLATED;
    case DOMWrapperWorld::WorldType::kRegExp:
      return WorldType::WORLD_REG_EXP;
    case DOMWrapperWorld::WorldType::kForV8ContextSnapshotNonMain:
      return WorldType::WORLD_FOR_V8_CONTEXT_SNAPSHOT_NON_MAIN;
    case DOMWrapperWorld::WorldType::kWorkerOrWorklet:
      return WorldType::WORLD_WORKER;
    case DOMWrapperWorld::WorldType::kShadowRealm:
      return WorldType::WORLD_SHADOW_REALM;
    default:
      return WorldType::WORLD_UNKNOWN;
  }
}
}  // namespace

void ExecutionContext::WriteIntoTrace(
    perfetto::TracedProto<ExecutionContext::Proto> proto) const {
  proto->set_url(Url().GetString().Utf8());
  proto->set_origin(GetSecurityOrigin()->ToString().Utf8());
  proto->set_type(GetContextType(*this));
  proto->set_world_type(GetWorldType(*this));
}

bool ExecutionContext::CrossOriginIsolatedCapabilityOrDisabledWebSecurity()
    const {
  return Agent::IsWebSecurityDisabled() || CrossOriginIsolatedCapability();
}

bool ExecutionContext::IsInjectionMitigatedContext() const {
  // Isolated Contexts have multiple layers of defense against injection, which
  // allows them to have a CSP that doesn't exactly match the way we need to
  // defend against injection on the broader web. We'll consider those contexts
  // to sufficiently mitigate injection attacks, and check the page's policy for
  // all other cases.
  if (IsIsolatedContext())
    return true;

  if (!GetContentSecurityPolicy()) {
    return false;
  }
  return GetContentSecurityPolicy()->IsStrictPolicyEnforced() &&
         GetContentSecurityPolicy()->RequiresTrustedTypes();
}

}  // namespace blink
