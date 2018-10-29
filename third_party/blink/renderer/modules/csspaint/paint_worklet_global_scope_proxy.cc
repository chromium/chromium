// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_worklet_global_scope_proxy.h"

#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_content_settings_client.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

PaintWorkletGlobalScopeProxy* PaintWorkletGlobalScopeProxy::From(
    WorkletGlobalScopeProxy* proxy) {
  return static_cast<PaintWorkletGlobalScopeProxy*>(proxy);
}

PaintWorkletGlobalScopeProxy::PaintWorkletGlobalScopeProxy(
    LocalFrame* frame,
    WorkletModuleResponsesMap* module_responses_map,
    PaintWorkletPendingGeneratorRegistry* pending_generator_registry,
    size_t global_scope_number) {
  DCHECK(IsMainThread());
  Document* document = frame->GetDocument();
  reporting_proxy_ =
      std::make_unique<MainThreadWorkletReportingProxy>(document);

  WorkerClients* worker_clients = WorkerClients::Create();
  ProvideWorkerFetchContextToWorker(
      worker_clients, frame->Client()->CreateWorkerFetchContext());
  ProvideContentSettingsClientToWorker(
      worker_clients, frame->Client()->CreateWorkerContentSettingsClient());

  auto creation_params = std::make_unique<GlobalScopeCreationParams>(
      document->Url(), ScriptType::kModule, document->UserAgent(),
      document->GetContentSecurityPolicy()->Headers(),
      document->GetReferrerPolicy(), document->GetSecurityOrigin(),
      document->IsSecureContext(), document->GetHttpsState(), worker_clients,
      document->AddressSpace(), OriginTrialContext::GetTokens(document).get(),
      base::UnguessableToken::Create(), nullptr /* worker_settings */,
      kV8CacheOptionsDefault, module_responses_map);
  global_scope_ = PaintWorkletGlobalScope::Create(
      frame, std::move(creation_params), *reporting_proxy_,
      pending_generator_registry, global_scope_number);
}

void PaintWorkletGlobalScopeProxy::FetchAndInvokeScript(
    const KURL& module_url_record,
    network::mojom::FetchCredentialsMode credentials_mode,
    FetchClientSettingsObjectSnapshot* outside_settings_object,
    scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
    WorkletPendingTasks* pending_tasks) {
  DCHECK(IsMainThread());
  global_scope_->FetchAndInvokeScript(
      module_url_record, credentials_mode, outside_settings_object,
      std::move(outside_settings_task_runner), pending_tasks);
}

void PaintWorkletGlobalScopeProxy::WorkletObjectDestroyed() {
  DCHECK(IsMainThread());
  // Do nothing.
}

void PaintWorkletGlobalScopeProxy::TerminateWorkletGlobalScope() {
  DCHECK(IsMainThread());
  global_scope_->Dispose();
  // Nullify these fields to cut a potential reference cycle.
  global_scope_ = nullptr;
  reporting_proxy_.reset();
}

CSSPaintDefinition* PaintWorkletGlobalScopeProxy::FindDefinition(
    const String& name) {
  DCHECK(IsMainThread());
  return global_scope_->FindDefinition(name);
}

void PaintWorkletGlobalScopeProxy::Trace(blink::Visitor* visitor) {
  visitor->Trace(global_scope_);
}

}  // namespace blink
