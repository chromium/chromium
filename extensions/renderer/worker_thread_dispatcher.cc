// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/worker_thread_dispatcher.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_local.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/renderer/api/messaging/native_renderer_messaging_service.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_interaction_provider.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/service_worker_data.h"
#include "extensions/renderer/worker_script_context_set.h"
#include "extensions/renderer/worker_thread_util.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

namespace extensions {

namespace {

ABSL_CONST_INIT thread_local extensions::ServiceWorkerData*
    service_worker_data = nullptr;

ServiceWorkerData* GetServiceWorkerDataChecked() {
  ServiceWorkerData* data = WorkerThreadDispatcher::GetServiceWorkerData();
  DCHECK(data);
  return data;
}

// Calls mojom::EventRouter::AddListenerForServiceWorker(). It should be called
// on the IO thread.
void AddEventListenerOnIO(const std::string& extension_id,
                          const GURL& scope,
                          const std::string& event_name,
                          int64_t service_worker_version_id,
                          int worker_thread_id) {
  auto* dispatcher = WorkerThreadDispatcher::Get();
  dispatcher->GetEventRouterOnIO()->AddListenerForServiceWorker(
      extension_id, scope, event_name, service_worker_version_id,
      worker_thread_id);
}

// Calls mojom::EventRouter::RemoveListenerForServiceWorker(). It should be
// called on the IO thread.
void RemoveEventListenerOnIO(const std::string& extension_id,
                             const GURL& scope,
                             const std::string& event_name,
                             int64_t service_worker_version_id,
                             int worker_thread_id) {
  auto* dispatcher = WorkerThreadDispatcher::Get();
  dispatcher->GetEventRouterOnIO()->RemoveListenerForServiceWorker(
      extension_id, scope, event_name, service_worker_version_id,
      worker_thread_id);
}

// Calls mojom::EventRouter::AddLazyListenerForServiceWorker(). It should be
// called on the IO thread.
void AddEventLazyListenerOnIO(const std::string& extension_id,
                              const GURL& scope,
                              const std::string& event_name) {
  auto* dispatcher = WorkerThreadDispatcher::Get();
  dispatcher->GetEventRouterOnIO()->AddLazyListenerForServiceWorker(
      extension_id, scope, event_name);
}

// Calls mojom::EventRouter::RemoveLazyListenerForServiceWorker(). It should be
// called on the IO thread.
void RemoveEventLazyListenerOnIO(const std::string& extension_id,
                                 const GURL& scope,
                                 const std::string& event_name) {
  auto* dispatcher = WorkerThreadDispatcher::Get();
  dispatcher->GetEventRouterOnIO()->RemoveLazyListenerForServiceWorker(
      extension_id, scope, event_name);
}

// Calls mojom::EventRouter::AddFilteredListenerForServiceWorker(). It should be
// called on the IO thread.
void AddEventFilteredListenerOnIO(const std::string& extension_id,
                                  const GURL& scope,
                                  const std::string& event_name,
                                  int64_t service_worker_version_id,
                                  int worker_thread_id,
                                  base::Value::Dict filter,
                                  bool add_lazy_listener) {
  auto* dispatcher = WorkerThreadDispatcher::Get();
  dispatcher->GetEventRouterOnIO()->AddFilteredListenerForServiceWorker(
      extension_id, scope, event_name, service_worker_version_id,
      worker_thread_id, std::move(filter), add_lazy_listener);
}

// Calls mojom::EventRouter::RemoveFilteredListenerForServiceWorker(). It should
// be called on the IO thread.
void RemoveEventFilteredListenerOnIO(const std::string& extension_id,
                                     const GURL& scope,
                                     const std::string& event_name,
                                     int64_t service_worker_version_id,
                                     int worker_thread_id,
                                     base::Value::Dict filter,
                                     bool remove_lazy_listener) {
  auto* dispatcher = WorkerThreadDispatcher::Get();
  dispatcher->GetEventRouterOnIO()->RemoveFilteredListenerForServiceWorker(
      extension_id, scope, event_name, service_worker_version_id,
      worker_thread_id, std::move(filter), remove_lazy_listener);
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
  DCHECK(!message_filter_);
  message_filter_ = render_thread->GetSyncMessageFilter();
  io_task_runner_ = render_thread->GetIOTaskRunner();
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
  return service_worker_data;
}

// static
bool WorkerThreadDispatcher::HandlesMessageOnWorkerThread(
    const IPC::Message& message) {
  return message.type() == ExtensionMsg_ResponseWorker::ID ||
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

  ServiceWorkerData* data = WorkerThreadDispatcher::GetServiceWorkerData();
  // Bail out if the worker was destroyed.
  if (!data)
    return;
  data->bindings_system()->UpdateBindings(
      extension_id, true /* permissions_changed */,
      Dispatcher::GetWorkerScriptContextSet());
}

// static
void WorkerThreadDispatcher::DispatchEventOnWorkerThread(
    mojom::DispatchEventParamsPtr params,
    base::Value::List event_args) {
  auto* dispatcher = WorkerThreadDispatcher::Get();
  dispatcher->DispatchEventHelper(std::move(params), std::move(event_args));
}

bool WorkerThreadDispatcher::OnControlMessageReceived(
    const IPC::Message& message) {
  if (!HandlesMessageOnWorkerThread(message)) {
    return false;
  }

  int worker_thread_id = content::WorkerThread::kInvalidWorkerThreadId;
  // TODO(lazyboy): Route |message| directly to the child thread using routed
  // IPC. Probably using mojo?
  bool found = base::PickleIterator(message).ReadInt(&worker_thread_id);
  CHECK(found);
  if (worker_thread_id == kMainThreadId) {
    return false;
  }

  // If posting the task fails, we still have to return true, which effectively
  // drops the message. Otherwise, the caller will dispatch the message to the
  // main thread, which is wrong.
  if (!PostTaskToWorkerThread(
          worker_thread_id, base::BindOnce(&WorkerThreadDispatcher::ForwardIPC,
                                           worker_thread_id, message))) {
    LOG(ERROR) << "Failed to post task for message: " << message.type();
  }
  return true;
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

void WorkerThreadDispatcher::SendAddEventListener(
    const std::string& extension_id,
    const GURL& scope,
    const std::string& event_name,
    int64_t service_worker_version_id,
    int worker_thread_id) {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AddEventListenerOnIO, extension_id, scope, event_name,
                     service_worker_version_id, worker_thread_id));
}

void WorkerThreadDispatcher::SendAddEventLazyListener(
    const std::string& extension_id,
    const GURL& scope,
    const std::string& event_name) {
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AddEventLazyListenerOnIO, extension_id, scope,
                                event_name));
}

