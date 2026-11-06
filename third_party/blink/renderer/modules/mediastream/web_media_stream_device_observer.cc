// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/mediastream/web_media_stream_device_observer.h"

#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_device_observer.h"

namespace blink {

WebMediaStreamDeviceObserver::WebMediaStreamDeviceObserver(
    WebLocalFrame* frame) {
  auto* local_frame =
      frame ? static_cast<LocalFrame*>(WebFrame::ToCoreFrame(*frame)) : nullptr;
  observer_ = std::make_unique<MediaStreamDeviceObserver>(local_frame);
}

WebMediaStreamDeviceObserver::~WebMediaStreamDeviceObserver() = default;

MediaStreamDevices WebMediaStreamDeviceObserver::GetNonScreenCaptureDevices() {
  return observer_->GetNonScreenCaptureDevices();
}

void WebMediaStreamDeviceObserver::AddStreams(
    const WebString& label,
    const mojom::blink::StreamDevicesSet& stream_devices_set,
    const StreamCallbacks& stream_callbacks) {
  observer_->AddStreams(label, stream_devices_set, stream_callbacks);
}

void WebMediaStreamDeviceObserver::RemoveStreamDevice(
    const MediaStreamDevice& device) {
  observer_->RemoveStreamDevice(device);
}

}  // namespace blink
