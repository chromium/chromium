// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_WORKER_THREAD_DISPATCHER_H_
#define EXTENSIONS_RENDERER_WORKER_THREAD_DISPATCHER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "content/public/renderer/render_thread_observer.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/common/extension_id.h"
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

namespace base {
class UnguessableToken;
}

namespace blink {
class WebServiceWorkerContextProxy;
}

namespace content {
class RenderThread;
}

namespace extensions {
class NativeExtensionBindingsSystem;
class ScriptContext;
class ServiceWorkerData;
class V8SchemaRegistry;

// Sends and receives IPC in an extension Service Worker.
// TODO(lazyboy): This class should really be a combination of the following
// two:
// 1) A content::WorkerThreadMessageFilter, so that we can receive IPC directly
// on worker thread.
// 2) A thread-safe version of IPC::Sender, so we can safely send IPC from
// worker thread (this TODO formerly referred to content::ThreadSafeSender
// which no longer exists).
class WorkerThreadDispatcher :
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

  void AddWorkerData(
      blink::WebServiceWorkerContextProxy* proxy,
      int64_t service_worker_version_id,
      const std::optional<base::UnguessableToken>& activation_sequence,
      const blink::ServiceWorkerToken& service_worker_token,
      ScriptContext* script_context,
      std::unique_ptr<NativeExtensionBindingsSystem> bindings_system);
  void RemoveWorkerData(int64_t service_worker_version_id);

  // Updates bindings of all Service Workers for |extension_id|, after extension
  // permission update.
  // Returns whether or not the update request was successfully issued to
  // each Service Workers.
  bool UpdateBindingsForWorkers(const ExtensionId& extension_id);

  // Updates bindings for every extension service worker context, assuming
  // changes that can affect API availability.
  void UpdateAllServiceWorkerBindings();

  // NativeExtensionBindingsSystem::Delegate implementation.
  ScriptContextSetIterable* GetScriptContextSet() override;

 private:
  static void UpdateBindingsOnWorkerThread(
      const std::optional<ExtensionId>& extension_id);

  // Helper method to update bindings. If `extension_id` is non-null, updates
  // only bindings for that extension; otherwise, updates all bindings.
  // Returns true if the task to each worker thread posts correctly.
  bool UpdateBindingsHelper(const std::optional<ExtensionId>& extension_id);

  using IDToTaskRunnerMap = std::map<base::PlatformThreadId, base::TaskRunner*>;
  IDToTaskRunnerMap task_runner_map_;
  base::Lock task_runner_map_lock_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_WORKER_THREAD_DISPATCHER_H_
