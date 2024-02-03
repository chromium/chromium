// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_FUCHSIA_VIDEO_CAPTURE_DEVICE_FACTORY_FUCHSIA_H_
#define MEDIA_CAPTURE_VIDEO_FUCHSIA_VIDEO_CAPTURE_DEVICE_FACTORY_FUCHSIA_H_

#include <fuchsia/camera3/cpp/fidl.h>

#include <map>
#include <optional>

#include "base/containers/small_map.h"
#include "media/capture/video/video_capture_device_factory.h"

namespace media {

class CAPTURE_EXPORT VideoCaptureDeviceFactoryFuchsia
    : public VideoCaptureDeviceFactory {
 public:
  VideoCaptureDeviceFactoryFuchsia();
  ~VideoCaptureDeviceFactoryFuchsia() override;

  VideoCaptureDeviceFactoryFuchsia(const VideoCaptureDeviceFactoryFuchsia&) =
      delete;
  VideoCaptureDeviceFactoryFuchsia& operator=(
      const VideoCaptureDeviceFactoryFuchsia&) = delete;

  // VideoCaptureDeviceFactory implementation.
  VideoCaptureErrorOrDevice CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDevicesInfo(GetDevicesInfoCallback callback) override;

 private:
  // Helper class used to fetch per-device information.
  class DeviceConfigFetcher;

  void Initialize();

  void OnDeviceWatcherDisconnected(zx_status_t status);

  void WatchDevices();
  void OnWatchDevicesResult(
      std::vector<fuchsia::camera3::WatchDevicesEvent> events);

  void OnDeviceInfoFetched();

  std::vector<VideoCaptureDeviceInfo> MakeDevicesInfo();
  void MaybeResolvePendingDeviceInfoCallbacks();

  bool received_initial_list_ = false;

  fuchsia::camera3::DeviceWatcherPtr device_watcher_;

  // Current list of devices. Set to nullopt if the list hasn't been received
  // yet.
  std::optional<
      base::small_map<std::map<uint64_t, std::unique_ptr<DeviceConfigFetcher>>>>
      devices_;

  size_t num_pending_device_info_requests_ = 0;

  std::vector<GetDevicesInfoCallback> pending_devices_info_requests_;

  base::WeakPtrFactory<VideoCaptureDeviceFactoryFuchsia> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_FUCHSIA_VIDEO_CAPTURE_DEVICE_FACTORY_FUCHSIA_H_
