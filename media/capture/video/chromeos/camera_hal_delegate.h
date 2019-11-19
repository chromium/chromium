// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_HAL_DELEGATE_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_HAL_DELEGATE_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "media/capture/video/chromeos/mojom/camera3.mojom.h"
#include "media/capture/video/chromeos/mojom/camera_common.mojom.h"
#include "media/capture/video/chromeos/vendor_tag_ops_delegate.h"
#include "media/capture/video/video_capture_device_factory.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

class CameraAppDeviceBridgeImpl;
class CameraBufferFactory;

// CameraHalDelegate is the component which does Mojo IPCs to the camera HAL
// process on Chrome OS to access the module-level camera functionalities such
// as camera device info look-up and opening camera devices.
//
// CameraHalDelegate is refcounted because VideoCaptureDeviceFactoryChromeOS and
// CameraDeviceDelegate both need to reference CameraHalDelegate, and
// VideoCaptureDeviceFactoryChromeOS may be destroyed while CameraDeviceDelegate
// is still alive.
class CAPTURE_EXPORT CameraHalDelegate final
    : public base::RefCountedThreadSafe<CameraHalDelegate>,
      public cros::mojom::CameraModuleCallbacks {
 public:
  // All the Mojo IPC operations happen on |ipc_task_runner|.
  explicit CameraHalDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner);

  // Registers the camera client observer to the CameraHalDispatcher instance.
  void RegisterCameraClient();

  void SetCameraModule(
      mojo::PendingRemote<cros::mojom::CameraModule> camera_module);

  // Resets various mojo bindings, WaitableEvents, and cached information.
  void Reset();

  // Delegation methods for the VideoCaptureDeviceFactory interface.  These
  // methods are called by VideoCaptureDeviceFactoryChromeOS directly.  They
  // operate on the same thread that the VideoCaptureDeviceFactoryChromeOS runs
  // on.
  std::unique_ptr<VideoCaptureDevice> CreateDevice(
      scoped_refptr<base::SingleThreadTaskRunner>
          task_runner_for_screen_observer,
      const VideoCaptureDeviceDescriptor& device_descriptor,
      CameraAppDeviceBridgeImpl* app_device_bridge);
  void GetSupportedFormats(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      VideoCaptureFormats* supported_formats);
  void GetDeviceDescriptors(VideoCaptureDeviceDescriptors* device_descriptors);

  // Asynchronous method to open the camera device designated by |camera_id|.
  // This method may be called on any thread; |callback| will run on
  // |ipc_task_runner_|.
  using OpenDeviceCallback = base::OnceCallback<void(int32_t)>;
  void OpenDevice(
      int32_t camera_id,
      mojo::PendingReceiver<cros::mojom::Camera3DeviceOps> device_ops_receiver,
      OpenDeviceCallback callback);

  // Gets camera id from device id. Returns -1 on error.
  int GetCameraIdFromDeviceId(const std::string& device_id);

  // Gets the camera info of |device_id|. Returns null CameraInfoPtr on error.
  cros::mojom::CameraInfoPtr GetCameraInfoFromDeviceId(
      const std::string& device_id);

 private:
  friend class base::RefCountedThreadSafe<CameraHalDelegate>;

  ~CameraHalDelegate() final;

  void SetCameraModuleOnIpcThread(
      mojo::PendingRemote<cros::mojom::CameraModule> camera_module);

  // Resets the Mojo interface and bindings.
  void ResetMojoInterfaceOnIpcThread();

  // Internal method to update the camera info for all built-in cameras. Runs on
  // the same thread as CreateDevice, GetSupportedFormats, and
  // GetDeviceDescriptors.
  bool UpdateBuiltInCameraInfo();
  void UpdateBuiltInCameraInfoOnIpcThread();

  // Callback for GetNumberOfCameras Mojo IPC function.  GetNumberOfCameras
  // returns the number of built-in cameras on the device.
  void OnGotNumberOfCamerasOnIpcThread(int32_t num_cameras);

  // Callback for SetCallbacks Mojo IPC function. SetCallbacks is called after
  // GetNumberOfCameras is called for the first time, and before any other calls
  // to |camera_module_|.
  void OnSetCallbacksOnIpcThread(int32_t result);

  // Callback for GetVendorTagOps Mojo IPC function, which will initialize the
  // |vendor_tag_ops_delegate_|.
  void OnGotVendorTagOpsOnIpcThread();

  using GetCameraInfoCallback =
      base::OnceCallback<void(int32_t, cros::mojom::CameraInfoPtr)>;
  void GetCameraInfoOnIpcThread(int32_t camera_id,
                                GetCameraInfoCallback callback);
  void OnGotCameraInfoOnIpcThread(int32_t camera_id,
                                  int32_t result,
                                  cros::mojom::CameraInfoPtr camera_info);

  // Called by OpenDevice to actually open the device specified by |camera_id|.
  // This method runs on |ipc_task_runner_|.
  void OpenDeviceOnIpcThread(
      int32_t camera_id,
      mojo::PendingReceiver<cros::mojom::Camera3DeviceOps> device_ops_receiver,
      OpenDeviceCallback callback);

  // CameraModuleCallbacks implementation. Operates on |ipc_task_runner_|.
  void CameraDeviceStatusChange(
      int32_t camera_id,
      cros::mojom::CameraDeviceStatus new_status) final;
  void TorchModeStatusChange(int32_t camera_id,
                             cros::mojom::TorchModeStatus new_status) final;

  base::WaitableEvent camera_module_has_been_set_;

  // Signaled when |num_builtin_cameras_| and |camera_info_| are updated.
  // Queried and waited by UpdateBuiltInCameraInfo, signaled by
  // OnGotCameraInfoOnIpcThread.
  base::WaitableEvent builtin_camera_info_updated_;

  // Signaled/Reset when |pending_external_camera_info_.empty()| is changed.
  base::WaitableEvent external_camera_info_updated_;
  std::unordered_set<int> pending_external_camera_info_;

  // Signaled/Reset when |camera_info_.empty()| is changed.
  base::WaitableEvent has_camera_connected_;

  // |num_builtin_cameras_| stores the number of built-in camera devices
  // reported by the camera HAL, and |camera_info_| stores the camera info of
  // each camera device. They are modified only on |ipc_task_runner_|. They
  // are also read in GetSupportedFormats and GetDeviceDescriptors, in which the
  // access is protected by |camera_info_lock_| and sequenced through
  // UpdateBuiltInCameraInfo and |builtin_camera_info_updated_| to avoid race
  // conditions. For external cameras, the |camera_info_| would be read nad
  // updated in CameraDeviceStatusChange, which is also protected by
  // |camera_info_lock_|.
  size_t num_builtin_cameras_;
  base::Lock camera_info_lock_;
  std::unordered_map<int, cros::mojom::CameraInfoPtr> camera_info_;

  // A map from |VideoCaptureDeviceDescriptor.device_id| to camera id, which is
  // updated in GetDeviceDescriptors() and queried in
  // GetCameraIdFromDeviceId().
  base::Lock device_id_to_camera_id_lock_;
  std::map<std::string, int> device_id_to_camera_id_;

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<CameraBufferFactory> camera_buffer_factory_;

  // The task runner where all the camera module Mojo communication takes place.
  const scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

  // The Mojo proxy to access the camera module at the remote camera HAL.  Bound
  // to |ipc_task_runner_|.
  mojo::Remote<cros::mojom::CameraModule> camera_module_;

  // The Mojo receiver serving the camera module callbacks.  Bound to
  // |ipc_task_runner_|.
  mojo::Receiver<cros::mojom::CameraModuleCallbacks> camera_module_callbacks_;

  // An internal delegate to handle VendorTagOps mojo connection and query
  // information of vendor tags.  Bound to |ipc_task_runner_|.
  VendorTagOpsDelegate vendor_tag_ops_delegate_;

  DISALLOW_COPY_AND_ASSIGN(CameraHalDelegate);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_HAL_DELEGATE_H_
