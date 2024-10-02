// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_HAL_DELEGATE_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_HAL_DELEGATE_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/mojo_service_manager_observer.h"
#include "media/capture/video/chromeos/mojom/camera3.mojom.h"
#include "media/capture/video/chromeos/mojom/camera_common.mojom.h"
#include "media/capture/video/chromeos/vendor_tag_ops_delegate.h"
#include "media/capture/video/video_capture_device_factory.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

class CameraBufferFactory;
class VideoCaptureDeviceChromeOSDelegate;

// CameraHalDelegate is the component which does Mojo IPCs to the camera HAL
// process on Chrome OS to access the module-level camera functionalities such
// as camera device info look-up and opening camera devices.
//
// CameraHalDelegate is owned by VideoCaptureDeviceFactoryChromeOS.
// VideoCaptureDeviceChromeOSDelegate and CameraDeviceDelegate have
// CameraHalDelegate's raw pointer.
// When VideoCaptureDeviceFactoryChromeOS destroys,
// CameraHalDelegate destroys VideoCaptureDeviceChromeOSDelegate and
// VideoCaptureDeviceChromeOSDelegate destroys CameraDeviceDelegate.
class CAPTURE_EXPORT CameraHalDelegate final
    : public cros::mojom::CameraModuleCallbacks {
 public:
  // Top 20 Popular Camera peripherals from go/usb-popularity-study. Since
  // 4 cameras of Sonix have the same vids and pids, they are
  // aggregated to |kCam_Sonix|. Original hex strings in the format of
  // 0123:abcd are decoded to integers. These are the same values as
  // PopularCamPeriphModuleID in tools/metrics/histograms/enums.xml
  enum class PopularCamPeriphModuleID {
    kOthers = 0,
    kLifeCamHD3000_Microsoft = 73271312,   // 045e:0810
    kC270_Logitech = 74254373,             // 046d:0825
    kHDC615_Logitech = 74254380,           // 046d:082c
    kHDProC920_Logitech = 74254381,        // 046d:082d
    kC930e_Logitech = 74254403,            // 046d:0843
    kC925e_Logitech = 74254427,            // 046d:085b
    kC922ProStream_Logitech = 74254428,    // 046d:085c
    kBRIOUltraHD_Logitech = 74254430,      // 046d:085e
    kC920HDPro_Logitech = 74254482,        // 046d:0892
    kC920PROHD_Logitech = 74254565,        // 046d:08e5
    kCam_ARC = 94606129,                   // 05a3:9331
    kLiveStreamer313_Sunplus = 130691386,  // 07ca:313a
    kVitadeAF_Microdia = 205874022,        // 0c45:6366
    kCam_Sonix = 205874027,                // 0c45:636b
    kVZR_IPEVO = 393793569,                // 1778:d021
    k808Camera9_Generalplus = 457121794,   // 1b3f:2002
    kNexiGoN60FHD_2MUVC = 493617411,       // 1d6c:0103
    kMaxValue = kNexiGoN60FHD_2MUVC,
  };

  explicit CameraHalDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  // CameraHalDelegate is functional only after this call succeeds.
  bool Init();

  ~CameraHalDelegate() final;

  CameraHalDelegate(const CameraHalDelegate&) = delete;
  CameraHalDelegate& operator=(const CameraHalDelegate&) = delete;

  // Start observing the status of the CrosCameraService service on the Mojo
  // Service Manager. Once the CrosCameraService service is registered,
  // CameraHalDelegate will request camera module from it.
  void BootStrapCameraServiceConnection();

  void SetCameraModule(
      mojo::PendingRemote<cros::mojom::CameraModule> camera_module);

  // Delegation methods for the VideoCaptureDeviceFactory interface.  These
  // methods are called by VideoCaptureDeviceFactoryChromeOS directly.  They
  // operate on the same thread that the VideoCaptureDeviceFactoryChromeOS runs
  // on.
  std::unique_ptr<VideoCaptureDevice> CreateDevice(
      scoped_refptr<base::SingleThreadTaskRunner>
          task_runner_for_screen_observer,
      const VideoCaptureDeviceDescriptor& device_descriptor);

  void GetDevicesInfo(
      VideoCaptureDeviceFactory::GetDevicesInfoCallback callback);

  // Returns camera pan, tilt, zoom capability support.
  VideoCaptureControlSupport GetControlSupport(
      const cros::mojom::CameraInfoPtr& camera_info);

  // Gets the camera info of |device_id|. Returns null CameraInfoPtr on error.
  cros::mojom::CameraInfoPtr GetCameraInfoFromDeviceId(
      const std::string& device_id);

  void EnableVirtualDevice(const std::string& device_id, bool enable);
  void DisableAllVirtualDevices();

  const VendorTagInfo* GetVendorTagInfoByName(const std::string& full_name);

  // Asynchronous method to open the camera device designated by |camera_id|.
  // This method may be called on any thread; |callback| will run on
  // |ipc_task_runner_|.
  using OpenDeviceCallback = base::OnceCallback<void(int32_t)>;
  void OpenDevice(
      int32_t camera_id,
      const std::string& model_id,
      mojo::PendingReceiver<cros::mojom::Camera3DeviceOps> device_ops_receiver,
      OpenDeviceCallback callback);

  // Gets camera id from device id. Returns -1 on error.
  int GetCameraIdFromDeviceId(const std::string& device_id);

  // Waiting for the camera module to be ready for testing.
  bool WaitForCameraModuleReadyForTesting();

 private:
  class SystemEventMonitorProxy;
  class VCDInfoMonitorImpl;
  class VideoCaptureDeviceDelegateMap;
  class CameraModuleConnector;

  void NotifyVideoCaptureDevicesChanged();

  void OnRegisteredCameraHalClient(int32_t result);

  void GetSupportedFormats(const cros::mojom::CameraInfoPtr& camera_info,
                           VideoCaptureFormats* supported_formats);

  VideoCaptureDeviceChromeOSDelegate* GetVCDDelegate(
      scoped_refptr<base::SingleThreadTaskRunner>
          task_runner_for_screen_observer,
      const VideoCaptureDeviceDescriptor& device_descriptor);

  void SetCameraModuleOnIpcThread(
      mojo::PendingRemote<cros::mojom::CameraModule> camera_module);

  // Resets the Mojo interface and bindings.
  void ResetMojoInterfaceOnIpcThread();

  // Internal method to update the camera info for all built-in cameras. Runs on
  // the same thread as CreateDevice, GetSupportedFormats, and
  // GetDevicesInfo.
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

  // Changes hex string |module_id| into decimal integer and check if
  // |module_id| is one of the popular camera peripherals. If it is, it returns
  // the decimal integer and if not, it returns 0.
  int32_t GetMaskedModuleID(const std::string& module_id);

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
      const std::string& model_id,
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
  // are also read in GetSupportedFormats and GetDevicesInfo, in which the
  // access is protected by |camera_info_lock_| and sequenced through
  // UpdateBuiltInCameraInfo and |builtin_camera_info_updated_| to avoid race
  // conditions. For external cameras, the |camera_info_| would be read nad
  // updated in CameraDeviceStatusChange, which is also protected by
  // |camera_info_lock_|.
  base::Lock camera_info_lock_;
  size_t num_builtin_cameras_ GUARDED_BY(camera_info_lock_);
  std::unordered_map<int, cros::mojom::CameraInfoPtr> camera_info_
      GUARDED_BY(camera_info_lock_);

  // A map from |VideoCaptureDeviceDescriptor.device_id| to camera id, which is
  // updated in GetDevicesInfo() and queried in
  // GetCameraIdFromDeviceId().
  base::Lock device_id_to_camera_id_lock_;
  std::map<std::string, int> device_id_to_camera_id_
      GUARDED_BY(device_id_to_camera_id_lock_);
  // A virtual device is enabled/disabled for camera id.
  base::Lock enable_virtual_device_lock_;
  base::flat_map<int, bool> enable_virtual_device_
      GUARDED_BY(enable_virtual_device_lock_);

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<CameraBufferFactory> camera_buffer_factory_;

  // The thread that all the Mojo operations of CameraHalDelegate take
  // place.  Started in constructor and stopped in destructor.
  base::Thread camera_hal_ipc_thread_;

  // The task runner where all the camera module Mojo communication takes place.
  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

  // The Mojo proxy to access the camera module at the remote camera HAL.  Bound
  // to |ipc_task_runner_|.
  mojo::Remote<cros::mojom::CameraModule> camera_module_;

  // The Mojo receiver serving the camera module callbacks.  Bound to
  // |ipc_task_runner_|.
  mojo::AssociatedReceiver<cros::mojom::CameraModuleCallbacks>
      camera_module_callbacks_;

  // An internal delegate to handle VendorTagOps mojo connection and query
  // information of vendor tags.  Bound to |ipc_task_runner_|.
  std::unique_ptr<VendorTagOpsDelegate> vendor_tag_ops_delegate_;

  // A map from camera id to corresponding delegate instance.
  std::unique_ptr<VideoCaptureDeviceDelegateMap> vcd_delegate_map_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::vector<std::unique_ptr<CameraClientObserver>> local_client_observers_;

  std::unique_ptr<SystemEventMonitorProxy> system_event_monitor_proxy_;

  base::SequenceBound<VCDInfoMonitorImpl> vcd_info_monitor_impl_;

  base::SequenceBound<CameraModuleConnector> camera_module_connector_;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_HAL_DELEGATE_H_
