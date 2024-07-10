// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/tests/native_window.h"

#include "base/containers/flat_map.h"
#include "base/not_fatal_until.h"
#include "build/build_config.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace gpu {
namespace {

class Window : public ui::PlatformWindowDelegate {
 public:
  Window() = default;
  ~Window() override = default;

  void Initialize(const gfx::Rect& bounds) {
    DCHECK(!platform_window_);

#if BUILDFLAG(IS_OZONE)
    ui::PlatformWindowInitProperties props(bounds);
    platform_window_ = ui::OzonePlatform::GetInstance()->CreatePlatformWindow(
        this, std::move(props));
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
  void OnWindowStateChanged(ui::PlatformWindowState old_state,
                            ui::PlatformWindowState new_state) override {}
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
  CHECK(it != g_windows_.end(), base::NotFatalUntil::M130);
  g_windows_.erase(it);
}

}  // namespace gpu
