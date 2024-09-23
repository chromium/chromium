// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/apple/video_capture_device_apple.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/timestamp_constants.h"
#include "media/capture/mojom/image_capture_types.h"
#import "media/capture/video/apple/video_capture_device_avfoundation.h"
#include "media/capture/video/apple/video_capture_device_avfoundation_utils.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_MAC)
#include "media/capture/video/mac/uvc_control_mac.h"
#endif

namespace media {

namespace {
// Apple specific limits for minimum and maximum frame rate.
const float kMinFrameRate = 1.0f;
const float kMaxFrameRate = 60.0f;
}  // namespace

VideoCaptureDeviceApple::VideoCaptureDeviceApple(
    const VideoCaptureDeviceDescriptor& device_descriptor)
    : device_descriptor_(device_descriptor),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      state_(kNotInitialized),
      capture_device_(nil),
      weak_factory_(this) {}

VideoCaptureDeviceApple::~VideoCaptureDeviceApple() {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void VideoCaptureDeviceApple::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceApple::AllocateAndStart");
  if (state_ != kIdle) {
    return;
  }

  client_ = std::move(client);
  if (device_descriptor_.capture_api == VideoCaptureApi::MACOSX_AVFOUNDATION) {
    LogMessage("Using AVFoundation for device: " +
               device_descriptor_.display_name());
  }

  NSString* deviceId = base::SysUTF8ToNSString(device_descriptor_.device_id);

  [capture_device_ setFrameReceiver:this];

  if (params.buffer_type == VideoCaptureBufferType::kGpuMemoryBuffer) {
    [capture_device_ setUseGPUMemoryBuffer:true];
  } else if (params.buffer_type == VideoCaptureBufferType::kSharedMemory) {
    [capture_device_ setUseGPUMemoryBuffer:false];
  }

  NSString* errorMessage = nil;
  if (![capture_device_ setCaptureDevice:deviceId errorMessage:&errorMessage]) {
    SetErrorState(VideoCaptureError::kMacSetCaptureDeviceFailed, FROM_HERE,
                  base::SysNSStringToUTF8(errorMessage));
    return;
  }

  capture_format_.frame_size = params.requested_format.frame_size;
  capture_format_.frame_rate =
      std::max(kMinFrameRate,
               std::min(params.requested_format.frame_rate, kMaxFrameRate));
  // Leave the pixel format selection to AVFoundation. The pixel format
  // will be passed to |ReceiveFrame|.
  capture_format_.pixel_format = PIXEL_FORMAT_UNKNOWN;

  if (!UpdateCaptureResolution()) {
    return;
  }

#if BUILDFLAG(IS_MAC)
  UvcControl::SetPowerLineFrequency(device_descriptor_, params);
#endif

  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                 "startCapture");
    if (![capture_device_ startCapture]) {
      SetErrorState(VideoCaptureError::kMacCouldNotStartCaptureDevice,
                    FROM_HERE, "Could not start capture device.");
      return;
    }
  }

  client_->OnStarted();
  state_ = kCapturing;
}

void VideoCaptureDeviceApple::StopAndDeAllocate() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kCapturing || state_ == kError) << state_;

  NSString* errorMessage = nil;
  if (![capture_device_ setCaptureDevice:nil errorMessage:&errorMessage]) {
    LogMessage(base::SysNSStringToUTF8(errorMessage));
  }

  [capture_device_ setFrameReceiver:nil];
  client_.reset();
  state_ = kIdle;
}

void VideoCaptureDeviceApple::TakePhoto(TakePhotoCallback callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kCapturing) << state_;

  if (photo_callback_) {  // Only one picture can be in flight at a time.
    return;
  }

  photo_callback_ = std::move(callback);
  [capture_device_ takePhoto];
}

