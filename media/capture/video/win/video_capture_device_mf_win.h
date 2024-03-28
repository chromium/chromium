// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Windows specific implementation of VideoCaptureDevice.
// MediaFoundation is used for capturing. MediaFoundation provides its own
// threads for capturing.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_MF_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_MF_WIN_H_

#include <d3d11_4.h>
#include <ks.h>
#include <ksmedia.h>
#include <mfcaptureengine.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <stdint.h>
#include <strmif.h>
#include <wrl/client.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/win/capability_list_win.h"
#include "media/capture/video/win/metrics.h"

interface IMFSourceReader;

namespace base {
class Location;
}  // namespace base

namespace media {

class MFVideoCallback;

class CAPTURE_EXPORT VideoCaptureDeviceMFWin : public VideoCaptureDevice {
 public:
  static bool GetPixelFormatFromMFSourceMediaSubtype(const GUID& guid,
                                                     bool use_hardware_format,
                                                     VideoPixelFormat* format);
  static VideoCaptureControlSupport GetControlSupport(
      Microsoft::WRL::ComPtr<IMFMediaSource> source);

  VideoCaptureDeviceMFWin() = delete;

  explicit VideoCaptureDeviceMFWin(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      Microsoft::WRL::ComPtr<IMFMediaSource> source,
      scoped_refptr<DXGIDeviceManager> dxgi_device_manager,
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner);
  explicit VideoCaptureDeviceMFWin(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      Microsoft::WRL::ComPtr<IMFMediaSource> source,
      scoped_refptr<DXGIDeviceManager> dxgi_device_manager,
      Microsoft::WRL::ComPtr<IMFCaptureEngine> engine,
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner);

  VideoCaptureDeviceMFWin(const VideoCaptureDeviceMFWin&) = delete;
  VideoCaptureDeviceMFWin& operator=(const VideoCaptureDeviceMFWin&) = delete;

  ~VideoCaptureDeviceMFWin() override;

  // Opens the device driver for this device.
  bool Init();

  // VideoCaptureDevice implementation.
  void AllocateAndStart(
      const VideoCaptureParams& params,
      std::unique_ptr<VideoCaptureDevice::Client> client) override;
  void StopAndDeAllocate() override;
  void TakePhoto(TakePhotoCallback callback) override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void OnUtilizationReport(media::VideoCaptureFeedback feedback) override;

  // Captured configuration changes.
  void OnCameraControlChange(REFGUID control_set, UINT32 id);
  void OnCameraControlError(HRESULT status) const;

  // Captured new video data.
  void OnIncomingCapturedData(Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer,
                              base::TimeTicks reference_time,
                              base::TimeDelta timestamp,
                              base::TimeTicks capture_begin_time);
  void OnFrameDropped(VideoCaptureFrameDropReason reason);
  void OnEvent(IMFMediaEvent* media_event);

  using CreateMFPhotoCallbackCB =
      base::RepeatingCallback<scoped_refptr<IMFCaptureEngineOnSampleCallback>(
          VideoCaptureDevice::TakePhotoCallback callback,
          VideoCaptureFormat format)>;

  bool get_use_photo_stream_to_take_photo_for_testing() {
    return !photo_capabilities_.empty();
  }

  void set_create_mf_photo_callback_for_testing(CreateMFPhotoCallbackCB cb) {
    create_mf_photo_callback_ = cb;
  }

  void set_max_retry_count_for_testing(int max_retry_count) {
    max_retry_count_ = max_retry_count;
  }

  void set_retry_delay_in_ms_for_testing(int retry_delay_in_ms) {
    retry_delay_in_ms_ = retry_delay_in_ms;
  }

  void set_dxgi_device_manager_for_testing(
      scoped_refptr<DXGIDeviceManager> dxgi_device_manager) {
    dxgi_device_manager_ = std::move(dxgi_device_manager);
  }

  std::optional<int> camera_rotation() const { return camera_rotation_; }

 private:
  class MFVideoCallback;
  class MFActivitiesReportCallback;

  bool CreateMFCameraControlMonitor();
  void DeinitVideoCallbacksControlsAndMonitors();
  HRESULT ExecuteHresultCallbackWithRetries(
      base::RepeatingCallback<HRESULT()> callback,
      MediaFoundationFunctionRequiringRetry which_function);
  HRESULT GetDeviceStreamCount(IMFCaptureSource* source, DWORD* count);
  HRESULT GetDeviceStreamCategory(
      IMFCaptureSource* source,
      DWORD stream_index,
      MF_CAPTURE_ENGINE_STREAM_CATEGORY* stream_category);
  HRESULT GetAvailableDeviceMediaType(IMFCaptureSource* source,
                                      DWORD stream_index,
                                      DWORD media_type_index,
                                      IMFMediaType** type);

