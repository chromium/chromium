// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/video_capture_device_mac.h"

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
#include "media/capture/video/mac/uvc_control_mac.h"
#import "media/capture/video/mac/video_capture_device_avfoundation_mac.h"
#include "media/capture/video/mac/video_capture_device_avfoundation_utils_mac.h"
#include "ui/gfx/geometry/size.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation DeviceNameAndTransportType {
  NSString* __strong _deviceName;
  // The transport type of the device (USB, PCI, etc), values are defined in
  // <IOKit/audio/IOAudioTypes.h> as kIOAudioDeviceTransportType*.
  media::VideoCaptureTransportType _transportType;
}

- (instancetype)initWithName:(NSString*)deviceName
               transportType:(media::VideoCaptureTransportType)transportType {
  if (self = [super init]) {
    _deviceName = [deviceName copy];
    _transportType = transportType;
  }
  return self;
}

- (NSString*)deviceName {
  return _deviceName;
}

- (media::VideoCaptureTransportType)deviceTransportType {
  return _transportType;
}

@end  // @implementation DeviceNameAndTransportType

namespace media {

namespace {

BASE_FEATURE(kExposeAllUvcControls,
             "ExposeAllUvcControls",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Mac specific limits for minimum and maximum frame rate.
const float kMinFrameRate = 1.0f;
const float kMaxFrameRate = 60.0f;

struct PanTilt {
  int32_t pan;
  int32_t tilt;
};

// Print the pan-tilt values. Used for friendly log messages.
::std::ostream& operator<<(::std::ostream& os, const PanTilt& pan_tilt) {
  return os << " pan: " << pan_tilt.pan << ", tilt " << pan_tilt.tilt;
}

// Update the control range and current value of pan-tilt control if
// possible.
static void MaybeUpdatePanTiltControlRange(UvcControl& uvc,
                                           mojom::Range* pan_range,
                                           mojom::Range* tilt_range) {
  PanTilt pan_tilt_max, pan_tilt_min, pan_tilt_step, pan_tilt_current;
  if (!uvc.GetControlMax<PanTilt>(uvc::kCtPanTiltAbsoluteControl, &pan_tilt_max,
                                  "pan-tilt") ||
      !uvc.GetControlMin<PanTilt>(uvc::kCtPanTiltAbsoluteControl, &pan_tilt_min,
                                  "pan-tilt") ||
      !uvc.GetControlStep<PanTilt>(uvc::kCtPanTiltAbsoluteControl,
                                   &pan_tilt_step, "pan-tilt") ||
      !uvc.GetControlCurrent<PanTilt>(uvc::kCtPanTiltAbsoluteControl,
                                      &pan_tilt_current, "pan-tilt")) {
    return;
  }
  pan_range->max = pan_tilt_max.pan;
  pan_range->min = pan_tilt_min.pan;
  pan_range->step = pan_tilt_step.pan;
  pan_range->current = pan_tilt_current.pan;
  tilt_range->max = pan_tilt_max.tilt;
  tilt_range->min = pan_tilt_min.tilt;
  tilt_range->step = pan_tilt_step.tilt;
  tilt_range->current = pan_tilt_current.tilt;
}

// Set pan and tilt values for a USB camera device.
static void SetPanTiltCurrent(UvcControl& uvc,
                              absl::optional<int> pan,
                              absl::optional<int> tilt) {
  DCHECK(pan.has_value() || tilt.has_value());

  PanTilt pan_tilt_current;
  if ((!pan.has_value() || !tilt.has_value()) &&
      !uvc.GetControlCurrent<PanTilt>(uvc::kCtPanTiltAbsoluteControl,
                                      &pan_tilt_current, "pan-tilt")) {
    return;
  }

  PanTilt pan_tilt_data;
  pan_tilt_data.pan =
      CFSwapInt32HostToLittle((int32_t)pan.value_or(pan_tilt_current.pan));
  pan_tilt_data.tilt =
      CFSwapInt32HostToLittle((int32_t)tilt.value_or(pan_tilt_current.tilt));

  uvc.SetControlCurrent<PanTilt>(uvc::kCtPanTiltAbsoluteControl, pan_tilt_data,
                                 "pan-tilt");
}

static bool IsNonEmptyRange(const mojom::RangePtr& range) {
  return range->min < range->max;
}

}  // namespace

VideoCaptureDeviceMac::VideoCaptureDeviceMac(
    const VideoCaptureDeviceDescriptor& device_descriptor)
    : device_descriptor_(device_descriptor),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      state_(kNotInitialized),
      capture_device_(nil),
      weak_factory_(this) {}

VideoCaptureDeviceMac::~VideoCaptureDeviceMac() {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void VideoCaptureDeviceMac::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMac::AllocateAndStart");
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

  if (!UpdateCaptureResolution())
    return;

  PowerLineFrequency frequency = GetPowerLineFrequency(params);
  if (frequency != PowerLineFrequency::kDefault) {
    // Try setting the power line frequency removal (anti-flicker). The built-in
    // cameras are normally suspended so the configuration must happen right
    // before starting capture and during configuration.
    const std::string device_model = GetDeviceModelId(
        device_descriptor_.device_id, device_descriptor_.capture_api,
        device_descriptor_.transport_type);
    if (device_model.length() > 2 * kVidPidSize) {
      if (UvcControl uvc(device_model, uvc::kVcProcessingUnit); uvc.Good()) {
        int power_line_flag_value =
            (frequency == PowerLineFrequency::k50Hz) ? uvc::k50Hz : uvc::k60Hz;
        uvc.SetControlCurrent<uint8_t>(uvc::kPuPowerLineFrequencyControl,
                                       power_line_flag_value,
                                       "power line frequency");
      }
    }
  }

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

void VideoCaptureDeviceMac::StopAndDeAllocate() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kCapturing || state_ == kError) << state_;

