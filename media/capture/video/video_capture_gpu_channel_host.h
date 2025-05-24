// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_GPU_CHANNEL_HOST_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_GPU_CHANNEL_HOST_H_

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "media/capture/capture_export.h"

namespace media {

class VideoCaptureGpuContextLostObserver : public base::CheckedObserver {
 public:
  virtual void OnContextLost() = 0;

 protected:
  ~VideoCaptureGpuContextLostObserver() override = default;
};

// This class provides the access to `gpu::SharedImageInterface` for the
// `V4L2GpuMemoryBufferTracker`. It listens the GPU context lost event and
// broadcast it to trackers.
class CAPTURE_EXPORT VideoCaptureGpuChannelHost final
    : public VideoCaptureGpuContextLostObserver {
 public:
  static VideoCaptureGpuChannelHost& GetInstance();

  VideoCaptureGpuChannelHost(const VideoCaptureGpuChannelHost&) = delete;
  VideoCaptureGpuChannelHost& operator=(const VideoCaptureGpuChannelHost&) =
      delete;

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
  scoped_refptr<gpu::SharedImageInterface> GetSharedImageInterface();

  // VideoCaptureGpuContextLostObserver implementation.
  void OnContextLost() override;

  void AddObserver(VideoCaptureGpuContextLostObserver*);
  void RemoveObserver(VideoCaptureGpuContextLostObserver*);

 private:
  friend class base::NoDestructor<VideoCaptureGpuChannelHost>;

  VideoCaptureGpuChannelHost();
  ~VideoCaptureGpuChannelHost() override;

  mutable base::Lock lock_;

  // Protects observer list. The observer list will be operated from the
  // |v4l2_task_runner| of V4L2CaptureDelegate and the |main_task_runner_| of
  // VideoCaptureServiceImpl::VizGpuContextProvider.
  base::ObserverList<VideoCaptureGpuContextLostObserver> observers_
      GUARDED_BY(lock_);

  // The |shared_image_interface_| is nullptr before set by the
  // `VideoCaptureServiceImpl::VizGpuContextProvider::StartContextProviderIfNeeded()`
  // It is created by Gpu Channel Host that viz::Gpu owns.
  scoped_refptr<gpu::SharedImageInterface> shared_image_interface_
      GUARDED_BY(lock_);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_GPU_CHANNEL_HOST_H_
