// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/vulkan_browser_compositor_output_surface.h"

#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/service/display/output_surface_client.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_surface.h"

namespace content {

VulkanBrowserCompositorOutputSurface::VulkanBrowserCompositorOutputSurface(
    scoped_refptr<viz::VulkanContextProvider> context,
    const UpdateVSyncParametersCallback& update_vsync_parameters_callback)
    : BrowserCompositorOutputSurface(std::move(context),
                                     update_vsync_parameters_callback),
      weak_ptr_factory_(this) {}

VulkanBrowserCompositorOutputSurface::~VulkanBrowserCompositorOutputSurface() {
  Destroy();
}

bool VulkanBrowserCompositorOutputSurface::Initialize(
    gfx::AcceleratedWidget widget) {
  DCHECK(!surface_);
  std::unique_ptr<gpu::VulkanSurface> surface =
      vulkan_context_provider()->GetVulkanImplementation()->CreateViewSurface(
          widget);
  if (!surface->Initialize(vulkan_context_provider()->GetDeviceQueue(),
                           gpu::VulkanSurface::DEFAULT_SURFACE_FORMAT)) {
    return false;
  }
  surface_ = std::move(surface);

  return true;
}

void VulkanBrowserCompositorOutputSurface::Destroy() {
  if (surface_) {
    surface_->Destroy();
    surface_.reset();
  }
}

void VulkanBrowserCompositorOutputSurface::BindToClient(
    viz::OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void VulkanBrowserCompositorOutputSurface::EnsureBackbuffer() {
  NOTIMPLEMENTED();
}

void VulkanBrowserCompositorOutputSurface::DiscardBackbuffer() {
  NOTIMPLEMENTED();
}

void VulkanBrowserCompositorOutputSurface::BindFramebuffer() {
  NOTIMPLEMENTED();
}

bool VulkanBrowserCompositorOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned VulkanBrowserCompositorOutputSurface::GetOverlayTextureId() const {
  NOTIMPLEMENTED();
  return 0;
}

gfx::BufferFormat VulkanBrowserCompositorOutputSurface::GetOverlayBufferFormat()
    const {
  NOTIMPLEMENTED();
  return gfx::BufferFormat::RGBX_8888;
}

void VulkanBrowserCompositorOutputSurface::Reshape(
    const gfx::Size& size,
    float device_scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha,
    bool use_stencil) {
  surface_->SetSize(size);
}

void VulkanBrowserCompositorOutputSurface::SetDrawRectangle(
    const gfx::Rect& rect) {
  NOTREACHED();
}

unsigned VulkanBrowserCompositorOutputSurface::UpdateGpuFence() {
  return 0;
}

uint32_t
VulkanBrowserCompositorOutputSurface::GetFramebufferCopyTextureFormat() {
  NOTIMPLEMENTED();
  return 0;
}

void VulkanBrowserCompositorOutputSurface::SwapBuffers(
    viz::OutputSurfaceFrame frame) {
  surface_->SwapBuffers();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&VulkanBrowserCompositorOutputSurface::SwapBuffersAck,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VulkanBrowserCompositorOutputSurface::SwapBuffersAck() {
  DCHECK(client_);
  client_->DidReceiveSwapBuffersAck();
}

gpu::VulkanSurface* VulkanBrowserCompositorOutputSurface::GetVulkanSurface() {
  return surface_.get();
}

}  // namespace content
