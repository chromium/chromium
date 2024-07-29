// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/win/video_capture_device_utils_win.h"

#include <cmath>
#include <iostream>

#include "base/check_op.h"
#include "base/win/win_util.h"

namespace media {

namespace {

const int kDegreesToArcSeconds = 3600;
const int kSecondsTo100MicroSeconds = 10000;

// Determines if camera is mounted on a device with naturally portrait display.
bool IsPortraitDevice(DWORD display_height,
                      DWORD display_width,
                      DWORD display_orientation) {
  if (display_orientation == DMDO_DEFAULT || display_orientation == DMDO_180)
    return display_height >= display_width;
  else
    return display_height < display_width;
}

}  // namespace

// Windows platform stores pan and tilt (min, max, step and current) in
// degrees. Spec expects them in arc seconds.
// https://docs.microsoft.com/en-us/windows/win32/api/strmif/ne-strmif-cameracontrolproperty
// spec: https://w3c.github.io/mediacapture-image/#pan
long CaptureAngleToPlatformValue(double arc_seconds) {
  return std::round(arc_seconds / kDegreesToArcSeconds);
}

double PlatformAngleToCaptureValue(long degrees) {
  return 1.0 * degrees * kDegreesToArcSeconds;
}

double PlatformAngleToCaptureStep(long step, double min, double max) {
  return PlatformAngleToCaptureValue(step);
}

// Windows platform stores exposure time (min, max and current) in log base 2
// seconds. If value is n, exposure time is 2^n seconds. Spec expects exposure
// times in 100 micro seconds.
// https://docs.microsoft.com/en-us/windows/win32/api/strmif/ne-strmif-cameracontrolproperty
// spec: https://w3c.github.io/mediacapture-image/#exposure-time
long CaptureExposureTimeToPlatformValue(double hundreds_of_microseconds) {
  return std::log2(hundreds_of_microseconds / kSecondsTo100MicroSeconds);
}

double PlatformExposureTimeToCaptureValue(long log_seconds) {
  return std::exp2(log_seconds) * kSecondsTo100MicroSeconds;
}

double PlatformExposureTimeToCaptureStep(long log_step,
                                         double min,
                                         double max) {
  // The smallest possible value is
  // |exp2(min_log_seconds) * kSecondsTo100MicroSeconds|.
  // That value can be computed by PlatformExposureTimeToCaptureValue and is
  // passed to this function as |min| thus there is not need to recompute it
  // here.
  // The second smallest possible value is
  // |exp2(min_log_seconds + log_step) * kSecondsTo100MicroSeconds| which equals
  // to |exp2(log_step) * min|.
  // While the relative step or ratio between consecutive values is always the
  // same (|std::exp2(log_step)|), the smallest absolute step is between the
  // smallest and the second smallest possible values i.e. between |min| and
  // |exp2(log_step) * min|.
  return (std::exp2(log_step) - 1) * min;
}

// Note: Because we can't find a solid way to detect camera location (front/back
// or external USB camera) with Win32 APIs, assume it's always front camera when
// auto rotation is enabled for now.
int GetCameraRotation(VideoFacingMode facing) {
  int rotation = 0;

  if (!IsInternalCamera(facing)) {
    return rotation;
  }

  // When display is only on external monitors, the auto-rotation state still
  // may be ENABLED on the target device. In that case, we shouldn't query the
  // display orientation and the built-in camera will be treated as an external
  // one.
  DISPLAY_DEVICE internal_display_device;
  if (!HasActiveInternalDisplayDevice(&internal_display_device)) {
    return rotation;
  }

  // Windows cameras with VideoFacingMode::MEDIA_VIDEO_FACING_NONE should early
  // exit as part of the IsInternalCamera(facing) check above.
  DCHECK_NE(facing, VideoFacingMode::MEDIA_VIDEO_FACING_NONE);

  DEVMODE mode;
  ::ZeroMemory(&mode, sizeof(mode));
  mode.dmSize = sizeof(mode);
  mode.dmDriverExtra = 0;
  if (::EnumDisplaySettings(internal_display_device.DeviceName,
                            ENUM_CURRENT_SETTINGS, &mode)) {
    int camera_offset = 0;  // Measured in degrees, clockwise.
    bool portrait_device = IsPortraitDevice(mode.dmPelsHeight, mode.dmPelsWidth,
                                            mode.dmDisplayOrientation);
    switch (mode.dmDisplayOrientation) {
      case DMDO_DEFAULT:
        if (portrait_device &&
            facing == VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT) {
          camera_offset = 270;  // Adjust portrait device rear camera by 180.
        } else if (portrait_device) {
          camera_offset = 90;  // Portrait device front camera is offset by 90.
        } else {
          camera_offset = 0;
        }
        break;
      case DMDO_90:
        if (portrait_device)
          camera_offset = 180;
        else if (facing == VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT)
          camera_offset = 270;  // Adjust landscape device rear camera by 180.
        else
          camera_offset = 90;
        break;
      case DMDO_180:
        if (portrait_device &&
            facing == VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT) {
          camera_offset = 90;  // Adjust portrait device rear camera by 180.
        } else if (portrait_device) {
          camera_offset = 270;
        } else {
          camera_offset = 180;
        }
        break;
      case DMDO_270:
        if (portrait_device)
          camera_offset = 0;
        else if (facing == VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT)
          camera_offset = 90;  // Adjust landscape device rear camera by 180.
        else
          camera_offset = 270;
        break;
    }
    rotation = (360 - camera_offset) % 360;
  }

  return rotation;
}

bool IsAutoRotationEnabled() {
  typedef BOOL(WINAPI * GetAutoRotationState)(PAR_STATE state);
  static const auto get_rotation_state = reinterpret_cast<GetAutoRotationState>(
      base::win::GetUser32FunctionPointer("GetAutoRotationState"));

  if (get_rotation_state) {
    AR_STATE auto_rotation_state;
    ::ZeroMemory(&auto_rotation_state, sizeof(AR_STATE));

    if (get_rotation_state(&auto_rotation_state)) {
      // AR_ENABLED is defined as '0x0', while AR_STATE enumeration is defined
      // as bitwise. See the example codes in
      // https://msdn.microsoft.com/en-us/library/windows/desktop/dn629263(v=vs.85).aspx.
      if (auto_rotation_state == AR_ENABLED) {
        return true;
      }
    }
  }

  return false;
}

bool IsInternalCamera(VideoFacingMode facing) {
  return facing == MEDIA_VIDEO_FACING_USER ||
         facing == MEDIA_VIDEO_FACING_ENVIRONMENT;
}

bool HasActiveInternalDisplayDevice(DISPLAY_DEVICE* internal_display_device) {
  DISPLAY_DEVICE display_device;
  display_device.cb = sizeof(display_device);

  for (int device_index = 0;; ++device_index) {
    BOOL enum_result =
        ::EnumDisplayDevices(NULL, device_index, &display_device, 0);
    if (!enum_result)
      break;
    if (!(display_device.StateFlags & DISPLAY_DEVICE_ACTIVE))
      continue;

    HRESULT hr = CheckPathInfoForInternal(display_device.DeviceName);
    if (SUCCEEDED(hr)) {
      *internal_display_device = display_device;
      return true;
    }
  }
  return false;
}

HRESULT CheckPathInfoForInternal(const PCWSTR device_name) {
  HRESULT hr = S_OK;
  UINT32 path_info_array_size = 0;
  UINT32 mode_info_array_size = 0;
  DISPLAYCONFIG_PATH_INFO* path_info_array = nullptr;
  DISPLAYCONFIG_MODE_INFO* mode_info_array = nullptr;

  do {
    // In case this isn't the first time through the loop, delete the buffers
    // allocated.
    delete[] path_info_array;
    path_info_array = nullptr;

    delete[] mode_info_array;
    mode_info_array = nullptr;

    hr = HRESULT_FROM_WIN32(::GetDisplayConfigBufferSizes(
        QDC_ONLY_ACTIVE_PATHS, &path_info_array_size, &mode_info_array_size));
    if (FAILED(hr)) {
      break;
    }

    path_info_array =
        new (std::nothrow) DISPLAYCONFIG_PATH_INFO[path_info_array_size];
    if (path_info_array == nullptr) {
      hr = E_OUTOFMEMORY;
      break;
    }

    mode_info_array =
        new (std::nothrow) DISPLAYCONFIG_MODE_INFO[mode_info_array_size];
    if (mode_info_array == nullptr) {
      hr = E_OUTOFMEMORY;
      break;
    }

    hr = HRESULT_FROM_WIN32(::QueryDisplayConfig(
        QDC_ONLY_ACTIVE_PATHS, &path_info_array_size, path_info_array,
        &mode_info_array_size, mode_info_array, nullptr));
  } while (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER));

