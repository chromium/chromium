// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_DEVICE_DELEGATE_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_DEVICE_DELEGATE_H_

#include <memory>
#include <queue>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/capture_metadata_dispatcher.h"
#include "media/capture/video/chromeos/mojom/camera3.mojom.h"
#include "media/capture/video/chromeos/mojom/camera_common.mojom.h"
#include "media/capture/video/chromeos/mojom/effects_pipeline.mojom.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/range/range.h"

namespace media {

class Camera3AController;
class CameraHalDelegate;
class RequestManager;

enum class StreamType : uint64_t {
  kPreviewOutput = 0,
  kJpegOutput = 1,
  kYUVInput = 2,
  kYUVOutput = 3,
  kRecordingOutput = 4,
  kUnknown = 5,
};

// A map to know that each StreamType belongs to which ClientType.
// The index is StreamType value.
constexpr std::array<ClientType, static_cast<int>(StreamType::kUnknown)>
    kStreamClientTypeMap = {
        ClientType::kPreviewClient,  // kPreviewOutput
        ClientType::kPreviewClient,  // kJpegOutput
        ClientType::kPreviewClient,  // kYUVInput
        ClientType::kPreviewClient,  // kYUVOutput
        ClientType::kVideoClient,    // kRecordingOutput
};

// The metadata might be large so clone a whole metadata might be relatively
// expensive. We only keep the needed data by this structure.
struct ResultMetadata {
  ResultMetadata();
  ~ResultMetadata();

  absl::optional<uint8_t> ae_mode;
  absl::optional<int32_t> ae_compensation;
  absl::optional<uint8_t> af_mode;
  absl::optional<uint8_t> awb_mode;
  absl::optional<int32_t> brightness;
  absl::optional<int32_t> contrast;
  absl::optional<int64_t> exposure_time;
  absl::optional<float> focus_distance;
  absl::optional<int32_t> pan;
  absl::optional<int32_t> saturation;
  absl::optional<int32_t> sensitivity;
  absl::optional<int32_t> sharpness;
  absl::optional<int32_t> tilt;
  absl::optional<int32_t> zoom;
  absl::optional<gfx::Rect> scaler_crop_region;
};

// Returns true if the given stream type is an input stream.
bool IsInputStream(StreamType stream_type);

StreamType StreamIdToStreamType(uint64_t stream_id);

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
//
// CameraDeviceDelegate supports multiple clients.
// It will use the first client for preview stream and photo stream and use
// second client for recording stream.
// The second client will be a virtual camera device which is only used in CCA.
class CAPTURE_EXPORT CameraDeviceDelegate final
    : public CaptureMetadataDispatcher::ResultMetadataObserver,
      public media::CameraEffectObserver {
 public:
  CameraDeviceDelegate() = delete;

  CameraDeviceDelegate(
      VideoCaptureDeviceDescriptor device_descriptor,
      CameraHalDelegate* camera_hal_delegate,
      scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner);

  CameraDeviceDelegate(const CameraDeviceDelegate&) = delete;
  CameraDeviceDelegate& operator=(const CameraDeviceDelegate&) = delete;

  ~CameraDeviceDelegate() final;

  // Delegation methods for the VideoCaptureDevice interface.
  void AllocateAndStart(
      const base::flat_map<ClientType, VideoCaptureParams>& params,
      CameraDeviceContext* device_context);
  void StopAndDeAllocate(base::OnceClosure device_close_callback);
  void TakePhoto(VideoCaptureDevice::TakePhotoCallback callback);
  void GetPhotoState(VideoCaptureDevice::GetPhotoStateCallback callback);
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       VideoCaptureDevice::SetPhotoOptionsCallback callback);

  void ReconfigureStreams(
      const base::flat_map<ClientType, VideoCaptureParams>& params);

  // Sets the frame rotation angle in |rotation_|.  |rotation_| is clockwise
  // rotation in degrees, and is passed to |client_| along with the captured
  // frames.
  void SetRotation(int rotation);

  base::WeakPtr<CameraDeviceDelegate> GetWeakPtr();

 private:
  class StreamCaptureInterfaceImpl;

  friend class CameraDeviceDelegateTest;

  // Reconfigures the streams to include photo stream according to |settings|.
  // Returns true if the reconfigure process is triggered.
  bool MaybeReconfigureForPhotoStream(mojom::PhotoSettingsPtr settings);

  void TakePhotoImpl();

  // Mojo connection error handler.
  void OnMojoConnectionError();

  // Reconfigure streams for picture taking and recording.
  void OnFlushed(bool require_photo,
                 absl::optional<gfx::Size> new_blob_resolution,
                 int32_t result);

  // Callback method for the Close Mojo IPC call.  This method resets the Mojo
  // connection and closes the camera device.
  void OnClosed(int32_t result);

  // Resets the Mojo interface and bindings.
  void ResetMojoInterface();

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
  void ConfigureStreams(bool require_photo,
                        absl::optional<gfx::Size> new_blob_resolution);
  void OnConfiguredStreams(
      gfx::Size blob_resolution,
      int32_t result,
      cros::mojom::Camera3StreamConfigurationPtr updated_config);

