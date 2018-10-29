// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/experimental/thread_pool.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/dedicated_worker_factory.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/threaded_messaging_proxy_base.h"
#include "third_party/blink/renderer/core/workers/threaded_object_proxy_base.h"

namespace blink {

class ThreadPoolObjectProxy final : public ThreadedObjectProxyBase {
 public:
  ThreadPoolObjectProxy(ThreadPoolMessagingProxy* messaging_proxy,
                        ParentExecutionContextTaskRunners* task_runners)
      : ThreadedObjectProxyBase(task_runners),
        messaging_proxy_(messaging_proxy) {}
  ~ThreadPoolObjectProxy() override = default;

  CrossThreadWeakPersistent<ThreadedMessagingProxyBase> MessagingProxyWeakPtr()
      override {
    return messaging_proxy_;
  }

 private:
  CrossThreadWeakPersistent<ThreadPoolMessagingProxy> messaging_proxy_;
};

class ThreadPoolMessagingProxy final : public ThreadedMessagingProxyBase {
 public:
  explicit ThreadPoolMessagingProxy(ExecutionContext* context)
      : ThreadedMessagingProxyBase(context) {
    object_proxy_ = std::make_unique<ThreadPoolObjectProxy>(
        this, GetParentExecutionContextTaskRunners());
  }
  ~ThreadPoolMessagingProxy() override = default;

  void StartWorker(std::unique_ptr<GlobalScopeCreationParams> creation_params) {
    InitializeWorkerThread(std::move(creation_params),
                           WorkerBackingThreadStartupData::CreateDefault());
  }
  std::unique_ptr<WorkerThread> CreateWorkerThread() override {
    return std::make_unique<ThreadPoolThread>(
        GetExecutionContext(), *object_proxy_.get(), ThreadPoolThread::kWorker);
  }

  ThreadPoolThread* GetWorkerThread() const {
    return static_cast<ThreadPoolThread*>(
        ThreadedMessagingProxyBase::GetWorkerThread());
  }

 private:
  std::unique_ptr<ThreadPoolObjectProxy> object_proxy_;
};

service_manager::mojom::blink::InterfaceProviderPtrInfo
ConnectToWorkerInterfaceProviderForThreadPool(
    ExecutionContext* execution_context,
    scoped_refptr<const SecurityOrigin> script_origin) {
  // TODO(japhet): Implement a proper factory.
  mojom::blink::DedicatedWorkerFactoryPtr worker_factory;
  execution_context->GetInterfaceProvider()->GetInterface(&worker_factory);
  service_manager::mojom::blink::InterfaceProviderPtrInfo
      interface_provider_ptr;
  worker_factory->CreateDedicatedWorker(
      script_origin, mojo::MakeRequest(&interface_provider_ptr));
  return interface_provider_ptr;
}

const char ThreadPool::kSupplementName[] = "ThreadPool";

ThreadPool* ThreadPool::From(Document& document) {
  ThreadPool* thread_pool = Supplement<Document>::From<ThreadPool>(document);
  if (!thread_pool) {
    thread_pool = new ThreadPool(document);
    Supplement<Document>::ProvideTo(document, thread_pool);
  }
  return thread_pool;
}

static const size_t kMaxThreadCount = 4;

ThreadPool::ThreadPool(Document& document)
    : Supplement<Document>(document), ContextLifecycleObserver(&document) {}

ThreadPool::~ThreadPool() {
  DCHECK(IsMainThread());
  for (auto proxy : thread_proxies_) {
    proxy->ParentObjectDestroyed();
  }
}

ThreadPoolThread* ThreadPool::CreateNewThread() {
  DCHECK(IsMainThread());
  DCHECK_LT(thread_proxies_.size(), kMaxThreadCount);
  base::UnguessableToken devtools_worker_token =
      GetFrame() ? GetFrame()->GetDevToolsFrameToken()
                 : base::UnguessableToken::Create();
  ExecutionContext* context = GetExecutionContext();

  ThreadPoolMessagingProxy* proxy = new ThreadPoolMessagingProxy(context);
  std::unique_ptr<WorkerSettings> settings =
      std::make_unique<WorkerSettings>(GetFrame()->GetSettings());

  proxy->StartWorker(std::make_unique<GlobalScopeCreationParams>(
      context->Url(), ScriptType::kClassic, context->UserAgent(),
      context->GetContentSecurityPolicy()->Headers(), kReferrerPolicyDefault,
      context->GetSecurityOrigin(), context->IsSecureContext(),
      context->GetHttpsState(), WorkerClients::Create(),
      context->GetSecurityContext().AddressSpace(),
      OriginTrialContext::GetTokens(context).get(), devtools_worker_token,
      std::move(settings), kV8CacheOptionsDefault,
      nullptr /* worklet_module_responses_map */,
      ConnectToWorkerInterfaceProviderForThreadPool(
          context, context->GetSecurityOrigin())));
  thread_proxies_.insert(proxy);
  return proxy->GetWorkerThread();
}

ThreadPoolThread* ThreadPool::GetLeastBusyThread() {
  DCHECK(IsMainThread());
  ThreadPoolThread* least_busy_thread = nullptr;
  size_t lowest_task_count = std::numeric_limits<std::size_t>::max();
  for (auto proxy : thread_proxies_) {
    ThreadPoolThread* current_thread = proxy->GetWorkerThread();
    size_t current_task_count = current_thread->GetTasksInProgressCount();
    // If there's an idle thread, use it.
    if (current_task_count == 0)
      return current_thread;
    if (current_task_count < lowest_task_count) {
      least_busy_thread = current_thread;
      lowest_task_count = current_task_count;
    }
  }

  if (thread_proxies_.size() == kMaxThreadCount)
    return least_busy_thread;
  return CreateNewThread();
}

void ThreadPool::ContextDestroyed(ExecutionContext*) {
  DCHECK(IsMainThread());
  DCHECK(GetExecutionContext()->IsContextThread());
  for (auto proxy : thread_proxies_) {
    proxy->TerminateGlobalScope();
  }
}

void ThreadPool::Trace(blink::Visitor* visitor) {
  Supplement<Document>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(thread_proxies_);
}

}  // namespace blink
