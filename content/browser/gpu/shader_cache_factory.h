// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_SHADER_CACHE_FACTORY_H_
#define CONTENT_BROWSER_GPU_SHADER_CACHE_FACTORY_H_

#include "content/common/content_export.h"
#include "gpu/ipc/host/shader_disk_cache.h"

namespace content {

// Initializes the ShaderCacheFactory singleton instance.
CONTENT_EXPORT void InitShaderCacheFactorySingleton();

// Returns an instance previously created by InitShaderCacheFactorySingleton().
// This can return nullptr if an instance has not yet been created.
CONTENT_EXPORT gpu::ShaderCacheFactory* GetShaderCacheFactorySingleton();

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_SHADER_CACHE_FACTORY_H_
