// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_device_delegate.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/numerics/ranges.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/mojom/image_capture_types.h"
#include "media/capture/video/blob_utils.h"
#include "media/capture/video/chromeos/camera_3a_controller.h"
#include "media/capture/video/chromeos/camera_buffer_factory.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_hal_delegate.h"
#include "media/capture/video/chromeos/camera_metadata_utils.h"
#include "media/capture/video/chromeos/request_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace media {

namespace {

// The result of max_width and max_height could be zero if the stream
// is not in the pre-defined configuration.
void GetStreamResolutions(const cros::mojom::CameraMetadataPtr& static_metadata,
                          cros::mojom::Camera3StreamType stream_type,
                          cros::mojom::HalPixelFormat stream_format,
                          std::vector<gfx::Size>* resolutions) {
  const cros::mojom::CameraMetadataEntryPtr* stream_configurations =
      GetMetadataEntry(static_metadata,
                       cros::mojom::CameraMetadataTag::
                           ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
  DCHECK(stream_configurations);
  // The available stream configurations are stored as tuples of four int32s:
  // (hal_pixel_format, width, height, type) x n
  const size_t kStreamFormatOffset = 0;
  const size_t kStreamWidthOffset = 1;
  const size_t kStreamHeightOffset = 2;
  const size_t kStreamTypeOffset = 3;
  const size_t kStreamConfigurationSize = 4;
  int32_t* iter =
      reinterpret_cast<int32_t*>((*stream_configurations)->data.data());
  for (size_t i = 0; i < (*stream_configurations)->count;
       i += kStreamConfigurationSize) {
    auto format =
        static_cast<cros::mojom::HalPixelFormat>(iter[kStreamFormatOffset]);
    int32_t width = iter[kStreamWidthOffset];
    int32_t height = iter[kStreamHeightOffset];
    auto type =
        static_cast<cros::mojom::Camera3StreamType>(iter[kStreamTypeOffset]);
    iter += kStreamConfigurationSize;

    if (type != stream_type || format != stream_format) {
      continue;
    }

    resolutions->emplace_back(width, height);
  }

  std::sort(resolutions->begin(), resolutions->end(),
            [](const gfx::Size& a, const gfx::Size& b) -> bool {
              return a.width() * a.height() < b.width() * b.height();
            });
}

// VideoCaptureDevice::TakePhotoCallback is given by the application and is used
// to return the captured JPEG blob buffer.  The second base::OnceClosure is
// created locally by the caller of TakePhoto(), and can be used to, for
// example, restore some settings to the values before TakePhoto() is called to
// facilitate the switch between photo and non-photo modes.
void TakePhotoCallbackBundle(VideoCaptureDevice::TakePhotoCallback callback,
                             base::OnceClosure on_photo_taken_callback,
                             mojom::BlobPtr blob) {
  std::move(callback).Run(std::move(blob));
  std::move(on_photo_taken_callback).Run();
}

}  // namespace

bool IsInputStream(StreamType stream_type) {
  // Currently there is only one input stream. Modify this method if there is
  // any other input streams.
  return stream_type == StreamType::kYUVInput;
}

StreamType StreamIdToStreamType(uint64_t stream_id) {
  switch (stream_id) {
    case 0:
      return StreamType::kPreviewOutput;
    case 1:
      return StreamType::kJpegOutput;
    case 2:
      return StreamType::kYUVInput;
    case 3:
      return StreamType::kYUVOutput;
    default:
      return StreamType::kUnknown;
  }
}  // namespace media

std::string StreamTypeToString(StreamType stream_type) {
  switch (stream_type) {
    case StreamType::kPreviewOutput:
      return std::string("StreamType::kPreviewOutput");
    case StreamType::kJpegOutput:
      return std::string("StreamType::kJpegOutput");
    case StreamType::kYUVInput:
      return std::string("StreamType::kYUVInput");
    case StreamType::kYUVOutput:
      return std::string("StreamType::kYUVOutput");
    default:
      return std::string("Unknown StreamType value: ") +
             base::NumberToString(static_cast<int32_t>(stream_type));
  }
}  // namespace media

std::ostream& operator<<(std::ostream& os, StreamType stream_type) {
  return os << StreamTypeToString(stream_type);
}

StreamCaptureInterface::Plane::Plane() = default;

StreamCaptureInterface::Plane::~Plane() = default;

class CameraDeviceDelegate::StreamCaptureInterfaceImpl final
    : public StreamCaptureInterface {
 public:
  StreamCaptureInterfaceImpl(
      base::WeakPtr<CameraDeviceDelegate> camera_device_delegate)
      : camera_device_delegate_(std::move(camera_device_delegate)) {}

  void ProcessCaptureRequest(cros::mojom::Camera3CaptureRequestPtr request,
                             base::OnceCallback<void(int32_t)> callback) final {
    if (camera_device_delegate_) {
      camera_device_delegate_->ProcessCaptureRequest(std::move(request),
                                                     std::move(callback));
    }
  }

  void Flush(base::OnceCallback<void(int32_t)> callback) final {
    if (camera_device_delegate_) {
      camera_device_delegate_->Flush(std::move(callback));
    }
  }

 private:
  const base::WeakPtr<CameraDeviceDelegate> camera_device_delegate_;
};

CameraDeviceDelegate::CameraDeviceDelegate(
    VideoCaptureDeviceDescriptor device_descriptor,
    scoped_refptr<CameraHalDelegate> camera_hal_delegate,
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner,
    CameraAppDeviceImpl* camera_app_device)
    : device_descriptor_(device_descriptor),
      camera_hal_delegate_(std::move(camera_hal_delegate)),
      ipc_task_runner_(std::move(ipc_task_runner)),
      camera_app_device_(camera_app_device) {}

CameraDeviceDelegate::~CameraDeviceDelegate() = default;

void CameraDeviceDelegate::AllocateAndStart(
    const VideoCaptureParams& params,
    CameraDeviceContext* device_context) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  chrome_capture_params_ = params;
  device_context_ = device_context;
  device_context_->SetState(CameraDeviceContext::State::kStarting);

