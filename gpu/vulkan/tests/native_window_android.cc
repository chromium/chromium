// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/tests/native_window.h"

#include "ui/gl/android/scoped_a_native_window.h"
#include "ui/gl/android/surface_texture.h"

namespace gpu {

scoped_refptr<gl::SurfaceTexture> g_surface_texture;
gl::ScopedANativeWindow g_widget;

gfx::AcceleratedWidget CreateNativeWindow(const gfx::Rect& bounds) {
  DCHECK(!g_surface_texture);
  DCHECK(g_widget.a_native_window() == gfx::kNullAcceleratedWidget);
  // TODO(penghuang): Not depend on gl.
  uint texture = 0;
  g_surface_texture = gl::SurfaceTexture::Create(texture);
  g_widget = g_surface_texture->CreateSurface();
  return g_widget.a_native_window();
}

void DestroyNativeWindow(gfx::AcceleratedWidget window) {
  DCHECK_EQ(g_widget.a_native_window(), window);
  g_widget = nullptr;
  g_surface_texture = nullptr;
}

}  // namespace gpu