  NSString* errorMessage = nil;
  if (![capture_device_ setCaptureDevice:nil errorMessage:&errorMessage])
    LogMessage(base::SysNSStringToUTF8(errorMessage));

  [capture_device_ setFrameReceiver:nil];
  client_.reset();
  state_ = kIdle;
}

void VideoCaptureDeviceMac::TakePhoto(TakePhotoCallback callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kCapturing) << state_;

  if (photo_callback_)  // Only one picture can be in flight at a time.
    return;

  photo_callback_ = std::move(callback);
  [capture_device_ takePhoto];
}

void VideoCaptureDeviceMac::GetPhotoState(GetPhotoStateCallback callback) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMac::GetPhotoState");
  DCHECK(task_runner_->BelongsToCurrentThread());

  auto photo_state = mojo::CreateEmptyPhotoState();

  photo_state->height = mojom::Range::New(
      capture_format_.frame_size.height(), capture_format_.frame_size.height(),
      capture_format_.frame_size.height(), 0 /* step */);
  photo_state->width = mojom::Range::New(
      capture_format_.frame_size.width(), capture_format_.frame_size.width(),
      capture_format_.frame_size.width(), 0 /* step */);

  const std::string device_model = GetDeviceModelId(
      device_descriptor_.device_id, device_descriptor_.capture_api,
      device_descriptor_.transport_type);
  if (UvcControl uvc(device_model, uvc::kVcInputTerminal);
      uvc.Good() && base::FeatureList::IsEnabled(kExposeAllUvcControls)) {
    photo_state->current_focus_mode = mojom::MeteringMode::NONE;
    uvc.MaybeUpdateControlRange<uint16_t>(uvc::kCtFocusAbsoluteControl,
                                          photo_state->focus_distance.get(),
                                          "focus");
    if (IsNonEmptyRange(photo_state->focus_distance)) {
      photo_state->supported_focus_modes.push_back(mojom::MeteringMode::MANUAL);
    }
    bool current_auto_focus;
    if (uvc.GetControlCurrent<bool>(uvc::kCtFocusAutoControl,

                                    &current_auto_focus, "focus auto")) {
      photo_state->current_focus_mode = current_auto_focus
                                            ? mojom::MeteringMode::CONTINUOUS
                                            : mojom::MeteringMode::MANUAL;
      photo_state->supported_focus_modes.push_back(
          mojom::MeteringMode::CONTINUOUS);
    }

    photo_state->current_exposure_mode = mojom::MeteringMode::NONE;
    uvc.MaybeUpdateControlRange<uint16_t>(uvc::kCtExposureTimeAbsoluteControl,
                                          photo_state->exposure_time.get(),
                                          "exposure time");
    if (IsNonEmptyRange(photo_state->exposure_time)) {
      photo_state->supported_exposure_modes.push_back(
          mojom::MeteringMode::MANUAL);
    }
    uint8_t current_auto_exposure;
    if (uvc.GetControlCurrent<uint8_t>(uvc::kCtAutoExposureModeControl,
                                       &current_auto_exposure,
                                       "auto-exposure mode")) {
      if (current_auto_exposure & uvc::kExposureManual ||
          current_auto_exposure & uvc::kExposureShutterPriority) {
        photo_state->current_exposure_mode = mojom::MeteringMode::MANUAL;
      } else {
        photo_state->current_exposure_mode = mojom::MeteringMode::CONTINUOUS;
      }
      photo_state->supported_exposure_modes.push_back(
          mojom::MeteringMode::CONTINUOUS);
    }
  }
  if (UvcControl uvc(device_model, uvc::kVcInputTerminal); uvc.Good()) {
    MaybeUpdatePanTiltControlRange(uvc, photo_state->pan.get(),
                                   photo_state->tilt.get());

    uvc.MaybeUpdateControlRange<uint16_t>(uvc::kCtZoomAbsoluteControl,
                                          photo_state->zoom.get(), "zoom");
  }
  if (UvcControl uvc(device_model, uvc::kVcProcessingUnit);
      uvc.Good() && base::FeatureList::IsEnabled(kExposeAllUvcControls)) {
    uvc.MaybeUpdateControlRange<int16_t>(uvc::kPuBrightnessAbsoluteControl,
                                         photo_state->brightness.get(),
                                         "brightness");
    uvc.MaybeUpdateControlRange<uint16_t>(uvc::kPuContrastAbsoluteControl,
                                          photo_state->contrast.get(),
                                          "contrast");
    uvc.MaybeUpdateControlRange<uint16_t>(uvc::kPuSaturationAbsoluteControl,
                                          photo_state->saturation.get(),
                                          "saturation");
    uvc.MaybeUpdateControlRange<uint16_t>(uvc::kPuSharpnessAbsoluteControl,
                                          photo_state->sharpness.get(),
                                          "sharpness");

    photo_state->current_white_balance_mode = mojom::MeteringMode::NONE;
    uvc.MaybeUpdateControlRange<uint16_t>(
        uvc::kPuWhiteBalanceTemperatureControl,
        photo_state->color_temperature.get(), "white balance temperature");
    if (IsNonEmptyRange(photo_state->color_temperature)) {
      photo_state->supported_white_balance_modes.push_back(
          mojom::MeteringMode::MANUAL);
    }
    bool current_auto_white_balance;
    if (uvc.GetControlCurrent<bool>(uvc::kPuWhiteBalanceTemperatureAutoControl,
                                    &current_auto_white_balance,
                                    "white balance temperature auto")) {
      photo_state->current_white_balance_mode =
          current_auto_white_balance ? mojom::MeteringMode::CONTINUOUS
                                     : mojom::MeteringMode::MANUAL;
      photo_state->supported_white_balance_modes.push_back(
          mojom::MeteringMode::CONTINUOUS);
    }
  }

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

