// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_SYSTEM_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_SYSTEM_IMPL_H_

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/capture/video/video_capture_system.h"

namespace media {

// Layer on top of VideoCaptureDeviceFactory that translates device descriptors
// to string identifiers and consolidates and caches device descriptors and
// supported formats into VideoCaptureDeviceInfos.
class CAPTURE_EXPORT VideoCaptureSystemImpl : public VideoCaptureSystem {
 public:
  explicit VideoCaptureSystemImpl(
      std::unique_ptr<VideoCaptureDeviceFactory> factory);
  ~VideoCaptureSystemImpl() override;

  void GetDeviceInfosAsync(
      mojom::VideoCaptureDeviceEnumerationMode enumeration_mode,
      DeviceInfoCallback result_callback) override;
  bool HasDeviceInfosWithCapabilities() const override;
  VideoCaptureErrorOrDevice CreateDevice(const std::string& device_id) override;

  VideoCaptureDeviceFactory* GetFactory() override;

  static void ConsolidateCaptureFormatsForTesting(
      media::VideoCaptureFormats* formats);

 private:
  struct DeviceEnumRequest {
    mojom::VideoCaptureDeviceEnumerationMode enumeration_mode;
    DeviceInfoCallback callback;
  };
  using DeviceEnumQueue = std::list<DeviceEnumRequest>;

  // Returns nullptr if no descriptor found.
  const VideoCaptureDeviceInfo* LookupDeviceInfoFromId(
      const std::string& device_id);

  void StartDeviceEnumeration(
      mojom::VideoCaptureDeviceEnumerationMode enumeration_mode);
  void DevicesInfoReady(
      mojom::VideoCaptureDeviceEnumerationMode enumeration_mode,
      std::vector<VideoCaptureDeviceInfo> devices_info);

  const std::unique_ptr<VideoCaptureDeviceFactory> factory_;
  DeviceEnumQueue device_enum_request_queue_;
  // Set while an enumeration is running to prevent another from starting.
  // The stored mode is checked when results arrive.
  std::optional<mojom::VideoCaptureDeviceEnumerationMode>
      enumeration_mode_in_progress_;
  // Stores VideoCaptureDeviceInfo from the latest enumeration. Only
  // descriptors are guaranteed to be populated. On Windows, supported
  // formats and control support may not have been queried.
  std::vector<VideoCaptureDeviceInfo> devices_descriptors_cache_;
  bool devices_descriptors_cache_includes_capabilities_ = false;
  bool has_logged_capture_device_metrics_ = false;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<VideoCaptureSystemImpl> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_SYSTEM_IMPL_H_