  auto camera_info = camera_hal_delegate_->GetCameraInfoFromDeviceId(
      device_descriptor_.device_id);
  if (camera_info.is_null()) {
    device_context_->SetErrorState(
        media::VideoCaptureError::kCrosHalV3DeviceDelegateFailedToGetCameraInfo,
        FROM_HERE, "Failed to get camera info");
    return;
  }

  SortCameraMetadata(&camera_info->static_camera_characteristics);
  static_metadata_ = std::move(camera_info->static_camera_characteristics);

  auto sensor_orientation = GetMetadataEntryAsSpan<int32_t>(
      static_metadata_,
      cros::mojom::CameraMetadataTag::ANDROID_SENSOR_ORIENTATION);
  if (sensor_orientation.empty()) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3DeviceDelegateMissingSensorOrientationInfo,
        FROM_HERE, "Camera is missing required sensor orientation info");
    return;
  }
  device_context_->SetSensorOrientation(sensor_orientation[0]);

  // |device_ops_| is bound after the BindNewPipeAndPassReceiver call.
  camera_hal_delegate_->OpenDevice(
      camera_hal_delegate_->GetCameraIdFromDeviceId(
          device_descriptor_.device_id),
      device_ops_.BindNewPipeAndPassReceiver(),
      BindToCurrentLoop(
          base::BindOnce(&CameraDeviceDelegate::OnOpenedDevice, GetWeakPtr())));
  device_ops_.set_disconnect_handler(base::BindOnce(
      &CameraDeviceDelegate::OnMojoConnectionError, GetWeakPtr()));
}

void CameraDeviceDelegate::StopAndDeAllocate(
    base::OnceClosure device_close_callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!device_context_ ||
      device_context_->GetState() == CameraDeviceContext::State::kStopped ||
      (device_context_->GetState() == CameraDeviceContext::State::kError &&
       !request_manager_)) {
    // In case of Mojo connection error the device may be stopped before
    // StopAndDeAllocate is called; in case of device open failure, the state
    // is set to kError and |request_manager_| is uninitialized.
    std::move(device_close_callback).Run();
    return;
  }

  // StopAndDeAllocate may be called at any state except
  // CameraDeviceContext::State::kStopping.
  DCHECK_NE(device_context_->GetState(), CameraDeviceContext::State::kStopping);

  device_close_callback_ = std::move(device_close_callback);
  device_context_->SetState(CameraDeviceContext::State::kStopping);
  if (device_ops_.is_bound()) {
    device_ops_->Close(
        base::BindOnce(&CameraDeviceDelegate::OnClosed, GetWeakPtr()));
  }
  // |request_manager_| might be still uninitialized if StopAndDeAllocate() is
  // called before the device is opened.
  if (request_manager_) {
    request_manager_->StopPreview(base::NullCallback());
  }
}

