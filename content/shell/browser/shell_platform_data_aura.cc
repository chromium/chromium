// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_platform_data_aura.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/shell/browser/shell.h"
#include "ui/aura/client/cursor_shape_client.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/test/test_window_parenting_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/input_method.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/wm/core/cursor_loader.h"
#include "ui/wm/core/default_activation_client.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/aura/screen_ozone.h"
#endif

namespace content {

namespace {

class FillLayout : public aura::LayoutManager {
 public:
  explicit FillLayout(aura::Window* root)
      : root_(root), has_bounds_(!root->bounds().IsEmpty()) {}

  FillLayout(const FillLayout&) = delete;
  FillLayout& operator=(const FillLayout&) = delete;

  ~FillLayout() override {}

 private:
  // aura::LayoutManager:
  void OnWindowResized() override {
    // If window bounds were not set previously then resize all children to
    // match the size of the parent.
    if (!has_bounds_) {
      has_bounds_ = true;
      for (aura::Window* child : root_->children())
        SetChildBoundsDirect(child, gfx::Rect(root_->bounds().size()));
    }
  }

  void OnWindowAddedToLayout(aura::Window* child) override {
    child->SetBounds(root_->bounds());
  }

  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}

  void OnWindowRemovedFromLayout(aura::Window* child) override {}

  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}

  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {
    SetChildBoundsDirect(child, requested_bounds);
  }

  raw_ptr<aura::Window> root_;
  bool has_bounds_;
};

}

ShellPlatformDataAura::ShellPlatformDataAura(const gfx::Size& initial_size) {
  CHECK(aura::Env::GetInstance());

#if BUILDFLAG(IS_OZONE)
  // Setup global display::Screen singleton.
  if (!display::Screen::HasScreen()) {
    screen_ = std::make_unique<aura::ScreenOzone>();
  }
#endif  // BUILDFLAG(IS_OZONE)

  ui::PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(initial_size);

  host_ = aura::WindowTreeHost::Create(std::move(properties));
  host_->InitHost();
  host_->window()->Show();
  host_->window()->SetLayoutManager(
      std::make_unique<FillLayout>(host_->window()));

  focus_client_ =
      std::make_unique<aura::test::TestFocusClient>(host_->window());

  new wm::DefaultActivationClient(host_->window());
  capture_client_ =
      std::make_unique<aura::client::DefaultCaptureClient>(host_->window());
  window_parenting_client_ =
      std::make_unique<aura::test::TestWindowParentingClient>(host_->window());

  // TODO(https://crbug.com/1336055): this is needed for
  // mouse_cursor_overlay_controller_browsertest.cc on cast_shell_linux as
  // currently, when is_castos = true, the views toolkit isn't used.
  cursor_shape_client_ = std::make_unique<wm::CursorLoader>();
  aura::client::SetCursorShapeClient(cursor_shape_client_.get());
}

ShellPlatformDataAura::~ShellPlatformDataAura() {
  aura::client::SetCursorShapeClient(nullptr);
}

void ShellPlatformDataAura::ShowWindow() {
  host_->Show();
}

void ShellPlatformDataAura::ResizeWindow(const gfx::Size& size) {
  host_->SetBoundsInPixels(gfx::Rect(size));
}

}  // namespace content
