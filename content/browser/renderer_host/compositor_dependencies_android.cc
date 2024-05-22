// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositor_dependencies_android.h"

#include <utility>

#include "base/allocator/partition_alloc_support.h"
#include "base/features.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/viz/client/frame_eviction_manager.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
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
  content::GpuProcessHost::CallOnUI(
      FROM_HERE, content::GPU_PROCESS_KIND_SANDBOXED, false /* force_create */,
      base::BindOnce([](content::GpuProcessHost* host) {
        // This is not necessarily the most logical place to notify the
        // allocator, but it matches the call made on the GPU process side.
        base::allocator::PartitionAllocSupport::Get()->OnBackgrounded();
        if (host) {
          host->gpu_service()->OnBackgrounded();
        }
      }));
}

void SendOnForegroundedToGpuService() {
  content::GpuProcessHost::CallOnUI(
      FROM_HERE, content::GPU_PROCESS_KIND_SANDBOXED, false /* force_create */,
      base::BindOnce([](content::GpuProcessHost* host) {
        base::allocator::PartitionAllocSupport::Get()->OnForegrounded();
        if (host) {
          host->gpu_service()->OnForegrounded();
        }
      }));
}

}  // namespace

// static
CompositorDependenciesAndroid& CompositorDependenciesAndroid::Get() {
  static base::NoDestructor<CompositorDependenciesAndroid> instance;
  return *instance;
}

CompositorDependenciesAndroid::CompositorDependenciesAndroid()
    : frame_sink_id_allocator_(kDefaultClientId) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Set up a callback to automatically re-connect if we lose our connection.
  // Unretained is safe due to base::NoDestructor.
  host_frame_sink_manager_.SetConnectionLostCallback(base::BindRepeating(
      &CompositorDependenciesAndroid::CreateVizFrameSinkManager,
      base::Unretained(this)));

  CreateVizFrameSinkManager();
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
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      std::move(frame_sink_manager));

  // Set up a pending request which will be run once we've successfully
  // connected to the GPU process.
  pending_connect_viz_on_main_thread_ = base::BindOnce(
      &CompositorDependenciesAndroid::ConnectVizFrameSinkManagerOnMainThread,
      std::move(frame_sink_manager_receiver),
      std::move(frame_sink_manager_client),
      host_frame_sink_manager_.debug_renderer_settings());
}

viz::FrameSinkId CompositorDependenciesAndroid::AllocateFrameSinkId() {
  return frame_sink_id_allocator_.NextFrameSinkId();
}

void CompositorDependenciesAndroid::TryEstablishVizConnectionIfNeeded() {
  if (!pending_connect_viz_on_main_thread_)
    return;
  std::move(pending_connect_viz_on_main_thread_).Run();
}

// Called on the GpuProcessHost thread, after a GPU connection has already been
// established. |gpu_process_host| should only be invalid if a channel has been
// established and lost. In this case the ConnectionLost callback will be
// re-run when the request is deleted (goes out of scope).
// static
void CompositorDependenciesAndroid::ConnectVizFrameSinkManagerOnMainThread(
    mojo::PendingReceiver<viz::mojom::FrameSinkManager> receiver,
    mojo::PendingRemote<viz::mojom::FrameSinkManagerClient> client,
    const viz::DebugRendererSettings& debug_renderer_settings) {
  auto* gpu_process_host = GpuProcessHost::Get();
  if (!gpu_process_host)
    return;

  gpu_process_host->gpu_host()->ConnectFrameSinkManager(
      std::move(receiver), std::move(client), debug_renderer_settings);
}

void CompositorDependenciesAndroid::EnqueueLowEndBackgroundCleanup() {
  if (base::SysInfo::IsLowEndDevice()) {
    low_end_background_cleanup_task_.Reset(base::BindOnce(
        &CompositorDependenciesAndroid::DoLowEndBackgroundCleanup,
        base::Unretained(this)));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, low_end_background_cleanup_task_.callback(),
        base::Seconds(5));
  }
}

void CompositorDependenciesAndroid::DoLowEndBackgroundCleanup() {
  // When we become visible, we immediately cancel the callback that runs this
  // code. First, evict all unlocked frames, allowing resources to be
  // reclaimed.
  viz::FrameEvictionManager::GetInstance()->PurgeAllUnlockedFrames();

  // Next, notify the GPU process to do background processing, which will
  // lose all renderer contexts.
  auto* host = GpuProcessHost::Get();
  if (host) {
    host->gpu_service()->OnBackgroundCleanup();
  }
}

void CompositorDependenciesAndroid::OnCompositorVisible(
    CompositorImpl* compositor) {
  CHECK(!visible_synchronous_compositors_);
  bool element_inserted = visible_compositors_.insert(compositor).second;
  DCHECK(element_inserted);
  if (visible_compositors_.size() == 1)
    OnVisibilityChanged();
}

void CompositorDependenciesAndroid::OnCompositorHidden(
    CompositorImpl* compositor) {
  CHECK(!visible_synchronous_compositors_);
  size_t elements_removed = visible_compositors_.erase(compositor);
  DCHECK_EQ(1u, elements_removed);
  if (visible_compositors_.size() == 0)
    OnVisibilityChanged();
}

void CompositorDependenciesAndroid::OnSynchronousCompositorVisible() {
  CHECK(visible_compositors_.empty());
  visible_synchronous_compositors_++;
  if (visible_synchronous_compositors_ == 1u) {
    OnVisibilityChanged();
  }
}

void CompositorDependenciesAndroid::OnSynchronousCompositorHidden() {
  CHECK(visible_compositors_.empty());
  CHECK_GT(visible_synchronous_compositors_, 0u);
  visible_synchronous_compositors_--;
  if (visible_synchronous_compositors_ == 0u) {
    OnVisibilityChanged();
  }
}

// This function runs when our first CompositorImpl becomes visible or when
// our last Compositormpl is hidden.
void CompositorDependenciesAndroid::OnVisibilityChanged() {
  if (visible_compositors_.size() > 0 ||
      visible_synchronous_compositors_ > 0u) {
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
