// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_DISK_CACHE_FACTORY_H_
#define CONTENT_BROWSER_GPU_GPU_DISK_CACHE_FACTORY_H_

#include "content/common/content_export.h"
#include "gpu/ipc/host/gpu_disk_cache.h"

namespace content {

// Initializes the GpuDiskCacheFactory singleton instance.
CONTENT_EXPORT void InitGpuDiskCacheFactorySingleton();

// Returns an instance previously created by InitGpuDiskCacheFactorySingleton().
// This can return nullptr if an instance has not yet been created.
CONTENT_EXPORT gpu::GpuDiskCacheFactory* GetGpuDiskCacheFactorySingleton();

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_DISK_CACHE_FACTORY_H_
