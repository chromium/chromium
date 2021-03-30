// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/tests/native_window.h"

#include "base/containers/flat_map.h"
#include "ui/base/ui_base_features.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(USE_X11)
#include "ui/platform_window/x11/x11_window.h"  // nogncheck
#endif

namespace gpu {
namespace {

class Window : public ui::PlatformWindowDelegate {
 public:
  Window() = default;
  ~Window() override = default;

  void Initialize(const gfx::Rect& bounds) {
    DCHECK(!platform_window_);

#if defined(USE_OZONE) || defined(USE_X11)
    ui::PlatformWindowInitProperties props(bounds);
#if defined(USE_OZONE)
    if (features::IsUsingOzonePlatform()) {
      platform_window_ = ui::OzonePlatform::GetInstance()->CreatePlatformWindow(
          this, std::move(props));
    }
#endif
#if defined(USE_X11)
    if (!platform_window_) {
      DCHECK(!features::IsUsingOzonePlatform());
      auto x11_window = std::make_unique<ui::X11Window>(this);
      x11_window->Initialize(std::move(props));
      platform_window_ = std::move(x11_window);
    }
#endif
#else
    NOTIMPLEMENTED();
    return;
#endif
    platform_window_->Show();
  }

  gfx::AcceleratedWidget widget() const { return widget_; }

 private:
  // ui::PlatformWindowDelegate:
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {
    widget_ = widget;
  }
  void OnBoundsChanged(const BoundsChange& change) override {}
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(ui::Event* event) override {}
  void OnCloseRequest() override {}
  void OnClosed() override {}
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override {}
  void OnLostCapture() override {}
  void OnWillDestroyAcceleratedWidget() override {}
  void OnAcceleratedWidgetDestroyed() override {}
  void OnActivationChanged(bool active) override {}
  void OnMouseEnter() override {}

  std::unique_ptr<ui::PlatformWindow> platform_window_;
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
};

base::flat_map<gfx::AcceleratedWidget, std::unique_ptr<Window>> g_windows_;

}  // namespace

gfx::AcceleratedWidget CreateNativeWindow(const gfx::Rect& bounds) {
  auto window = std::make_unique<Window>();
  window->Initialize(bounds);
  gfx::AcceleratedWidget widget = window->widget();
  if (widget != gfx::kNullAcceleratedWidget)
    g_windows_[widget] = std::move(window);
  return widget;
}

void DestroyNativeWindow(gfx::AcceleratedWidget window) {
  auto it = g_windows_.find(window);
  DCHECK(it != g_windows_.end());
  g_windows_.erase(it);
}

}  // namespace gpu
