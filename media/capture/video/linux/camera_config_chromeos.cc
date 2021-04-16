// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/camera_config_chromeos.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace media {

namespace {

// The value for each field in the enum matches the value in
// /etc/camera/camera_characteristics.conf.
enum LensFacing { FRONT = 0, BACK = 1 };

bool ParseCameraId(const base::StringPiece& sub_key, int* camera_id) {
  const base::StringPiece camera_id_prefix = "camera";
  if (!base::StartsWith(sub_key, camera_id_prefix))
    return false;
  return base::StringToInt(sub_key.substr(camera_id_prefix.size()), camera_id);
}
}

// /etc/camera/camera_characteristics.conf contains camera information which
// driver cannot provide.
static const char kCameraCharacteristicsConfigFile[] =
    "/etc/camera/camera_characteristics.conf";
static const char kLensFacing[] = "lens_facing";
static const char kSensorOrientation[] = "sensor_orientation";
static const char kUsbVidPid[] = "usb_vid_pid";
static const char kUsbPath[] = "usb_path";
static const int kOrientationDefault = 0;
static const int kCameraIdNotFound = -1;

CameraConfigChromeOS::CameraConfigChromeOS() {
  InitializeDeviceInfo(std::string(kCameraCharacteristicsConfigFile));
}

CameraConfigChromeOS::CameraConfigChromeOS(
    const std::string& config_file_path) {
  InitializeDeviceInfo(config_file_path);
}

CameraConfigChromeOS::~CameraConfigChromeOS() = default;

VideoFacingMode CameraConfigChromeOS::GetCameraFacing(
    const std::string& device_id,
    const std::string& model_id) const {
  int camera_id = GetCameraId(device_id, model_id);
  const auto& camera_id_to_facing_const = camera_id_to_facing_;
  const auto facing_found = camera_id_to_facing_const.find(camera_id);
  if (facing_found == camera_id_to_facing_const.end()) {
    DLOG(ERROR) << "Can't find lens_facing of camera ID " << camera_id
                << " in config file";
    return kLensFacingDefault;
  }
  return facing_found->second;
}

int CameraConfigChromeOS::GetOrientation(const std::string& device_id,
                                         const std::string& model_id) const {
  int camera_id = GetCameraId(device_id, model_id);
  const auto& camera_id_to_orientation = camera_id_to_orientation_;
  const auto orientation_found = camera_id_to_orientation.find(camera_id);
  if (orientation_found == camera_id_to_orientation.end()) {
    DLOG(ERROR) << "Can't find sensor_orientation of camera ID " << camera_id
                << " in config file";
    return kOrientationDefault;
  }
  return orientation_found->second;
}

int CameraConfigChromeOS::GetCameraId(const std::string& device_id,
                                      const std::string& model_id) const {
  std::string usb_id = GetUsbId(device_id);
  const auto& usb_id_to_camera_id = usb_id_to_camera_id_;
  const auto& model_id_to_camera_id = model_id_to_camera_id_;

  const auto usb_id_found = usb_id_to_camera_id.find(usb_id);
  if (usb_id_found != usb_id_to_camera_id.end())
    return usb_id_found->second;

  // Can't find Usb ID. Fall back to use |model_id|.
  const auto model_id_found = model_id_to_camera_id.find(model_id);
  if (model_id_found != model_id_to_camera_id.end())
    return model_id_found->second;

  DLOG(ERROR) << "Can't find model ID in config file: " << model_id;
  return kCameraIdNotFound;
}

