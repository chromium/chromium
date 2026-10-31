// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/tests/native_window.h"

#include <windows.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "ui/gfx/win/window_impl.h"

namespace gpu {
namespace {

class Window;
using WindowMap =
    base::flat_map<gfx::AcceleratedWidget, std::unique_ptr<Window>>;
WindowMap* g_windows = nullptr;

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

}  // namespace

gfx::AcceleratedWidget CreateNativeWindow(const gfx::Rect& bounds) {
  auto window = std::make_unique<Window>();
  window->Init(/*parent=*/nullptr, bounds);
  gfx::AcceleratedWidget widget = window->hwnd();
  if (!g_windows) {
    g_windows = new WindowMap();
  }
  (*g_windows)[widget] = std::move(window);
  return widget;
}

void DestroyNativeWindow(gfx::AcceleratedWidget window) {
  CHECK(g_windows);
  auto it = g_windows->find(window);
  CHECK(it != g_windows->end());

  it->second.reset();
  g_windows->erase(it);
  if (g_windows->empty()) {
    delete g_windows;
    g_windows = nullptr;
  }
}

}  // namespace gpu
