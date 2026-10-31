// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/tests/native_window.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gl/android/scoped_a_native_window.h"
#include "ui/gl/android/surface_texture.h"

namespace gpu {
namespace {

struct WindowState {
  scoped_refptr<gl::SurfaceTexture> surface_texture;
  gl::ScopedANativeWindow widget;
};

WindowState* g_window_state = nullptr;

}  // namespace

gfx::AcceleratedWidget CreateNativeWindow(const gfx::Rect& bounds) {
  DCHECK(!g_window_state);
  // TODO(penghuang): Not depend on gl.
  uint texture = 0;
  auto surface_texture = gl::SurfaceTexture::Create(texture);
  auto widget = surface_texture->CreateSurface();
  g_window_state =
      new WindowState{std::move(surface_texture), std::move(widget)};
  return g_window_state->widget.a_native_window();
}

void DestroyNativeWindow(gfx::AcceleratedWidget window) {
  CHECK(g_window_state);
  DCHECK_EQ(g_window_state->widget.a_native_window(), window);
  delete g_window_state;
  g_window_state = nullptr;
}

}  // namespace gpu
