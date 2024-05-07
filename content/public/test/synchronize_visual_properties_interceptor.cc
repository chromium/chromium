// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/synchronize_visual_properties_interceptor.h"

#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/public/browser/browser_thread.h"

namespace content {

SynchronizeVisualPropertiesInterceptor::SynchronizeVisualPropertiesInterceptor(
    RenderFrameProxyHost* render_frame_proxy_host)
    : local_root_rect_run_loop_(std::make_unique<base::RunLoop>()),
      swapped_impl_(render_frame_proxy_host->frame_host_receiver_for_testing(),
                    this) {}

SynchronizeVisualPropertiesInterceptor::
    ~SynchronizeVisualPropertiesInterceptor() = default;

blink::mojom::RemoteFrameHost*
SynchronizeVisualPropertiesInterceptor::GetForwardingInterface() {
  return swapped_impl_.old_impl();
}

void SynchronizeVisualPropertiesInterceptor::WaitForRect() {
  local_root_rect_run_loop_->Run();
}

void SynchronizeVisualPropertiesInterceptor::ResetRectRunLoop() {
  last_rect_ = gfx::Rect();
  local_root_rect_run_loop_ = std::make_unique<base::RunLoop>();
  local_root_rect_received_ = false;
}

viz::LocalSurfaceId SynchronizeVisualPropertiesInterceptor::WaitForSurfaceId() {
  surface_id_run_loop_ = std::make_unique<base::RunLoop>();
  surface_id_run_loop_->Run();
  return last_surface_id_;
}

void SynchronizeVisualPropertiesInterceptor::SynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties) {
  // Monitor |is_pinch_gesture_active| to determine when pinch gesture ends.
  if (!visual_properties.is_pinch_gesture_active &&
      last_pinch_gesture_active_) {
    pinch_end_run_loop_.Quit();
  }
  last_pinch_gesture_active_ = visual_properties.is_pinch_gesture_active;

  gfx::Rect local_root_rect_in_dip = visual_properties.rect_in_local_root;
  const float dsf =
      visual_properties.screen_infos.current().device_scale_factor;
  local_root_rect_in_dip =
      gfx::Rect(gfx::ScaleToFlooredPoint(
                    visual_properties.rect_in_local_root.origin(), 1.f / dsf),
                gfx::ScaleToCeiledSize(
                    visual_properties.rect_in_local_root.size(), 1.f / dsf));

  // Track each rect updates.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SynchronizeVisualPropertiesInterceptor::OnUpdatedFrameRectOnUI,
          weak_factory_.GetWeakPtr(), local_root_rect_in_dip));

  // Track each surface id update.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SynchronizeVisualPropertiesInterceptor::OnUpdatedSurfaceIdOnUI,
          weak_factory_.GetWeakPtr(), visual_properties.local_surface_id));

  // We can't nest on the IO thread. So tests will wait on the UI thread, so
  // post there to exit the nesting.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SynchronizeVisualPropertiesInterceptor::OnUpdatedFrameSinkIdOnUI,
          weak_factory_.GetWeakPtr()));

  GetForwardingInterface()->SynchronizeVisualProperties(visual_properties);
}

void SynchronizeVisualPropertiesInterceptor::OnUpdatedFrameRectOnUI(
    const gfx::Rect& rect) {
  last_rect_ = rect;
  if (!local_root_rect_received_) {
    local_root_rect_received_ = true;
    // Tests looking at the rect currently expect all received input to finish
    // processing before the test continutes.
    local_root_rect_run_loop_->QuitWhenIdle();
  }
}

void SynchronizeVisualPropertiesInterceptor::OnUpdatedFrameSinkIdOnUI() {
  run_loop_.Quit();
}

void SynchronizeVisualPropertiesInterceptor::OnUpdatedSurfaceIdOnUI(
    viz::LocalSurfaceId surface_id) {
  last_surface_id_ = surface_id;
  if (surface_id_run_loop_) {
    surface_id_run_loop_->QuitWhenIdle();
  }
}

void SynchronizeVisualPropertiesInterceptor::WaitForPinchGestureEnd() {
  pinch_end_run_loop_.Run();
}

}  // namespace content
