// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBRTC_MEDIA_STREAM_DEVICE_ENUMERATOR_H_
#define COMPONENTS_WEBRTC_MEDIA_STREAM_DEVICE_ENUMERATOR_H_

#include <string>

#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

namespace content {
class BrowserContext;
}

namespace webrtc {

// Enumerates system devices for MediaStreamDevicesController.
class MediaStreamDeviceEnumerator {
 public:
  // Helper to get the default devices which can be used by the media request.
  // Uses the first available devices if the default devices are not available.
  // If the return list is empty, it means there is no available device on the
  // OS.
  virtual const blink::MediaStreamDevices& GetAudioCaptureDevices() const = 0;
  virtual const blink::MediaStreamDevices& GetVideoCaptureDevices() const = 0;

  // Helper to get the default devices which can be used by the media request.
  // Uses the first available devices if the default devices are not available.
  // If the return list is empty, it means there is no available device on the
  // OS.
  virtual void GetDefaultDevicesForBrowserContext(
      content::BrowserContext* context,
      bool audio,
      bool video,
      blink::MediaStreamDevices* devices) = 0;

  // Helpers for picking particular requested devices, identified by raw id.
  // If the device requested is not available it will return nullptr.
  virtual const blink::MediaStreamDevice* GetRequestedAudioDevice(
      const std::string& requested_audio_device_id) = 0;
  virtual const blink::MediaStreamDevice* GetRequestedVideoDevice(
      const std::string& requested_video_device_id) = 0;

 protected:
  virtual ~MediaStreamDeviceEnumerator() = default;
};

}  // namespace webrtc

#endif  // COMPONENTS_WEBRTC_MEDIA_STREAM_DEVICE_ENUMERATOR_H_
