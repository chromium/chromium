// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_DEVICE_OBSERVER_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_DEVICE_OBSERVER_H_

#include <memory>

#include "base/unguessable_token.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"

namespace blink {

namespace mojom::blink {
class StreamDevicesSet;
}

class MediaStreamDeviceObserver;
class WebLocalFrame;

class BLINK_MODULES_EXPORT WebMediaStreamDeviceObserver {
 public:
  explicit WebMediaStreamDeviceObserver(WebLocalFrame* frame);
  ~WebMediaStreamDeviceObserver();

  MediaStreamDevices GetNonScreenCaptureDevices();

  using OnDeviceStoppedCb =
      base::RepeatingCallback<void(const MediaStreamDevice& device)>;
  using OnDeviceChangedCb =
      base::RepeatingCallback<void(const MediaStreamDevice& old_device,
                                   const MediaStreamDevice& new_device)>;
  using OnDeviceRequestStateChangeCb = base::RepeatingCallback<void(
      const MediaStreamDevice& device,
      const mojom::MediaStreamStateChange new_state)>;
  using OnDeviceCaptureHandleChangeCb =
      base::RepeatingCallback<void(const MediaStreamDevice& device)>;
  void AddStreams(
      const WebString& label,
      const mojom::blink::StreamDevicesSet& stream_devices_set,
      OnDeviceStoppedCb on_device_stopped_cb,
      OnDeviceChangedCb on_device_changed_cb,
      OnDeviceRequestStateChangeCb on_device_request_state_change_cb,
      OnDeviceCaptureHandleChangeCb on_device_capture_handle_change_cb);
  void AddStream(const WebString& label, const MediaStreamDevice& device);
  bool RemoveStreams(const WebString& label);
  void RemoveStreamDevice(const MediaStreamDevice& device);

  // Get the video session_id given a label. The label identifies a stream.
  base::UnguessableToken GetVideoSessionId(const WebString& label);
  // Returns an audio session_id given a label.
  base::UnguessableToken GetAudioSessionId(const WebString& label);

 private:
  std::unique_ptr<MediaStreamDeviceObserver> observer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_DEVICE_OBSERVER_H_
