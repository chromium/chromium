// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_THREADED_WORKLET_MESSAGING_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_THREADED_WORKLET_MESSAGING_PROXY_H_

#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/workers/threaded_messaging_proxy_base.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope_proxy.h"

namespace blink {

class ThreadedWorkletObjectProxy;
class WorkerClients;
class WorkletModuleResponsesMap;

class CORE_EXPORT ThreadedWorkletMessagingProxy
    : public ThreadedMessagingProxyBase,
      public WorkletGlobalScopeProxy {
  USING_GARBAGE_COLLECTED_MIXIN(ThreadedWorkletMessagingProxy);

 public:
  // WorkletGlobalScopeProxy implementation.
  void FetchAndInvokeScript(
      const KURL& module_url_record,
      network::mojom::CredentialsMode,
      const FetchClientSettingsObjectSnapshot& outside_settings_object,
      WorkerResourceTimingNotifier& outside_resource_timing_notifier,
      scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
      WorkletPendingTasks*) final;
  void WorkletObjectDestroyed() final;
  void TerminateWorkletGlobalScope() final;

  void Initialize(
      WorkerClients*,
      WorkletModuleResponsesMap*,
      const base::Optional<WorkerBackingThreadStartupData>& = base::nullopt);

  void Trace(blink::Visitor*) override;

 protected:
  explicit ThreadedWorkletMessagingProxy(ExecutionContext*);

  ThreadedWorkletObjectProxy& WorkletObjectProxy();

 private:
  friend class ThreadedWorkletMessagingProxyForTest;

  virtual std::unique_ptr<ThreadedWorkletObjectProxy> CreateObjectProxy(
      ThreadedWorkletMessagingProxy*,
      ParentExecutionContextTaskRunners*);

  std::unique_ptr<ThreadedWorkletObjectProxy> worklet_object_proxy_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_THREADED_WORKLET_MESSAGING_PROXY_H_
