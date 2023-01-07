// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_TESTS_NATIVE_WINDOW_H_
#define GPU_VULKAN_TESTS_NATIVE_WINDOW_H_

#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace gpu {

gfx::AcceleratedWidget CreateNativeWindow(const gfx::Rect& bounds);
void DestroyNativeWindow(gfx::AcceleratedWidget window);

}  // namespace gpu

#endif  // GPU_VULKAN_TESTS_NATIVE_WINDOW_H_