void CameraDeviceDelegate::TakePhoto(
    VideoCaptureDevice::TakePhotoCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  take_photo_callbacks_.push(std::move(callback));

  if (!device_context_ ||
      (device_context_->GetState() !=
           CameraDeviceContext::State::kStreamConfigured &&
       device_context_->GetState() != CameraDeviceContext::State::kCapturing)) {
    return;
  }

  TakePhotoImpl();
}

void CameraDeviceDelegate::GetPhotoState(
    VideoCaptureDevice::GetPhotoStateCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  auto photo_state = mojo::CreateEmptyPhotoState();

  if (!device_context_ ||
      (device_context_->GetState() !=
           CameraDeviceContext::State::kStreamConfigured &&
       device_context_->GetState() != CameraDeviceContext::State::kCapturing)) {
    std::move(callback).Run(std::move(photo_state));
    return;
  }

  std::vector<gfx::Size> blob_resolutions;
  GetStreamResolutions(
      static_metadata_, cros::mojom::Camera3StreamType::CAMERA3_STREAM_OUTPUT,
      cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_BLOB, &blob_resolutions);
  if (blob_resolutions.empty()) {
    std::move(callback).Run(std::move(photo_state));
    return;
  }

  // Sets the correct range of min/max resolution in order to bypass checks that
  // the resolution caller request should fall within the range when taking
  // photos. And since we are not actually use the mechanism to get other
  // resolutions, we set the step to 0.0 here.
  photo_state->width->current = current_blob_resolution_.width();
  photo_state->width->min = blob_resolutions.front().width();
  photo_state->width->max = blob_resolutions.back().width();
  photo_state->width->step = 0.0;
  photo_state->height->current = current_blob_resolution_.height();
  photo_state->height->min = blob_resolutions.front().height();
  photo_state->height->max = blob_resolutions.back().height();
  photo_state->height->step = 0.0;
  std::move(callback).Run(std::move(photo_state));
}

// On success, invokes |callback| with value |true|. On failure, drops
// callback without invoking it.
void CameraDeviceDelegate::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    VideoCaptureDevice::SetPhotoOptionsCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() != CameraDeviceContext::State::kCapturing) {
    return;
  }

  bool ret = SetPointsOfInterest(settings->points_of_interest);
  if (!ret) {
    LOG(ERROR) << "Failed to set points of interest";
    return;
  }

  bool is_resolution_specified = settings->has_width && settings->has_height;
  bool should_reconfigure_streams =
      is_resolution_specified && (current_blob_resolution_.IsEmpty() ||
                                  current_blob_resolution_.width() !=
                                      static_cast<int32_t>(settings->width) ||
                                  current_blob_resolution_.height() !=
                                      static_cast<int32_t>(settings->height));
  if (!request_manager_->HasStreamsConfiguredForTakePhoto() ||
      should_reconfigure_streams) {
    if (is_resolution_specified) {
      gfx::Size new_blob_resolution(static_cast<int32_t>(settings->width),
                                    static_cast<int32_t>(settings->height));
      request_manager_->StopPreview(
          base::BindOnce(&CameraDeviceDelegate::OnFlushed, GetWeakPtr(),
                         std::move(new_blob_resolution)));
    } else {
      request_manager_->StopPreview(base::BindOnce(
          &CameraDeviceDelegate::OnFlushed, GetWeakPtr(), base::nullopt));
    }
    set_photo_option_callback_ = std::move(callback);
  } else {
    set_photo_option_callback_.Reset();
    std::move(callback).Run(true);
  }
}

void CameraDeviceDelegate::SetRotation(int rotation) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(rotation >= 0 && rotation < 360 && rotation % 90 == 0);
  device_context_->SetScreenRotation(rotation);
}

base::WeakPtr<CameraDeviceDelegate> CameraDeviceDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void CameraDeviceDelegate::TakePhotoImpl() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  auto construct_request_cb =
      base::BindOnce(&CameraDeviceDelegate::ConstructDefaultRequestSettings,
                     GetWeakPtr(), StreamType::kJpegOutput);

  if (request_manager_->HasStreamsConfiguredForTakePhoto()) {
    camera_3a_controller_->Stabilize3AForStillCapture(
        std::move(construct_request_cb));
    return;
  }

  SetPhotoOptions(
      mojom::PhotoSettings::New(),
      base::BindOnce(
          [](base::WeakPtr<Camera3AController> controller,
             base::OnceClosure callback, bool result) {
            controller->Stabilize3AForStillCapture(std::move(callback));
          },
          camera_3a_controller_->GetWeakPtr(),
          std::move(construct_request_cb)));
}

