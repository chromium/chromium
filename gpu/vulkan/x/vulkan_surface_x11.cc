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
      XSelectInput(gfx::GetXDisplay(), static_cast<uint32_t>(surface_->window_),
                   ExposureMask);
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
  XDisplay* display = gfx::GetXDisplay();
  XWindowAttributes attributes;
  if (!XGetWindowAttributes(display, static_cast<uint32_t>(parent_window),
                            &attributes)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window "
               << static_cast<uint32_t>(parent_window) << ".";
    return nullptr;
  }
  Window window = XCreateWindow(
      display, static_cast<uint32_t>(parent_window), 0, 0, attributes.width,
      attributes.height, 0, static_cast<int>(x11::WindowClass::CopyFromParent),
      static_cast<int>(x11::WindowClass::InputOutput), nullptr, 0, nullptr);
  if (!window) {
    LOG(ERROR) << "XCreateWindow failed.";
    return nullptr;
  }
  XMapWindow(display, window);
  XFlush(display);

  VkSurfaceKHR vk_surface;
  VkXlibSurfaceCreateInfoKHR surface_create_info = {
      VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
  surface_create_info.dpy = display;
  surface_create_info.window = window;
  VkResult result = vkCreateXlibSurfaceKHR(vk_instance, &surface_create_info,
                                           nullptr, &vk_surface);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreateXlibSurfaceKHR() failed: " << result;
    return nullptr;
  }
  return std::make_unique<VulkanSurfaceX11>(
      vk_instance, vk_surface, parent_window, static_cast<x11::Window>(window));
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
    Display* display = gfx::GetXDisplay();
    XDestroyWindow(display, static_cast<uint32_t>(window_));
    window_ = x11::Window::None;
    XFlush(display);
  }
}

bool VulkanSurfaceX11::Reshape(const gfx::Size& size,
                               gfx::OverlayTransform pre_transform) {
  DCHECK_EQ(pre_transform, gfx::OVERLAY_TRANSFORM_NONE);

  Display* display = gfx::GetXDisplay();
  XResizeWindow(display, static_cast<uint32_t>(window_), size.width(),
                size.height());
  XFlush(display);
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