void VideoCaptureDeviceMac::SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                                            SetPhotoOptionsCallback callback) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMac::SetPhotoOptions");
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

  const std::string device_model = GetDeviceModelId(
      device_descriptor_.device_id, device_descriptor_.capture_api,
      device_descriptor_.transport_type);
  if (UvcControl uvc(device_model, uvc::kVcInputTerminal);
      uvc.Good() && base::FeatureList::IsEnabled(kExposeAllUvcControls)) {
    if (settings->has_focus_mode &&
        (settings->focus_mode == mojom::MeteringMode::CONTINUOUS ||
         settings->focus_mode == mojom::MeteringMode::MANUAL)) {
      int focus_auto =
          (settings->focus_mode == mojom::MeteringMode::CONTINUOUS);
      uvc.SetControlCurrent<bool>(uvc::kCtFocusAutoControl, focus_auto,
                                  "focus auto");
    }
    if (settings->has_focus_distance) {
      uvc.SetControlCurrent<uint16_t>(uvc::kCtFocusAbsoluteControl,
                                      settings->focus_distance, "focus");
    }
    if (settings->has_exposure_mode &&
        (settings->exposure_mode == mojom::MeteringMode::CONTINUOUS ||
         settings->exposure_mode == mojom::MeteringMode::MANUAL)) {
      int auto_exposure_mode =
          settings->exposure_mode == mojom::MeteringMode::CONTINUOUS
              ? uvc::kExposureAperturePriority
              : uvc::kExposureManual;
      uvc.SetControlCurrent<uint8_t>(uvc::kCtAutoExposureModeControl,
                                     auto_exposure_mode, "auto-exposure mode");
    }
    if (settings->has_exposure_time) {
      uvc.SetControlCurrent<uint16_t>(uvc::kCtExposureTimeAbsoluteControl,
                                      settings->exposure_time, "exposure time");
    }
  }
  if (UvcControl uvc(device_model, uvc::kVcInputTerminal); uvc.Good()) {
    if (settings->has_pan || settings->has_tilt) {
      SetPanTiltCurrent(uvc,
                        settings->has_pan ? absl::make_optional(settings->pan)
                                          : absl::nullopt,
                        settings->has_tilt ? absl::make_optional(settings->tilt)
                                           : absl::nullopt);
    }
    if (settings->has_zoom) {
      uvc.SetControlCurrent<uint16_t>(uvc::kCtZoomAbsoluteControl,
                                      settings->zoom, "zoom");
    }
  }
  if (UvcControl uvc(device_model, uvc::kVcProcessingUnit);
      uvc.Good() && base::FeatureList::IsEnabled(kExposeAllUvcControls)) {
    if (settings->has_brightness) {
      uvc.SetControlCurrent<int16_t>(uvc::kPuBrightnessAbsoluteControl,
                                     settings->brightness, "brightness");
    }
    if (settings->has_contrast) {
      uvc.SetControlCurrent<uint16_t>(uvc::kPuContrastAbsoluteControl,
                                      settings->contrast, "contrast");
    }
    if (settings->has_saturation) {
      uvc.SetControlCurrent<uint16_t>(uvc::kPuSaturationAbsoluteControl,
                                      settings->saturation, "saturation");
    }
    if (settings->has_sharpness) {
      uvc.SetControlCurrent<uint16_t>(uvc::kPuSharpnessAbsoluteControl,
                                      settings->sharpness, "sharpness");
    }
    if (settings->has_white_balance_mode &&
        (settings->white_balance_mode == mojom::MeteringMode::CONTINUOUS ||
         settings->white_balance_mode == mojom::MeteringMode::MANUAL)) {
      int white_balance_temperature_auto =
          (settings->white_balance_mode == mojom::MeteringMode::CONTINUOUS);
      uvc.SetControlCurrent<uint8_t>(uvc::kPuWhiteBalanceTemperatureAutoControl,
                                     white_balance_temperature_auto,
                                     "white balance temperature auto");
    }
    if (settings->has_color_temperature) {
      uvc.SetControlCurrent<uint16_t>(uvc::kPuWhiteBalanceTemperatureControl,
                                      settings->color_temperature,
                                      "white balance temperature");
    }
  }

  std::move(callback).Run(true);
}

