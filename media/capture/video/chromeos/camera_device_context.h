// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_DEVICE_CONTEXT_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_DEVICE_CONTEXT_H_

#include <memory>
#include <string>

#include "base/sequence_checker.h"
#include "media/capture/video/video_capture_device.h"

namespace media {

// A class storing the context of a running CameraDeviceDelegate.
//
// The class is also used to forward/translate events and method calls to a
// given VideoCaptureDevice::Client.
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
    //   |stream_buffer_manager_|->StartPreview()
    //
    // In the kCapturing state the |stream_buffer_manager_| runs the capture
    // loop to send capture requests and process capture results.
    kCapturing,

    // When the camera device is in the kCapturing state, a capture loop is
    // constantly running in |stream_buffer_manager_|:
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

  explicit CameraDeviceContext(
      std::unique_ptr<VideoCaptureDevice::Client> client);

  ~CameraDeviceContext();

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
      VideoCaptureDevice::Client::Buffer buffer,
      const VideoCaptureFormat& frame_format,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp);

  // Submits the captured camera frame through a locally-allocated
  // GpuMemoryBuffer.  The captured buffer would be submitted through
  // |client_->OnIncomingCapturedGfxBuffer|, which would perform buffer copy
  // and/or format conversion to an I420 SharedMemory-based video capture buffer
  // for client consumption.
  void SubmitCapturedGpuMemoryBuffer(gfx::GpuMemoryBuffer* buffer,
                                     const VideoCaptureFormat& frame_format,
                                     base::TimeTicks reference_time,
                                     base::TimeDelta timestamp);

  void SetSensorOrientation(int sensor_orientation);

  void SetScreenRotation(int screen_rotation);

  int GetCameraFrameOrientation();

  // Reserves a video capture buffer from the buffer pool provided by the video
  // |client_|.  Returns true if the operation succeeds; false otherwise.
  bool ReserveVideoCaptureBufferFromPool(
      gfx::Size size,
      VideoPixelFormat format,
      VideoCaptureDevice::Client::Buffer* buffer);

 private:
  friend class RequestManagerTest;

  SEQUENCE_CHECKER(sequence_checker_);

  // The state the CameraDeviceDelegate currently is in.
  State state_;

  // Clockwise angle through which the output image needs to be rotated to be
  // upright on the device screen in its native orientation.  This value should
  // be 0, 90, 180, or 270.
  int sensor_orientation_;

  // Clockwise screen rotation in degrees. This value should be 0, 90, 180, or
  // 270.
  int screen_rotation_;

  std::unique_ptr<VideoCaptureDevice::Client> client_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CameraDeviceContext);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_DEVICE_CONTEXT_CHROMEOS_H_
