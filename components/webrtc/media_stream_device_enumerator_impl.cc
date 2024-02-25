// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc/media_stream_device_enumerator_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_capture_devices.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

using blink::MediaStreamDevices;
using content::BrowserThread;
using content::MediaCaptureDevices;

namespace webrtc {

const MediaStreamDevices&
MediaStreamDeviceEnumeratorImpl::GetAudioCaptureDevices() const {
  return MediaCaptureDevices::GetInstance()->GetAudioCaptureDevices();
}

const MediaStreamDevices&
MediaStreamDeviceEnumeratorImpl::GetVideoCaptureDevices() const {
  return MediaCaptureDevices::GetInstance()->GetVideoCaptureDevices();
}

const std::optional<blink::MediaStreamDevice>
MediaStreamDeviceEnumeratorImpl::GetPreferredAudioDeviceForBrowserContext(
    content::BrowserContext* context,
    const std::vector<std::string>& eligible_device_ids) const {
  NOTIMPLEMENTED();
  return std::nullopt;
}

const std::optional<blink::MediaStreamDevice>
MediaStreamDeviceEnumeratorImpl::GetPreferredVideoDeviceForBrowserContext(
    content::BrowserContext* context,
    const std::vector<std::string>& eligible_device_ids) const {
  NOTIMPLEMENTED();
  return std::nullopt;
}

}  // namespace webrtc
