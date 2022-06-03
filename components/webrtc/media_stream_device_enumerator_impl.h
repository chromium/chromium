// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBRTC_MEDIA_STREAM_DEVICE_ENUMERATOR_IMPL_H_
#define COMPONENTS_WEBRTC_MEDIA_STREAM_DEVICE_ENUMERATOR_IMPL_H_

#include "components/webrtc/media_stream_device_enumerator.h"

namespace webrtc {

// A default MediaStreamDeviceEnumerator that passes through to
// content::MediaCaptureDevices.
class MediaStreamDeviceEnumeratorImpl : public MediaStreamDeviceEnumerator {
 public:
  MediaStreamDeviceEnumeratorImpl() = default;
  MediaStreamDeviceEnumeratorImpl(const MediaStreamDeviceEnumeratorImpl&) =
      delete;
  MediaStreamDeviceEnumeratorImpl& operator=(MediaStreamDeviceEnumeratorImpl&) =
      delete;
  ~MediaStreamDeviceEnumeratorImpl() override = default;

  // MediaStreamDeviceEnumerator:
  const blink::MediaStreamDevices& GetAudioCaptureDevices() const override;
  const blink::MediaStreamDevices& GetVideoCaptureDevices() const override;
  void GetDefaultDevicesForBrowserContext(
      content::BrowserContext* context,
      bool audio,
      bool video,
      blink::MediaStreamDevices* devices) override;
  const blink::MediaStreamDevice* GetRequestedAudioDevice(
      const std::string& requested_audio_device_id) override;
  const blink::MediaStreamDevice* GetRequestedVideoDevice(
      const std::string& requested_video_device_id) override;
};

}  // namespace webrtc

#endif  // COMPONENTS_WEBRTC_MEDIA_STREAM_DEVICE_ENUMERATOR_IMPL_H_
