// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worklet.h"

#include <optional>

#include "base/task/single_thread_task_runner.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_worklet_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/worker_resource_timing_notifier_impl.h"
#include "third_party/blink/renderer/core/workers/worklet_pending_tasks.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

Worklet::Worklet(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(&window),
      module_responses_map_(MakeGarbageCollected<WorkletModuleResponsesMap>()) {
  DCHECK(IsMainThread());
}

Worklet::~Worklet() {
  DCHECK(!HasPendingTasks());
}

void Worklet::Dispose() {
  for (const auto& proxy : proxies_)
    proxy->WorkletObjectDestroyed();
}

// Implementation of the first half of the "addModule(moduleURL, options)"
// algorithm:
// https://drafts.css-houdini.org/worklets/#dom-worklet-addmodule
ScriptPromise<IDLUndefined> Worklet::addModule(
    ScriptState* script_state,
    const String& module_url,
    const WorkletOptions* options,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  if (!GetExecutionContext()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "This frame is already detached");
    return EmptyPromise();
  }
  UseCounter::Count(GetExecutionContext(),
                    mojom::WebFeature::kWorkletAddModule);

  // Step 1: "Let promise be a new promise."
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  // Step 2: "Let worklet be the current Worklet."
  // |this| is the current Worklet.

  // Step 3: "Let moduleURLRecord be the result of parsing the moduleURL
  // argument relative to the relevant settings object of this."
  KURL module_url_record = GetExecutionContext()->CompleteURL(module_url);

  // Step 4: "If moduleURLRecord is failure, then reject promise with a
  // "SyntaxError" DOMException and return promise."
  if (!module_url_record.IsValid()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSyntaxError,
        "'" + module_url + "' is not a valid URL."));
    return promise;
  }

  WorkletPendingTasks* pending_tasks =
      MakeGarbageCollected<WorkletPendingTasks>(this, resolver);
  pending_tasks_set_.insert(pending_tasks);

  // Step 5: "Return promise, and then continue running this algorithm in
  // parallel."
  // |kInternalLoading| is used here because this is a part of script module
  // loading.
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kInternalLoading)
      ->PostTask(
          FROM_HERE,
          WTF::BindOnce(&Worklet::FetchAndInvokeScript, WrapPersistent(this),
                        module_url_record, options->credentials().AsEnum(),
                        WrapPersistent(pending_tasks)));
  return promise;
}

void Worklet::ContextDestroyed() {
  DCHECK(IsMainThread());
  module_responses_map_->Dispose();
  for (const auto& proxy : proxies_)
    proxy->TerminateWorkletGlobalScope();
}

bool Worklet::HasPendingTasks() const {
  return pending_tasks_set_.size() > 0;
}

void Worklet::FinishPendingTasks(WorkletPendingTasks* pending_tasks) {
  DCHECK(IsMainThread());
  DCHECK(pending_tasks_set_.Contains(pending_tasks));
  pending_tasks_set_.erase(pending_tasks);
}

WorkletGlobalScopeProxy* Worklet::FindAvailableGlobalScope() {
  DCHECK(IsMainThread());
  return proxies_.at(SelectGlobalScope()).Get();
}

// Implementation of the second half of the "addModule(moduleURL, options)"
// algorithm:
// https://drafts.css-houdini.org/worklets/#dom-worklet-addmodule
void Worklet::FetchAndInvokeScript(const KURL& module_url_record,
                                   V8RequestCredentials::Enum credentials,
                                   WorkletPendingTasks* pending_tasks) {
  DCHECK(IsMainThread());
  if (!GetExecutionContext())
    return;

  // Step 6: "Let credentialOptions be the credentials member of options."
  network::mojom::CredentialsMode credentials_mode =
      Request::V8RequestCredentialsToCredentialsMode(credentials);

  // Step 7: "Let outsideSettings be the relevant settings object of this."
  auto* outside_settings_object =
      MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
          GetExecutionContext()
              ->Fetcher()
              ->GetProperties()
              .GetFetchClientSettingsObject());

  auto* outside_resource_timing_notifier =
      WorkerResourceTimingNotifierImpl::CreateForInsideResourceFetcher(
          *GetExecutionContext());

  // Specify TaskType::kInternalLoading because it's commonly used for module
  // loading.
  scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kInternalLoading);

  // Step 8: "Let moduleResponsesMap be worklet's module responses map."
  // ModuleResponsesMap() returns moduleResponsesMap.

  // Step 9: "Let workletGlobalScopeType be worklet's worklet global scope
  // type."
  // workletGlobalScopeType is encoded into the class name (e.g., PaintWorklet).

  // Step 10: "If the worklet's WorkletGlobalScopes is empty, run the following
  // steps:"
  //   10.1: "Create a WorkletGlobalScope given workletGlobalScopeType,
  //          moduleResponsesMap, and outsideSettings."
  //   10.2: "Add the WorkletGlobalScope to worklet's WorkletGlobalScopes."
  // "Depending on the type of worklet the user agent may create additional
  // WorkletGlobalScopes at this time."

  while (NeedsToCreateGlobalScope())
    proxies_.push_back(CreateGlobalScope());

  // Step 11: "Let pendingTaskStruct be a new pending tasks struct with counter
  // initialized to the length of worklet's WorkletGlobalScopes."
  pending_tasks->InitializeCounter(GetNumberOfGlobalScopes());

  // Step 12: "For each workletGlobalScope in the worklet's
  // WorkletGlobalScopes, queue a task on the workletGlobalScope to fetch and
  // invoke a worklet script given workletGlobalScope, moduleURLRecord,
  // moduleResponsesMap, credentialOptions, outsideSettings, pendingTaskStruct,
  // and promise."
  // moduleResponsesMap is already passed via CreateGlobalScope().
  // TODO(nhiroki): Queue a task instead of executing this here.
  for (const auto& proxy : proxies_) {
    proxy->FetchAndInvokeScript(module_url_record, credentials_mode,
                                *outside_settings_object,
                                *outside_resource_timing_notifier,
                                outside_settings_task_runner, pending_tasks);
  }
}

wtf_size_t Worklet::SelectGlobalScope() {
  DCHECK_EQ(GetNumberOfGlobalScopes(), 1u);
  return 0u;
}

void Worklet::Trace(Visitor* visitor) const {
  visitor->Trace(proxies_);
  visitor->Trace(module_responses_map_);
  visitor->Trace(pending_tasks_set_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