void WorkerThreadDispatcher::SendAddEventFilteredListener(
    const std::string& extension_id,
    const GURL& scope,
    const std::string& event_name,
    int64_t service_worker_version_id,
    int worker_thread_id,
    base::Value::Dict filter,
    bool add_lazy_listener) {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AddEventFilteredListenerOnIO, extension_id, scope,
                     event_name, service_worker_version_id, worker_thread_id,
                     std::move(filter), add_lazy_listener));
}

void WorkerThreadDispatcher::SendRemoveEventListener(
    const std::string& extension_id,
    const GURL& scope,
    const std::string& event_name,
    int64_t service_worker_version_id,
    int worker_thread_id) {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RemoveEventListenerOnIO, extension_id, scope, event_name,
                     service_worker_version_id, worker_thread_id));
}

void WorkerThreadDispatcher::SendRemoveEventLazyListener(
    const std::string& extension_id,
    const GURL& scope,
    const std::string& event_name) {
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RemoveEventLazyListenerOnIO, extension_id,
                                scope, event_name));
}

void WorkerThreadDispatcher::SendRemoveEventFilteredListener(
    const std::string& extension_id,
    const GURL& scope,
    const std::string& event_name,
    int64_t service_worker_version_id,
    int worker_thread_id,
    base::Value::Dict filter,
    bool remove_lazy_listener) {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RemoveEventFilteredListenerOnIO, extension_id, scope,
                     event_name, service_worker_version_id, worker_thread_id,
                     std::move(filter), remove_lazy_listener));
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

void WorkerThreadDispatcher::PostTaskToIOThread(base::OnceClosure task) {
  bool task_posted = io_task_runner_->PostTask(FROM_HERE, std::move(task));
  DCHECK(task_posted) << "Could not PostTask IPC to IO thread.";
}

bool WorkerThreadDispatcher::Send(IPC::Message* message) {
  return message_filter_->Send(message);
}

