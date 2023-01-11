// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/win32/vulkan_surface_win32.h"

#include <windows.h>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "ui/gfx/win/window_impl.h"

namespace gpu {

namespace {

// It is set by WindowThread ctor, and cleared by WindowThread dtor.
VulkanSurfaceWin32::WindowThread* g_thread = nullptr;

// It is set by HiddenToplevelWindow ctor, and cleared by HiddenToplevelWindow
// dtor.
class HiddenToplevelWindow* g_initial_parent_window = nullptr;

class HiddenToplevelWindow : public gfx::WindowImpl,
                             public base::RefCounted<HiddenToplevelWindow> {
 public:
  HiddenToplevelWindow() : gfx::WindowImpl("VulkanHiddenToplevelWindow") {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(!g_initial_parent_window);
    set_initial_class_style(CS_OWNDC);
    set_window_style(WS_POPUP | WS_DISABLED);
    Init(GetDesktopWindow(), gfx::Rect());
    g_initial_parent_window = this;
  }

  HiddenToplevelWindow(const HiddenToplevelWindow&) = delete;
  HiddenToplevelWindow& operator=(const HiddenToplevelWindow&) = delete;

 private:
  friend class base::RefCounted<HiddenToplevelWindow>;

  // gfx::WindowImpl:
  BOOL ProcessWindowMessage(HWND window,
                            UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT& result,
                            DWORD msg_map_id) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (message == WM_CLOSE) {
      // Prevent closing the window, since external apps may get a handle to
      // this window and attempt to close it.
      result = 0;
      return true;
    }
    return false;
  }

  ~HiddenToplevelWindow() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK_EQ(g_initial_parent_window, this);
    g_initial_parent_window = nullptr;
  }

  THREAD_CHECKER(thread_checker_);
};

class ChildWindow : public gfx::WindowImpl {
 public:
  explicit ChildWindow(HWND parent_window)
      : gfx::WindowImpl("VulkanHiddenToplevelWindow") {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    // If there is no other ChildWindow instance, |g_initial_parent_window| will
    // be nullptr, and then we need to create one. If |g_initial_parent_window|
    // is not nullptr, so |g_initial_parent_window| will not be destroyed until
    // this ChildWindow is destroyed.
    initial_parent_window_ = g_initial_parent_window;
    if (!initial_parent_window_)
      initial_parent_window_ = base::MakeRefCounted<HiddenToplevelWindow>();

    set_initial_class_style(CS_OWNDC);
    set_window_style(WS_VISIBLE | WS_CHILD | WS_DISABLED);
    RECT window_rect;
    if (!GetClientRect(parent_window, &window_rect))
      PLOG(DFATAL) << "GetClientRect() failed.";
    gfx::Rect bounds(window_rect);
    bounds.set_origin(gfx::Point(0, 0));
    Init(initial_parent_window_->hwnd(), bounds);
  }

  ~ChildWindow() override { DCHECK_CALLED_ON_VALID_THREAD(thread_checker_); }

  ChildWindow(const ChildWindow&) = delete;
  ChildWindow& operator=(const ChildWindow&) = delete;

 private:
  // gfx::WindowImpl:
  BOOL ProcessWindowMessage(HWND window,
                            UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT& result,
                            DWORD msg_map_id) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    switch (message) {
      case WM_ERASEBKGND:
        // Prevent windows from erasing the background.
        return true;
      case WM_PAINT:
        // Do not paint anything.
        PAINTSTRUCT paint;
        if (BeginPaint(window, &paint))
          EndPaint(window, &paint);
        return false;
      default:
        return false;
    }
  }

  // A temporary parent window for this child window, it will be reparented
  // by browser soon. All child windows share one initial parent window.
  // The initial parent window will be destroyed with the last child window.
  scoped_refptr<HiddenToplevelWindow> initial_parent_window_;

  THREAD_CHECKER(thread_checker_);
};

