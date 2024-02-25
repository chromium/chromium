// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/layout_worklet_global_scope_proxy.h"

#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

LayoutWorkletGlobalScopeProxy* LayoutWorkletGlobalScopeProxy::From(
    WorkletGlobalScopeProxy* proxy) {
  return static_cast<LayoutWorkletGlobalScopeProxy*>(proxy);
}

LayoutWorkletGlobalScopeProxy::LayoutWorkletGlobalScopeProxy(
    LocalFrame* frame,
    WorkletModuleResponsesMap* module_responses_map,
    PendingLayoutRegistry* pending_layout_registry,
    size_t global_scope_number) {
  DCHECK(IsMainThread());
  LocalDOMWindow* window = frame->DomWindow();
  reporting_proxy_ = std::make_unique<MainThreadWorkletReportingProxy>(window);

  String global_scope_name =
      StringView("LayoutWorklet #") + String::Number(global_scope_number);

  LocalFrameClient* frame_client = frame->Client();
  auto creation_params = std::make_unique<GlobalScopeCreationParams>(
      window->Url(), mojom::blink::ScriptType::kModule, global_scope_name,
      frame_client->UserAgent(), frame_client->UserAgentMetadata(),
      frame_client->CreateWorkerFetchContext(),
      mojo::Clone(window->GetContentSecurityPolicy()->GetParsedPolicies()),
      Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
      window->GetReferrerPolicy(), window->GetSecurityOrigin(),
      window->IsSecureContext(), window->GetHttpsState(),
      nullptr /* worker_clients */,
      frame_client->CreateWorkerContentSettingsClient(),
      OriginTrialContext::GetInheritedTrialFeatures(window).get(),
      base::UnguessableToken::Create(), nullptr /* worker_settings */,
      mojom::blink::V8CacheOptions::kDefault, module_responses_map,
      mojo::NullRemote() /* browser_interface_broker */,
      mojo::NullRemote() /* code_cache_host_interface */,
      mojo::NullRemote() /* blob_url_store */, BeginFrameProviderParams(),
      nullptr /* parent_permissions_policy */, window->GetAgentClusterID(),
      ukm::kInvalidSourceId, window->GetExecutionContextToken(),
      window->CrossOriginIsolatedCapability(), window->IsIsolatedContext());
  global_scope_ = LayoutWorkletGlobalScope::Create(
      frame, std::move(creation_params), *reporting_proxy_,
      pending_layout_registry);
}

void LayoutWorkletGlobalScopeProxy::FetchAndInvokeScript(
    const KURL& module_url_record,
    network::mojom::CredentialsMode credentials_mode,
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    WorkerResourceTimingNotifier& outside_resource_timing_notifier,
    scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
    WorkletPendingTasks* pending_tasks) {
  DCHECK(IsMainThread());
  global_scope_->FetchAndInvokeScript(
      module_url_record, credentials_mode, outside_settings_object,
      outside_resource_timing_notifier, std::move(outside_settings_task_runner),
      pending_tasks);
}

void LayoutWorkletGlobalScopeProxy::WorkletObjectDestroyed() {
  DCHECK(IsMainThread());
  // Do nothing.
}

void LayoutWorkletGlobalScopeProxy::TerminateWorkletGlobalScope() {
  DCHECK(IsMainThread());
  global_scope_->Dispose();
  // Nullify these fields to cut a potential reference cycle.
  global_scope_ = nullptr;
  reporting_proxy_.reset();
}

CSSLayoutDefinition* LayoutWorkletGlobalScopeProxy::FindDefinition(
    const AtomicString& name) {
  DCHECK(IsMainThread());
  return global_scope_->FindDefinition(name);
}

void LayoutWorkletGlobalScopeProxy::Trace(Visitor* visitor) const {
  visitor->Trace(global_scope_);
}

}  // namespace blink
