// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/threaded_worklet_object_proxy.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/workers/threaded_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"

namespace blink {

std::unique_ptr<ThreadedWorkletObjectProxy> ThreadedWorkletObjectProxy::Create(
    ThreadedWorkletMessagingProxy* messaging_proxy_weak_ptr,
    ParentExecutionContextTaskRunners* parent_execution_context_task_runners,
    scoped_refptr<base::SingleThreadTaskRunner>
        parent_agent_group_task_runner) {
  DCHECK(messaging_proxy_weak_ptr);
  return base::WrapUnique(new ThreadedWorkletObjectProxy(
      messaging_proxy_weak_ptr, parent_execution_context_task_runners,
      std::move(parent_agent_group_task_runner)));
}

ThreadedWorkletObjectProxy::~ThreadedWorkletObjectProxy() = default;

void ThreadedWorkletObjectProxy::FetchAndInvokeScript(
    const KURL& module_url_record,
    network::mojom::CredentialsMode credentials_mode,
    std::unique_ptr<CrossThreadFetchClientSettingsObjectData>
        outside_settings_object,
    WorkerResourceTimingNotifier* outside_resource_timing_notifier,
    scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
    WorkletPendingTasks* pending_tasks,
    WorkerThread* worker_thread) {
  DCHECK(outside_resource_timing_notifier);
  auto* global_scope = To<WorkletGlobalScope>(worker_thread->GlobalScope());
  global_scope->FetchAndInvokeScript(
      module_url_record, credentials_mode,
      *MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
          std::move(outside_settings_object)),
      *outside_resource_timing_notifier,
      std::move(outside_settings_task_runner), pending_tasks);
}

ThreadedWorkletObjectProxy::ThreadedWorkletObjectProxy(
    ThreadedWorkletMessagingProxy* messaging_proxy_weak_ptr,
    ParentExecutionContextTaskRunners* parent_execution_context_task_runners,
    scoped_refptr<base::SingleThreadTaskRunner> parent_agent_group_task_runner)
    : ThreadedObjectProxyBase(parent_execution_context_task_runners,
                              std::move(parent_agent_group_task_runner)),
      messaging_proxy_weak_ptr_(messaging_proxy_weak_ptr) {}

CrossThreadWeakPersistent<ThreadedMessagingProxyBase>
ThreadedWorkletObjectProxy::MessagingProxyWeakPtr() {
  return messaging_proxy_weak_ptr_;
}

}  // namespace blink
