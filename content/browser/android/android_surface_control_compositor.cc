// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/android_surface_control_compositor.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/slim/frame_sink.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/renderer_host/compositor_dependencies_android.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/ipc/common/gpu_surface_tracker.h"
#include "ui/android/window_android.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gl/android/scoped_java_surface_control.h"

namespace content {

AndroidSurfaceControlCompositor::AndroidSurfaceControlCompositor(
    viz::FrameSinkId frame_sink_id)
    : HostDisplayClient(gfx::kNullAcceleratedWidget),
      frame_sink_id_(frame_sink_id) {}

AndroidSurfaceControlCompositor::~AndroidSurfaceControlCompositor() {
  display_private_.reset();
  layer_tree_.reset();
  if (is_registered_) {
    GetHostFrameSinkManager()->InvalidateFrameSinkId(
        frame_sink_id_, this,
        base::BindOnce(
            [](gpu::SurfaceHandle surface_handle) {
              if (surface_handle != gpu::kNullSurfaceHandle) {
                gpu::GpuSurfaceTracker::Get()->RemoveSurface(surface_handle);
              }
            },
            surface_handle_));
  } else if (surface_handle_ != gpu::kNullSurfaceHandle) {
    gpu::GpuSurfaceTracker::Get()->RemoveSurface(surface_handle_);
  }
}

bool AndroidSurfaceControlCompositor::Initialize(
    ui::WindowAndroid& window_android,
    const base::android::JavaRef<jobject>& j_surface_control,
    cc::slim::LayerTreeClient* client,
    scoped_refptr<cc::slim::Layer> root_layer,
    const gfx::Size& size_pixels,
    float device_scale_factor) {
  CHECK(!j_surface_control.is_null());
  CHECK(root_layer);

  bool release_on_destroy = true;
  gl::ScopedJavaSurfaceControl scoped_java_surface_control(j_surface_control,
                                                           release_on_destroy);
  surface_handle_ = gpu::GpuSurfaceTracker::Get()->AddSurfaceForNativeWidget(
      gpu::SurfaceRecord(std::move(scoped_java_surface_control)));
  if (surface_handle_ == gpu::kNullSurfaceHandle) {
    return false;
  }

  GetHostFrameSinkManager()->RegisterFrameSinkId(
      frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
  is_registered_ = true;

  layer_tree_ = cc::slim::LayerTree::Create(client);
  layer_tree_->set_background_color(SkColors::kTransparent);
  layer_tree_->SetRoot(std::move(root_layer));

  return CreateDisplayAndFrameSink(window_android, size_pixels,
                                   device_scale_factor);
}

void AndroidSurfaceControlCompositor::Resize(
    const gfx::Size& size_pixels,
    float device_scale_factor,
    const viz::LocalSurfaceId& local_surface_id) {
  if (display_private_) {
    display_private_->Resize(size_pixels);
  }
  if (layer_tree_) {
    layer_tree_->SetViewportRectAndScale(gfx::Rect(size_pixels),
                                         device_scale_factor, local_surface_id);
  }
}

bool AndroidSurfaceControlCompositor::CreateDisplayAndFrameSink(
    ui::WindowAndroid& window_android,
    const gfx::Size& size_pixels,
    float device_scale_factor) {
  CHECK_NE(surface_handle_, gpu::kNullSurfaceHandle);

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
      BrowserGpuChannelHostFactory::instance()->GetGpuChannel();
  if (!gpu_channel_host) {
    return false;
  }

  CompositorDependenciesAndroid::Get().TryEstablishVizConnectionIfNeeded();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetUIThreadTaskRunner({BrowserTaskType::kUserInput});

  auto root_params = viz::mojom::RootCompositorFrameSinkParams::New();

  mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink> sink_remote;
  root_params->compositor_frame_sink =
      sink_remote.InitWithNewEndpointAndPassReceiver();
  mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient> client_receiver =
      root_params->compositor_frame_sink_client
          .InitWithNewPipeAndPassReceiver();
  display_private_.reset();
  root_params->display_private =
      display_private_.BindNewEndpointAndPassReceiver();

  root_params->display_client = GetBoundRemote(task_runner);

  gfx::DisplayColorSpaces display_color_spaces =
      display::Screen::Get()
          ->GetDisplayNearestWindow(&window_android)
          .GetColorSpaces();

  viz::RendererSettings renderer_settings;
  renderer_settings.partial_swap_enabled = true;
  renderer_settings.allow_antialiasing = false;
  renderer_settings.highp_threshold_min = 2048;
  renderer_settings.requires_alpha_channel = true;

  root_params->frame_sink_id = frame_sink_id_;
  root_params->widget = surface_handle_;
  root_params->gpu_compositing = true;
  root_params->renderer_settings = renderer_settings;
  root_params->refresh_rate = window_android.GetRefreshRate();

  GetHostFrameSinkManager()->CreateRootCompositorFrameSink(
      std::move(root_params), /*maybe_wait_on_destruction=*/false);

  display_private_->SetDisplayVisible(true);
  display_private_->Resize(size_pixels);
  display_private_->SetDisplayColorSpaces(display_color_spaces);
  display_private_->SetSupportedRefreshRates(
      window_android.GetSupportedRefreshRates());

  layer_tree_->SetFrameSink(cc::slim::FrameSink::Create(
      std::move(sink_remote), std::move(client_receiver), nullptr,
      GetUIThreadTaskRunner({BrowserTaskType::kUserInput}),
      base::kInvalidThreadId));
  layer_tree_->SetVisible(true);
  return true;
}

}  // namespace content
