// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_DEPENDENCIES_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_DEPENDENCIES_ANDROID_H_

#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"

namespace content {

class CompositorImpl;

class CONTENT_EXPORT CompositorDependenciesAndroid {
 public:
  static CompositorDependenciesAndroid& Get();

  CompositorDependenciesAndroid(const CompositorDependenciesAndroid&) = delete;
  CompositorDependenciesAndroid& operator=(
      const CompositorDependenciesAndroid&) = delete;

  viz::HostFrameSinkManager* host_frame_sink_manager() {
    return &host_frame_sink_manager_;
  }

  viz::FrameSinkId AllocateFrameSinkId();
  void TryEstablishVizConnectionIfNeeded();
  void OnCompositorVisible(CompositorImpl* compositor);
  void OnCompositorHidden(CompositorImpl* compositor);
  void OnSynchronousCompositorVisible();
  void OnSynchronousCompositorHidden();

  void DoLowEndBackgroundCleanupForTesting() { DoLowEndBackgroundCleanup(); }

 private:
  friend class base::NoDestructor<CompositorDependenciesAndroid>;

  static void ConnectVizFrameSinkManagerOnMainThread(
      mojo::PendingReceiver<viz::mojom::FrameSinkManager> receiver,
      mojo::PendingRemote<viz::mojom::FrameSinkManagerClient> client,
      const viz::DebugRendererSettings& debug_renderer_settings);

  CompositorDependenciesAndroid();
  ~CompositorDependenciesAndroid();

  void CreateVizFrameSinkManager();
  void EnqueueLowEndBackgroundCleanup();
  void DoLowEndBackgroundCleanup();
  void OnVisibilityChanged();

  viz::HostFrameSinkManager host_frame_sink_manager_;
  viz::FrameSinkIdAllocator frame_sink_id_allocator_;

  // A task which runs cleanup tasks on low-end Android after a delay. Enqueued
  // when we hide, canceled when we're shown.
  base::CancelableOnceClosure low_end_background_cleanup_task_;

  // A callback which connects to the viz service on the main thread.
  base::OnceClosure pending_connect_viz_on_main_thread_;

  // The set of visible CompositorImpls.
  base::flat_set<raw_ptr<CompositorImpl, CtnExperimental>> visible_compositors_;
  size_t visible_synchronous_compositors_ = 0u;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_DEPENDENCIES_ANDROID_H_
