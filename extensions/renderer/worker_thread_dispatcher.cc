// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/worker_thread_dispatcher.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_local.h"
#include "base/values.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_interaction_provider.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_renderer_messaging_service.h"
#include "extensions/renderer/service_worker_data.h"
#include "extensions/renderer/worker_script_context_set.h"
#include "extensions/renderer/worker_thread_util.h"

namespace extensions {

namespace {

base::LazyInstance<WorkerThreadDispatcher>::DestructorAtExit
    g_worker_thread_dispatcher_instance = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::ThreadLocalPointer<extensions::ServiceWorkerData>>::
    DestructorAtExit g_data_tls = LAZY_INSTANCE_INITIALIZER;

ServiceWorkerData* GetServiceWorkerDataChecked() {
  ServiceWorkerData* data = WorkerThreadDispatcher::GetServiceWorkerData();
  DCHECK(data);
  return data;
}

}  // namespace

WorkerThreadDispatcher::WorkerThreadDispatcher() {}
WorkerThreadDispatcher::~WorkerThreadDispatcher() {}

WorkerThreadDispatcher* WorkerThreadDispatcher::Get() {
  return g_worker_thread_dispatcher_instance.Pointer();
}

void WorkerThreadDispatcher::Init(content::RenderThread* render_thread) {
  DCHECK(render_thread);
  DCHECK_EQ(content::RenderThread::Get(), render_thread);
  DCHECK(!message_filter_);
  message_filter_ = render_thread->GetSyncMessageFilter();
  render_thread->AddObserver(this);
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
  return g_data_tls.Pointer()->Get();
}

// static
bool WorkerThreadDispatcher::HandlesMessageOnWorkerThread(
    const IPC::Message& message) {
  return message.type() == ExtensionMsg_ResponseWorker::ID ||
         message.type() == ExtensionMsg_DispatchEvent::ID ||
         message.type() == ExtensionMsg_DispatchOnConnect::ID ||
         message.type() == ExtensionMsg_DeliverMessage::ID ||
         message.type() == ExtensionMsg_DispatchOnDisconnect::ID ||
         message.type() == ExtensionMsg_ValidateMessagePort::ID;
}

// static
void WorkerThreadDispatcher::ForwardIPC(int worker_thread_id,
                                        const IPC::Message& message) {
  WorkerThreadDispatcher::Get()->OnMessageReceivedOnWorkerThread(
      worker_thread_id, message);
}

// static
void WorkerThreadDispatcher::UpdateBindingsOnWorkerThread(
    const ExtensionId& extension_id) {
  DCHECK(worker_thread_util::IsWorkerThread());
  DCHECK(!extension_id.empty());
  GetBindingsSystem()->UpdateBindings(extension_id,
                                      true /* permissions_changed */,
                                      Dispatcher::GetWorkerScriptContextSet());
}

bool WorkerThreadDispatcher::OnControlMessageReceived(
    const IPC::Message& message) {
  if (HandlesMessageOnWorkerThread(message)) {
    int worker_thread_id = content::WorkerThread::kInvalidWorkerThreadId;
    // TODO(lazyboy): Route |message| directly to the child thread using routed
    // IPC. Probably using mojo?
    bool found = base::PickleIterator(message).ReadInt(&worker_thread_id);
    CHECK(found);
    if (worker_thread_id == kMainThreadId)
      return false;
    return PostTaskToWorkerThread(
        worker_thread_id, base::BindOnce(&WorkerThreadDispatcher::ForwardIPC,
                                         worker_thread_id, message));
  }
  return false;
}

bool WorkerThreadDispatcher::UpdateBindingsForWorkers(
    const ExtensionId& extension_id) {
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

void WorkerThreadDispatcher::OnMessageReceivedOnWorkerThread(
    int worker_thread_id,
    const IPC::Message& message) {
  CHECK_EQ(content::WorkerThread::GetCurrentId(), worker_thread_id);

  // If the worker state was already destroyed via
  // Dispatcher::WillDestroyServiceWorkerContextOnWorkerThread, then
  // drop this IPC. See https://crbug.com/1008143 for details.
  if (!GetServiceWorkerData())
    return;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WorkerThreadDispatcher, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ResponseWorker, OnResponseWorker)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchEvent, OnDispatchEvent)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchOnConnect, OnDispatchOnConnect)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DeliverMessage, OnDeliverMessage)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchOnDisconnect,
                        OnDispatchOnDisconnect)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ValidateMessagePort, OnValidateMessagePort)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  CHECK(handled);
}

bool WorkerThreadDispatcher::PostTaskToWorkerThread(int worker_thread_id,
                                                    base::OnceClosure task) {
  base::AutoLock lock(task_runner_map_lock_);
  auto it = task_runner_map_.find(worker_thread_id);
  if (it == task_runner_map_.end())
    return false;

  bool task_posted = it->second->PostTask(FROM_HERE, std::move(task));
  DCHECK(task_posted) << "Could not PostTask IPC to worker thread.";
  return task_posted;
}

bool WorkerThreadDispatcher::Send(IPC::Message* message) {
  return message_filter_->Send(message);
}

void WorkerThreadDispatcher::OnResponseWorker(int worker_thread_id,
                                              int request_id,
                                              bool succeeded,
                                              const base::ListValue& response,
                                              const std::string& error) {
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  data->bindings_system()->HandleResponse(request_id, succeeded, response,
                                          error);
}