void CameraDeviceDelegate::OnMojoConnectionError() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() == CameraDeviceContext::State::kStopping) {
    // When in stopping state the camera HAL adapter may terminate the Mojo
    // channel before we do, in which case the OnClosed callback will not be
    // called.
    OnClosed(0);
  } else {
    // The Mojo channel terminated unexpectedly.
    if (request_manager_) {
      request_manager_->StopPreview(base::NullCallback());
    }
    device_context_->SetState(CameraDeviceContext::State::kStopped);
    device_context_->SetErrorState(
        media::VideoCaptureError::kCrosHalV3DeviceDelegateMojoConnectionError,
        FROM_HERE, "Mojo connection error");
    ResetMojoInterface();
    // We cannot reset |device_context_| here because
    // |device_context_->SetErrorState| above will call StopAndDeAllocate later
    // to handle the class destruction.
  }
}

void CameraDeviceDelegate::OnFlushed(
    base::Optional<gfx::Size> new_blob_resolution,
    int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  if (result) {
    device_context_->SetErrorState(
        media::VideoCaptureError::kCrosHalV3DeviceDelegateFailedToFlush,
        FROM_HERE,
        std::string("Flush failed: ") + base::safe_strerror(-result));
    return;
  }
  device_context_->SetState(CameraDeviceContext::State::kInitialized);
  ConfigureStreams(true, std::move(new_blob_resolution));
}

void CameraDeviceDelegate::OnClosed(int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(device_context_->GetState(), CameraDeviceContext::State::kStopping);

  device_context_->SetState(CameraDeviceContext::State::kStopped);
  if (result) {
    device_context_->LogToClient(std::string("Failed to close device: ") +
                                 base::safe_strerror(-result));
  }
  ResetMojoInterface();
  device_context_ = nullptr;
  current_blob_resolution_.SetSize(0, 0);
  std::move(device_close_callback_).Run();
}

void CameraDeviceDelegate::ResetMojoInterface() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  device_ops_.reset();
  camera_3a_controller_.reset();
  request_manager_.reset();
}

void CameraDeviceDelegate::OnOpenedDevice(int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_ == nullptr) {
    // The OnOpenedDevice() might be called after the camera device already
    // closed by StopAndDeAllocate(), because |camera_hal_delegate_| and
    // |device_ops_| are on different IPC channels. In such case,
    // OnMojoConnectionError() might call OnClosed() and the
    // |device_context_| is cleared, so we just return here.
    return;
  }

  if (device_context_->GetState() != CameraDeviceContext::State::kStarting) {
    if (device_context_->GetState() == CameraDeviceContext::State::kError) {
      // In case of camera open failed, the HAL can terminate the Mojo channel
      // before we do and set the state to kError in OnMojoConnectionError.
      return;
    }
    DCHECK_EQ(device_context_->GetState(),
              CameraDeviceContext::State::kStopping);
    OnClosed(0);
    return;
  }

  if (result) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3DeviceDelegateFailedToOpenCameraDevice,
        FROM_HERE, "Failed to open camera device");
    return;
  }
  Initialize();
}

void CameraDeviceDelegate::Initialize() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(device_context_->GetState(), CameraDeviceContext::State::kStarting);

  mojo::PendingRemote<cros::mojom::Camera3CallbackOps> callback_ops;
  request_manager_ = std::make_unique<RequestManager>(
      callback_ops.InitWithNewPipeAndPassReceiver(),
      std::make_unique<StreamCaptureInterfaceImpl>(GetWeakPtr()),
      device_context_, chrome_capture_params_.buffer_type,
      std::make_unique<CameraBufferFactory>(),
      base::BindRepeating(&RotateAndBlobify), ipc_task_runner_,
      camera_app_device_);
  camera_3a_controller_ = std::make_unique<Camera3AController>(
      static_metadata_, request_manager_.get(), ipc_task_runner_);
  device_ops_->Initialize(
      std::move(callback_ops),
      base::BindOnce(&CameraDeviceDelegate::OnInitialized, GetWeakPtr()));
}

