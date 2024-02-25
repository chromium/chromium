// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/service/xr_frame_sink_client_impl.h"

#include <memory>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#endif

namespace content {
XrFrameSinkClientImpl::XrFrameSinkClientImpl(int32_t render_process_id,
                                             int32_t render_frame_id)
    : ui_thread_task_runner_(GetUIThreadTaskRunner({})),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id) {
  DCHECK(IsOnUiThread())
      << "XrFrameSinkClientImpl must be constructed on the UI thread.";
}

XrFrameSinkClientImpl::~XrFrameSinkClientImpl() {
  DCHECK(IsOnUiThread())
      << "XrFrameSinkClientImpl must be destructed on the UI thread.";
  if (!initialized_)
    return;

  SurfaceDestroyed();
}

bool XrFrameSinkClientImpl::IsOnUiThread() const {
  return ui_thread_task_runner_->BelongsToCurrentThread();
}

void XrFrameSinkClientImpl::SurfaceDestroyed() {
  DCHECK(IsOnUiThread());
  if (!initialized_)
    return;

  auto* frame_sink_manager = GetHostFrameSinkManager();

  // Since this code can be run during destruction, it's theoretically possible,
  // though unlikely, that the FrameSinkManager no longer exists.
  if (frame_sink_manager)
    frame_sink_manager->InvalidateFrameSinkId(root_frame_sink_id_, this);

  // Reset the initialized state and the root FrameSinkId to an invalid value.
  initialized_ = false;
  root_frame_sink_id_ = viz::FrameSinkId();
}

std::optional<viz::SurfaceId> XrFrameSinkClientImpl::GetDOMSurface() {
  base::AutoLock lock(dom_surface_lock_);
  return dom_surface_id_;
}

viz::FrameSinkId XrFrameSinkClientImpl::FrameSinkId() {
  return root_frame_sink_id_;
}

void XrFrameSinkClientImpl::InitializeRootCompositorFrameSink(
    viz::mojom::RootCompositorFrameSinkParamsPtr root_params,
    device::DomOverlaySetup dom_setup,
    base::OnceClosure on_initialized) {
  DCHECK(!initialized_);
  DVLOG(1) << __func__;

  ui_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&XrFrameSinkClientImpl::InitializeOnUiThread,
                     weak_ptr_factory_.GetWeakPtr(), std::move(root_params),
                     dom_setup, std::move(on_initialized)));
}

void XrFrameSinkClientImpl::InitializeOnUiThread(
    viz::mojom::RootCompositorFrameSinkParamsPtr root_params,
    device::DomOverlaySetup dom_setup,
    base::OnceClosure on_initialized) {
  // AllocateFrameSinkId needs to be called from the UI thread.
  DCHECK(IsOnUiThread());
  DVLOG(1) << __func__;

  root_frame_sink_id_ = AllocateFrameSinkId();
  root_params->frame_sink_id = root_frame_sink_id_;

  GetHostFrameSinkManager()->RegisterFrameSinkId(
      root_params->frame_sink_id, this, viz::ReportFirstSurfaceActivation::kNo);
  GetHostFrameSinkManager()->CreateRootCompositorFrameSink(
      std::move(root_params));

  if (dom_setup != device::DomOverlaySetup::kNone) {
    ConfigureDOMOverlay();
  }

  initialized_ = true;
  std::move(on_initialized).Run();
}

void XrFrameSinkClientImpl::ConfigureDOMOverlay() {
  DCHECK(IsOnUiThread());
  base::AutoLock lock(dom_surface_lock_);

  // This is left outside of the OS_ANDROID ifdef to prevent warnings about the
  // render_process_id and render_frame_id from being unused. Since we check
  // the render_frame_host for an early return, it is in fact used.
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return;

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      render_frame_host->GetOutermostMainFrameOrEmbedder()->GetView());
  CHECK(!root_view || !root_view->IsRenderWidgetHostViewChildFrame());

// Since we don't have the ability to get updates to the surface id on non-
// Android OS's, we let it stay null, which callers can use to as a signal that
// DOMOverlay will not work.
#if BUILDFLAG(IS_ANDROID)
  RenderWidgetHostViewAndroid* view =
      static_cast<RenderWidgetHostViewAndroid*>(root_view);
  if (!view)
    return;

  // The returned CallbackListSubscription manages the lifetime of this callback
  // and thus makes Unretained safe.
  surface_id_changed_subscription_ =
      view->SubscribeToSurfaceIdChanges(base::BindRepeating(
          &XrFrameSinkClientImpl::OnSurfaceIdUpdated, base::Unretained(this)));
  dom_surface_id_ = view->GetCurrentSurfaceId();
#endif

  if (dom_surface_id_ && dom_surface_id_->is_valid()) {
    GetHostFrameSinkManager()->RegisterFrameSinkHierarchy(
        root_frame_sink_id_, dom_surface_id_->frame_sink_id());
  }
}

void XrFrameSinkClientImpl::OnSurfaceIdUpdated(
    const viz::SurfaceId& dom_surface_id) {
  base::AutoLock lock(dom_surface_lock_);
  dom_surface_id_ = dom_surface_id;
}

}  // namespace content