void WorkerThreadDispatcher::OnDispatchEvent(
    const ExtensionMsg_DispatchEvent_Params& params,
    const base::ListValue& event_args) {
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  DCHECK(data);

  ScriptContext* script_context = data->context();
  // Note |scoped_extension_interaction| requires a HandleScope.
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);
  std::unique_ptr<InteractionProvider::Scope> scoped_extension_interaction;
  if (params.is_user_gesture) {
    scoped_extension_interaction =
        ExtensionInteractionProvider::Scope::ForWorker(
            script_context->v8_context());
  }
  data->bindings_system()->DispatchEventInContext(
      params.event_name, &event_args, &params.filtering_info, data->context());
  const int worker_thread_id = content::WorkerThread::GetCurrentId();
  Send(new ExtensionHostMsg_EventAckWorker(data->context()->GetExtensionID(),
                                           data->service_worker_version_id(),
                                           worker_thread_id, params.event_id));
}

void WorkerThreadDispatcher::OnDispatchOnConnect(
    int worker_thread_id,
    const PortId& target_port_id,
    const std::string& channel_name,
    const ExtensionMsg_TabConnectionInfo& source,
    const ExtensionMsg_ExternalConnectionInfo& info) {
  DCHECK_EQ(worker_thread_id, content::WorkerThread::GetCurrentId());
  WorkerThreadDispatcher::GetBindingsSystem()
      ->messaging_service()
      ->DispatchOnConnect(Dispatcher::GetWorkerScriptContextSet(),
                          target_port_id, channel_name, source, info,
                          // Render frames do not matter.
                          nullptr);
}

void WorkerThreadDispatcher::OnValidateMessagePort(int worker_thread_id,
                                                   const PortId& id) {
  DCHECK_EQ(content::WorkerThread::GetCurrentId(), worker_thread_id);
  WorkerThreadDispatcher::GetBindingsSystem()
      ->messaging_service()
      ->ValidateMessagePort(Dispatcher::GetWorkerScriptContextSet(), id,
                            // Render frames do not matter.
                            nullptr);
}

void WorkerThreadDispatcher::OnDeliverMessage(int worker_thread_id,
                                              const PortId& target_port_id,
                                              const Message& message) {
  WorkerThreadDispatcher::GetBindingsSystem()
      ->messaging_service()
      ->DeliverMessage(Dispatcher::GetWorkerScriptContextSet(), target_port_id,
                       message,
                       // Render frames do not matter.
                       nullptr);
}

void WorkerThreadDispatcher::OnDispatchOnDisconnect(
    int worker_thread_id,
    const PortId& port_id,
    const std::string& error_message) {
  WorkerThreadDispatcher::GetBindingsSystem()
      ->messaging_service()
      ->DispatchOnDisconnect(Dispatcher::GetWorkerScriptContextSet(), port_id,
                             error_message,
                             // Render frames do not matter.
                             nullptr);
}

void WorkerThreadDispatcher::AddWorkerData(
    int64_t service_worker_version_id,
    ScriptContext* script_context,
    std::unique_ptr<NativeExtensionBindingsSystem> bindings_system) {
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  if (!data) {
    ServiceWorkerData* new_data = new ServiceWorkerData(
        service_worker_version_id, script_context, std::move(bindings_system));
    g_data_tls.Pointer()->Set(new_data);
  }

  int worker_thread_id = content::WorkerThread::GetCurrentId();
  {
    base::AutoLock lock(task_runner_map_lock_);
    auto* task_runner = base::ThreadTaskRunnerHandle::Get().get();
    CHECK(task_runner);
    task_runner_map_[worker_thread_id] = task_runner;
  }
}

void WorkerThreadDispatcher::DidInitializeContext(
    int64_t service_worker_version_id) {
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  DCHECK_EQ(service_worker_version_id, data->service_worker_version_id());
  const int thread_id = content::WorkerThread::GetCurrentId();
  DCHECK_NE(thread_id, kMainThreadId);
  Send(new ExtensionHostMsg_DidInitializeServiceWorkerContext(
      data->context()->GetExtensionID(), service_worker_version_id, thread_id));
}

void WorkerThreadDispatcher::DidStartContext(
    const GURL& service_worker_scope,
    int64_t service_worker_version_id) {
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  DCHECK_EQ(service_worker_version_id, data->service_worker_version_id());
  const int thread_id = content::WorkerThread::GetCurrentId();
  DCHECK_NE(thread_id, kMainThreadId);
  Send(new ExtensionHostMsg_DidStartServiceWorkerContext(
      data->context()->GetExtensionID(), service_worker_scope,
      service_worker_version_id, thread_id));
}

void WorkerThreadDispatcher::DidStopContext(const GURL& service_worker_scope,
                                            int64_t service_worker_version_id) {
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  const int thread_id = content::WorkerThread::GetCurrentId();
  DCHECK_NE(thread_id, kMainThreadId);
  DCHECK_EQ(service_worker_version_id, data->service_worker_version_id());
  Send(new ExtensionHostMsg_DidStopServiceWorkerContext(
      data->context()->GetExtensionID(), service_worker_scope,
      service_worker_version_id, thread_id));
}

void WorkerThreadDispatcher::RemoveWorkerData(
    int64_t service_worker_version_id) {
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  if (data) {
    DCHECK_EQ(service_worker_version_id, data->service_worker_version_id());
    delete data;
    g_data_tls.Pointer()->Set(nullptr);
  }

  int worker_thread_id = content::WorkerThread::GetCurrentId();
  {
    base::AutoLock lock(task_runner_map_lock_);
    task_runner_map_.erase(worker_thread_id);
  }
}

}  // namespace extensions