void CreateChildWindow(HWND parent_window,
                       std::unique_ptr<ChildWindow>* window,
                       base::WaitableEvent* event) {
  *window = std::make_unique<ChildWindow>(parent_window);
  event->Signal();
}

}  // namespace

class VulkanSurfaceWin32::WindowThread : public base::Thread,
                                         public base::RefCounted<WindowThread> {
 public:
  WindowThread() : base::Thread("VulkanWindowThread") {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(!g_thread);
    g_thread = this;
    base::Thread::Options options(base::MessagePumpType::UI, 0);
    StartWithOptions(std::move(options));
  }

  WindowThread(const WindowThread&) = delete;
  WindowThread& operator=(const WindowThread&) = delete;

 private:
  friend class base::RefCounted<WindowThread>;

  ~WindowThread() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK_EQ(g_thread, this);
    Stop();
    g_thread = nullptr;
  }

  THREAD_CHECKER(thread_checker_);
};

// static
std::unique_ptr<VulkanSurfaceWin32> VulkanSurfaceWin32::Create(
    VkInstance vk_instance,
    HWND parent_window) {
  scoped_refptr<WindowThread> thread = g_thread;
  if (!thread) {
    // If there is no other VulkanSurfaceWin32, g_thread will be nullptr, and
    // we need to create a thread for running child window message loop.
    // Otherwise keep a ref of g_thread, so it will not be destroyed until the
    // this VulkanSurfaceWin32 is destroyed.
    thread = base::MakeRefCounted<WindowThread>();
    g_thread = thread.get();
  }

  // vkCreateSwapChainKHR() fails in sandbox with a window which is created by
  // other process with NVIDIA driver. Workaround the problem by creating a
  // child window and use it to create vulkan surface.
  // TODO(penghuang): Only apply this workaround with NVIDIA GPU?
  // https://crbug.com/1068742
  std::unique_ptr<ChildWindow> window;
  base::WaitableEvent event;
  thread->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateChildWindow, parent_window, &window, &event));
  event.Wait();

  VkSurfaceKHR surface;
  VkWin32SurfaceCreateInfoKHR surface_create_info = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .hinstance = reinterpret_cast<HINSTANCE>(
          GetWindowLongPtr(window->hwnd(), GWLP_HINSTANCE)),
      .hwnd = window->hwnd(),
  };
  VkResult result = vkCreateWin32SurfaceKHR(vk_instance, &surface_create_info,
                                            nullptr, &surface);
  if (VK_SUCCESS != result) {
    LOG(DFATAL) << "vkCreatWin32SurfaceKHR() failed: " << result;
    return nullptr;
  }
  return std::make_unique<VulkanSurfaceWin32>(
      base::PassKey<VulkanSurfaceWin32>(), vk_instance, surface,
      std::move(thread), std::move(window));
}

VulkanSurfaceWin32::VulkanSurfaceWin32(
    base::PassKey<VulkanSurfaceWin32> pass_key,
    VkInstance vk_instance,
    VkSurfaceKHR vk_surface,
    scoped_refptr<WindowThread> thread,
    std::unique_ptr<gfx::WindowImpl> window)
    : VulkanSurface(vk_instance, window->hwnd(), vk_surface),
      thread_(std::move(thread)),
      window_(std::move(window)) {}

VulkanSurfaceWin32::~VulkanSurfaceWin32() {
  thread_->task_runner()->DeleteSoon(FROM_HERE, std::move(window_));
}

bool VulkanSurfaceWin32::Reshape(const gfx::Size& size,
                                 gfx::OverlayTransform pre_transform) {
  DCHECK_EQ(pre_transform, gfx::OVERLAY_TRANSFORM_NONE);
  constexpr auto kFlags = SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOCOPYBITS |
                          SWP_NOOWNERZORDER | SWP_NOZORDER;
  if (!SetWindowPos(window_->hwnd(), nullptr, 0, 0, size.width(), size.height(),
                    kFlags)) {
    PLOG(DFATAL) << "SetWindowPos() failed";
    return false;
  }
  return VulkanSurface::Reshape(size, pre_transform);
}

}  // namespace gpu
