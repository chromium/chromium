// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/video_capture_gpu_memory_buffer_manager.h"

namespace media {

VideoCaptureGpuMemoryBufferManager::VideoCaptureGpuMemoryBufferManager()
    : gpu_buffer_manager_(nullptr) {}

VideoCaptureGpuMemoryBufferManager::~VideoCaptureGpuMemoryBufferManager() =
    default;

// static
VideoCaptureGpuMemoryBufferManager&
VideoCaptureGpuMemoryBufferManager::GetInstance() {
  static base::NoDestructor<VideoCaptureGpuMemoryBufferManager> instance;
  return *instance;
}

void VideoCaptureGpuMemoryBufferManager::SetGpuMemoryBufferManager(
    gpu::GpuMemoryBufferManager* gbm) {
  base::AutoLock lock(lock_);
  gpu_buffer_manager_ = gbm;
}

gpu::GpuMemoryBufferManager*
VideoCaptureGpuMemoryBufferManager::GetGpuMemoryBufferManager() {
  base::AutoLock lock(lock_);
  return gpu_buffer_manager_;
}

void VideoCaptureGpuMemoryBufferManager::OnContextLost() {
  base::AutoLock lock(lock_);
  for (auto& observer : observers_) {
    observer.OnContextLost();
  }
}

void VideoCaptureGpuMemoryBufferManager::AddObserver(
    VideoCaptureGpuContextLostObserver* observer) {
  base::AutoLock lock(lock_);
  if (observers_.HasObserver(observer)) {
    return;
  }

  observers_.AddObserver(observer);
}

void VideoCaptureGpuMemoryBufferManager::RemoveObserver(
    VideoCaptureGpuContextLostObserver* to_remove) {
  base::AutoLock lock(lock_);
  observers_.RemoveObserver(to_remove);
}

}  // namespace media
