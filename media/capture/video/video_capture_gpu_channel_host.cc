// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_gpu_channel_host.h"

namespace media {

VideoCaptureGpuChannelHost::VideoCaptureGpuChannelHost()
    : gpu_buffer_manager_(nullptr) {}

VideoCaptureGpuChannelHost::~VideoCaptureGpuChannelHost() = default;

// static
VideoCaptureGpuChannelHost& VideoCaptureGpuChannelHost::GetInstance() {
  static base::NoDestructor<VideoCaptureGpuChannelHost> instance;
  return *instance;
}

void VideoCaptureGpuChannelHost::SetGpuMemoryBufferManager(
    gpu::GpuMemoryBufferManager* gbm) {
  base::AutoLock lock(lock_);
  gpu_buffer_manager_ = gbm;
}

gpu::GpuMemoryBufferManager*
VideoCaptureGpuChannelHost::GetGpuMemoryBufferManager() {
  base::AutoLock lock(lock_);
  return gpu_buffer_manager_;
}

void VideoCaptureGpuChannelHost::SetSharedImageInterface(
    scoped_refptr<gpu::SharedImageInterface> shared_image_interface) {
  base::AutoLock lock(lock_);
  shared_image_interface_ = std::move(shared_image_interface);
}

gpu::SharedImageInterface* VideoCaptureGpuChannelHost::SharedImageInterface() {
  base::AutoLock lock(lock_);
  return shared_image_interface_.get();
}

void VideoCaptureGpuChannelHost::OnContextLost() {
  base::AutoLock lock(lock_);
  for (auto& observer : observers_) {
    observer.OnContextLost();
  }
}

void VideoCaptureGpuChannelHost::AddObserver(
    VideoCaptureGpuContextLostObserver* observer) {
  base::AutoLock lock(lock_);
  if (observers_.HasObserver(observer)) {
    return;
  }

  observers_.AddObserver(observer);
}

void VideoCaptureGpuChannelHost::RemoveObserver(
    VideoCaptureGpuContextLostObserver* to_remove) {
  base::AutoLock lock(lock_);
  observers_.RemoveObserver(to_remove);
}

}  // namespace media
