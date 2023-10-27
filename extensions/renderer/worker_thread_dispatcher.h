// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_WORKER_THREAD_DISPATCHER_H_
#define EXTENSIONS_RENDERER_WORKER_THREAD_DISPATCHER_H_

#include <map>
#include <memory>
#include <string>

#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "content/public/renderer/render_thread_observer.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/mojom/automation_registry.mojom.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/mojom/event_router.mojom.h"
#include "extensions/common/mojom/service_worker_host.mojom.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "ipc/ipc_sync_message_filter.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/shared_associated_remote.h"
#include "services/accessibility/public/mojom/automation.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class SingleThreadTaskRunner;
class UnguessableToken;
class Uuid;
}

namespace blink {
class WebServiceWorkerContextProxy;
}

namespace content {
class RenderThread;
}

class GURL;
struct ExtensionMsg_OnConnectData;

namespace extensions {
class NativeExtensionBindingsSystem;
class ScriptContext;
class ServiceWorkerData;
class V8SchemaRegistry;
struct Message;
struct PortId;

// Sends and receives IPC in an extension Service Worker.
// TODO(lazyboy): This class should really be a combination of the following
// two:
// 1) A content::WorkerThreadMessageFilter, so that we can receive IPC directly
// on worker thread.
// 2) A thread-safe version of IPC::Sender, so we can safely send IPC from
// worker thread (this TODO formerly referred to content::ThreadSafeSender
// which no longer exists).
class WorkerThreadDispatcher : public content::RenderThreadObserver,
                               public IPC::Sender,
#if BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
                               public mojom::EventDispatcher,
#endif
                               public NativeExtensionBindingsSystem::Delegate {
 public:
  WorkerThreadDispatcher();

  WorkerThreadDispatcher(const WorkerThreadDispatcher&) = delete;
  WorkerThreadDispatcher& operator=(const WorkerThreadDispatcher&) = delete;

  ~WorkerThreadDispatcher() override;

  // Thread safe.
  static WorkerThreadDispatcher* Get();
  static NativeExtensionBindingsSystem* GetBindingsSystem();
  static V8SchemaRegistry* GetV8SchemaRegistry();
  static ScriptContext* GetScriptContext();
  static ServiceWorkerData* GetServiceWorkerData();

  void Init(content::RenderThread* render_thread);

  // IPC::Sender:
  bool Send(IPC::Message* message) override;

  void AddWorkerData(
      blink::WebServiceWorkerContextProxy* proxy,
      int64_t service_worker_version_id,
      const absl::optional<base::UnguessableToken>& activation_sequence,
      ScriptContext* script_context,
      std::unique_ptr<NativeExtensionBindingsSystem> bindings_system);
  void RemoveWorkerData(int64_t service_worker_version_id);

#if BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
  // Called when a service worker context was initialized.
  void DidInitializeContext(int64_t service_worker_version_id);

  // Called when a service worker context started running.
  void DidStartContext(const GURL& service_worker_scope,
                       int64_t service_worker_version_id);
  // Called when a service worker context was destroyed.
  void DidStopContext(const GURL& service_worker_scope,
                      int64_t service_worker_version_id);

  void RequestWorker(mojom::RequestParamsPtr params);
  void SendResponseAck(const base::Uuid& request_uuid);

  // content::RenderThreadObserver:
  bool OnControlMessageReceived(const IPC::Message& message) override;
#endif

  // Updates bindings of all Service Workers for |extension_id|, after extension
  // permission update.
  // Returns whether or not the update request was successfully issued to
  // each Service Workers.
  bool UpdateBindingsForWorkers(const ExtensionId& extension_id);

  // Updates bindings for every extension service worker context, assuming
  // changes that can affect API availability.
  void UpdateAllServiceWorkerBindings();

#if BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
  // Posts mojom::EventRouter::AddListenerForServiceWorker to the IO thread to
  // call it with GetEventRouterOnIO().
  void SendAddEventListener(mojom::EventListenerPtr event_listener);

  // Posts mojom::EventRouter::AddLazyListenerForServiceWorker to the IO thread
  // to call it with GetEventRouterOnIO().
  void SendAddEventLazyListener(const std::string& extension_id,
                                const GURL& scope,
                                const std::string& event_name);

  // Posts mojom::EventRouter::AddFilteredListenerForServiceWorker to the IO
  // thread to call it with GetEventRouterOnIO().
  void SendAddEventFilteredListener(const std::string& extension_id,
                                    const GURL& scope,
                                    const std::string& event_name,
                                    int64_t service_worker_version_id,
                                    int worker_thread_id,
                                    base::Value::Dict filter,
                                    bool add_lazy_listener);

  // Uses the RendererAutomationRegistry to connect the Automation remote. Uses
  // the IO thread to bind the RendererAutomationRegistryRemote, if needed.
  void SendBindAutomation(
      mojo::PendingAssociatedRemote<ax::mojom::Automation> pending_remote);

  // Posts mojom::EventRouter::RemoveListenerForServiceWorker to the IO thread
  // to call it with GetEventRouterOnIO().
  void SendRemoveEventListener(mojom::EventListenerPtr event_listener);

  // Posts mojom::EventRouter::RemoveLazyListenerForServiceWorker to the IO
  // thread to call it with GetEventRouterOnIO().
  void SendRemoveEventLazyListener(const std::string& extension_id,
                                   const GURL& scope,
                                   const std::string& event_name);

  // Posts mojom::EventRouter::RemoveFilteredListenerForServiceWorker to the IO
  // thread to call it with GetEventRouterOnIO().
  void SendRemoveEventFilteredListener(const std::string& extension_id,
                                       const GURL& scope,
                                       const std::string& event_name,
                                       int64_t service_worker_version_id,
                                       int worker_thread_id,
                                       base::Value::Dict filter,
                                       bool remove_lazy_listener);

  // NOTE: This must be called on the IO thread because it can call
  // SyncMessageFilter::GetRemoteAssociatedInterface() which must be called on
  // the IO thread.
  // TODO(https://crbug.com/1364183): Obtain these interfaces at the worker
  // thread once `AssociatedInterfaceRegistry` for ServiceWorker is added.
  mojom::EventRouter* GetEventRouterOnIO();
  mojom::ServiceWorkerHost* GetServiceWorkerHostOnIO();
  mojom::RendererAutomationRegistry* GetAutomationRegistryOnIO();

  mojo::PendingAssociatedRemote<mojom::EventDispatcher> BindEventDispatcher(
      int worker_thread_id);
  void UnbindEventDispatcher(int worker_thread_id);

  // Mojo interface implementation, called from the main thread.
  void DispatchEvent(mojom::DispatchEventParamsPtr params,
                     base::Value::List event_args,
                     DispatchEventCallback callback) override;
#endif

  // NativeExtensionBindingsSystem::Delegate implementation.
  ScriptContextSetIterable* GetScriptContextSet() override;

 private:
  static void UpdateBindingsOnWorkerThread(
      const absl::optional<ExtensionId>& extension_id);
#if BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
  static bool HandlesMessageOnWorkerThread(const IPC::Message& message);
  static void ForwardIPC(int worker_thread_id, const IPC::Message& message);
  static void DispatchEventOnWorkerThread(mojom::DispatchEventParamsPtr params,
                                          base::Value::List event_args);
  void OnMessageReceivedOnWorkerThread(int worker_thread_id,
                                       const IPC::Message& message);
#endif

  bool PostTaskToWorkerThread(int worker_thread_id, base::OnceClosure task);
  void PostTaskToIOThread(base::OnceClosure task);

#if BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
  // IPC handlers.
  void OnValidateMessagePort(int worker_thread_id, const PortId& id);
  void OnDispatchOnConnect(int worker_thread_id,
                           const ExtensionMsg_OnConnectData& connect_data);
  void OnDeliverMessage(int worker_thread_id,
                        const PortId& target_port_id,
                        const Message& message);
  void OnDispatchOnDisconnect(int worker_thread_id,
                              const PortId& port_id,
                              const std::string& error_message);
  void DispatchEventHelper(mojom::DispatchEventParamsPtr params,
                           base::Value::List event_args);

  void PostTaskToMainThread(base::OnceClosure task);
#endif

  // Helper method to update bindings. If `extension_id` is non-null, updates
  // only bindings for that extension; otherwise, updates all bindings.
  // Returns true if the task to each worker thread posts correctly.
  bool UpdateBindingsHelper(const absl::optional<ExtensionId>& extension_id);

  // IPC sender. Belongs to the render thread, but thread safe.
  scoped_refptr<IPC::SyncMessageFilter> message_filter_;

  using IDToTaskRunnerMap = std::map<base::PlatformThreadId, base::TaskRunner*>;
  IDToTaskRunnerMap task_runner_map_;
  base::Lock task_runner_map_lock_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
#if BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  mojo::AssociatedRemote<mojom::EventRouter> event_router_remote_;
  mojo::AssociatedRemote<mojom::ServiceWorkerHost> service_worker_host_;
  mojo::AssociatedRemote<mojom::RendererAutomationRegistry>
      renderer_automation_registry_remote_;

  // The set of receivers for mojom::EventDispatcher. `event_dispatcher_ids`
  // keeps track which receiver is associated to the worker thread.
  mojo::AssociatedReceiverSet<mojom::EventDispatcher> event_dispatchers_;
  std::map<int /*worker_thread_id*/, mojo::ReceiverId> event_dispatcher_ids_;
#endif
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_WORKER_THREAD_DISPATCHER_H_
