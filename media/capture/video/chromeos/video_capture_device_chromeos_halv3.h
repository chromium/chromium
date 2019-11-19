// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_CHROMEOS_HALV3_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_CHROMEOS_HALV3_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "media/capture/video/chromeos/display_rotation_observer.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video_capture_types.h"

namespace display {

class Display;

}  // namespace display

namespace media {

class CameraAppDeviceImpl;
class CameraHalDelegate;
class CameraDeviceContext;
class CameraDeviceDelegate;

// Implementation of VideoCaptureDevice for ChromeOS with CrOS camera HALv3.
class CAPTURE_EXPORT VideoCaptureDeviceChromeOSHalv3 final
    : public VideoCaptureDevice,
      public DisplayRotationObserver {
 public:
  VideoCaptureDeviceChromeOSHalv3(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      const VideoCaptureDeviceDescriptor& device_descriptor,
      scoped_refptr<CameraHalDelegate> camera_hal_delegate,
      CameraAppDeviceImpl* camera_app_device,
      base::OnceClosure cleanup_callback);

  ~VideoCaptureDeviceChromeOSHalv3() final;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<Client> client) final;
  void StopAndDeAllocate() final;
  void TakePhoto(TakePhotoCallback callback) final;
  void GetPhotoState(GetPhotoStateCallback callback) final;
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) final;

 private:
  // Helper to interact with PowerManagerClient on DBus original thread.
  class PowerManagerClientProxy;

  void OpenDevice();
  void CloseDevice(base::UnguessableToken unblock_suspend_token);

  // DisplayRotationDelegate implementation.
  void SetDisplayRotation(const display::Display& display) final;
  void SetRotation(int rotation);

  const VideoCaptureDeviceDescriptor device_descriptor_;

  // A reference to the CameraHalDelegate instance in the VCD factory.  This is
  // used by AllocateAndStart to query camera info and create the camera device.
  const scoped_refptr<CameraHalDelegate> camera_hal_delegate_;

  // A reference to the thread that all the VideoCaptureDevice interface methods
  // are expected to be called on.
  const scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;

  // The thread that all the Mojo operations of |camera_device_delegate_| take
  // place.  Started in AllocateAndStart and stopped in StopAndDeAllocate, where
  // the access to the base::Thread methods are sequenced on
  // |capture_task_runner_|.
  base::Thread camera_device_ipc_thread_;

  VideoCaptureParams capture_params_;
  // |device_context_| is created and owned by VideoCaptureDeviceChromeOSHalv3
  // and is only accessed by |camera_device_delegate_|.
  std::unique_ptr<CameraDeviceContext> device_context_;

  // Internal delegate doing the actual capture setting, buffer allocation and
  // circulation with the camera HAL. Created in AllocateAndStart and deleted in
  // StopAndDeAllocate on |capture_task_runner_|.  All methods of
  // |camera_device_delegate_| operate on |camera_device_ipc_thread_|.
  std::unique_ptr<CameraDeviceDelegate> camera_device_delegate_;

  scoped_refptr<ScreenObserverDelegate> screen_observer_delegate_;
  const VideoFacingMode lens_facing_;
  // Whether the incoming frames should rotate when the device rotates.
  const bool rotates_with_device_;
  int rotation_;

  CameraAppDeviceImpl* camera_app_device_;  // Weak.

  base::OnceClosure cleanup_callback_;

  scoped_refptr<PowerManagerClientProxy> power_manager_client_proxy_;

  base::WeakPtrFactory<VideoCaptureDeviceChromeOSHalv3> weak_ptr_factory_{this};

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceChromeOSHalv3);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_CHROMEOS_HALV3_H_