  HRESULT FillCapabilities(IMFCaptureSource* source,
                           bool photo,
                           CapabilityList* capabilities);
  HRESULT SetAndCommitExtendedCameraControlFlags(
      KSPROPERTY_CAMERACONTROL_EXTENDED_PROPERTY property_id,
      ULONGLONG flags);
  HRESULT SetCameraControlProperty(CameraControlProperty property,
                                   long value,
                                   long flags);
  HRESULT SetVideoControlProperty(VideoProcAmpProperty property,
                                  long value,
                                  long flags);
  void OnError(VideoCaptureError error,
               const base::Location& from_here,
               HRESULT hr);
  void OnError(VideoCaptureError error,
               const base::Location& from_here,
               const char* message);
  void SendOnStartedIfNotYetSent();
  HRESULT WaitOnCaptureEvent(GUID capture_event_guid);
  HRESULT DeliverTextureToClient(
      Microsoft::WRL::ComPtr<IMFMediaBuffer> imf_buffer,
      ID3D11Texture2D* texture,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      base::TimeTicks capture_begin_time);
  HRESULT DeliverExternalBufferToClient(
      Microsoft::WRL::ComPtr<IMFMediaBuffer> imf_buffer,
      ID3D11Texture2D* texture,
      const gfx::Size& texture_size,
      const VideoPixelFormat& pixel_format,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      base::TimeTicks capture_begin_time);
  void OnCameraControlChangeInternal(REFGUID control_set, UINT32 id);
  void OnIncomingCapturedDataInternal();
  bool RecreateMFSource();
  void OnFrameDroppedInternal(VideoCaptureFrameDropReason reason);
  void ProcessEventError(HRESULT hr);
  void OnCameraInUseReport(bool in_use, bool is_default_action);

  VideoCaptureDeviceDescriptor device_descriptor_;
  CreateMFPhotoCallbackCB create_mf_photo_callback_;
  scoped_refptr<MFVideoCallback> video_callback_;
  scoped_refptr<MFActivitiesReportCallback> activities_report_callback_;
  bool activity_report_pending_ = false;
  bool is_initialized_;
  int max_retry_count_;
  int retry_delay_in_ms_;

  std::unique_ptr<VideoCaptureDevice::Client> client_;
  Microsoft::WRL::ComPtr<IMFMediaSource> source_;
  Microsoft::WRL::ComPtr<IAMCameraControl> camera_control_;
  Microsoft::WRL::ComPtr<IAMVideoProcAmp> video_control_;
  Microsoft::WRL::ComPtr<IMFCameraControlMonitor> camera_control_monitor_;
  Microsoft::WRL::ComPtr<IMFExtendedCameraController>
      extended_camera_controller_;
  Microsoft::WRL::ComPtr<IMFSensorActivityMonitor> activity_monitor_;
  Microsoft::WRL::ComPtr<IMFCaptureEngine> engine_;
  std::unique_ptr<CapabilityWin> selected_video_capability_;
  gfx::ColorSpace color_space_;
  CapabilityList photo_capabilities_;
  std::unique_ptr<CapabilityWin> selected_photo_capability_;
  bool is_started_;
  bool has_sent_on_started_to_client_;
  // These flags keep the manual/auto mode between cycles of SetPhotoOptions().
  bool exposure_mode_manual_;
  bool focus_mode_manual_;
  bool white_balance_mode_manual_;
  base::queue<TakePhotoCallback> video_stream_take_photo_callbacks_;
  base::WaitableEvent capture_initialize_;
  base::WaitableEvent capture_error_;
  base::WaitableEvent capture_stopped_;
  base::WaitableEvent capture_started_;
  HRESULT last_error_hr_ = S_OK;
  scoped_refptr<DXGIDeviceManager> dxgi_device_manager_;
  std::optional<int> camera_rotation_;
  VideoCaptureParams params_;
  int num_restarts_ = 0;

  struct ValueAndFlags {
    long value;
    long flags;
  };

  base::flat_map<UINT32, ValueAndFlags> set_camera_control_properties_;
  base::flat_map<UINT32, ULONGLONG> set_extended_camera_control_flags_;
  base::flat_map<UINT32, ValueAndFlags> set_video_control_properties_;

  // Main thread task runner, used to serialize work triggered by
  // IMFCaptureEngine callbacks (OnEvent, OnSample) and work triggered
  // by video capture service API calls (Init, AllocateAndStart,
  // StopAndDeallocate) This can be left as nullptr in test environment where
  // callbacks are called from the same thread as API methods.
  scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;

  base::TimeTicks last_premapped_request_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::Lock queueing_lock_;
  // Last input for the posted task OnIncomingCapturedDataInternal.
  // If new input arrives while the task is pending, the input will be
  // overridden. So only 2 IMFSampleBuffer would be used at any time.
  Microsoft::WRL::ComPtr<IMFMediaBuffer> input_buffer_
      GUARDED_BY(queueing_lock_);
  base::TimeTicks input_reference_time_ GUARDED_BY(queueing_lock_);
  base::TimeDelta input_timestamp_ GUARDED_BY(queueing_lock_);
  base::TimeTicks input_capture_begin_time_ GUARDED_BY(queueing_lock_);

  base::WeakPtrFactory<VideoCaptureDeviceMFWin> weak_factory_{this};
};

// Creates a new sensor activity monitor and returns it on `monitor`.
// The monitor will execute `report_callback` every time a new activity report
// becomes available.
// This function return `true` on success and `false` on failure.
bool CreateMFSensorActivityMonitor(
    IMFSensorActivitiesReportCallback* report_callback,
    IMFSensorActivityMonitor** monitor);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_MF_WIN_H_
