// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_LINUX_CAMERA_CONFIG_CHROMEOS_H_
#define MEDIA_CAPTURE_VIDEO_LINUX_CAMERA_CONFIG_CHROMEOS_H_

#include <stddef.h>

#include <string>
#include <unordered_map>

#include "base/strings/string_piece.h"
#include "media/base/video_facing.h"
#include "media/capture/capture_export.h"

namespace media {

// CameraConfigChromeOS reads the file /etc/camera/camera_characteristics.conf
// and populates |camera_id_to_facing_|, |usb_id_to_camera_id_| and
// |model_id_to_camera_id_|.
//
// Each line in the config file can be:
// 1. Empty line
// 2. Line starts with '#': comments
// 3. Line follows the format:
// camera[camera_id].[root_level_attribute]=[value] OR
// camera[camera_id].module[module_id].[module_level_attribute]=[value]
//
// There are several assumptions of the config file:
//  1. One camera ID has exactly one lens_facing attribute, at root level.
//  2. usb_path is specified at module level. usb_path may not present at all,
//  but if it presents, the same usb_path does not appear accross different
//  camera IDs.
//  3. usb_vid_pid is specified at module level. If usb_path does not present,
//  each module needs to have one unique usb_vid_pid.
//
// Example of the config file:
//  camera0.lens_facing=0
//  camera0.sensor_orientation=0
//  camera0.module0.usb_vid_pid=0123:4567
//  camera0.module0.horizontal_view_angle=68.4
//  camera0.module0.lens_info_available_focal_lengths=1.64
//  camera0.module0.lens_info_minimum_focus_distance=0.22
//  camera0.module0.lens_info_optimal_focus_distance=0.5
//  camera0.module0.vertical_view_angle=41.6
//  camera0.module1.usb_vid_pid=89ab:cdef
//  camera0.module1.lens_info_available_focal_lengths=1.69,2
//  camera1.lens_facing=1
//  camera1.sensor_orientation=180
class CAPTURE_EXPORT CameraConfigChromeOS {
 public:
  CameraConfigChromeOS();
  CAPTURE_EXPORT CameraConfigChromeOS(const std::string& config_file_path);
  CAPTURE_EXPORT ~CameraConfigChromeOS();

  // Get camera facing by specifying USB vid and pid and device path. |model_id|
  // should be formatted as "vid:pid". |device_id| is something like
  // "/dev/video2". It first tries to match usb path, obtained from |device_id|.
  // If fails, |model_id| is then used.
  CAPTURE_EXPORT VideoFacingMode
  GetCameraFacing(const std::string& device_id,
                  const std::string& model_id) const;
  int GetOrientation(const std::string& device_id,
                     const std::string& model_id) const;

  static const VideoFacingMode kLensFacingDefault =
      VideoFacingMode::MEDIA_VIDEO_FACING_NONE;

 private:
  std::string GetUsbId(const std::string& device_id) const;
  int GetCameraId(const std::string& device_id,
                  const std::string& model_id) const;
  // Read file content and populate |camera_id_to_facing_|,
  // |usb_id_to_camera_id_| and |model_id_to_camera_id_|.
  void InitializeDeviceInfo(const std::string& config_file_path);

  std::unordered_map<int, VideoFacingMode> camera_id_to_facing_;
  std::unordered_map<int, int> camera_id_to_orientation_;
  std::unordered_map<std::string, int> usb_id_to_camera_id_;
  std::unordered_map<std::string, int> model_id_to_camera_id_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_LINUX_CAMERA_CONFIG_CHROMEOS_H_
