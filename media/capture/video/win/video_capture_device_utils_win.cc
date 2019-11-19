// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/video_capture_device_utils_win.h"

#include <iostream>

#include "base/win/win_util.h"
#include "base/win/windows_version.h"

namespace media {

// Note: Because we can't find a solid way to detect camera location (front/back
// or external USB camera) with Win32 APIs, assume it's always front camera when
// auto rotation is enabled for now.
int GetCameraRotation(VideoFacingMode facing) {
  int rotation = 0;

  if (!IsAutoRotationEnabled()) {
    return rotation;
  }

  // Before Win10, we can't distinguish if the selected camera is an internal or
  // external one. So we assume it's internal and do the frame rotation if the
  // auto rotation is enabled to cover most user cases.
  if (!IsInternalCamera(facing)) {
    return rotation;
  }

  // When display is only on external monitors, the auto-rotation state still
  // may be ENALBED on the target device. In that case, we shouldn't query the
  // display orientation and the built-in camera will be treated as an external
  // one.
  DISPLAY_DEVICE internal_display_device;
  if (!HasActiveInternalDisplayDevice(&internal_display_device)) {
    return rotation;
  }

  DEVMODE mode;
  ::ZeroMemory(&mode, sizeof(mode));
  mode.dmSize = sizeof(mode);
  mode.dmDriverExtra = 0;
  if (::EnumDisplaySettings(internal_display_device.DeviceName,
                            ENUM_CURRENT_SETTINGS, &mode)) {
    int device_orientation = 0;
    switch (mode.dmDisplayOrientation) {
      case DMDO_DEFAULT:
        device_orientation = 0;
        break;
      case DMDO_90:
        device_orientation = 90;
        break;
      case DMDO_180:
        device_orientation = 180;
        break;
      case DMDO_270:
        device_orientation = 270;
        break;
    }
    rotation = (360 - device_orientation) % 360;
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
  if (base::win::GetVersion() < base::win::Version::WIN10) {
    return true;
  }

  if (facing == MEDIA_VIDEO_FACING_USER ||
      facing == MEDIA_VIDEO_FACING_ENVIRONMENT) {
    return true;
  }

  return false;
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
