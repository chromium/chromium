// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_disk_cache_factory.h"

#include "gpu/ipc/host/gpu_disk_cache.h"

namespace content {

namespace {

gpu::GpuDiskCacheFactory* factory_instance = nullptr;

void CreateFactoryInstance() {
  DCHECK(!factory_instance);
  factory_instance = new gpu::GpuDiskCacheFactory();
}

}  // namespace

void InitGpuDiskCacheFactorySingleton() {
  CreateFactoryInstance();
}

gpu::GpuDiskCacheFactory* GetGpuDiskCacheFactorySingleton() {
  return factory_instance;
}

}  // namespace content
