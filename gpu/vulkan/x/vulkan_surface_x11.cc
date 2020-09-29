// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/x/vulkan_surface_x11.h"

#include "base/logging.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_util.h"

namespace gpu {

class VulkanSurfaceX11::ExposeEventForwarder : public ui::XEventDispatcher {
 public:
  explicit ExposeEventForwarder(VulkanSurfaceX11* surface) : surface_(surface) {
    if (auto* event_source = ui::X11EventSource::GetInstance()) {
      x11::Connection::Get()->ChangeWindowAttributes(
          {.window = static_cast<x11::Window>(surface_->window_),
           .event_mask = x11::EventMask::Exposure});
      event_source->AddXEventDispatcher(this);
    }
  }

  ~ExposeEventForwarder() override {
    if (auto* event_source = ui::X11EventSource::GetInstance())
      event_source->RemoveXEventDispatcher(this);
  }

  // ui::XEventDispatcher:
  bool DispatchXEvent(x11::Event* xevent) override {
    if (!surface_->CanDispatchXEvent(xevent))
      return false;
    surface_->ForwardXExposeEvent(xevent);
    return true;
  }

 private:
  VulkanSurfaceX11* const surface_;
  DISALLOW_COPY_AND_ASSIGN(ExposeEventForwarder);
};

// static
std::unique_ptr<VulkanSurfaceX11> VulkanSurfaceX11::Create(
    VkInstance vk_instance,
    x11::Window parent_window) {
  auto* connection = x11::Connection::Get();
  auto geometry = connection->GetGeometry({parent_window}).Sync();
  if (!geometry) {
    LOG(ERROR) << "GetGeometry failed for window "
               << static_cast<uint32_t>(parent_window) << ".";
    return nullptr;
  }

  auto window = connection->GenerateId<x11::Window>();
  connection->CreateWindow({
      .wid = window,
      .parent = parent_window,
      .width = geometry->width,
      .height = geometry->height,
      .c_class = x11::WindowClass::InputOutput,
  });
  if (connection->MapWindow({window}).Sync().error) {
    LOG(ERROR) << "Failed to create or map window.";
    return nullptr;
  }

  VkSurfaceKHR vk_surface;
  VkXlibSurfaceCreateInfoKHR surface_create_info = {
      VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
  surface_create_info.dpy = connection->display();
  surface_create_info.window = static_cast<uint32_t>(window);
  VkResult result = vkCreateXlibSurfaceKHR(vk_instance, &surface_create_info,
                                           nullptr, &vk_surface);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreateXlibSurfaceKHR() failed: " << result;
    return nullptr;
  }
  return std::make_unique<VulkanSurfaceX11>(vk_instance, vk_surface,
                                            parent_window, window);
}

VulkanSurfaceX11::VulkanSurfaceX11(VkInstance vk_instance,
                                   VkSurfaceKHR vk_surface,
                                   x11::Window parent_window,
                                   x11::Window window)
    : VulkanSurface(vk_instance,
                    static_cast<gfx::AcceleratedWidget>(window),
                    vk_surface,
                    false /* use_protected_memory */),
      parent_window_(parent_window),
      window_(window),
      expose_event_forwarder_(std::make_unique<ExposeEventForwarder>(this)) {}

VulkanSurfaceX11::~VulkanSurfaceX11() = default;

void VulkanSurfaceX11::Destroy() {
  VulkanSurface::Destroy();
  expose_event_forwarder_.reset();
  if (window_ != x11::Window::None) {
    auto* connection = x11::Connection::Get();
    connection->DestroyWindow({window_});
    window_ = x11::Window::None;
    connection->Flush();
  }
}

bool VulkanSurfaceX11::Reshape(const gfx::Size& size,
                               gfx::OverlayTransform pre_transform) {
  DCHECK_EQ(pre_transform, gfx::OVERLAY_TRANSFORM_NONE);

  auto* connection = x11::Connection::Get();
  connection->ConfigureWindow(
      {.window = window_, .width = size.width(), .height = size.height()});
  connection->Flush();
  return VulkanSurface::Reshape(size, pre_transform);
}

bool VulkanSurfaceX11::CanDispatchXEvent(const x11::Event* x11_event) {
  auto* expose = x11_event->As<x11::ExposeEvent>();
  return expose && expose->window == window_;
}

void VulkanSurfaceX11::ForwardXExposeEvent(const x11::Event* event) {
  auto forwarded_event = *event->As<x11::ExposeEvent>();
  forwarded_event.window = parent_window_;
  x11::SendEvent(forwarded_event, parent_window_, x11::EventMask::Exposure);
  x11::Connection::Get()->Flush();
}

}  // namespace gpu
