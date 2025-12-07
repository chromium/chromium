// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_info_loader.h"

#include <windows.h>

#include <algorithm>
#include <limits>
#include <vector>

#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"

namespace remoting {

namespace {

struct PathWithName {
  DISPLAYCONFIG_PATH_INFO path;
  DISPLAYCONFIG_SOURCE_DEVICE_NAME source_device_name;
};

std::vector<DISPLAYCONFIG_PATH_INFO> GetDisplayConfigPathInfos() {
  LONG result;
  do {
    uint32_t path_elements, mode_elements;
    if (::GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_elements,
                                      &mode_elements) != ERROR_SUCCESS) {
      return {};
    }
    std::vector<DISPLAYCONFIG_PATH_INFO> path_infos(path_elements);
    std::vector<DISPLAYCONFIG_MODE_INFO> mode_infos(mode_elements);
    result = ::QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_elements,
                                  path_infos.data(), &mode_elements,
                                  mode_infos.data(), nullptr);
    if (result == ERROR_SUCCESS) {
      path_infos.resize(path_elements);
      return path_infos;
    }
  } while (result == ERROR_INSUFFICIENT_BUFFER);
  return {};
}

std::vector<PathWithName> GetDisplayPathsWithNames() {
  std::vector<PathWithName> result;
  for (const auto& path : GetDisplayConfigPathInfos()) {
    DISPLAYCONFIG_SOURCE_DEVICE_NAME source_name = {};
    source_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
    source_name.header.size = sizeof(source_name);
    source_name.header.adapterId = path.sourceInfo.adapterId;
    source_name.header.id = path.sourceInfo.id;
    if (::DisplayConfigGetDeviceInfo(&source_name.header) == ERROR_SUCCESS) {
      result.push_back({path, source_name});
    }
  }
  return result;
}

std::string GetFriendlyDeviceName(const DISPLAYCONFIG_PATH_INFO& path) {
  DISPLAYCONFIG_TARGET_DEVICE_NAME target_name = {};
  target_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
  target_name.header.size = sizeof(target_name);
  target_name.header.adapterId = path.targetInfo.adapterId;
  target_name.header.id = path.targetInfo.id;
  if (::DisplayConfigGetDeviceInfo(&target_name.header) == ERROR_SUCCESS) {
    return base::WideToUTF8(target_name.monitorFriendlyDeviceName);
  }
  return std::string();
}

class DesktopDisplayInfoLoaderWin : public DesktopDisplayInfoLoader {
 public:
  DesktopDisplayInfoLoaderWin() = default;
  ~DesktopDisplayInfoLoaderWin() override = default;

  DesktopDisplayInfo GetCurrentDisplayInfo() override;
};

DesktopDisplayInfo DesktopDisplayInfoLoaderWin::GetCurrentDisplayInfo() {
  // Obtain the paths and names of all display devices on the system. This
  // list will be used to lookup the path for each DISPLAY_DEVICE enumerated
  // below. If found, the path is used to obtain the device's friendly name.
  auto paths_with_names = GetDisplayPathsWithNames();

  int32_t lowest_x = std::numeric_limits<int32_t>::max();
  int32_t lowest_y = std::numeric_limits<int32_t>::max();
  std::vector<DisplayGeometry> displays;
  BOOL enum_result = TRUE;
  for (int device_index = 0;; ++device_index) {
    DISPLAY_DEVICE device = {};
    device.cb = sizeof(device);
    enum_result = EnumDisplayDevices(NULL, device_index, &device, 0);

    // |enum_result| is 0 if we have enumerated all devices.
    if (!enum_result) {
      break;
    }

    // We only care about active displays.
    if (!(device.StateFlags & DISPLAY_DEVICE_ACTIVE)) {
      continue;
    }

    bool is_default = false;
    if (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
      is_default = true;
    }

    // Find the path corresponding to this display device. If found, use it to
    // get the friendly name for the device.
    std::string monitor_name;
    for (const auto& entry : paths_with_names) {
      if (UNSAFE_TODO(wcscmp(entry.source_device_name.viewGdiDeviceName,
                             device.DeviceName)) == 0) {
        monitor_name = GetFriendlyDeviceName(entry.path);
        break;
      }
    }

    // Get additional info about device.
    DEVMODE devmode;
    devmode.dmSize = sizeof(devmode);
    EnumDisplaySettingsEx(device.DeviceName, ENUM_CURRENT_SETTINGS, &devmode,
                          0);

    int32_t x = devmode.dmPosition.x;
    int32_t y = devmode.dmPosition.y;
    displays.emplace_back(
        /* id */ device_index, x, y, devmode.dmPelsWidth, devmode.dmPelsHeight,
        /* dpi */ devmode.dmLogPixels, devmode.dmBitsPerPel, is_default,
        monitor_name);

    lowest_x = std::min(x, lowest_x);
    lowest_y = std::min(y, lowest_y);
  }

  // Normalize the displays so the bounding-box's top-left corner is at (0, 0).
  // This matches the coordinate system used by InputInjectorWin, so that
  // the FractionalInputFilter produces correct x,y-coordinates for injection.
  DesktopDisplayInfo result;
  result.set_pixel_type(DesktopDisplayInfo::PixelType::PHYSICAL);
  for (DisplayGeometry& info : displays) {
    info.x -= lowest_x;
    info.y -= lowest_y;
    result.AddDisplay(info);
  }
  return result;
}

}  // namespace

// static
std::unique_ptr<DesktopDisplayInfoLoader> DesktopDisplayInfoLoader::Create() {
  return std::make_unique<DesktopDisplayInfoLoaderWin>();
}

}  // namespace remoting
