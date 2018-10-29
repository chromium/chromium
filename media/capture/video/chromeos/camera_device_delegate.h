// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_DEVICE_DELEGATE_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_DEVICE_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "media/capture/video/chromeos/mojo/camera3.mojom.h"
#include "media/capture/video/chromeos/mojo/camera_common.mojom.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video_capture_types.h"

namespace media {

class Camera3AController;
class CameraDeviceContext;
class CameraHalDelegate;
class StreamBufferManager;

enum class StreamType : uint64_t {
  kPreview = 0,
  kStillCapture = 1,
  kUnknown,
};

std::string StreamTypeToString(StreamType stream_type);

std::ostream& operator<<(std::ostream& os, StreamType stream_type);

// The interface to register buffer with and send capture request to the
// camera HAL.
class CAPTURE_EXPORT StreamCaptureInterface {
 public:
  struct Plane {
    Plane();
    ~Plane();
    mojo::ScopedHandle fd;
    uint32_t stride;
    uint32_t offset;
  };

  virtual ~StreamCaptureInterface() {}

  // Registers a buffer to the camera HAL.
  virtual void RegisterBuffer(uint64_t buffer_id,
                              cros::mojom::Camera3DeviceOps::BufferType type,
                              uint32_t drm_format,
                              cros::mojom::HalPixelFormat hal_pixel_format,
                              uint32_t width,
                              uint32_t height,
                              std::vector<Plane> planes,
                              base::OnceCallback<void(int32_t)> callback) = 0;

  // Sends a capture request to the camera HAL.
  virtual void ProcessCaptureRequest(
      cros::mojom::Camera3CaptureRequestPtr request,
      base::OnceCallback<void(int32_t)> callback) = 0;

  // Send flush to cancel all pending requests to the camera HAL.
  virtual void Flush(base::OnceCallback<void(int32_t)> callback) = 0;
};

// CameraDeviceDelegate is instantiated on the capture thread where
// AllocateAndStart of VideoCaptureDeviceArcChromeOS runs on.  All the methods
// in CameraDeviceDelegate run on |ipc_task_runner_| and hence all the
// access to member variables is sequenced.
class CAPTURE_EXPORT CameraDeviceDelegate final {
 public:
  CameraDeviceDelegate(
      VideoCaptureDeviceDescriptor device_descriptor,
      scoped_refptr<CameraHalDelegate> camera_hal_delegate,
      scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner);

  ~CameraDeviceDelegate();

  // Delegation methods for the VideoCaptureDevice interface.
  void AllocateAndStart(const VideoCaptureParams& params,
                        CameraDeviceContext* device_context);
  void StopAndDeAllocate(base::OnceClosure device_close_callback);
  void TakePhoto(VideoCaptureDevice::TakePhotoCallback callback);
  void GetPhotoState(VideoCaptureDevice::GetPhotoStateCallback callback);
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       VideoCaptureDevice::SetPhotoOptionsCallback callback);

  // Sets the frame rotation angle in |rotation_|.  |rotation_| is clockwise
  // rotation in degrees, and is passed to |client_| along with the captured
  // frames.
  void SetRotation(int rotation);

  base::WeakPtr<CameraDeviceDelegate> GetWeakPtr();

 private:
  class StreamCaptureInterfaceImpl;

  friend class CameraDeviceDelegateTest;

  // Mojo connection error handler.
  void OnMojoConnectionError();

  // Reconfigure streams for picture taking.
  void OnFlushed(int32_t result);

  // Callback method for the Close Mojo IPC call.  This method resets the Mojo
  // connection and closes the camera device.
  void OnClosed(int32_t result);

  // Resets the Mojo interface and bindings.
  void ResetMojoInterface();

  // Sets |static_metadata_| from |camera_info|.
  void OnGotCameraInfo(int32_t result, cros::mojom::CameraInfoPtr camera_info);

  // Creates the Mojo connection to the camera device.
  void OnOpenedDevice(int32_t result);

  // Initializes the camera HAL.  Initialize sets up the Camera3CallbackOps with
  // the camera HAL.  OnInitialized continues to ConfigureStreams if the
  // Initialize call succeeds.
  void Initialize();
  void OnInitialized(int32_t result);

  // ConfigureStreams sets up stream context in |streams_| and configure the
  // streams with the camera HAL.  OnConfiguredStreams updates |streams_| with
  // the stream config returned, and allocates buffers as per |updated_config|
  // indicates.  If there's no error OnConfiguredStreams notifies
  // |client_| the capture has started by calling OnStarted, and proceeds to
  // ConstructDefaultRequestSettings.
  void ConfigureStreams(bool require_photo);
  void OnConfiguredStreams(
      int32_t result,
      cros::mojom::Camera3StreamConfigurationPtr updated_config);

  // ConstructDefaultRequestSettings asks the camera HAL for the default request
  // settings of the stream in |stream_context_|.
  // OnConstructedDefaultRequestSettings sets the request settings in
  // |streams_context_|.  If there's no error
  // OnConstructedDefaultPreviewRequestSettings calls StartPreview to start the
  // video capture loop.
  // OnConstructDefaultStillCaptureRequestSettings triggers
  // |stream_buffer_manager_| to request a still capture.
  void ConstructDefaultRequestSettings(StreamType stream_type);
  void OnConstructedDefaultPreviewRequestSettings(
      cros::mojom::CameraMetadataPtr settings);
  void OnConstructedDefaultStillCaptureRequestSettings(
      cros::mojom::CameraMetadataPtr settings);

  // StreamCaptureInterface implementations.  These methods are called by
  // |stream_buffer_manager_| on |ipc_task_runner_|.
  void RegisterBuffer(uint64_t buffer_id,
                      cros::mojom::Camera3DeviceOps::BufferType type,
                      uint32_t drm_format,
                      cros::mojom::HalPixelFormat hal_pixel_format,
                      uint32_t width,
                      uint32_t height,
                      std::vector<StreamCaptureInterface::Plane> planes,
                      base::OnceCallback<void(int32_t)> callback);
  void ProcessCaptureRequest(cros::mojom::Camera3CaptureRequestPtr request,
                             base::OnceCallback<void(int32_t)> callback);
  void Flush(base::OnceCallback<void(int32_t)> callback);

  bool SetPointsOfInterest(
      const std::vector<mojom::Point2DPtr>& points_of_interest);

  const VideoCaptureDeviceDescriptor device_descriptor_;

  int32_t camera_id_;

  const scoped_refptr<CameraHalDelegate> camera_hal_delegate_;

  VideoCaptureParams chrome_capture_params_;

  CameraDeviceContext* device_context_;

  std::queue<VideoCaptureDevice::TakePhotoCallback> take_photo_callbacks_;

  std::unique_ptr<StreamBufferManager> stream_buffer_manager_;

  std::unique_ptr<Camera3AController> camera_3a_controller_;

  // Stores the static camera characteristics of the camera device. E.g. the
  // supported formats and resolution, various available exposure and apeture
  // settings, etc.
  cros::mojom::CameraMetadataPtr static_metadata_;

  cros::mojom::Camera3DeviceOpsPtr device_ops_;

  // Where all the Mojo IPC calls takes place.
  const scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

  base::OnceClosure device_close_callback_;

  VideoCaptureDevice::SetPhotoOptionsCallback set_photo_option_callback_;

  base::WeakPtrFactory<CameraDeviceDelegate> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CameraDeviceDelegate);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_DEVICE_DELEGATE_H_