void CameraDeviceDelegate::OnInitialized(int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() != CameraDeviceContext::State::kStarting) {
    DCHECK_EQ(device_context_->GetState(),
              CameraDeviceContext::State::kStopping);
    return;
  }
  if (result) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice,
        FROM_HERE,
        std::string("Failed to initialize camera device: ") +
            base::safe_strerror(-result));
    return;
  }
  device_context_->SetState(CameraDeviceContext::State::kInitialized);
  bool require_photo = [&] {
    if (camera_app_device_ == nullptr) {
      return false;
    }
    auto capture_intent = camera_app_device_->GetCaptureIntent();
    switch (capture_intent) {
      case cros::mojom::CaptureIntent::DEFAULT:
        return false;
      case cros::mojom::CaptureIntent::STILL_CAPTURE:
        return true;
      case cros::mojom::CaptureIntent::VIDEO_RECORD:
        return false;
      default:
        NOTREACHED() << "Unknown capture intent: " << capture_intent;
        return false;
    }
  }();
  ConfigureStreams(require_photo, base::nullopt);
}

void CameraDeviceDelegate::ConfigureStreams(
    bool require_photo,
    base::Optional<gfx::Size> new_blob_resolution) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(device_context_->GetState(),
            CameraDeviceContext::State::kInitialized);

  // Set up context for preview stream.
  cros::mojom::Camera3StreamPtr preview_stream =
      cros::mojom::Camera3Stream::New();
  preview_stream->id = static_cast<uint64_t>(StreamType::kPreviewOutput);
  preview_stream->stream_type =
      cros::mojom::Camera3StreamType::CAMERA3_STREAM_OUTPUT;
  preview_stream->width =
      chrome_capture_params_.requested_format.frame_size.width();
  preview_stream->height =
      chrome_capture_params_.requested_format.frame_size.height();
  preview_stream->format =
      cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888;
  preview_stream->usage = cros::mojom::GRALLOC_USAGE_HW_COMPOSER;
  preview_stream->data_space = 0;
  preview_stream->rotation =
      cros::mojom::Camera3StreamRotation::CAMERA3_STREAM_ROTATION_0;

  cros::mojom::Camera3StreamConfigurationPtr stream_config =
      cros::mojom::Camera3StreamConfiguration::New();
  stream_config->streams.push_back(std::move(preview_stream));

  // Set up context for still capture stream. We set still capture stream to the
  // JPEG stream configuration with maximum supported resolution.
  int32_t blob_width = 0;
  int32_t blob_height = 0;
  if (require_photo) {
    std::vector<gfx::Size> blob_resolutions;
    GetStreamResolutions(
        static_metadata_, cros::mojom::Camera3StreamType::CAMERA3_STREAM_OUTPUT,
        cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_BLOB, &blob_resolutions);
    if (blob_resolutions.empty()) {
      LOG(ERROR) << "Failed to configure streans: No BLOB resolution found.";
      return;
    }

    if (new_blob_resolution.has_value() &&
        std::find(blob_resolutions.begin(), blob_resolutions.end(),
                  *new_blob_resolution) != blob_resolutions.end()) {
      blob_width = new_blob_resolution->width();
      blob_height = new_blob_resolution->height();
    } else {
      // Use the largest resolution as default.
      blob_width = blob_resolutions.back().width();
      blob_height = blob_resolutions.back().height();
    }
    cros::mojom::Camera3StreamPtr still_capture_stream =
        cros::mojom::Camera3Stream::New();
    still_capture_stream->id = static_cast<uint64_t>(StreamType::kJpegOutput);
    still_capture_stream->stream_type =
        cros::mojom::Camera3StreamType::CAMERA3_STREAM_OUTPUT;
    still_capture_stream->width = blob_width;
    still_capture_stream->height = blob_height;
    still_capture_stream->format =
        cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_BLOB;
    // Set usage flag to allow HAL adapter to identify a still capture stream.
    still_capture_stream->usage = cros::mojom::GRALLOC_USAGE_STILL_CAPTURE;
    still_capture_stream->data_space = 0;
    still_capture_stream->rotation =
        cros::mojom::Camera3StreamRotation::CAMERA3_STREAM_ROTATION_0;
    stream_config->streams.push_back(std::move(still_capture_stream));

    int32_t max_yuv_width = 0, max_yuv_height = 0;
    if (IsYUVReprocessingSupported(&max_yuv_width, &max_yuv_height)) {
      auto reprocessing_stream_input = cros::mojom::Camera3Stream::New();
      reprocessing_stream_input->id =
          static_cast<uint64_t>(StreamType::kYUVInput);
      reprocessing_stream_input->stream_type =
          cros::mojom::Camera3StreamType::CAMERA3_STREAM_INPUT;
      reprocessing_stream_input->width = max_yuv_width;
      reprocessing_stream_input->height = max_yuv_height;
      reprocessing_stream_input->format =
          cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888;
      reprocessing_stream_input->data_space = 0;
      reprocessing_stream_input->rotation =
          cros::mojom::Camera3StreamRotation::CAMERA3_STREAM_ROTATION_0;

      auto reprocessing_stream_output = cros::mojom::Camera3Stream::New();
      reprocessing_stream_output->id =
          static_cast<uint64_t>(StreamType::kYUVOutput);
      reprocessing_stream_output->stream_type =
          cros::mojom::Camera3StreamType::CAMERA3_STREAM_OUTPUT;
      reprocessing_stream_output->width = max_yuv_width;
      reprocessing_stream_output->height = max_yuv_height;
      reprocessing_stream_output->format =
          cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888;
      // Set usage flag to allow HAL adapter to identify a still capture stream.
      reprocessing_stream_output->usage =
          cros::mojom::GRALLOC_USAGE_STILL_CAPTURE;
      reprocessing_stream_output->data_space = 0;
      reprocessing_stream_output->rotation =
          cros::mojom::Camera3StreamRotation::CAMERA3_STREAM_ROTATION_0;

      stream_config->streams.push_back(std::move(reprocessing_stream_input));
      stream_config->streams.push_back(std::move(reprocessing_stream_output));
    }
  }

  stream_config->operation_mode = cros::mojom::Camera3StreamConfigurationMode::
      CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE;
  device_ops_->ConfigureStreams(
      std::move(stream_config),
      base::BindOnce(&CameraDeviceDelegate::OnConfiguredStreams, GetWeakPtr(),
                     gfx::Size(blob_width, blob_height)));
}