std::string CameraConfigChromeOS::GetUsbId(const std::string& device_id) const {
  // |device_id| is of the form "/dev/video2".  We want to retrieve "video2"
  // into |file_name|.
  const std::string device_dir = "/dev/";
  if (!base::StartsWith(device_id, device_dir)) {
    DLOG(ERROR) << "device_id is invalid: " << device_id;
    return std::string();
  }
  const std::string file_name = device_id.substr(device_dir.length());

  // Usb ID can be obtained by "readlink /sys/class/video4linux/video2/device".
  const std::string symlink =
      base::StringPrintf("/sys/class/video4linux/%s/device", file_name.c_str());
  base::FilePath symlinkTarget;
  if (!base::ReadSymbolicLink(base::FilePath(symlink), &symlinkTarget)) {
    DPLOG(ERROR) << "Failed to readlink: " << symlink;
    return std::string();
  }

  // |symlinkTarget| is of the format "../../../A-B:C.D". Remove the path
  // prefix.
  base::StringPiece usb_part = symlinkTarget.BaseName().value();

  // |usb_part| is of the format "A-B:C.D" or "A-B.C:D". We want everything
  // before ":".
  std::vector<base::StringPiece> usb_id_pieces = base::SplitStringPiece(
      usb_part, ":", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_ALL);

  if (usb_id_pieces.empty()) {
    DLOG(ERROR) << "Error after split: " << usb_part;
    return std::string();
  }
  return usb_id_pieces[0].as_string();
}

void CameraConfigChromeOS::InitializeDeviceInfo(
    const std::string& config_file_path) {
  const base::FilePath path(config_file_path);
  std::string content;
  if (!base::ReadFileToString(path, &content)) {
    DPLOG(ERROR) << "ReadFileToString fails";
    return;
  }
  const std::vector<base::StringPiece> lines = base::SplitStringPiece(
      content, "\n", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);

  for (const base::StringPiece& line : lines) {
    // Ignore the comments that starts with "#".
    if (base::StartsWith(line, "#"))
      continue;
    const std::vector<base::StringPiece> key_value = base::SplitStringPiece(
        line, "=", base::WhitespaceHandling::TRIM_WHITESPACE,
        base::SplitResult::SPLIT_WANT_ALL);
    if (key_value.size() != 2) {
      DLOG(ERROR) << "Invalid line in config file: " << line;
      continue;
    }
    const auto& key = key_value[0];
    const auto& value = key_value[1];
    const std::vector<base::StringPiece> sub_keys = base::SplitStringPiece(
        key, ".", base::WhitespaceHandling::TRIM_WHITESPACE,
        base::SplitResult::SPLIT_WANT_ALL);

    if (sub_keys.size() < 1) {
      DLOG(ERROR) << "No valid sub key exists. Line format is invalid: "
                  << line;
      continue;
    }
    int camera_id = 0;
    if (!ParseCameraId(sub_keys[0], &camera_id)) {
      DLOG(ERROR) << "Invalid sub key for camera id: " << sub_keys[0];
      continue;
    }

    if (sub_keys.size() == 2 && sub_keys[1] == kLensFacing) {
      int lens_facing = -1;
      if (!base::StringToInt(value, &lens_facing)) {
        DLOG(ERROR) << "Invalid value for lens_facing: " << value;
        continue;
      }
      switch (lens_facing) {
        case LensFacing::FRONT:
          camera_id_to_facing_[camera_id] =
              VideoFacingMode::MEDIA_VIDEO_FACING_USER;
          break;
        case LensFacing::BACK:
          camera_id_to_facing_[camera_id] =
              VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT;
          break;
        default:
          DLOG(ERROR) << "Invalid value for lens_facing: " << lens_facing;
          continue;
      }
    } else if (sub_keys.size() == 2 && sub_keys[1] == kSensorOrientation) {
      int orientation = 0;
      if (!base::StringToInt(value, &orientation)) {
        DLOG(ERROR) << "Invalid value for sensor_orientation: " << value;
        continue;
      }
      camera_id_to_orientation_[camera_id] = orientation;
    } else if (sub_keys.size() == 3 && sub_keys[2] == kUsbVidPid) {
      if (value.empty()) {
        DLOG(ERROR) << "model_id is empty";
        continue;
      }
      std::string model_id = value.as_string();
      std::transform(model_id.begin(), model_id.end(), model_id.begin(),
                     ::tolower);
      model_id_to_camera_id_[model_id] = camera_id;
    } else if (sub_keys.size() == 3 && sub_keys[2] == kUsbPath) {
      if (value.empty()) {
        DLOG(ERROR) << "usb_path is empty";
        continue;
      }
      usb_id_to_camera_id_[value.as_string()] = camera_id;
    }
    // Ignore unknown or unutilized attributes.
  }
}

}  // namespace media