  int desired_path_index = -1;
  if (SUCCEEDED(hr)) {
    // Loop through all sources until the one which matches the |device_name|
    // is found.
    for (UINT32 path_index = 0; path_index < path_info_array_size;
         ++path_index) {
      DISPLAYCONFIG_SOURCE_DEVICE_NAME source_name = {};
      source_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
      source_name.header.size = sizeof(source_name);
      source_name.header.adapterId =
          path_info_array[path_index].sourceInfo.adapterId;
      source_name.header.id = path_info_array[path_index].sourceInfo.id;

      hr =
          HRESULT_FROM_WIN32(::DisplayConfigGetDeviceInfo(&source_name.header));
      if (SUCCEEDED(hr)) {
        if (wcscmp(device_name, source_name.viewGdiDeviceName) == 0 &&
            IsInternalVideoOutput(
                path_info_array[path_index].targetInfo.outputTechnology)) {
          desired_path_index = path_index;
          break;
        }
      }
    }
  }

  if (desired_path_index == -1) {
    hr = E_INVALIDARG;
  }

  delete[] path_info_array;
  path_info_array = nullptr;

  delete[] mode_info_array;
  mode_info_array = nullptr;

  return hr;
}

bool IsInternalVideoOutput(
    const DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY video_output_tech_type) {
  switch (video_output_tech_type) {
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL:
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED:
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_UDI_EMBEDDED:
      return TRUE;

    default:
      return FALSE;
  }
}

}  // namespace media
