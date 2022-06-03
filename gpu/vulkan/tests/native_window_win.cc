// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/tests/native_window.h"

#include <windows.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "ui/gfx/win/window_impl.h"

namespace gpu {

class Window;

base::flat_map<gfx::AcceleratedWidget, std::unique_ptr<Window>> g_windows_;

class Window : public gfx::WindowImpl {
 public:
  Window() { set_window_style(WS_VISIBLE | WS_POPUP); }
  ~Window() override = default;

 private:
  // Overridden from gfx::WindowImpl:
  BOOL ProcessWindowMessage(HWND window,
                            UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT& result,
                            DWORD msg_map_id) override {
    return false;  // Results in DefWindowProc().
  }
};

gfx::AcceleratedWidget CreateNativeWindow(const gfx::Rect& bounds) {
  auto window = std::make_unique<Window>();
  window->Init(/*parent=*/nullptr, bounds);
  gfx::AcceleratedWidget widget = window->hwnd();
  g_windows_[widget] = std::move(window);
  return widget;
}

void DestroyNativeWindow(gfx::AcceleratedWidget window) {
  auto it = g_windows_.find(window);
  DCHECK(it != g_windows_.end());

  it->second.reset();
  g_windows_.erase(it);
}

}  // namespace gpu
