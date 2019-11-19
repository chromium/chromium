// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositor_dependencies_android.h"

#include <utility>

#include "base/bind.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "cc/raster/single_thread_task_graph_runner.h"
#include "components/viz/client/frame_eviction_manager.h"
#include "components/viz/common/features.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {

namespace {

// The client_id used here should not conflict with the client_id generated
// from RenderWidgetHostImpl.
constexpr uint32_t kDefaultClientId = 0u;

void BrowserGpuChannelHostFactorySetApplicationVisible(bool is_visible) {
  // This code relies on the browser's GpuChannelEstablishFactory being the
  // BrowserGpuChannelHostFactory.
  DCHECK_EQ(BrowserMainLoop::GetInstance()->gpu_channel_establish_factory(),
            BrowserGpuChannelHostFactory::instance());
  BrowserGpuChannelHostFactory::instance()->SetApplicationVisible(is_visible);
}

// These functions are called based on application visibility status.
void SendOnBackgroundedToGpuService() {
  content::GpuProcessHost::CallOnIO(
      content::GPU_PROCESS_KIND_SANDBOXED, false /* force_create */,
      base::BindRepeating([](content::GpuProcessHost* host) {
        if (host) {
          host->gpu_service()->OnBackgrounded();
        }
      }));
}

void SendOnForegroundedToGpuService() {
  content::GpuProcessHost::CallOnIO(
      content::GPU_PROCESS_KIND_SANDBOXED, false /* force_create */,
      base::BindRepeating([](content::GpuProcessHost* host) {
        if (host) {
          host->gpu_service()->OnForegrounded();
        }
      }));
}

class SingleThreadTaskGraphRunner : public cc::SingleThreadTaskGraphRunner {
 public:
  SingleThreadTaskGraphRunner() {
    Start("CompositorTileWorker1", base::SimpleThread::Options());
  }

  ~SingleThreadTaskGraphRunner() override { Shutdown(); }
};

}  // namespace

// static
CompositorDependenciesAndroid& CompositorDependenciesAndroid::Get() {
  static base::NoDestructor<CompositorDependenciesAndroid> instance;
  return *instance;
}

CompositorDependenciesAndroid::CompositorDependenciesAndroid()
    : frame_sink_id_allocator_(kDefaultClientId) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  bool enable_viz = features::IsVizDisplayCompositorEnabled();
  if (!enable_viz) {
    // The SharedBitmapManager can be null as software compositing is not
    // supported or used on Android.
    frame_sink_manager_impl_ = std::make_unique<viz::FrameSinkManagerImpl>(
        /*shared_bitmap_manager=*/nullptr);
    surface_utils::ConnectWithLocalFrameSinkManager(
        &host_frame_sink_manager_, frame_sink_manager_impl_.get());
  } else {
    CreateVizFrameSinkManager();
  }
}

CompositorDependenciesAndroid::~CompositorDependenciesAndroid() = default;

void CompositorDependenciesAndroid::CreateVizFrameSinkManager() {
  mojo::PendingRemote<viz::mojom::FrameSinkManager> frame_sink_manager;
  mojo::PendingReceiver<viz::mojom::FrameSinkManager>
      frame_sink_manager_receiver =
          frame_sink_manager.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<viz::mojom::FrameSinkManagerClient>
      frame_sink_manager_client;
  mojo::PendingReceiver<viz::mojom::FrameSinkManagerClient>
      frame_sink_manager_client_receiver =
          frame_sink_manager_client.InitWithNewPipeAndPassReceiver();

  // Setup HostFrameSinkManager with interface endpoints.
  host_frame_sink_manager_.BindAndSetManager(
      std::move(frame_sink_manager_client_receiver),
      base::ThreadTaskRunnerHandle::Get(), std::move(frame_sink_manager));

  // Set up a callback to automatically re-connect if we lose our
  // connection.
  host_frame_sink_manager_.SetConnectionLostCallback(base::BindRepeating([]() {
    CompositorDependenciesAndroid::Get().CreateVizFrameSinkManager();
  }));

  // Set up a pending request which will be run once we've successfully
  // connected to the GPU process.
  pending_connect_viz_on_io_thread_ = base::BindOnce(
      &CompositorDependenciesAndroid::ConnectVizFrameSinkManagerOnIOThread,
      std::move(frame_sink_manager_receiver),
      std::move(frame_sink_manager_client));
}

