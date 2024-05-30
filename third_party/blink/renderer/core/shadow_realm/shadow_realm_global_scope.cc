// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/shadow_realm/shadow_realm_global_scope.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"

namespace blink {

ShadowRealmGlobalScope::ShadowRealmGlobalScope(
    ExecutionContext* initiator_execution_context)
    : ExecutionContext(initiator_execution_context->GetIsolate(),
                       initiator_execution_context->GetAgent()),
      initiator_execution_context_(initiator_execution_context) {}

ExecutionContext* ShadowRealmGlobalScope::GetRootInitiatorExecutionContext()
    const {
  return initiator_execution_context_->IsShadowRealmGlobalScope()
             ? To<ShadowRealmGlobalScope>(initiator_execution_context_.Get())
                   ->GetRootInitiatorExecutionContext()
             : initiator_execution_context_.Get();
}

void ShadowRealmGlobalScope::Trace(Visitor* visitor) const {
  visitor->Trace(initiator_execution_context_);
  EventTarget::Trace(visitor);
  ExecutionContext::Trace(visitor);
}

const AtomicString& ShadowRealmGlobalScope::InterfaceName() const {
  return event_target_names::kShadowRealmGlobalScope;
}

ExecutionContext* ShadowRealmGlobalScope::GetExecutionContext() const {
  return const_cast<ShadowRealmGlobalScope*>(this);
}

const BrowserInterfaceBrokerProxy&
ShadowRealmGlobalScope::GetBrowserInterfaceBroker() const {
  return GetEmptyBrowserInterfaceBroker();
}

scoped_refptr<base::SingleThreadTaskRunner>
ShadowRealmGlobalScope::GetTaskRunner(TaskType task_type) {
  return initiator_execution_context_->GetTaskRunner(task_type);
}

void ShadowRealmGlobalScope::CountUse(mojom::blink::WebFeature feature) {}

void ShadowRealmGlobalScope::CountDeprecation(
    mojom::blink::WebFeature feature) {}

void ShadowRealmGlobalScope::CountWebDXFeature(
    mojom::blink::WebDXFeature feature) {}

bool ShadowRealmGlobalScope::IsShadowRealmGlobalScope() const {
  return true;
}

const KURL& ShadowRealmGlobalScope::Url() const {
  return GetRootInitiatorExecutionContext()->Url();
}

const KURL& ShadowRealmGlobalScope::BaseURL() const {
  NOTREACHED_IN_MIGRATION();
  return url_;
}

KURL ShadowRealmGlobalScope::CompleteURL(const String& url) const {
  NOTREACHED_IN_MIGRATION();
  return url_;
}

void ShadowRealmGlobalScope::DisableEval(const String& error_message) {
  NOTREACHED_IN_MIGRATION();
}

void ShadowRealmGlobalScope::SetWasmEvalErrorMessage(
    const String& error_message) {
  NOTREACHED_IN_MIGRATION();
}

String ShadowRealmGlobalScope::UserAgent() const {
  NOTREACHED_IN_MIGRATION();
  return g_empty_string;
}

HttpsState ShadowRealmGlobalScope::GetHttpsState() const {
  return CalculateHttpsState(GetSecurityOrigin());
}

ResourceFetcher* ShadowRealmGlobalScope::Fetcher() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void ShadowRealmGlobalScope::ExceptionThrown(ErrorEvent* error_event) {
  NOTREACHED_IN_MIGRATION();
}

void ShadowRealmGlobalScope::AddInspectorIssue(AuditsIssue issue) {
  NOTREACHED_IN_MIGRATION();
}

EventTarget* ShadowRealmGlobalScope::ErrorEventTarget() {
  return nullptr;
}

FrameOrWorkerScheduler* ShadowRealmGlobalScope::GetScheduler() {
  return initiator_execution_context_->GetScheduler();
}

bool ShadowRealmGlobalScope::CrossOriginIsolatedCapability() const {
  return false;
}

bool ShadowRealmGlobalScope::IsIsolatedContext() const {
  return false;
}

ukm::UkmRecorder* ShadowRealmGlobalScope::UkmRecorder() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

ukm::SourceId ShadowRealmGlobalScope::UkmSourceID() const {
  NOTREACHED_IN_MIGRATION();
  return ukm::kInvalidSourceId;
}

ExecutionContextToken ShadowRealmGlobalScope::GetExecutionContextToken() const {
  return token_;
}

void ShadowRealmGlobalScope::AddConsoleMessageImpl(ConsoleMessage* message,
                                                   bool discard_duplicates) {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace blink
