// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/shader_cache_factory.h"

#include "gpu/ipc/host/shader_disk_cache.h"

namespace content {

namespace {

gpu::ShaderCacheFactory* factory_instance = nullptr;

void CreateFactoryInstance() {
  DCHECK(!factory_instance);
  factory_instance = new gpu::ShaderCacheFactory();
}

}  // namespace

void InitShaderCacheFactorySingleton() {
  CreateFactoryInstance();
}

gpu::ShaderCacheFactory* GetShaderCacheFactorySingleton() {
  DCHECK(!factory_instance || factory_instance->CalledOnValidThread());
  return factory_instance;
}

}  // namespace content
