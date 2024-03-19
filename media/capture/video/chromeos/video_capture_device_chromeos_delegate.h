// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_CHROMEOS_DELEGATE_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_CHROMEOS_DELEGATE_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/display_rotation_observer.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video_capture_types.h"

namespace media {

class CameraHalDelegate;
class CameraDeviceDelegate;

// Implementation of delegate for ChromeOS with CrOS camera HALv3.
class CAPTURE_EXPORT VideoCaptureDeviceChromeOSDelegate {
 public:
  VideoCaptureDeviceChromeOSDelegate() = delete;

  VideoCaptureDeviceChromeOSDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      const VideoCaptureDeviceDescriptor& device_descriptor,
      CameraHalDelegate* camera_hal_delegate,
      base::OnceClosure cleanup_callback);

  VideoCaptureDeviceChromeOSDelegate(
      const VideoCaptureDeviceChromeOSDelegate&) = delete;
  VideoCaptureDeviceChromeOSDelegate& operator=(
      const VideoCaptureDeviceChromeOSDelegate&) = delete;

  ~VideoCaptureDeviceChromeOSDelegate();

  void Shutdown();
  bool HasDeviceClient();

  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<VideoCaptureDevice::Client> client,
                        ClientType client_type);
  void StopAndDeAllocate(ClientType client_type);
  void TakePhoto(VideoCaptureDevice::TakePhotoCallback callback);
  void GetPhotoState(VideoCaptureDevice::GetPhotoStateCallback callback);
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       VideoCaptureDevice::SetPhotoOptionsCallback callback);

 private:
  class PowerObserver;

  void OpenDevice();
  void ReconfigureStreams();
  void CloseDevice(base::OnceClosure suspend_callback);

  void SetRotation(int rotation);

  const VideoCaptureDeviceDescriptor device_descriptor_;

  // A reference to the CameraHalDelegate instance in the VCD factory.  This is
  // used by AllocateAndStart to query camera info and create the camera device.
  raw_ptr<CameraHalDelegate> camera_hal_delegate_;

  // A reference to the thread that all the VideoCaptureDevice interface methods
  // are expected to be called on.
  const scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;

  // The thread that all the Mojo operations of |camera_device_delegate_| take
  // place.  Started in AllocateAndStart and stopped in StopAndDeAllocate, where
  // the access to the base::Thread methods are sequenced on
  // |capture_task_runner_|.
  base::Thread camera_device_ipc_thread_;

  // Map client type to VideoCaptureParams.
  base::flat_map<ClientType, VideoCaptureParams> capture_params_;

  // |device_context_| is created and owned by
  // VideoCaptureDeviceChromeOSDelegate and is only accessed by
  // |camera_device_delegate_|.
  std::unique_ptr<CameraDeviceContext> device_context_;

  // Internal delegate doing the actual capture setting, buffer allocation and
  // circulation with the camera HAL. Created in AllocateAndStart and deleted in
  // StopAndDeAllocate on |capture_task_runner_|.  All methods of
  // |camera_device_delegate_| operate on |camera_device_ipc_thread_|.
  std::unique_ptr<CameraDeviceDelegate> camera_device_delegate_;

  const VideoFacingMode lens_facing_;
  // Whether the incoming frames should rotate when the device rotates.
  const bool rotates_with_device_;
  int rotation_;

  base::OnceClosure cleanup_callback_;

  base::WaitableEvent device_closed_;

  base::SequenceBound<PowerObserver> power_observer_;

  base::SequenceBound<ScreenObserverDelegate> screen_observer_delegate_;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  base::WeakPtrFactory<VideoCaptureDeviceChromeOSDelegate> weak_ptr_factory_{
      this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_CHROMEOS_DELEGATE_H_
