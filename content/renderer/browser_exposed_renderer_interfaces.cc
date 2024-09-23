// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_exposed_renderer_interfaces.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/common/features.h"
#include "content/common/frame.mojom.h"
#include "content/public/common/content_client.h"
#include "content/public/common/resource_usage_reporter.mojom.h"
#include "content/public/common/resource_usage_reporter_type_converters.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"
#include "content/renderer/worker/shared_worker_factory_impl.h"
#include "content/services/auction_worklet/auction_worklet_service_impl.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-statistics.h"

namespace content {

namespace {

constexpr int kWaitForWorkersStatsTimeoutMS = 20;

class ResourceUsageReporterImpl : public content::mojom::ResourceUsageReporter {
 public:
  explicit ResourceUsageReporterImpl(base::WeakPtr<RenderThread> thread)
      : thread_(std::move(thread)) {}
  ResourceUsageReporterImpl(const ResourceUsageReporterImpl&) = delete;
  ~ResourceUsageReporterImpl() override = default;

  ResourceUsageReporterImpl& operator=(const ResourceUsageReporterImpl&) =
      delete;

 private:
  static void CollectOnWorkerThread(
      const scoped_refptr<base::TaskRunner>& master,
      base::WeakPtr<ResourceUsageReporterImpl> impl) {
    size_t total_bytes = 0;
    size_t used_bytes = 0;
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (isolate) {
      v8::HeapStatistics heap_stats;
      isolate->GetHeapStatistics(&heap_stats);
      total_bytes = heap_stats.total_heap_size();
      used_bytes = heap_stats.used_heap_size();
    }
    master->PostTask(FROM_HERE,
                     base::BindOnce(&ResourceUsageReporterImpl::ReceiveStats,
                                    impl, total_bytes, used_bytes));
  }

  void ReceiveStats(size_t total_bytes, size_t used_bytes) {
    usage_data_->v8_bytes_allocated += total_bytes;
    usage_data_->v8_bytes_used += used_bytes;
    workers_to_go_--;
    if (!workers_to_go_)
      SendResults();
  }

  void SendResults() {
    if (!callback_.is_null())
      std::move(callback_).Run(std::move(usage_data_));
    callback_.Reset();
    weak_factory_.InvalidateWeakPtrs();
    workers_to_go_ = 0;
  }

  void GetUsageData(GetUsageDataCallback callback) override {
    DCHECK(callback_.is_null());
    weak_factory_.InvalidateWeakPtrs();
    usage_data_ = mojom::ResourceUsageData::New();
    usage_data_->reports_v8_stats = true;
    callback_ = std::move(callback);

    // Since it is not safe to call any Blink or V8 functions until Blink has
    // been initialized (which also initializes V8), early out and send 0 back
    // for all resources.
    if (!thread_) {
      SendResults();
      return;
    }

    blink::WebCacheResourceTypeStats stats;
    blink::WebCache::GetResourceTypeStats(&stats);
    usage_data_->web_cache_stats = mojom::ResourceTypeStats::From(stats);

    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (isolate) {
      v8::HeapStatistics heap_stats;
      isolate->GetHeapStatistics(&heap_stats);
      usage_data_->v8_bytes_allocated = heap_stats.total_heap_size();
      usage_data_->v8_bytes_used = heap_stats.used_heap_size();
    }
    base::RepeatingClosure collect =
        base::BindRepeating(&ResourceUsageReporterImpl::CollectOnWorkerThread,
                            base::SingleThreadTaskRunner::GetCurrentDefault(),
                            weak_factory_.GetWeakPtr());
    workers_to_go_ =
        RenderThread::Get()->PostTaskToAllWebWorkers(std::move(collect));
    if (workers_to_go_) {
      // The guard task to send out partial stats
      // in case some workers are not responsive.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ResourceUsageReporterImpl::SendResults,
                         weak_factory_.GetWeakPtr()),
          base::Milliseconds(kWaitForWorkersStatsTimeoutMS));
    } else {
      // No worker threads so just send out the main thread data right away.
      SendResults();
    }
  }

  const base::WeakPtr<RenderThread> thread_;
  mojom::ResourceUsageDataPtr usage_data_;
  GetUsageDataCallback callback_;
  int workers_to_go_ = 0;

