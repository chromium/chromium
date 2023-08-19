// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_disk_cache_factory.h"

#include "base/command_line.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/ipc/host/gpu_disk_cache.h"

namespace content {

namespace {

gpu::GpuDiskCacheFactory* factory_instance = nullptr;

void CreateFactoryInstance() {
  DCHECK(!factory_instance);
  // Setup static reserved handles and their mapping to specific paths.
  gpu::GpuDiskCacheFactory::HandleToPathMap handle_to_path_map;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuShaderDiskCache)) {
    base::FilePath compositor_cache_dir =
        GetContentClient()->browser()->GetShaderDiskCacheDirectory();
    if (!compositor_cache_dir.empty()) {
      handle_to_path_map.emplace(gpu::kDisplayCompositorGpuDiskCacheHandle,
                                 compositor_cache_dir);
    }
    base::FilePath gr_cache_dir =
        GetContentClient()->browser()->GetGrShaderDiskCacheDirectory();
    if (!gr_cache_dir.empty()) {
      handle_to_path_map.emplace(gpu::kGrShaderGpuDiskCacheHandle,
                                 gr_cache_dir);
    }
    base::FilePath graphite_dawn_dir =
        GetContentClient()->browser()->GetGraphiteDawnDiskCacheDirectory();
    if (!graphite_dawn_dir.empty()) {
      handle_to_path_map.emplace(gpu::kGraphiteDawnGpuDiskCacheHandle,
                                 graphite_dawn_dir);
    }
  }

  factory_instance = new gpu::GpuDiskCacheFactory(handle_to_path_map);
}

}  // namespace

void InitGpuDiskCacheFactorySingleton() {
  CreateFactoryInstance();
}

gpu::GpuDiskCacheFactory* GetGpuDiskCacheFactorySingleton() {
  return factory_instance;
}

}  // namespace content
