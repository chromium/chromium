// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/shadow_realm/shadow_realm_global_scope.h"

#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"

namespace blink {

ShadowRealmGlobalScope::ShadowRealmGlobalScope(
    ExecutionContext* initiator_execution_context)
    : ExecutionContext(initiator_execution_context->GetIsolate(),
                       initiator_execution_context->GetAgent()),
      initiator_execution_context_(initiator_execution_context) {}

void ShadowRealmGlobalScope::Trace(Visitor* visitor) const {
  visitor->Trace(initiator_execution_context_);
  EventTargetWithInlineData::Trace(visitor);
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

bool ShadowRealmGlobalScope::IsShadowRealmGlobalScope() const {
  return true;
}

const KURL& ShadowRealmGlobalScope::Url() const {
  NOTREACHED();
  return url_;
}

const KURL& ShadowRealmGlobalScope::BaseURL() const {
  NOTREACHED();
  return url_;
}

KURL ShadowRealmGlobalScope::CompleteURL(const String& url) const {
  NOTREACHED();
  return url_;
}

void ShadowRealmGlobalScope::DisableEval(const String& error_message) {
  NOTREACHED();
}

void ShadowRealmGlobalScope::SetWasmEvalErrorMessage(
    const String& error_message) {
  NOTREACHED();
}

String ShadowRealmGlobalScope::UserAgent() const {
  NOTREACHED();
  return g_empty_string;
}

HttpsState ShadowRealmGlobalScope::GetHttpsState() const {
  return CalculateHttpsState(GetSecurityOrigin());
}

ResourceFetcher* ShadowRealmGlobalScope::Fetcher() {
  NOTREACHED();
  return nullptr;
}

void ShadowRealmGlobalScope::ExceptionThrown(ErrorEvent* error_event) {
  NOTREACHED();
}

void ShadowRealmGlobalScope::AddInspectorIssue(
    mojom::blink::InspectorIssueInfoPtr issue) {
  NOTREACHED();
}

void ShadowRealmGlobalScope::AddInspectorIssue(AuditsIssue issue) {
  NOTREACHED();
}

EventTarget* ShadowRealmGlobalScope::ErrorEventTarget() {
  NOTREACHED();
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
  NOTREACHED();
  return nullptr;
}

ukm::SourceId ShadowRealmGlobalScope::UkmSourceID() const {
  NOTREACHED();
  return ukm::kInvalidSourceId;
}

ExecutionContextToken ShadowRealmGlobalScope::GetExecutionContextToken() const {
  return token_;
}

void ShadowRealmGlobalScope::AddConsoleMessageImpl(ConsoleMessage* message,
                                                   bool discard_duplicates) {
  NOTREACHED();
}

}  // namespace blink
