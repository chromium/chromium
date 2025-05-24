// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"

namespace gpu {

GpuMemoryBufferManager::GpuMemoryBufferManager() = default;

GpuMemoryBufferManager::~GpuMemoryBufferManager() = default;

void GpuMemoryBufferManager::AddObserver(
    GpuMemoryBufferManagerObserver* observer) {}

void GpuMemoryBufferManager::RemoveObserver(
    GpuMemoryBufferManagerObserver* observer) {}

void GpuMemoryBufferManager::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnGpuMemoryBufferManagerDestroyed();
  }
}

}  // namespace gpu
