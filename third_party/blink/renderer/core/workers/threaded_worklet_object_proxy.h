// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_THREADED_WORKLET_OBJECT_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_THREADED_WORKLET_OBJECT_PROXY_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/workers/threaded_object_proxy_base.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class ThreadedWorkletMessagingProxy;
class WorkletPendingTasks;
class WorkerThread;
class WorkerResourceTimingNotifier;
struct CrossThreadFetchClientSettingsObjectData;

// A proxy to talk to the parent worker object. See class comments on
// ThreadedObjectProxyBase.h for lifetime of this class etc.
// TODO(nhiroki): Consider merging this class into ThreadedObjectProxyBase
// after EvaluateScript() for classic script loading is removed in favor of
// module script loading.
class CORE_EXPORT ThreadedWorkletObjectProxy : public ThreadedObjectProxyBase {
  USING_FAST_MALLOC(ThreadedWorkletObjectProxy);

 public:
  static std::unique_ptr<ThreadedWorkletObjectProxy> Create(
      ThreadedWorkletMessagingProxy*,
      ParentExecutionContextTaskRunners*);
  ~ThreadedWorkletObjectProxy() override;

  void FetchAndInvokeScript(
      const KURL& module_url_record,
      network::mojom::CredentialsMode,
      std::unique_ptr<CrossThreadFetchClientSettingsObjectData>
          outside_settings_object,
      WorkerResourceTimingNotifier* outside_resource_timing_notifier,
      scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
      WorkletPendingTasks*,
      WorkerThread*);

 protected:
  ThreadedWorkletObjectProxy(ThreadedWorkletMessagingProxy*,
                             ParentExecutionContextTaskRunners*);

  CrossThreadWeakPersistent<ThreadedMessagingProxyBase> MessagingProxyWeakPtr()
      final;

 private:
  // No guarantees about the lifetimes of tasks posted by this proxy wrt the
  // ThreadedWorkletMessagingProxy so a weak pointer must be used when posting
  // the tasks.
  CrossThreadWeakPersistent<ThreadedWorkletMessagingProxy>
      messaging_proxy_weak_ptr_;
  DISALLOW_COPY_AND_ASSIGN(ThreadedWorkletObjectProxy);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_THREADED_WORKLET_OBJECT_PROXY_H_