void CameraDeviceDelegate::OnConfiguredStreams(
    gfx::Size blob_resolution,
    int32_t result,
    cros::mojom::Camera3StreamConfigurationPtr updated_config) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() != CameraDeviceContext::State::kInitialized) {
    DCHECK_EQ(device_context_->GetState(),
              CameraDeviceContext::State::kStopping);
    return;
  }
  if (result) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3DeviceDelegateFailedToConfigureStreams,
        FROM_HERE,
        std::string("Failed to configure streams: ") +
            base::safe_strerror(-result));
    return;
  }
  if (!updated_config ||
      updated_config->streams.size() > kMaxConfiguredStreams ||
      updated_config->streams.size() < kMinConfiguredStreams) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3DeviceDelegateWrongNumberOfStreamsConfigured,
        FROM_HERE,
        std::string("Wrong number of streams configured: ") +
            base::NumberToString(updated_config->streams.size()));
    return;
  }

  current_blob_resolution_.SetSize(blob_resolution.width(),
                                   blob_resolution.height());

  request_manager_->SetUpStreamsAndBuffers(
      chrome_capture_params_.requested_format, static_metadata_,
      std::move(updated_config->streams));

  device_context_->SetState(CameraDeviceContext::State::kStreamConfigured);
  // Kick off the preview stream.
  ConstructDefaultRequestSettings(StreamType::kPreviewOutput);
}

bool CameraDeviceDelegate::IsYUVReprocessingSupported(int* max_width,
                                                      int* max_height) {
  bool has_yuv_reprocessing_capability = [&] {
    auto capabilities = GetMetadataEntryAsSpan<uint8_t>(
        static_metadata_,
        cros::mojom::CameraMetadataTag::ANDROID_REQUEST_AVAILABLE_CAPABILITIES);
    auto capability_yuv_reprocessing = static_cast<uint8_t>(
        cros::mojom::AndroidRequestAvailableCapabilities::
            ANDROID_REQUEST_AVAILABLE_CAPABILITIES_YUV_REPROCESSING);
    for (auto capability : capabilities) {
      if (capability == capability_yuv_reprocessing) {
        return true;
      }
    }
    return false;
  }();

  if (!has_yuv_reprocessing_capability) {
    return false;
  }

  bool has_yuv_input_blob_output = [&] {
    auto formats_map = GetMetadataEntryAsSpan<int32_t>(
        static_metadata_,
        cros::mojom::CameraMetadataTag::
            ANDROID_SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP);
    // The formats map looks like: [
    //   {INPUT_FORMAT, NUM_OF_OUTPUTS, OUTPUT_FORMAT_1, OUTPUT_FORMAT_2, ...},
    //   {...},
    //   ...
    // ]
    auto format_yuv = static_cast<int32_t>(
        cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888);
    auto format_blob = static_cast<int32_t>(
        cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_BLOB);

    size_t idx = 0;
    while (idx < formats_map.size() && !has_yuv_input_blob_output) {
      auto in_format = formats_map[idx++];
      auto out_amount = formats_map[idx++];
      if (in_format != format_yuv) {
        idx += out_amount;
        continue;
      }
      for (size_t idx_end = idx + out_amount; idx < idx_end; idx++) {
        auto out_format = formats_map[idx];
        if (out_format == format_blob) {
          return true;
        }
      }
    }
    return false;
  }();

  if (!has_yuv_input_blob_output) {
    return false;
  }

  std::vector<gfx::Size> yuv_resolutions;
  GetStreamResolutions(
      static_metadata_, cros::mojom::Camera3StreamType::CAMERA3_STREAM_INPUT,
      cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888,
      &yuv_resolutions);
  if (yuv_resolutions.empty()) {
    return false;
  }
  *max_width = yuv_resolutions.back().width();
  *max_height = yuv_resolutions.back().height();
  return true;
}