mojom::EventRouter* WorkerThreadDispatcher::GetEventRouterOnIO() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!event_router_remote_) {
    mojo::PendingAssociatedRemote<mojom::EventRouter>
        pending_event_router_remote;
    message_filter_->GetRemoteAssociatedInterface(&pending_event_router_remote);
    event_router_remote_.Bind(std::move(pending_event_router_remote));
  }
  return event_router_remote_.get();
}

mojom::ServiceWorkerHost* WorkerThreadDispatcher::GetServiceWorkerHostOnIO() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!service_worker_host_) {
    mojo::PendingAssociatedRemote<mojom::ServiceWorkerHost>
        pending_service_worker_host;
    message_filter_->GetRemoteAssociatedInterface(&pending_service_worker_host);
    service_worker_host_.Bind(std::move(pending_service_worker_host));
  }
  return service_worker_host_.get();
}

void WorkerThreadDispatcher::OnResponseWorker(
    int worker_thread_id,
    int request_id,
    bool succeeded,
    ExtensionMsg_ResponseWorkerData response,
    const std::string& error) {
  service_worker_data->bindings_system()->HandleResponse(
      request_id, succeeded, response.results, error,
      std::move(response.extra_data));
}

void WorkerThreadDispatcher::DispatchEventHelper(
    mojom::DispatchEventParamsPtr params,
    base::Value::List event_args) {
  DCHECK_EQ(params->worker_thread_id, content::WorkerThread::GetCurrentId());

  // If the worker state was already destroyed via
  // Dispatcher::WillDestroyServiceWorkerContextOnWorkerThread, then
  // drop this mojo event. See https://crbug.com/1008143 for details.
  if (!service_worker_data) {
    return;
  }

  ScriptContext* script_context = service_worker_data->context();
  // Note |scoped_extension_interaction| requires a HandleScope.
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);
  std::unique_ptr<InteractionProvider::Scope> scoped_extension_interaction;
  if (params->is_user_gesture) {
    scoped_extension_interaction =
        ExtensionInteractionProvider::Scope::ForWorker(
            script_context->v8_context());
  }

  service_worker_data->bindings_system()->DispatchEventInContext(
      params->event_name, event_args, std::move(params->filtering_info),
      service_worker_data->context());
  const int worker_thread_id = content::WorkerThread::GetCurrentId();
  Send(new ExtensionHostMsg_EventAckWorker(
      service_worker_data->context()->GetExtensionID(),
      service_worker_data->service_worker_version_id(), worker_thread_id,
      params->event_id));
}