  // Checks metadata in |static_metadata_| to ensure field
  // request.availableCapabilities contains YUV reprocessing and field
  // scaler.availableInputOutputFormatsMap contains YUV => BLOB mapping.
  // If above checks both pass, fill the max yuv width and height in
  // |max_width| and |max_height| and return true if both width and height are
  // positive numbers. Return false otherwise.
  bool IsYUVReprocessingSupported(int* max_width, int* max_height);

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

  gfx::Size GetBlobResolution(absl::optional<gfx::Size> new_blob_resolution);

  // StreamCaptureInterface implementations.  These methods are called by
  // |stream_buffer_manager_| on |ipc_task_runner_|.
  void ProcessCaptureRequest(cros::mojom::Camera3CaptureRequestPtr request,
                             base::OnceCallback<void(int32_t)> callback);
  void Flush(base::OnceCallback<void(int32_t)> callback);

  bool SetPointsOfInterest(
      const std::vector<mojom::Point2DPtr>& points_of_interest);

  // This function gets the TYPE_INT32[3] array of [max, min, step] from static
  // metadata by |range_name| and current value of |current|.
  mojom::RangePtr GetControlRangeByVendorTagName(
      const std::string& range_name,
      const absl::optional<int32_t>& current);

  bool ShouldUseBlobVideoSnapshot();

  // CaptureMetadataDispatcher::ResultMetadataObserver implementation.
  void OnResultMetadataAvailable(
      uint32_t frame_number,
      const cros::mojom::CameraMetadataPtr& result_metadata) final;

  // media::CameraEffectObserver AddObserver callback.
  void OnCameraEffectObserverAdded(
      cros::mojom::EffectsConfigPtr current_effects);

  // media::CameraEffectObserver implementation.
  void OnCameraEffectChanged(
      const cros::mojom::EffectsConfigPtr& new_effects) final;

  void DoGetPhotoState(VideoCaptureDevice::GetPhotoStateCallback callback);

  // Gets the target frame rate range as std::pair<min, max>.
  // Returns 0 for min or max or both fps when fps range is not valid,
  // caller should handle that accordingly.
  std::pair<int32_t, int32_t> GetFrameRateRange();

  // Configures the session_parameters with initial values for the keys
  // found in ANDROID_REQUEST_AVAILABLE_SESSION_KEYS.
  void ConfigureSessionParameters(
      cros::mojom::CameraMetadataPtr* session_parameters);

  const VideoCaptureDeviceDescriptor device_descriptor_;

  // Current configured resolution of BLOB stream.
  gfx::Size current_blob_resolution_;

  CameraHalDelegate* camera_hal_delegate_;

  // Map client type to video capture parameter.
  base::flat_map<ClientType, VideoCaptureParams> chrome_capture_params_;

  CameraDeviceContext* device_context_;

  std::queue<VideoCaptureDevice::TakePhotoCallback> take_photo_callbacks_;

  std::unique_ptr<RequestManager> request_manager_;

  std::unique_ptr<Camera3AController> camera_3a_controller_;

  // Stores the static camera characteristics of the camera device. E.g. the
  // supported formats and resolution, various available exposure and apeture
  // settings, etc.
  cros::mojom::CameraMetadataPtr static_metadata_;

  // Records current effects that is applied to camera hal server.
  cros::mojom::EffectsConfigPtr current_effects_;

  mojo::Remote<cros::mojom::Camera3DeviceOps> device_ops_;

  // Where all the Mojo IPC calls takes place.
  const scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

  base::OnceClosure device_close_callback_;

  std::queue<base::OnceClosure> on_reconfigured_callbacks_;

  uint32_t device_api_version_;

  // States of SetPhotoOptions
  bool is_set_awb_mode_;
  bool is_set_brightness_;
  bool is_set_contrast_;
  bool is_set_exposure_compensation_;
  bool is_set_exposure_time_;
  bool is_set_focus_distance_;
  bool is_set_iso_;
  bool is_set_pan_;
  bool is_set_saturation_;
  bool is_set_sharpness_;
  bool is_set_tilt_;
  bool is_set_zoom_;

  // Whether |this| is added to camera effect observer list.
  bool camera_effect_observer_added_;

  std::vector<base::OnceClosure> get_photo_state_queue_;
  bool use_digital_zoom_;
  float ae_compensation_step_;

  // We reply GetPhotoState when |result_metadata_frame_number_| >
  // |result_metadata_frame_number_for_photo_state_|. Otherwise javascript API
  // getSettings() will get non-updated settings.
  // They call GetPhotoState after SetPhotoOptions, we don't have the related
  // result metadata that time.
  uint32_t current_request_frame_number_;
  uint32_t result_metadata_frame_number_for_photo_state_;
  uint32_t result_metadata_frame_number_;
  ResultMetadata result_metadata_;
  gfx::Rect active_array_size_;

  base::WeakPtrFactory<CameraDeviceDelegate> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_DEVICE_DELEGATE_H_
