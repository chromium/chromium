// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/worker_thread_dispatcher.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_local.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/renderer/api/messaging/native_renderer_messaging_service.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_interaction_provider.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/service_worker_data.h"
#include "extensions/renderer/worker_script_context_set.h"
#include "extensions/renderer/worker_thread_util.h"

namespace extensions {

namespace {

constinit thread_local extensions::ServiceWorkerData* service_worker_data =
    nullptr;

ServiceWorkerData* GetServiceWorkerDataChecked() {
  ServiceWorkerData* data = WorkerThreadDispatcher::GetServiceWorkerData();
  DCHECK(data);
  return data;
}

}  // namespace

WorkerThreadDispatcher::WorkerThreadDispatcher() = default;
WorkerThreadDispatcher::~WorkerThreadDispatcher() = default;

WorkerThreadDispatcher* WorkerThreadDispatcher::Get() {
  static base::NoDestructor<WorkerThreadDispatcher> dispatcher;
  return dispatcher.get();
}

void WorkerThreadDispatcher::Init(content::RenderThread* render_thread) {
  DCHECK(render_thread);
  DCHECK_EQ(content::RenderThread::Get(), render_thread);
}

// static
NativeExtensionBindingsSystem* WorkerThreadDispatcher::GetBindingsSystem() {
  return GetServiceWorkerDataChecked()->bindings_system();
}

// static
V8SchemaRegistry* WorkerThreadDispatcher::GetV8SchemaRegistry() {
  return GetServiceWorkerDataChecked()->v8_schema_registry();
}

// static
ScriptContext* WorkerThreadDispatcher::GetScriptContext() {
  return GetServiceWorkerDataChecked()->context();
}

// static
ServiceWorkerData* WorkerThreadDispatcher::GetServiceWorkerData() {
  return service_worker_data;
}

// static
void WorkerThreadDispatcher::UpdateBindingsOnWorkerThread(
    const std::optional<ExtensionId>& extension_id) {
  DCHECK(worker_thread_util::IsWorkerThread());
  DCHECK(!extension_id || !extension_id->empty())
      << "If provided, `extension_id` must be non-empty.";

  ServiceWorkerData* data = WorkerThreadDispatcher::GetServiceWorkerData();
  // Bail out if the worker was destroyed.
  if (!data || !data->bindings_system()) {
    return;
  }

  // NativeExtensionBindingsSystem::UpdateBindings() will update all bindings
  // if the provided ExtensionId is empty.
  data->bindings_system()->UpdateBindings(
      extension_id.value_or(ExtensionId()), true /* permissions_changed */,
      Dispatcher::GetWorkerScriptContextSet());
}

bool WorkerThreadDispatcher::UpdateBindingsForWorkers(
    const ExtensionId& extension_id) {
  return UpdateBindingsHelper(extension_id);
}

void WorkerThreadDispatcher::UpdateAllServiceWorkerBindings() {
  UpdateBindingsHelper(std::nullopt);
}

void WorkerThreadDispatcher::AddWorkerData(
    blink::WebServiceWorkerContextProxy* proxy,
    int64_t service_worker_version_id,
    const std::optional<base::UnguessableToken>& activation_sequence,
    const blink::ServiceWorkerToken& service_worker_token,
    ScriptContext* script_context,
    std::unique_ptr<NativeExtensionBindingsSystem> bindings_system) {
  if (!service_worker_data) {
    service_worker_data = new ServiceWorkerData(
        proxy, service_worker_version_id, std::move(activation_sequence),
        service_worker_token, script_context, std::move(bindings_system));
  }

  int worker_thread_id = content::WorkerThread::GetCurrentId();
  {
    base::AutoLock lock(task_runner_map_lock_);
    auto* task_runner = base::SingleThreadTaskRunner::GetCurrentDefault().get();
    CHECK(task_runner);
    task_runner_map_[worker_thread_id] = task_runner;
  }
}

void WorkerThreadDispatcher::RemoveWorkerData(
    int64_t service_worker_version_id) {
  if (service_worker_data) {
    DCHECK_EQ(service_worker_version_id,
              service_worker_data->service_worker_version_id());
    delete service_worker_data;
    service_worker_data = nullptr;
  }

  int worker_thread_id = content::WorkerThread::GetCurrentId();
  {
    base::AutoLock lock(task_runner_map_lock_);
    task_runner_map_.erase(worker_thread_id);
  }
}

ScriptContextSetIterable* WorkerThreadDispatcher::GetScriptContextSet() {
  return Dispatcher::GetWorkerScriptContextSet();
}

bool WorkerThreadDispatcher::UpdateBindingsHelper(
    const std::optional<ExtensionId>& extension_id) {
  bool success = true;
  base::AutoLock lock(task_runner_map_lock_);
  for (const auto& task_runner_info : task_runner_map_) {
    const int worker_thread_id = task_runner_info.first;
    base::TaskRunner* runner = task_runner_map_[worker_thread_id];
    bool posted = runner->PostTask(
        FROM_HERE,
        base::BindOnce(&WorkerThreadDispatcher::UpdateBindingsOnWorkerThread,
                       extension_id));
    success &= posted;
  }
  return success;
}

}  // namespace extensions
