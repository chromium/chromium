// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/tests/native_window.h"

#include <android/native_window_jni.h>

#include "ui/gl/android/surface_texture.h"

namespace gpu {

scoped_refptr<gl::SurfaceTexture> g_surface_texture;
gfx::AcceleratedWidget g_widget = gfx::kNullAcceleratedWidget;

gfx::AcceleratedWidget CreateNativeWindow(const gfx::Rect& bounds) {
  DCHECK(!g_surface_texture);
  DCHECK(g_widget == gfx::kNullAcceleratedWidget);
  // TODO(penghuang): Not depend on gl.
  uint texture = 0;
  g_surface_texture = gl::SurfaceTexture::Create(texture);
  g_widget = g_surface_texture->CreateSurface();
  return g_widget;
}

void DestroyNativeWindow(gfx::AcceleratedWidget window) {
  DCHECK_EQ(g_widget, window);
  ANativeWindow_release(window);
  g_widget = gfx::kNullAcceleratedWidget;
  g_surface_texture = nullptr;
}

}  // namespace gpu
