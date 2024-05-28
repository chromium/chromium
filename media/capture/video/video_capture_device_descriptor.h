// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_DESCRIPTOR_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_DESCRIPTOR_H_

#include <optional>
#include <string>
#include <vector>

#include "media/base/video_facing.h"
#include "media/capture/capture_export.h"

namespace media {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class VideoCaptureApi {
  UNKNOWN = 0,
  LINUX_V4L2_SINGLE_PLANE = 1,
  WIN_MEDIA_FOUNDATION = 2,
  WIN_MEDIA_FOUNDATION_SENSOR = 3,
  WIN_DIRECT_SHOW = 4,
  MACOSX_AVFOUNDATION = 5,
  MACOSX_DECKLINK = 6,
  ANDROID_API1 = 7,
  ANDROID_API2_LEGACY = 8,
  ANDROID_API2_FULL = 9,
  ANDROID_API2_LIMITED = 10,
  FUCHSIA_CAMERA3 = 11,
  VIRTUAL_DEVICE = 12,
  WEBRTC_LINUX_PIPEWIRE_SINGLE_PLANE = 13,
  kMaxValue = WEBRTC_LINUX_PIPEWIRE_SINGLE_PLANE,
};

// Represents capture device's support for different controls.
struct VideoCaptureControlSupport {
  bool pan = false;
  bool tilt = false;
  bool zoom = false;
};

enum class VideoCaptureTransportType {
  // For AVFoundation Api, identify devices that are built-in or USB.
  APPLE_USB_OR_BUILT_IN,
  OTHER_TRANSPORT
};

// LINT.IfChange
enum class CameraAvailability {
  kAvailable,
  kUnavailableExclusivelyUsedByOtherApplication,
};
// LINT.ThenChange(//media/capture/mojom/video_capture_types.mojom)

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
      const VideoCaptureControlSupport& control_support =
          VideoCaptureControlSupport(),
      VideoCaptureTransportType transport_type =
          VideoCaptureTransportType::OTHER_TRANSPORT);
  VideoCaptureDeviceDescriptor(
      const std::string& display_name,
      const std::string& device_id,
      const std::string& model_id,
      VideoCaptureApi capture_api,
      const VideoCaptureControlSupport& control_support,
      VideoCaptureTransportType transport_type =
          VideoCaptureTransportType::OTHER_TRANSPORT,
      VideoFacingMode facing = VideoFacingMode::MEDIA_VIDEO_FACING_NONE,
      std::optional<CameraAvailability> availability = std::nullopt);
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

  const VideoCaptureControlSupport& control_support() const {
    return control_support_;
  }
  void set_control_support(const VideoCaptureControlSupport& control_support) {
    control_support_ = control_support;
  }

  std::string device_id;
  // A unique hardware identifier of the capture device.
  // It is of the form "[vid]:[pid]" when a USB device is detected, and empty
  // otherwise.
  std::string model_id;

  VideoFacingMode facing;
  std::optional<CameraAvailability> availability;

  VideoCaptureApi capture_api;
  VideoCaptureTransportType transport_type;

 private:
  std::string display_name_;  // Name that is intended for display in the UI
  VideoCaptureControlSupport control_support_;
};

using VideoCaptureDeviceDescriptors = std::vector<VideoCaptureDeviceDescriptor>;

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_DESCRIPTOR_H_