void CameraDeviceDelegate::ConstructDefaultRequestSettings(
    StreamType stream_type) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(device_context_->GetState() ==
             CameraDeviceContext::State::kStreamConfigured ||
         device_context_->GetState() == CameraDeviceContext::State::kCapturing);

  if (stream_type == StreamType::kPreviewOutput) {
    // CCA uses the same stream for preview and video recording. Choose proper
    // template here so the underlying camera HAL can set 3A tuning accordingly.
    auto request_template =
        camera_app_device_ && camera_app_device_->GetCaptureIntent() ==
                                  cros::mojom::CaptureIntent::VIDEO_RECORD
            ? cros::mojom::Camera3RequestTemplate::CAMERA3_TEMPLATE_VIDEO_RECORD
            : cros::mojom::Camera3RequestTemplate::CAMERA3_TEMPLATE_PREVIEW;
    device_ops_->ConstructDefaultRequestSettings(
        request_template,
        base::BindOnce(
            &CameraDeviceDelegate::OnConstructedDefaultPreviewRequestSettings,
            GetWeakPtr()));
  } else if (stream_type == StreamType::kJpegOutput) {
    device_ops_->ConstructDefaultRequestSettings(
        cros::mojom::Camera3RequestTemplate::CAMERA3_TEMPLATE_STILL_CAPTURE,
        base::BindOnce(&CameraDeviceDelegate::
                           OnConstructedDefaultStillCaptureRequestSettings,
                       GetWeakPtr()));
  } else {
    NOTREACHED() << "No default request settings for stream: " << stream_type;
  }
}

void CameraDeviceDelegate::OnConstructedDefaultPreviewRequestSettings(
    cros::mojom::CameraMetadataPtr settings) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() !=
      CameraDeviceContext::State::kStreamConfigured) {
    DCHECK_EQ(device_context_->GetState(),
              CameraDeviceContext::State::kStopping);
    return;
  }
  if (!settings) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3DeviceDelegateFailedToGetDefaultRequestSettings,
        FROM_HERE, "Failed to get default request settings");
    return;
  }

  if (camera_app_device_) {
    camera_app_device_->GetFpsRange(
        chrome_capture_params_.requested_format.frame_size,
        media::BindToCurrentLoop(
            base::BindOnce(&CameraDeviceDelegate::OnGotFpsRange, GetWeakPtr(),
                           std::move(settings))));
  } else {
    OnGotFpsRange(std::move(settings), {});
  }
}

void CameraDeviceDelegate::OnConstructedDefaultStillCaptureRequestSettings(
    cros::mojom::CameraMetadataPtr settings) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  while (!take_photo_callbacks_.empty()) {
    auto take_photo_callback = base::BindOnce(
        &TakePhotoCallbackBundle, std::move(take_photo_callbacks_.front()),
        base::BindOnce(&Camera3AController::SetAutoFocusModeForStillCapture,
                       camera_3a_controller_->GetWeakPtr()));
    if (camera_app_device_) {
      camera_app_device_->ConsumeReprocessOptions(
          std::move(take_photo_callback),
          media::BindToCurrentLoop(base::BindOnce(
              &RequestManager::TakePhoto, request_manager_->GetWeakPtr(),
              settings.Clone())));
    } else {
      request_manager_->TakePhoto(
          settings.Clone(), CameraAppDeviceImpl::GetSingleShotReprocessOptions(
                                std::move(take_photo_callback)));
    }
    take_photo_callbacks_.pop();
  }
}

