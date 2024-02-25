// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_ANDROID_ANDROID_HARDWARE_BUFFER_UTILS_H_
#define GPU_IPC_COMMON_ANDROID_ANDROID_HARDWARE_BUFFER_UTILS_H_

#include "base/android/scoped_hardware_buffer_handle.h"
#include "gpu/gpu_export.h"
#include "ui/gfx/buffer_types.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace gpu {

GPU_EXPORT base::android::ScopedHardwareBufferHandle
CreateScopedHardwareBufferHandle(const gfx::Size& size,
                                 gfx::BufferFormat format,
                                 gfx::BufferUsage usage);

}  // namespace gpu

#endif  // GPU_IPC_COMMON_ANDROID_ANDROID_HARDWARE_BUFFER_UTILS_H_
