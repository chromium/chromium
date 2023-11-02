// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc/media_stream_device_enumerator_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_capture_devices.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

using blink::MediaStreamDevices;
using content::BrowserThread;
using content::MediaCaptureDevices;

namespace webrtc {

namespace {

// Finds a device in |devices| that has |device_id|, or nullptr if not found.
const blink::MediaStreamDevice* FindDeviceWithId(
    const MediaStreamDevices& devices,
    const std::string& device_id) {
  auto iter = devices.begin();
  for (; iter != devices.end(); ++iter) {
    if (iter->id == device_id)
      return &(*iter);
  }
  return nullptr;
}

}  // namespace

const MediaStreamDevices&
MediaStreamDeviceEnumeratorImpl::GetAudioCaptureDevices() const {
  return MediaCaptureDevices::GetInstance()->GetAudioCaptureDevices();
}

const MediaStreamDevices&
MediaStreamDeviceEnumeratorImpl::GetVideoCaptureDevices() const {
  return MediaCaptureDevices::GetInstance()->GetVideoCaptureDevices();
}

void MediaStreamDeviceEnumeratorImpl::GetDefaultDevicesForBrowserContext(
    content::BrowserContext* context,
    bool audio,
    bool video,
    blink::mojom::StreamDevices& devices) {
  std::string default_device;
  if (audio) {
    const MediaStreamDevices& audio_devices = GetAudioCaptureDevices();
    if (!audio_devices.empty())
      devices.audio_device = audio_devices.front();
  }

  if (video) {
    const MediaStreamDevices& video_devices = GetVideoCaptureDevices();
    if (!video_devices.empty())
      devices.video_device = video_devices.front();
  }
}

const blink::MediaStreamDevice*
MediaStreamDeviceEnumeratorImpl::GetRequestedAudioDevice(
    const std::string& requested_audio_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return FindDeviceWithId(GetAudioCaptureDevices(), requested_audio_device_id);
}

const blink::MediaStreamDevice*
MediaStreamDeviceEnumeratorImpl::GetRequestedVideoDevice(
    const std::string& requested_video_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return FindDeviceWithId(GetVideoCaptureDevices(), requested_video_device_id);
}

}  // namespace webrtc