void CameraDeviceDelegate::OnGotFpsRange(
    cros::mojom::CameraMetadataPtr settings,
    base::Optional<gfx::Range> specified_fps_range) {
  device_context_->SetState(CameraDeviceContext::State::kCapturing);
  camera_3a_controller_->SetAutoFocusModeForStillCapture();
  if (specified_fps_range.has_value()) {
    const int32_t entry_length = 2;

    // CameraMetadata is represented as an uint8 array. According to the
    // definition of the FPS metadata tag, its data type is int32, so we
    // reinterpret_cast here.
    std::vector<uint8_t> fps_range(sizeof(int32_t) * entry_length);
    auto* fps_ptr = reinterpret_cast<int32_t*>(fps_range.data());
    fps_ptr[0] = specified_fps_range->GetMin();
    fps_ptr[1] = specified_fps_range->GetMax();
    cros::mojom::CameraMetadataEntryPtr e =
        cros::mojom::CameraMetadataEntry::New();
    e->tag =
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_TARGET_FPS_RANGE;
    e->type = cros::mojom::EntryType::TYPE_INT32;
    e->count = entry_length;
    e->data = std::move(fps_range);

    AddOrUpdateMetadataEntry(&settings, std::move(e));
  }
  request_manager_->StartPreview(std::move(settings));

  if (!take_photo_callbacks_.empty()) {
    TakePhotoImpl();
  }

  if (set_photo_option_callback_) {
    std::move(set_photo_option_callback_).Run(true);
  }
}

void CameraDeviceDelegate::ProcessCaptureRequest(
    cros::mojom::Camera3CaptureRequestPtr request,
    base::OnceCallback<void(int32_t)> callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  for (const auto& output_buffer : request->output_buffers) {
    TRACE_EVENT2("camera", "Capture Request", "frame_number",
                 request->frame_number, "stream_id", output_buffer->stream_id);
  }

  if (device_context_->GetState() != CameraDeviceContext::State::kCapturing) {
    DCHECK_EQ(device_context_->GetState(),
              CameraDeviceContext::State::kStopping);
    return;
  }
  device_ops_->ProcessCaptureRequest(std::move(request), std::move(callback));
}

void CameraDeviceDelegate::Flush(base::OnceCallback<void(int32_t)> callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  device_ops_->Flush(std::move(callback));
}

bool CameraDeviceDelegate::SetPointsOfInterest(
    const std::vector<mojom::Point2DPtr>& points_of_interest) {
  if (points_of_interest.empty()) {
    return true;
  }

  if (!camera_3a_controller_->IsPointOfInterestSupported()) {
    LOG(WARNING) << "Point of interest is not supported on this device.";
    return false;
  }

  if (points_of_interest.size() > 1) {
    LOG(WARNING) << "Setting more than one point of interest is not "
                    "supported, only the first one is used.";
  }

  // A Point2D Point of Interest is interpreted to represent a pixel position in
  // a normalized square space (|{x,y} ∈ [0.0, 1.0]|). The origin of coordinates
  // |{x,y} = {0.0, 0.0}| represents the upper leftmost corner whereas the
  // |{x,y} = {1.0, 1.0}| represents the lower rightmost corner: the x
  // coordinate (columns) increases rightwards and the y coordinate (rows)
  // increases downwards. Values beyond the mentioned limits will be clamped to
  // the closest allowed value.
  // ref: https://www.w3.org/TR/image-capture/#points-of-interest

  double x = base::ClampToRange(points_of_interest[0]->x, 0.0, 1.0);
  double y = base::ClampToRange(points_of_interest[0]->y, 0.0, 1.0);

  // Handle rotation, still in normalized square space.
  std::tie(x, y) = [&]() -> std::pair<double, double> {
    switch (device_context_->GetCameraFrameOrientation()) {
      case 0:
        return {x, y};
      case 90:
        return {y, 1.0 - x};
      case 180:
        return {1.0 - x, 1.0 - y};
      case 270:
        return {1.0 - y, x};
      default:
        NOTREACHED() << "Invalid orientation";
    }
    return {x, y};
  }();

  // TODO(shik): Respect to SCALER_CROP_REGION, which is unused now.

  auto active_array_size = [&]() {
    auto rect = GetMetadataEntryAsSpan<int32_t>(
        static_metadata_,
        cros::mojom::CameraMetadataTag::ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);
    // (xmin, ymin, width, height)
    return gfx::Rect(rect[0], rect[1], rect[2], rect[3]);
  }();

  x *= active_array_size.width() - 1;
  y *= active_array_size.height() - 1;
  gfx::Point point = {static_cast<int>(x), static_cast<int>(y)};
  camera_3a_controller_->SetPointOfInterest(point);
  return true;
}

}  // namespace media
