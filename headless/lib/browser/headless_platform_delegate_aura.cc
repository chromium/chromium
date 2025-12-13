// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_platform_delegate.h"

#include <memory>

#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "headless/lib/browser/headless_screen.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/lib/browser/headless_window_tree_host.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"

namespace headless {

void HeadlessPlatformDelegate::Initialize(
    const HeadlessBrowser::Options& options) {
  HeadlessScreen* screen =
      HeadlessScreen::Create(options.window_size, options.screen_info_spec);
  display::Screen::SetScreenInstance(screen);
}

void HeadlessPlatformDelegate::Start() {
  DCHECK(aura::Env::GetInstance());
  ui::DeviceDataManager::CreateInstance();
}

void HeadlessPlatformDelegate::InitializeWebContents(
    HeadlessWebContentsImpl* web_contents) {
  auto window_tree_host = std::make_unique<HeadlessWindowTreeHost>(
      web_contents->begin_frame_control_enabled());
  window_tree_host->InitHost();
  gfx::NativeWindow parent_window = window_tree_host->window();
  parent_window->Show();
  window_tree_host->SetParentWindow(parent_window);
  web_contents->set_window_tree_host(std::move(window_tree_host));

  gfx::NativeView native_view = web_contents->web_contents()->GetNativeView();
  DCHECK(!parent_window->Contains(native_view));
  parent_window->AddChild(native_view);
  native_view->Show();
}

void HeadlessPlatformDelegate::SetWebContentsBounds(
    HeadlessWebContentsImpl* web_contents,
    const gfx::Rect& bounds) {
  // Aura windows hierarchy in headless shell:
  //   RootWindow
  //     WebContentsViewAura
  //       RenderWidgetHostViewAura

  // Update WebContentsViewAura. This also updates RenderWidgetHostViewAura.
  if (gfx::NativeView native_view =
          web_contents->web_contents()->GetNativeView()) {
    native_view->SetBounds(bounds);
  }

  // Update RootWindow.
  web_contents->window_tree_host()->SetBoundsInPixels(bounds);
}

ui::Compositor* HeadlessPlatformDelegate::GetCompositor(
    HeadlessWebContentsImpl* web_contents) {
  return web_contents->window_tree_host()->compositor();
}

}  // namespace headless
