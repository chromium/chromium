// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/x/vulkan_surface_x11.h"

#include "base/logging.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/x11/x11_event_source.h"

namespace gpu {

#if !defined(USE_OZONE)

class VulkanSurfaceX11::ExposeEventForwarder
    : public ui::PlatformEventDispatcher {
 public:
  explicit ExposeEventForwarder(VulkanSurfaceX11* surface) : surface_(surface) {
    if (auto* event_source = ui::PlatformEventSource::GetInstance()) {
      XSelectInput(gfx::GetXDisplay(), surface_->window_, ExposureMask);
      event_source->AddPlatformEventDispatcher(this);
    }
  }

  ~ExposeEventForwarder() override {
    if (auto* event_source = ui::PlatformEventSource::GetInstance())
      event_source->RemovePlatformEventDispatcher(this);
  }

  // ui::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override {
    return surface_->CanDispatchXEvent(event);
  }

  uint32_t DispatchEvent(const ui::PlatformEvent& event) override {
    surface_->ForwardXExposeEvent(event);
    return ui::POST_DISPATCH_STOP_PROPAGATION;
  }

 private:
  VulkanSurfaceX11* const surface_;
  DISALLOW_COPY_AND_ASSIGN(ExposeEventForwarder);
};

#else  // defined(USE_OZONE)

class VulkanSurfaceX11::ExposeEventForwarder : public ui::XEventDispatcher {
 public:
  explicit ExposeEventForwarder(VulkanSurfaceX11* surface) : surface_(surface) {
    if (auto* event_source = ui::X11EventSource::GetInstance()) {
      XSelectInput(gfx::GetXDisplay(), surface_->window_, ExposureMask);
      event_source->AddXEventDispatcher(this);
    }
  }

  ~ExposeEventForwarder() override {
    if (auto* event_source = ui::X11EventSource::GetInstance())
      event_source->RemoveXEventDispatcher(this);
  }

  // ui::XEventDispatcher:
  void CheckCanDispatchNextPlatformEvent(XEvent* xev) override {}
  void PlatformEventDispatchFinished() override {}
  ui::PlatformEventDispatcher* GetPlatformEventDispatcher() override {
    return nullptr;
  }

  bool DispatchXEvent(XEvent* xevent) override {
    if (!surface_->CanDispatchXEvent(xevent))
      return false;
    surface_->ForwardXExposeEvent(xevent);
    return true;
  }

 private:
  VulkanSurfaceX11* const surface_;
  DISALLOW_COPY_AND_ASSIGN(ExposeEventForwarder);
};

#endif

// static
std::unique_ptr<VulkanSurfaceX11> VulkanSurfaceX11::Create(
    VkInstance vk_instance,
    Window parent_window) {
  XDisplay* display = gfx::GetXDisplay();
  XWindowAttributes attributes;
  if (!XGetWindowAttributes(display, parent_window, &attributes)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window " << parent_window
               << ".";
    return nullptr;
  }
  Window window = XCreateWindow(display, parent_window, 0, 0, attributes.width,
                                attributes.height, 0, CopyFromParent,
                                InputOutput, CopyFromParent, 0, nullptr);
  if (!window) {
    LOG(ERROR) << "XCreateWindow failed.";
    return nullptr;
  }
  XMapWindow(display, window);

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
  return std::make_unique<VulkanSurfaceX11>(vk_instance, vk_surface,
                                            parent_window, window);
}

VulkanSurfaceX11::VulkanSurfaceX11(VkInstance vk_instance,
                                   VkSurfaceKHR vk_surface,
                                   Window parent_window,
                                   Window window)
    : VulkanSurface(vk_instance, vk_surface, false /* use_protected_memory */),
      parent_window_(parent_window),
      window_(window),
      expose_event_forwarder_(new ExposeEventForwarder(this)) {}

VulkanSurfaceX11::~VulkanSurfaceX11() {}

// VulkanSurface:
bool VulkanSurfaceX11::Reshape(const gfx::Size& size,
                               gfx::OverlayTransform pre_transform) {
  DCHECK_EQ(pre_transform, gfx::OVERLAY_TRANSFORM_NONE);

  XResizeWindow(gfx::GetXDisplay(), window_, size.width(), size.height());
  return VulkanSurface::Reshape(size, pre_transform);
}

bool VulkanSurfaceX11::CanDispatchXEvent(const XEvent* event) {
  return event->type == Expose && event->xexpose.window == window_;
}

void VulkanSurfaceX11::ForwardXExposeEvent(const XEvent* event) {
  XEvent forwarded_event = *event;
  forwarded_event.xexpose.window = parent_window_;
  XSendEvent(gfx::GetXDisplay(), parent_window_, False, ExposureMask,
             &forwarded_event);
  XFlush(gfx::GetXDisplay());
}

}  // namespace gpu
