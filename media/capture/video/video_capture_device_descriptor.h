// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_DESCRIPTOR_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_DESCRIPTOR_H_

#include <string>
#include <vector>

#include "media/base/video_facing.h"
#include "media/capture/capture_export.h"

namespace media {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
enum class VideoCaptureApi {
  LINUX_V4L2_SINGLE_PLANE,
  WIN_MEDIA_FOUNDATION,
  WIN_MEDIA_FOUNDATION_SENSOR,
  WIN_DIRECT_SHOW,
  MACOSX_AVFOUNDATION,
  MACOSX_DECKLINK,
  ANDROID_API1,
  ANDROID_API2_LEGACY,
  ANDROID_API2_FULL,
  ANDROID_API2_LIMITED,
  VIRTUAL_DEVICE,
  UNKNOWN
};

enum class VideoCaptureTransportType {
  // For AVFoundation Api, identify devices that are built-in or USB.
  MACOSX_USB_OR_BUILT_IN,
  OTHER_TRANSPORT
};

// Represents information about a capture device as returned by
// VideoCaptureDeviceFactory::GetDeviceDescriptors().
// |device_id| represents a unique id of a physical device. Since the same
// physical device may be accessible through different APIs |capture_api|
// disambiguates the API.
// TODO(tommi): Given that this struct has become more complex with private
// members, methods that are not just direct getters/setters
// (e.g., GetNameAndModel), let's turn it into a class in order to properly
// conform with the style guide and protect the integrity of the data that the
// class owns.
struct CAPTURE_EXPORT VideoCaptureDeviceDescriptor {
 public:
  VideoCaptureDeviceDescriptor();
  VideoCaptureDeviceDescriptor(
      const std::string& display_name,
      const std::string& device_id,
      VideoCaptureApi capture_api = VideoCaptureApi::UNKNOWN,
      VideoCaptureTransportType transport_type =
          VideoCaptureTransportType::OTHER_TRANSPORT);
  VideoCaptureDeviceDescriptor(
      const std::string& display_name,
      const std::string& device_id,
      const std::string& model_id,
      VideoCaptureApi capture_api,
      VideoCaptureTransportType transport_type =
          VideoCaptureTransportType::OTHER_TRANSPORT,
      VideoFacingMode facing = VideoFacingMode::MEDIA_VIDEO_FACING_NONE);
  VideoCaptureDeviceDescriptor(const VideoCaptureDeviceDescriptor& other);
  ~VideoCaptureDeviceDescriptor();

  // These operators are needed due to storing the name in an STL container.
  // In the shared build, all methods from the STL container will be exported
  // so even though they're not used, they're still depended upon.
  bool operator==(const VideoCaptureDeviceDescriptor& other) const {
    return (other.device_id == device_id) && (other.capture_api == capture_api);
  }
  bool operator<(const VideoCaptureDeviceDescriptor& other) const;

  const char* GetCaptureApiTypeString() const;
  // Friendly name of a device, plus the model identifier in parentheses.
  std::string GetNameAndModel() const;

  // Name that is intended for display in the UI.
  const std::string& display_name() const { return display_name_; }
  void set_display_name(const std::string& name);

  std::string device_id;
  // A unique hardware identifier of the capture device.
  // It is of the form "[vid]:[pid]" when a USB device is detected, and empty
  // otherwise.
  std::string model_id;

  VideoFacingMode facing;

  VideoCaptureApi capture_api;
  VideoCaptureTransportType transport_type;

 private:
  std::string display_name_;  // Name that is intended for display in the UI
};

using VideoCaptureDeviceDescriptors = std::vector<VideoCaptureDeviceDescriptor>;

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_DESCRIPTOR_H_