void WorkerThreadDispatcher::DispatchEvent(mojom::DispatchEventParamsPtr params,
                                           base::Value::List event_args) {
  DCHECK(!worker_thread_util::IsWorkerThread());
  const int worker_thread_id = params->worker_thread_id;
  PostTaskToWorkerThread(
      worker_thread_id,
      base::BindOnce(&WorkerThreadDispatcher::DispatchEventOnWorkerThread,
                     std::move(params), std::move(event_args)));
}
void WorkerThreadDispatcher::OnDispatchOnConnect(
    int worker_thread_id,
    const ExtensionMsg_OnConnectData& connect_data) {
  DCHECK_EQ(worker_thread_id, content::WorkerThread::GetCurrentId());
  WorkerThreadDispatcher::GetBindingsSystem()
      ->messaging_service()
      ->DispatchOnConnect(
          Dispatcher::GetWorkerScriptContextSet(), connect_data.target_port_id,
          connect_data.channel_type, connect_data.channel_name,
          connect_data.tab_source, connect_data.external_connection_info,
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
    base::UnguessableToken activation_sequence,
    ScriptContext* script_context,
    std::unique_ptr<NativeExtensionBindingsSystem> bindings_system) {
  if (!service_worker_data) {
    service_worker_data = new ServiceWorkerData(
        service_worker_version_id, std::move(activation_sequence),
        script_context, std::move(bindings_system));
  }

  int worker_thread_id = content::WorkerThread::GetCurrentId();
  {
    base::AutoLock lock(task_runner_map_lock_);
    auto* task_runner = base::SingleThreadTaskRunner::GetCurrentDefault().get();
    CHECK(task_runner);
    task_runner_map_[worker_thread_id] = task_runner;
  }
}

void WorkerThreadDispatcher::DidInitializeContext(
    int64_t service_worker_version_id) {
  DCHECK_EQ(service_worker_version_id,
            service_worker_data->service_worker_version_id());
  const int thread_id = content::WorkerThread::GetCurrentId();
  DCHECK_NE(thread_id, kMainThreadId);
  PostTaskToIOThread(base::BindOnce(
      [](const ExtensionId& extension_id, int64_t service_worker_version_id,
         int thread_id) {
        WorkerThreadDispatcher::Get()
            ->GetServiceWorkerHostOnIO()
            ->DidInitializeServiceWorkerContext(
                extension_id, service_worker_version_id, thread_id,
                WorkerThreadDispatcher::Get()->BindEventDispatcher(thread_id));
      },
      service_worker_data->context()->GetExtensionID(),
      service_worker_version_id, thread_id));
}

void WorkerThreadDispatcher::DidStartContext(
    const GURL& service_worker_scope,
    int64_t service_worker_version_id) {
  DCHECK_EQ(service_worker_version_id,
            service_worker_data->service_worker_version_id());
  const int thread_id = content::WorkerThread::GetCurrentId();
  DCHECK_NE(thread_id, kMainThreadId);
  PostTaskToIOThread(base::BindOnce(
      [](const ExtensionId& extension_id,
         const base::UnguessableToken& activation_token,
         const GURL& service_worker_scope, int64_t service_worker_version_id,
         int thread_id) {
        WorkerThreadDispatcher::Get()
            ->GetServiceWorkerHostOnIO()
            ->DidStartServiceWorkerContext(
                extension_id, activation_token, service_worker_scope,
                service_worker_version_id, thread_id);
      },
      service_worker_data->context()->GetExtensionID(),
      service_worker_data->activation_sequence(), service_worker_scope,
      service_worker_version_id, thread_id));
}

void WorkerThreadDispatcher::DidStopContext(const GURL& service_worker_scope,
                                            int64_t service_worker_version_id) {
  const int thread_id = content::WorkerThread::GetCurrentId();
  DCHECK_NE(thread_id, kMainThreadId);
  DCHECK_EQ(service_worker_version_id,
            service_worker_data->service_worker_version_id());
  PostTaskToIOThread(base::BindOnce(
      [](const ExtensionId& extension_id,
         const base::UnguessableToken& activation_token,
         const GURL& service_worker_scope, int64_t service_worker_version_id,
         int thread_id) {
        WorkerThreadDispatcher::Get()
            ->GetServiceWorkerHostOnIO()
            ->DidStopServiceWorkerContext(extension_id, activation_token,
                                          service_worker_scope,
                                          service_worker_version_id, thread_id);
        WorkerThreadDispatcher::Get()->UnbindEventDispatcher(thread_id);
      },
      service_worker_data->context()->GetExtensionID(),
      service_worker_data->activation_sequence(), service_worker_scope,
      service_worker_version_id, thread_id));
}

void WorkerThreadDispatcher::RequestWorker(mojom::RequestParamsPtr params) {
  PostTaskToIOThread(base::BindOnce(
      [](mojom::RequestParamsPtr params) {
        auto* dispatcher = WorkerThreadDispatcher::Get();
        dispatcher->GetServiceWorkerHostOnIO()->RequestWorker(
            std::move(params));
      },
      std::move(params)));
}

void WorkerThreadDispatcher::WorkerResponseAck(
    int request_id,
    int64_t service_worker_version_id) {
  PostTaskToIOThread(base::BindOnce(
      [](int request_id, int64_t service_worker_version_id) {
        WorkerThreadDispatcher::Get()
            ->GetServiceWorkerHostOnIO()
            ->WorkerResponseAck(request_id, service_worker_version_id);
      },
      request_id, service_worker_version_id));
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

mojo::PendingAssociatedRemote<mojom::EventDispatcher>
WorkerThreadDispatcher::BindEventDispatcher(int worker_thread_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  mojo::PendingAssociatedRemote<mojom::EventDispatcher> remote;
  mojo::ReceiverId receiver_id =
      event_dispatchers_.Add(this, remote.InitWithNewEndpointAndPassReceiver());
  event_dispatcher_ids_.insert({worker_thread_id, receiver_id});
  return remote;
}

void WorkerThreadDispatcher::UnbindEventDispatcher(int worker_thread_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  auto it = event_dispatcher_ids_.find(worker_thread_id);
  if (it == event_dispatcher_ids_.end()) {
    return;
  }
  mojo::ReceiverId receiver_id = it->second;
  event_dispatchers_.Remove(receiver_id);
  event_dispatcher_ids_.erase(receiver_id);
}

}  // namespace extensions