void VideoCaptureDeviceApple::GetPhotoState(GetPhotoStateCallback callback) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceApple::GetPhotoState");
  DCHECK(task_runner_->BelongsToCurrentThread());

  auto photo_state = mojo::CreateEmptyPhotoState();

  photo_state->height = mojom::Range::New(
      capture_format_.frame_size.height(), capture_format_.frame_size.height(),
      capture_format_.frame_size.height(), 0 /* step */);
  photo_state->width = mojom::Range::New(
      capture_format_.frame_size.width(), capture_format_.frame_size.width(),
      capture_format_.frame_size.width(), 0 /* step */);

#if BUILDFLAG(IS_MAC)
  UvcControl::GetPhotoState(photo_state, device_descriptor_);
#endif
  if ([capture_device_ isPortraitEffectSupported]) {
    bool isPortraitEffectActive = [capture_device_ isPortraitEffectActive];
    photo_state->supported_background_blur_modes = {
        isPortraitEffectActive ? mojom::BackgroundBlurMode::BLUR
                               : mojom::BackgroundBlurMode::OFF};
    photo_state->background_blur_mode = isPortraitEffectActive
                                            ? mojom::BackgroundBlurMode::BLUR
                                            : mojom::BackgroundBlurMode::OFF;
  }

  std::move(callback).Run(std::move(photo_state));
}

void VideoCaptureDeviceApple::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceApple::SetPhotoOptions");
  DCHECK(task_runner_->BelongsToCurrentThread());
  // Drop |callback| and return if there are any unsupported |settings|.
  // TODO(mcasas): centralise checks elsewhere, https://crbug.com/724285.
  if ((settings->has_width &&
       settings->width != capture_format_.frame_size.width()) ||
      (settings->has_height &&
       settings->height != capture_format_.frame_size.height()) ||
      settings->has_fill_light_mode || settings->has_red_eye_reduction) {
    return;
  }

  // Abort if background blur does not have already the desired value.
  if (settings->has_background_blur_mode &&
      (![capture_device_ isPortraitEffectSupported] ||
       settings->background_blur_mode !=
           ([capture_device_ isPortraitEffectActive]
                ? mojom::BackgroundBlurMode::BLUR
                : mojom::BackgroundBlurMode::OFF))) {
    return;
  }
#if BUILDFLAG(IS_MAC)
  UvcControl::SetPhotoState(settings, device_descriptor_);
#endif
  std::move(callback).Run(true);
}

bool VideoCaptureDeviceApple::Init(VideoCaptureApi capture_api_type) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kNotInitialized);

  if (capture_api_type != VideoCaptureApi::MACOSX_AVFOUNDATION) {
    return false;
  }

  capture_device_ =
      [[VideoCaptureDeviceAVFoundation alloc] initWithFrameReceiver:this];

  if (!capture_device_) {
    return false;
  }

  state_ = kIdle;
  return true;
}

void VideoCaptureDeviceApple::ReceiveFrame(
    const uint8_t* video_frame,
    int video_frame_length,
    const VideoCaptureFormat& frame_format,
    const gfx::ColorSpace color_space,
    int aspect_numerator,
    int aspect_denominator,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_time,
    int rotation) {
  if (capture_format_.frame_size != frame_format.frame_size) {
    ReceiveError(VideoCaptureError::kMacReceivedFrameWithUnexpectedResolution,
                 FROM_HERE,
                 "Captured resolution " + frame_format.frame_size.ToString() +
                     ", and expected " + capture_format_.frame_size.ToString());
    return;
  }

  client_->OnIncomingCapturedData(
      video_frame, video_frame_length, frame_format, color_space,
      rotation /* clockwise_rotation */, false /* flip_y */,
      base::TimeTicks::Now(), timestamp, capture_begin_time);
}

