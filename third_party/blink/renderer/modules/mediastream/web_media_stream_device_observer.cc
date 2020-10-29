// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

void WebMediaStreamDeviceObserver::AddStream(
    const WebString& label,
    const MediaStreamDevices& audio_devices,
    const MediaStreamDevices& video_devices,
    OnDeviceStoppedCb on_device_stopped_cb,
    OnDeviceChangedCb on_device_changed_cb,
    OnDeviceRequestStateChangeCb on_device_request_state_change_cb) {
  observer_->AddStream(label, audio_devices, video_devices,
                       std::move(on_device_stopped_cb),
                       std::move(on_device_changed_cb),
                       std::move(on_device_request_state_change_cb));
}

void WebMediaStreamDeviceObserver::AddStream(const WebString& label,
                                             const MediaStreamDevice& device) {
  observer_->AddStream(label, device);
}

bool WebMediaStreamDeviceObserver::RemoveStream(const WebString& label) {
  return observer_->RemoveStream(label);
}
void WebMediaStreamDeviceObserver::RemoveStreamDevice(
    const MediaStreamDevice& device) {
  observer_->RemoveStreamDevice(device);
}

// Get the video session_id given a label. The label identifies a stream.
base::UnguessableToken WebMediaStreamDeviceObserver::GetVideoSessionId(
    const WebString& label) {
  return observer_->GetVideoSessionId(label);
}

// Returns an audio session_id given a label.
base::UnguessableToken WebMediaStreamDeviceObserver::GetAudioSessionId(
    const WebString& label) {
  return observer_->GetAudioSessionId(label);
}

}  // namespace blink
