// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_WORKER_THREAD_DISPATCHER_H_
#define EXTENSIONS_RENDERER_WORKER_THREAD_DISPATCHER_H_

#include <map>
#include <memory>
#include <string>

#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "content/public/renderer/render_thread_observer.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/common/extension_id.h"
#include "ipc/ipc_sync_message_filter.h"

namespace base {
class ListValue;
}

namespace content {
class RenderThread;
}

class GURL;
struct ExtensionMsg_DispatchEvent_Params;
struct ExtensionMsg_TabConnectionInfo;
struct ExtensionMsg_ExternalConnectionInfo;

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
// 2) A content::ThreadSafeSender, so we can safely send IPC from worker thread.
class WorkerThreadDispatcher : public content::RenderThreadObserver,
                               public IPC::Sender {
 public:
  WorkerThreadDispatcher();
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
      int64_t service_worker_version_id,
      ScriptContext* script_context,
      std::unique_ptr<NativeExtensionBindingsSystem> bindings_system);
  void RemoveWorkerData(int64_t service_worker_version_id);

  // Called when a service worker context was initialized.
  void DidInitializeContext(int64_t service_worker_version_id);

  // Called when a service worker context started running.
  void DidStartContext(const GURL& service_worker_scope,
                       int64_t service_worker_version_id);
  // Called when a service worker context was destroyed.
  void DidStopContext(const GURL& service_worker_scope,
                      int64_t service_worker_version_id);

  // content::RenderThreadObserver:
  bool OnControlMessageReceived(const IPC::Message& message) override;

  // Updates bindings of all Service Workers for |extension_id|, after extension
  // permission update.
  // Returns whether or not the update request was successfully issued to
  // each Service Workers.
  bool UpdateBindingsForWorkers(const ExtensionId& extension_id);

 private:
  static bool HandlesMessageOnWorkerThread(const IPC::Message& message);
  static void ForwardIPC(int worker_thread_id, const IPC::Message& message);
  static void UpdateBindingsOnWorkerThread(const ExtensionId& extension_id);

  void OnMessageReceivedOnWorkerThread(int worker_thread_id,
                                       const IPC::Message& message);

  bool PostTaskToWorkerThread(int worker_thread_id, base::OnceClosure task);

  // IPC handlers.
  void OnResponseWorker(int worker_thread_id,
                        int request_id,
                        bool succeeded,
                        const base::ListValue& response,
                        const std::string& error);
  void OnDispatchEvent(const ExtensionMsg_DispatchEvent_Params& params,
                       const base::ListValue& event_args);
  void OnValidateMessagePort(int worker_thread_id, const PortId& id);
  void OnDispatchOnConnect(int worker_thread_id,
                           const PortId& target_port_id,
                           const std::string& channel_name,
                           const ExtensionMsg_TabConnectionInfo& source,
                           const ExtensionMsg_ExternalConnectionInfo& info);
  void OnDeliverMessage(int worker_thread_id,
                        const PortId& target_port_id,
                        const Message& message);
  void OnDispatchOnDisconnect(int worker_thread_id,
                              const PortId& port_id,
                              const std::string& error_message);

  // IPC sender. Belongs to the render thread, but thread safe.
  scoped_refptr<IPC::SyncMessageFilter> message_filter_;

  using IDToTaskRunnerMap = std::map<base::PlatformThreadId, base::TaskRunner*>;
  IDToTaskRunnerMap task_runner_map_;
  base::Lock task_runner_map_lock_;

  DISALLOW_COPY_AND_ASSIGN(WorkerThreadDispatcher);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_WORKER_THREAD_DISPATCHER_H_