bool VideoCaptureDeviceMac::Init(VideoCaptureApi capture_api_type) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kNotInitialized);

  if (capture_api_type != VideoCaptureApi::MACOSX_AVFOUNDATION)
    return false;

  capture_device_ =
      [[VideoCaptureDeviceAVFoundation alloc] initWithFrameReceiver:this];

  if (!capture_device_)
    return false;

  state_ = kIdle;
  return true;
}

void VideoCaptureDeviceMac::ReceiveFrame(const uint8_t* video_frame,
                                         int video_frame_length,
                                         const VideoCaptureFormat& frame_format,
                                         const gfx::ColorSpace color_space,
                                         int aspect_numerator,
                                         int aspect_denominator,
                                         base::TimeDelta timestamp) {
  if (capture_format_.frame_size != frame_format.frame_size) {
    ReceiveError(VideoCaptureError::kMacReceivedFrameWithUnexpectedResolution,
                 FROM_HERE,
                 "Captured resolution " + frame_format.frame_size.ToString() +
                     ", and expected " + capture_format_.frame_size.ToString());
    return;
  }

  client_->OnIncomingCapturedData(video_frame, video_frame_length, frame_format,
                                  color_space, 0 /* clockwise_rotation */,
                                  false /* flip_y */, base::TimeTicks::Now(),
                                  timestamp);
}

void VideoCaptureDeviceMac::ReceiveExternalGpuMemoryBufferFrame(
    CapturedExternalVideoBuffer frame,
    base::TimeDelta timestamp) {
  if (capture_format_.frame_size != frame.format.frame_size) {
    ReceiveError(VideoCaptureError::kMacReceivedFrameWithUnexpectedResolution,
                 FROM_HERE,
                 "Captured resolution " + frame.format.frame_size.ToString() +
                     ", and expected " + capture_format_.frame_size.ToString());
    return;
  }
  // TODO(https://crbug.com/1440075): Remove the `scaled_buffers` argument
  // because the vector is always empty and no consumers are interested in them.
  client_->OnIncomingCapturedExternalBuffer(
      std::move(frame), std::vector<CapturedExternalVideoBuffer>(),
      base::TimeTicks::Now(), timestamp, gfx::Rect(capture_format_.frame_size));
}

