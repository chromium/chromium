// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_GPU_CHANNEL_HOST_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_GPU_CHANNEL_HOST_H_

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "media/capture/capture_export.h"

namespace media {

class VideoCaptureGpuContextLostObserver {
 public:
  virtual void OnContextLost() = 0;

 protected:
  virtual ~VideoCaptureGpuContextLostObserver() = default;
};

// GPU memory buffer manager for Linux Video Capture.
// This class provides the access to `gpu::GpuMemoryBufferManager` for the
// `V4L2GpuMemoryBufferTracker`. It listens the GPU context lost event and
// broadcast it to trackers.
class CAPTURE_EXPORT VideoCaptureGpuChannelHost final
    : public VideoCaptureGpuContextLostObserver {
 public:
  static VideoCaptureGpuChannelHost& GetInstance();

  VideoCaptureGpuChannelHost(const VideoCaptureGpuChannelHost&) = delete;
  VideoCaptureGpuChannelHost& operator=(const VideoCaptureGpuChannelHost&) =
      delete;

  // Set gpu::GpuMemoryBufferManager by
  // `VideoCaptureServiceImpl::VizGpuContextProvider` from the main thead of
  // utility process. It will be set with
  // `viz::Gpu::GetGpuMemoryBufferManager()` when calling
  // `VideoCaptureServiceImpl::VizGpuContextProvider::StartContextProviderIfNeeded()`
  // success or set to nullptr if failed.
  void SetGpuMemoryBufferManager(gpu::GpuMemoryBufferManager*);

  // This method is called by `V4L2GpuMemoryBufferTracker::Init()` from the
  // single thread task runner created in the
  // `VideoCaptureDeviceLinux::VideoCaptureDeviceLinux()`. It will be called
  // when VideoCaptureBufferPoolImpl want to create new tracker for the v4l2
  // camera capture data. It will return nullptr when
  // `VideoCaptureServiceImpl::VizGpuContextProvider::StartContextProviderIfNeeded()`
  // failed.
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager();

  // Set `gpu::SharedImageInterface` by
  // `VideoCaptureServiceImpl::VizGpuContextProvider` from the main thead of
  // utility process. It will be set with
  // `gpu::GpuChannelHost::CreateClientSharedImageInterface()` when calling
  // `VideoCaptureServiceImpl::VizGpuContextProvider::StartContextProviderIfNeeded()`
  // success or set to nullptr if failed.
  void SetSharedImageInterface(scoped_refptr<gpu::SharedImageInterface>);

  // It will return nullptr when
  // `VideoCaptureServiceImpl::VizGpuContextProvider::StartContextProviderIfNeeded()`
  // failed.
  gpu::SharedImageInterface* SharedImageInterface();

  // VideoCaptureGpuContextLostObserver implementation.
  void OnContextLost() override;

  void AddObserver(VideoCaptureGpuContextLostObserver*);
  void RemoveObserver(VideoCaptureGpuContextLostObserver*);

 private:
  friend class base::NoDestructor<VideoCaptureGpuChannelHost>;

  VideoCaptureGpuChannelHost();
  ~VideoCaptureGpuChannelHost() override;

  mutable base::Lock lock_;
  // The |gpu_buffer_manager_| is nullptr before set by the
  // `VideoCaptureServiceImpl::VizGpuContextProvider::StartContextProviderIfNeeded()`
  // which is called with the memory buffer manager that viz::Gpu owns.
  raw_ptr<gpu::GpuMemoryBufferManager> gpu_buffer_manager_ GUARDED_BY(lock_);

  // Protects observer list. The observer list will be operated from the
  // |v4l2_task_runner| of V4L2CaptureDelegate and the |main_task_runner_| of
  // VideoCaptureServiceImpl::VizGpuContextProvider.
  base::ObserverList<VideoCaptureGpuContextLostObserver>::Unchecked observers_
      GUARDED_BY(lock_);

  // The |shared_image_interface_| is nullptr before set by the
  // `VideoCaptureServiceImpl::VizGpuContextProvider::StartContextProviderIfNeeded()`
  // It is created by Gpu Channel Host that viz::Gpu owns.
  scoped_refptr<gpu::SharedImageInterface> shared_image_interface_
      GUARDED_BY(lock_);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_GPU_CHANNEL_HOST_H_
