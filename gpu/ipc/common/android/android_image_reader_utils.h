// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_ANDROID_ANDROID_IMAGE_READER_UTILS_H_
#define GPU_IPC_COMMON_ANDROID_ANDROID_IMAGE_READER_UTILS_H_

#include "base/files/scoped_file.h"
#include "gpu/gpu_export.h"

namespace gpu {

// Create and inserts an egl fence and exports a ScopedFD from it.
GPU_EXPORT base::ScopedFD CreateEglFenceAndExportFd();

// Create and insert an EGL fence and imports the provided fence fd.
GPU_EXPORT bool InsertEglFenceAndWait(base::ScopedFD acquire_fence_fd);

}  // namespace gpu

#endif  // GPU_IPC_COMMON_ANDROID_ANDROID_IMAGE_READER_UTILS_H_