void VideoCaptureDeviceApple::ReceiveExternalGpuMemoryBufferFrame(
    CapturedExternalVideoBuffer frame,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_time) {
  if (capture_format_.frame_size != frame.format.frame_size) {
    ReceiveError(VideoCaptureError::kMacReceivedFrameWithUnexpectedResolution,
                 FROM_HERE,
                 "Captured resolution " + frame.format.frame_size.ToString() +
                     ", and expected " + capture_format_.frame_size.ToString());
    return;
  }
  client_->OnIncomingCapturedExternalBuffer(
      std::move(frame), base::TimeTicks::Now(), timestamp, capture_begin_time,
      gfx::Rect(capture_format_.frame_size));
}

void VideoCaptureDeviceApple::OnPhotoTaken(const uint8_t* image_data,
                                           size_t image_length,
                                           const std::string& mime_type) {
  DCHECK(photo_callback_);
  if (!image_data || !image_length) {
    OnPhotoError();
    return;
  }

  mojom::BlobPtr blob = mojom::Blob::New();
  blob->data.assign(image_data, image_data + image_length);
  blob->mime_type = mime_type;
  std::move(photo_callback_).Run(std::move(blob));
}

void VideoCaptureDeviceApple::OnPhotoError() {
  VLOG(1) << __func__ << " error taking picture";
  photo_callback_.Reset();
}

void VideoCaptureDeviceApple::ReceiveError(VideoCaptureError error,
                                           const base::Location& from_here,
                                           const std::string& reason) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoCaptureDeviceApple::SetErrorState,
                     weak_factory_.GetWeakPtr(), error, from_here, reason));
}

void VideoCaptureDeviceApple::LogMessage(const std::string& message) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (client_) {
    client_->OnLog(message);
  }
}

void VideoCaptureDeviceApple::ReceiveCaptureConfigurationChanged() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoCaptureDeviceApple::OnCaptureConfigurationChanged,
                     weak_factory_.GetWeakPtr()));
}

void VideoCaptureDeviceApple::OnLog(const std::string& message) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&VideoCaptureDeviceApple::LogMessage,
                                        weak_factory_.GetWeakPtr(), message));
}

void VideoCaptureDeviceApple::OnCaptureConfigurationChanged() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (client_) {
    client_->OnCaptureConfigurationChanged();
  }
}

void VideoCaptureDeviceApple::SetIsPortraitEffectSupportedForTesting(
    bool isPortraitEffectSupported) {
  [capture_device_
      setIsPortraitEffectSupportedForTesting:isPortraitEffectSupported];
}

void VideoCaptureDeviceApple::SetIsPortraitEffectActiveForTesting(
    bool isPortraitEffectActive) {
  [capture_device_ setIsPortraitEffectActiveForTesting:isPortraitEffectActive];
}

// static
std::string VideoCaptureDeviceApple::GetDeviceModelId(
    const std::string& device_id,
    VideoCaptureApi capture_api,
    VideoCaptureTransportType transport_type) {
#if BUILDFLAG(IS_MAC)
  return UvcControl::GetDeviceModelId(device_id, capture_api, transport_type);
#else
  return "";
#endif
}

// Check if the video capture device supports pan, tilt and zoom controls.
// static
VideoCaptureControlSupport VideoCaptureDeviceApple::GetControlSupport(
    const std::string& device_model) {
  VideoCaptureControlSupport control_support;
#if BUILDFLAG(IS_MAC)
  control_support = UvcControl::GetControlSupport(device_model);
#endif
  return control_support;
}

void VideoCaptureDeviceApple::SetErrorState(VideoCaptureError error,
                                            const base::Location& from_here,
                                            const std::string& reason) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  state_ = kError;
  client_->OnError(error, from_here, reason);
}

bool VideoCaptureDeviceApple::UpdateCaptureResolution() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceApple::UpdateCaptureResolution");
  if (![capture_device_ setCaptureHeight:capture_format_.frame_size.height()
                                   width:capture_format_.frame_size.width()
                               frameRate:capture_format_.frame_rate]) {
    ReceiveError(VideoCaptureError::kMacUpdateCaptureResolutionFailed,
                 FROM_HERE, "Could not configure capture device.");
    return false;
  }
  return true;
}

}  // namespace media
