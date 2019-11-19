// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_DEPENDENCIES_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_DEPENDENCIES_ANDROID_H_

#include <memory>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"

namespace cc {
class TaskGraphRunner;
}  // namespace cc

namespace viz {
class FrameSinkManagerImpl;
}  // namespace viz

namespace content {

class CompositorImpl;

class CompositorDependenciesAndroid {
 public:
  static CompositorDependenciesAndroid& Get();

  cc::TaskGraphRunner* GetTaskGraphRunner();

  viz::HostFrameSinkManager* host_frame_sink_manager() {
    return &host_frame_sink_manager_;
  }

  viz::FrameSinkManagerImpl* frame_sink_manager_impl() {
    return frame_sink_manager_impl_.get();
  }

  viz::FrameSinkId AllocateFrameSinkId();
  void TryEstablishVizConnectionIfNeeded();
  void OnCompositorVisible(CompositorImpl* compositor);
  void OnCompositorHidden(CompositorImpl* compositor);

 private:
  friend class base::NoDestructor<CompositorDependenciesAndroid>;

  static void ConnectVizFrameSinkManagerOnIOThread(
      mojo::PendingReceiver<viz::mojom::FrameSinkManager> receiver,
      mojo::PendingRemote<viz::mojom::FrameSinkManagerClient> client);

  CompositorDependenciesAndroid();
  ~CompositorDependenciesAndroid();

  void CreateVizFrameSinkManager();
  void EnqueueLowEndBackgroundCleanup();
  void DoLowEndBackgroundCleanup();
  void OnVisibilityChanged();

  viz::HostFrameSinkManager host_frame_sink_manager_;
  viz::FrameSinkIdAllocator frame_sink_id_allocator_;

  // Non-viz members:
  // This is owned here so that SurfaceManager will be accessible in process
  // when display is in the same process. Other than using SurfaceManager,
  // access to |in_process_frame_sink_manager_| should happen via
  // |host_frame_sink_manager_| instead which uses Mojo. See
  // http://crbug.com/657959.
  std::unique_ptr<viz::FrameSinkManagerImpl> frame_sink_manager_impl_;

  // A task which runs cleanup tasks on low-end Android after a delay. Enqueued
  // when we hide, canceled when we're shown.
  base::CancelableOnceClosure low_end_background_cleanup_task_;

  // A callback which connects to the viz service on the IO thread.
  base::OnceClosure pending_connect_viz_on_io_thread_;

  // The set of visible CompositorImpls.
  base::flat_set<CompositorImpl*> visible_compositors_;

  std::unique_ptr<cc::TaskGraphRunner> task_graph_runner_;

  DISALLOW_COPY_AND_ASSIGN(CompositorDependenciesAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_DEPENDENCIES_ANDROID_H_
