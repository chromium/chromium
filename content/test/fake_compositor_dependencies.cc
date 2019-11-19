// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fake_compositor_dependencies.h"

#include <stddef.h>

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/test_ukm_recorder_factory.h"
#include "content/renderer/frame_swap_message_queue.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/buffer_types.h"

namespace content {

FakeCompositorDependencies::FakeCompositorDependencies() {
}

FakeCompositorDependencies::~FakeCompositorDependencies() {
}

bool FakeCompositorDependencies::IsGpuRasterizationForced() {
  return false;
}

int FakeCompositorDependencies::GetGpuRasterizationMSAASampleCount() {
  return 0;
}

bool FakeCompositorDependencies::IsLcdTextEnabled() {
  return false;
}

bool FakeCompositorDependencies::IsZeroCopyEnabled() {
  return true;
}

bool FakeCompositorDependencies::IsPartialRasterEnabled() {
  return false;
}

bool FakeCompositorDependencies::IsGpuMemoryBufferCompositorResourcesEnabled() {
  return false;
}

bool FakeCompositorDependencies::IsElasticOverscrollEnabled() {
  return true;
}

bool FakeCompositorDependencies::IsUseZoomForDSFEnabled() {
  return use_zoom_for_dsf_;
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeCompositorDependencies::GetCompositorMainThreadTaskRunner() {
  return base::ThreadTaskRunnerHandle::Get();
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeCompositorDependencies::GetCompositorImplThreadTaskRunner() {
  return nullptr;  // Currently never threaded compositing in unit tests.
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeCompositorDependencies::GetCleanupTaskRunner() {
  return base::ThreadTaskRunnerHandle::Get();
}

blink::scheduler::WebThreadScheduler*
FakeCompositorDependencies::GetWebMainThreadScheduler() {
  return &main_thread_scheduler_;
}

cc::TaskGraphRunner* FakeCompositorDependencies::GetTaskGraphRunner() {
  return &task_graph_runner_;
}

bool FakeCompositorDependencies::IsScrollAnimatorEnabled() {
  return false;
}

std::unique_ptr<cc::UkmRecorderFactory>
FakeCompositorDependencies::CreateUkmRecorderFactory() {
  return std::make_unique<cc::TestUkmRecorderFactory>();
}

void FakeCompositorDependencies::RequestNewLayerTreeFrameSink(
    RenderWidget* render_widget,
    scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue,
    const GURL& url,
    LayerTreeFrameSinkCallback callback,
    mojo::PendingReceiver<mojom::RenderFrameMetadataObserverClient>
        render_frame_metadata_observer_client_receiver,
    mojo::PendingRemote<mojom::RenderFrameMetadataObserver>
        render_frame_metadata_observer_remote,
    const char* client_name) {
  std::move(callback).Run(cc::FakeLayerTreeFrameSink::Create3d());
}

#ifdef OS_ANDROID
bool FakeCompositorDependencies::UsingSynchronousCompositing() {
  return false;
}
#endif

}  // namespace content
