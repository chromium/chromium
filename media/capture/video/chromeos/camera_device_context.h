// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_DEVICE_CONTEXT_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_DEVICE_CONTEXT_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/system/sys_info.h"
#include "media/capture/video/video_capture_device.h"
#include "ui/gfx/color_space.h"

namespace media {

enum class ClientType : uint32_t {
  kPreviewClient = 0,
  kVideoClient = 1,
};

// A class storing the context of a running CameraDeviceDelegate.
//
// The class is also used to forward/translate events and method calls to a
// given VideoCaptureDevice::Client. This class supposes to have two clients
// at most. One is for preview and another is for video.
class CAPTURE_EXPORT CameraDeviceContext {
 public:
  // The internal state of the running CameraDeviceDelegate.  The state
  // transition happens when the corresponding methods are called inside
  // CameraDeviceDelegate.
  enum class State {
    // The camera device is completely stopped. This is the initial state, and
    // is also set in OnClosed().
    kStopped,

    // The camera device is starting and waiting to be initialized.
    //
    // The kStarting state is set in AllocateAndStart().
    kStarting,

    // The camera device is initialized and can accept stream configuration
    // requests.
    //
    // The state is transitioned to kInitialized through:
    //
    //   |hal_delegate_|->GetCameraInfo() -> OnGotCameraInfo() ->
    //   |hal_delegate_|->OpenDevice() -> OnOpenedDevice() ->
    //   Initialize() -> OnInitialized()
    kInitialized,

    // The various capture streams are configured and the camera device is ready
    // to process capture requests.
    //
    // The state is transitioned to kStreamConfigured through:
    //
    //   ConfigureStreams() -> OnConfiguredStreams()
    kStreamConfigured,

    // The camera device is capturing video streams.
    //
    // The state is transitioned to kCapturing through:
    //
    //   ConstructDefaultRequestSettings() ->
    //   OnConstructedDefaultRequestSettings() ->
    //   |request_manager_|->StartPreview()
    //
    // In the kCapturing state the |request_manager_| runs the capture
    // loop to send capture requests and process capture results.
    kCapturing,

    // When the camera device is in the kCapturing state, a capture loop is
    // constantly running in |request_manager_|:
    //
    // On the StreamBufferManager side, we register and submit a capture
    // request whenever a free buffer is available:
    //
    //   RegisterBuffer() -> OnRegisteredBuffer() ->
    //   ProcessCaptureRequest() -> OnProcessedCaptureRequest()
    //
    // We get various capture metadata and error notifications from the camera
    // HAL through the following callbacks:
    //
    //   ProcessCaptureResult()
    //   Notify()

    // The camera device is going through the shut down process; in order to
    // avoid race conditions, no new Mojo messages may be sent to camera HAL in
    // the kStopping state.
    //
    // The kStopping state is set in StopAndDeAllocate().
    kStopping,

    // The camera device encountered an unrecoverable error and needs to be
    // StopAndDeAllocate()'d.
    //
    // The kError state is set in SetErrorState().
    kError,
  };

  CameraDeviceContext();

  ~CameraDeviceContext();

  bool AddClient(ClientType client_type,
                 std::unique_ptr<VideoCaptureDevice::Client> client);
  void RemoveClient(ClientType client_type);
  void SetState(State state);

  State GetState();

  // Sets state to kError and call |client_->OnError| to tear down the
  // VideoCaptureDevice.
  void SetErrorState(media::VideoCaptureError error,
                     const base::Location& from_here,
                     const std::string& reason);

  // Logs |message| to |client_|.
  void LogToClient(std::string message);

  // Submits the captured camera frame directly in a video capture buffer.  No
  // buffer copy nor format conversion will be performed on the captured buffer.
  // The buffer would be passed to the renderer process directly through
  // |client_->OnIncomingCapturedBufferExt|.
  void SubmitCapturedVideoCaptureBuffer(
      ClientType client_type,
      VideoCaptureDevice::Client::Buffer buffer,
      const VideoCaptureFormat& frame_format,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      const VideoFrameMetadata& metadata);

  // Submits the captured camera frame through a locally-allocated
  // GpuMemoryBuffer.  The captured buffer would be submitted through
  // |client_->OnIncomingCapturedGfxBuffer|, which would perform buffer copy
  // and/or format conversion to an I420 SharedMemory-based video capture buffer
  // for client consumption.
  void SubmitCapturedGpuMemoryBuffer(ClientType client_type,
                                     gfx::GpuMemoryBuffer* buffer,
                                     const VideoCaptureFormat& frame_format,
                                     base::TimeTicks reference_time,
                                     base::TimeDelta timestamp);

  void SetSensorOrientation(int sensor_orientation);

  void SetScreenRotation(int screen_rotation);

  // Controls whether the Chrome OS video capture device applies frame rotation
  // according to sensor and UI rotation.
  void SetCameraFrameRotationEnabledAtSource(bool is_enabled);

  // Gets the accumulated rotation that the camera frame needs to be rotated
  // to match the display orientation.  This includes the sensor orientation and
  // the screen rotation.
  int GetCameraFrameRotation();

  // Gets whether the camera frame rotation is enabled inside the video capture
  // device.
  bool IsCameraFrameRotationEnabledAtSource();

  // Reserves a video capture buffer from the buffer pool provided by the video
  // |client_|. |require_new_buffer_id| and |retire_old_buffer_id| returns the
  // new buffer id and retired buffer id in the VCD buffer pool. Returns true if
  // the operation succeeds; false otherwise.
  bool ReserveVideoCaptureBufferFromPool(
      ClientType client_type,
      gfx::Size size,
      VideoPixelFormat format,
      VideoCaptureDevice::Client::Buffer* buffer,
      int* require_new_buffer_id = nullptr,
      int* retire_old_buffer_id = nullptr);

  // Returns true if there is a client.
  bool HasClient();

  // Expose MediaStreamTrack configuration changes through
  // |client_->OnCaptureConfigurationChanged|
  void OnCaptureConfigurationChanged();

 private:
  friend class RequestManagerTest;

  void OnGotHardwareInfo(base::SysInfo::HardwareInfo hardware_info);

  SEQUENCE_CHECKER(sequence_checker_);

  // The state the CameraDeviceDelegate currently is in.
  State state_;

  // Lock to serialize the access to the various camera rotation state variables
  // since they are access on multiple threads.
  base::Lock rotation_state_lock_;

  std::optional<gfx::ColorSpace> color_space_override_ GUARDED_BY(client_lock_);

  // Clockwise angle through which the output image needs to be rotated to be
  // upright on the device screen in its native orientation.  This value should
  // be 0, 90, 180, or 270.
  int sensor_orientation_ GUARDED_BY(rotation_state_lock_);

  // Clockwise screen rotation in degrees. This value should be 0, 90, 180, or
  // 270.
  int screen_rotation_ GUARDED_BY(rotation_state_lock_);

  // Whether the camera frame rotation is enabled inside the video capture
  // device.
  bool frame_rotation_at_source_ GUARDED_BY(rotation_state_lock_);

  base::Lock client_lock_;
  // A map for client type and client instance.
  base::flat_map<ClientType, std::unique_ptr<VideoCaptureDevice::Client>>
      clients_ GUARDED_BY(client_lock_);

  base::WeakPtrFactory<CameraDeviceContext> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_DEVICE_CONTEXT_H_
