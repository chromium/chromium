// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_exposed_renderer_interfaces.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "content/common/frame.mojom.h"
#include "content/public/common/content_client.h"
#include "content/public/common/resource_usage_reporter.mojom.h"
#include "content/public/common/resource_usage_reporter_type_converters.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"
#include "content/renderer/worker/shared_worker_factory_impl.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/features.h"
#include "v8/include/v8.h"

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
    base::RepeatingClosure collect = base::BindRepeating(
        &ResourceUsageReporterImpl::CollectOnWorkerThread,
        base::ThreadTaskRunnerHandle::Get(), weak_factory_.GetWeakPtr());
    workers_to_go_ =
        RenderThread::Get()->PostTaskToAllWebWorkers(std::move(collect));
    if (workers_to_go_) {
      // The guard task to send out partial stats
      // in case some workers are not responsive.
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ResourceUsageReporterImpl::SendResults,
                         weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kWaitForWorkersStatsTimeoutMS));
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

class FrameFactoryImpl : public mojom::FrameFactory {
 public:
  FrameFactoryImpl() = default;
  FrameFactoryImpl(const FrameFactoryImpl&) = delete;
  FrameFactoryImpl& operator=(const FrameFactoryImpl&) = delete;

 private:
  // mojom::FrameFactory:
  void CreateFrame(
      int32_t frame_routing_id,
      mojo::PendingReceiver<mojom::Frame> frame_receiver) override {
    // TODO(morrita): This is for investigating http://crbug.com/415059 and
    // should be removed once it is fixed.
    CHECK_LT(routing_id_highmark_, frame_routing_id);
    routing_id_highmark_ = frame_routing_id;

    RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(frame_routing_id);
    // We can receive a GetServiceProviderForFrame message for a frame not yet
    // created due to a race between the message and a
    // mojom::Renderer::CreateView IPC that triggers creation of the RenderFrame
    // we want.
    if (!frame) {
      RenderThreadImpl::current()->RegisterPendingFrameCreate(
          frame_routing_id, std::move(frame_receiver));
      return;
    }

    frame->BindFrame(std::move(frame_receiver));
  }

 private:
  int32_t routing_id_highmark_ = -1;
};

void CreateFrameFactory(mojo::PendingReceiver<mojom::FrameFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<FrameFactoryImpl>(),
                              std::move(receiver));
}

}  // namespace

void ExposeRendererInterfacesToBrowser(
    base::WeakPtr<RenderThreadImpl> render_thread,
    mojo::BinderMap* binders) {
  DCHECK(render_thread);

  binders->Add(base::BindRepeating(&SharedWorkerFactoryImpl::Create),
               base::ThreadTaskRunnerHandle::Get());
  binders->Add(base::BindRepeating(&CreateResourceUsageReporter, render_thread),
               base::ThreadTaskRunnerHandle::Get());

  if (base::FeatureList::IsEnabled(
          blink::features::kOffMainThreadServiceWorkerStartup)) {
    auto task_runner = base::CreateSingleThreadTaskRunner(
        {base::ThreadPool(), base::MayBlock(),
         base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    binders->Add(
        base::BindRepeating(&EmbeddedWorkerInstanceClientImpl::CreateForRequest,
                            task_runner),
        task_runner);
  } else {
    auto task_runner =
        render_thread->GetWebMainThreadScheduler()->DefaultTaskRunner();
    binders->Add(
        base::BindRepeating(&EmbeddedWorkerInstanceClientImpl::CreateForRequest,
                            task_runner),
        task_runner);
  }

  binders->Add(base::BindRepeating(&CreateFrameFactory),
               base::ThreadTaskRunnerHandle::Get());

  GetContentClient()->renderer()->ExposeInterfacesToBrowser(binders);
}

}  // namespace content
