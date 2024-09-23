// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_UTILS_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_UTILS_WIN_H_

#include <windows.h>

// Avoid including strsafe.h via dshow as it will cause build warnings.
#define NO_DSHOW_STRSAFE
#include <dshow.h>

#include "media/base/video_facing.h"
#include "media/capture/mojom/image_capture_types.h"

namespace media {

// Windows platform stores pan and tilt (min, max, step and current) in
// degrees. Spec expects them in arc seconds.
// https://docs.microsoft.com/en-us/windows/win32/api/strmif/ne-strmif-cameracontrolproperty
// spec: https://w3c.github.io/mediacapture-image/#pan
long CaptureAngleToPlatformValue(double arc_seconds);
double PlatformAngleToCaptureValue(long degrees);
double PlatformAngleToCaptureStep(long step, double min, double max);

// Windows platform stores exposure time (min, max and current) in log base 2
// seconds. If value is n, exposure time is 2^n seconds. Spec expects exposure
// times in 100 micro seconds.
// https://docs.microsoft.com/en-us/windows/win32/api/strmif/ne-strmif-cameracontrolproperty
// spec: https://w3c.github.io/mediacapture-image/#exposure-time
long CaptureExposureTimeToPlatformValue(double hundreds_of_microseconds);
double PlatformExposureTimeToCaptureValue(long log_seconds);
double PlatformExposureTimeToCaptureStep(long log_step, double min, double max);

// Returns the rotation of the camera. Returns 0 if it's not a built-in camera,
// or auto-rotation is not enabled, or only displays on external monitors.
int GetCameraRotation(VideoFacingMode facing);

bool IsAutoRotationEnabled();
bool IsInternalCamera(VideoFacingMode facing);

// Returns true if target device has active internal display panel, e.g. the
// screen attached to tablets or laptops, and stores its device info in
// |internal_display_device|.
bool HasActiveInternalDisplayDevice(DISPLAY_DEVICE* internal_display_device);

// Returns S_OK if the path info of the target display device with input
// |device_name| shows it is an internal display panel.
HRESULT CheckPathInfoForInternal(const PCWSTR device_name);

// Returns true if this is an integrated display panel.
bool IsInternalVideoOutput(
    const DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY video_output_tech_type);

static inline double PlatformToCaptureValue(long value) {
  return value;
}
static inline double PlatformToCaptureStep(long step, double min, double max) {
  return step;
}

// Retrieves the control range and value using the provided getters, and
// optionally returns the associated supported and current mode.
template <typename RangeGetter, typename CurrentValueGetter>
static mojom::RangePtr RetrieveControlRangeAndCurrent(
    RangeGetter range_getter,
    CurrentValueGetter current_value_getter,
    std::vector<mojom::MeteringMode>* supported_modes = nullptr,
    mojom::MeteringMode* current_mode = nullptr,
    double (*value_converter)(long) = PlatformToCaptureValue,
    double (*step_converter)(long, double, double) = PlatformToCaptureStep) {
  auto control_range = mojom::Range::New();

  long min, max, step, default_value, flags;
  HRESULT hr = range_getter(&min, &max, &step, &default_value, &flags);
  DLOG_IF(ERROR, FAILED(hr)) << "Control range reading failed: "
                             << logging::SystemErrorCodeToString(hr);
  if (SUCCEEDED(hr)) {
    control_range->min = value_converter(min);
    control_range->max = value_converter(max);
    control_range->step =
        step_converter(step, control_range->min, control_range->max);
    if (supported_modes != nullptr) {
      if (flags & CameraControl_Flags_Auto)
        supported_modes->push_back(mojom::MeteringMode::CONTINUOUS);
      if (flags & CameraControl_Flags_Manual)
        supported_modes->push_back(mojom::MeteringMode::MANUAL);
    }
  }

  long current;
  hr = current_value_getter(&current, &flags);
  DLOG_IF(ERROR, FAILED(hr)) << "Control value reading failed: "
                             << logging::SystemErrorCodeToString(hr);
  if (SUCCEEDED(hr)) {
    control_range->current = value_converter(current);
    if (current_mode != nullptr) {
      if (flags & CameraControl_Flags_Auto)
        *current_mode = mojom::MeteringMode::CONTINUOUS;
      else if (flags & CameraControl_Flags_Manual)
        *current_mode = mojom::MeteringMode::MANUAL;
    }
  }

  return control_range;
}

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_UTILS_WIN_H_