void VideoCaptureDeviceMac::OnPhotoTaken(const uint8_t* image_data,
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

void VideoCaptureDeviceMac::OnPhotoError() {
  VLOG(1) << __func__ << " error taking picture";
  photo_callback_.Reset();
}

void VideoCaptureDeviceMac::ReceiveError(VideoCaptureError error,
                                         const base::Location& from_here,
                                         const std::string& reason) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoCaptureDeviceMac::SetErrorState,
                     weak_factory_.GetWeakPtr(), error, from_here, reason));
}

void VideoCaptureDeviceMac::LogMessage(const std::string& message) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (client_)
    client_->OnLog(message);
}

void VideoCaptureDeviceMac::ReceiveCaptureConfigurationChanged() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoCaptureDeviceMac::OnCaptureConfigurationChanged,
                     weak_factory_.GetWeakPtr()));
}

void VideoCaptureDeviceMac::OnCaptureConfigurationChanged() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (client_) {
    client_->OnCaptureConfigurationChanged();
  }
}

void VideoCaptureDeviceMac::SetIsPortraitEffectSupportedForTesting(
    bool isPortraitEffectSupported) {
  [capture_device_
      setIsPortraitEffectSupportedForTesting:isPortraitEffectSupported];
}

void VideoCaptureDeviceMac::SetIsPortraitEffectActiveForTesting(
    bool isPortraitEffectActive) {
  [capture_device_ setIsPortraitEffectActiveForTesting:isPortraitEffectActive];
}

// static
std::string VideoCaptureDeviceMac::GetDeviceModelId(
    const std::string& device_id,
    VideoCaptureApi capture_api,
    VideoCaptureTransportType transport_type) {
  // Skip the AVFoundation's not USB nor built-in devices.
  if (capture_api == VideoCaptureApi::MACOSX_AVFOUNDATION &&
      transport_type != VideoCaptureTransportType::MACOSX_USB_OR_BUILT_IN)
    return "";
  if (capture_api == VideoCaptureApi::MACOSX_DECKLINK)
    return "";
  // Both PID and VID are 4 characters.
  if (device_id.size() < 2 * kVidPidSize)
    return "";

  // The last characters of device id is a concatenation of VID and then PID.
  const size_t vid_location = device_id.size() - 2 * kVidPidSize;
  std::string id_vendor = device_id.substr(vid_location, kVidPidSize);
  const size_t pid_location = device_id.size() - kVidPidSize;
  std::string id_product = device_id.substr(pid_location, kVidPidSize);

  return id_vendor + ":" + id_product;
}

// Check if the video capture device supports pan, tilt and zoom controls.
// static
VideoCaptureControlSupport VideoCaptureDeviceMac::GetControlSupport(
    const std::string& device_model) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMac::GetControlSupport");
  VideoCaptureControlSupport control_support;

  UvcControl uvc(device_model, uvc::kVcInputTerminal);
  if (!uvc.Good()) {
    return control_support;
  }

  uint16_t zoom_max, zoom_min = 0;
  if (uvc.GetControlMax<uint16_t>(uvc::kCtZoomAbsoluteControl, &zoom_max,
                                  "zoom") &&
      uvc.GetControlMin<uint16_t>(uvc::kCtZoomAbsoluteControl, &zoom_min,
                                  "zoom") &&
      zoom_min < zoom_max) {
    control_support.zoom = true;
  }
  PanTilt pan_tilt_max, pan_tilt_min;
  if (uvc.GetControlMax<PanTilt>(uvc::kCtPanTiltAbsoluteControl, &pan_tilt_max,
                                 "pan-tilt") &&
      uvc.GetControlMin<PanTilt>(uvc::kCtPanTiltAbsoluteControl, &pan_tilt_min,
                                 "pan-tilt")) {
    if (pan_tilt_min.pan < pan_tilt_max.pan) {
      control_support.pan = true;
    }
    if (pan_tilt_min.tilt < pan_tilt_max.tilt) {
      control_support.tilt = true;
    }
  }

  return control_support;
}

void VideoCaptureDeviceMac::SetErrorState(VideoCaptureError error,
                                          const base::Location& from_here,
                                          const std::string& reason) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  state_ = kError;
  client_->OnError(error, from_here, reason);
}

bool VideoCaptureDeviceMac::UpdateCaptureResolution() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMac::UpdateCaptureResolution");
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