  base::WeakPtrFactory<ResourceUsageReporterImpl> weak_factory_{this};
};

void CreateResourceUsageReporter(
    base::WeakPtr<RenderThreadImpl> render_thread,
    mojo::PendingReceiver<mojom::ResourceUsageReporter> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ResourceUsageReporterImpl>(std::move(render_thread)),
      std::move(receiver));
}

void CreateEmbeddedWorkerWithRenderMainThread(
    scoped_refptr<base::SingleThreadTaskRunner> initiator_task_runner,
    base::WeakPtr<RenderThreadImpl> render_thread,
    mojo::PendingReceiver<blink::mojom::EmbeddedWorkerInstanceClient>
        receiver) {
  TRACE_EVENT0("ServiceWorker", "CreateEmbeddedWorkerWithRenderMainThread");
  initiator_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&EmbeddedWorkerInstanceClientImpl::Create,
                                initiator_task_runner,
                                render_thread->cors_exempt_header_list(),
                                std::move(receiver)));
}

void CreateEmbeddedWorker(
    scoped_refptr<base::SingleThreadTaskRunner> initiator_task_runner,
    mojo::PendingReceiver<blink::mojom::EmbeddedWorkerInstanceClient>
        receiver) {
  TRACE_EVENT0("ServiceWorker", "CreateEmbeddedWorker");
  // An empty fake list is passed to
  // `EmbeddedWorkerInstanceClientImpl::Create()`. That will be overridden by
  // the actual cors exempt header list in
  // `EmbeddedWorkerInstanceClientImpl::StartWorker()`.
  //
  // TODO(crbug.com/40753993): Remove this fake empty list once we confirmed
  // this approach is fine.
  const std::vector<std::string> fake_cors_exempt_header_list;
  initiator_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerInstanceClientImpl::Create,
                     initiator_task_runner, fake_cors_exempt_header_list,
                     std::move(receiver)));
}
}  // namespace

void ExposeRendererInterfacesToBrowser(
    base::WeakPtr<RenderThreadImpl> render_thread,
    mojo::BinderMap* binders) {
  DCHECK(render_thread);

  binders->Add<blink::mojom::SharedWorkerFactory>(
      base::BindRepeating(&SharedWorkerFactoryImpl::Create),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  binders->Add<mojom::ResourceUsageReporter>(
      base::BindRepeating(&CreateResourceUsageReporter, render_thread),
      base::SingleThreadTaskRunner::GetCurrentDefault());
#if BUILDFLAG(IS_ANDROID)
  binders->Add<auction_worklet::mojom::AuctionWorkletService>(
      base::BindRepeating(
          &auction_worklet::AuctionWorkletServiceImpl::CreateForRenderer),
      base::SingleThreadTaskRunner::GetCurrentDefault());
#endif

  auto task_runner_for_service_worker_startup =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  // TODO(crbug.com/40753993): Remove the feature flag and
  // `CreateEmbeddedWorkerWithRenderMainThread()` once we confirmed this
  // approach is fine.
  //
  // The kServiceWorkerAvoidMainThreadForInitialization feature flag is the
  // experimental flag to avoid the additional thread hop over the main thread
  // for the ServiceWorker initialization. Currently it's on the main thread as
  // CreateEmbeddedWorker accesses `cors_exempt_header_list` from
  // `render_thread`. When this feature flag is enabled, binds on
  // `task_runner_for_service_worker_startup` instead of the main thread, so
  // startup isn't blocked on the main thread.
  if (base::FeatureList::IsEnabled(
          features::kServiceWorkerAvoidMainThreadForInitialization)) {
    binders->Add<blink::mojom::EmbeddedWorkerInstanceClient>(
        base::BindRepeating(&CreateEmbeddedWorker,
                            task_runner_for_service_worker_startup),
        task_runner_for_service_worker_startup);
  } else {
    binders->Add<blink::mojom::EmbeddedWorkerInstanceClient>(
        base::BindRepeating(&CreateEmbeddedWorkerWithRenderMainThread,
                            task_runner_for_service_worker_startup,
                            render_thread),
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  GetContentClient()->renderer()->ExposeInterfacesToBrowser(binders);
}

}  // namespace content