cc::TaskGraphRunner* CompositorDependenciesAndroid::GetTaskGraphRunner() {
  if (!task_graph_runner_)
    task_graph_runner_ = std::make_unique<SingleThreadTaskGraphRunner>();
  return task_graph_runner_.get();
}

viz::FrameSinkId CompositorDependenciesAndroid::AllocateFrameSinkId() {
  return frame_sink_id_allocator_.NextFrameSinkId();
}

void CompositorDependenciesAndroid::TryEstablishVizConnectionIfNeeded() {
  if (!pending_connect_viz_on_io_thread_)
    return;
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 std::move(pending_connect_viz_on_io_thread_));
}

// Called on IO thread, after a GPU connection has already been established.
// |gpu_process_host| should only be invalid if a channel has been
// established and lost. In this case the ConnectionLost callback will be
// re-run when the request is deleted (goes out of scope).
// static
void CompositorDependenciesAndroid::ConnectVizFrameSinkManagerOnIOThread(
    mojo::PendingReceiver<viz::mojom::FrameSinkManager> receiver,
    mojo::PendingRemote<viz::mojom::FrameSinkManagerClient> client) {
  auto* gpu_process_host = GpuProcessHost::Get();
  if (!gpu_process_host)
    return;
  gpu_process_host->gpu_host()->ConnectFrameSinkManager(std::move(receiver),
                                                        std::move(client));
}

void CompositorDependenciesAndroid::EnqueueLowEndBackgroundCleanup() {
  if (base::SysInfo::IsLowEndDevice()) {
    low_end_background_cleanup_task_.Reset(base::BindOnce(
        &CompositorDependenciesAndroid::DoLowEndBackgroundCleanup,
        base::Unretained(this)));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, low_end_background_cleanup_task_.callback(),
        base::TimeDelta::FromSeconds(5));
  }
}

void CompositorDependenciesAndroid::DoLowEndBackgroundCleanup() {
  // When we become visible, we immediately cancel the callback that runs this
  // code. First, evict all unlocked frames, allowing resources to be
  // reclaimed.
  viz::FrameEvictionManager::GetInstance()->PurgeAllUnlockedFrames();

  // Next, notify the GPU process to do background processing, which will
  // lose all renderer contexts.
  content::GpuProcessHost::CallOnIO(
      content::GPU_PROCESS_KIND_SANDBOXED, false /* force_create */,
      base::BindRepeating([](content::GpuProcessHost* host) {
        if (host) {
          host->gpu_service()->OnBackgroundCleanup();
        }
      }));
}

void CompositorDependenciesAndroid::OnCompositorVisible(
    CompositorImpl* compositor) {
  bool element_inserted = visible_compositors_.insert(compositor).second;
  DCHECK(element_inserted);
  if (visible_compositors_.size() == 1)
    OnVisibilityChanged();
}

void CompositorDependenciesAndroid::OnCompositorHidden(
    CompositorImpl* compositor) {
  size_t elements_removed = visible_compositors_.erase(compositor);
  DCHECK_EQ(1u, elements_removed);
  if (visible_compositors_.size() == 0)
    OnVisibilityChanged();
}

// This function runs when our first CompositorImpl becomes visible or when
// our last Compositormpl is hidden.
void CompositorDependenciesAndroid::OnVisibilityChanged() {
  if (visible_compositors_.size() > 0) {
    GpuDataManagerImpl::GetInstance()->SetApplicationVisible(true);
    BrowserGpuChannelHostFactorySetApplicationVisible(true);
    SendOnForegroundedToGpuService();
    low_end_background_cleanup_task_.Cancel();
  } else {
    GpuDataManagerImpl::GetInstance()->SetApplicationVisible(false);
    BrowserGpuChannelHostFactorySetApplicationVisible(false);
    SendOnBackgroundedToGpuService();
    EnqueueLowEndBackgroundCleanup();
  }
}

}  // namespace content
