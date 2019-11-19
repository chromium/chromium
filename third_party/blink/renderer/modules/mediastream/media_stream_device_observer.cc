// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_device_observer.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_processor.h"

namespace blink {

namespace {

bool RemoveStreamDeviceFromArray(const MediaStreamDevice& device,
                                 MediaStreamDevices* devices) {
  for (auto device_it = devices->begin(); device_it != devices->end();
       ++device_it) {
    if (device_it->IsSameDevice(device)) {
      devices->erase(device_it);
      return true;
    }
  }
  return false;
}

}  // namespace

struct MediaStreamDeviceObserver::Stream {
  Stream() {}
  ~Stream() {}
  WebMediaStreamDeviceObserver::OnDeviceStoppedCb on_device_stopped_cb;
  WebMediaStreamDeviceObserver::OnDeviceChangedCb on_device_changed_cb;
  MediaStreamDevices audio_devices;
  MediaStreamDevices video_devices;
};

MediaStreamDeviceObserver::MediaStreamDeviceObserver(WebLocalFrame* frame) {
  // There is no frame on unit tests.
  if (!frame)
    return;
  static_cast<LocalFrame*>(WebFrame::ToCoreFrame(*frame))
      ->GetInterfaceRegistry()
      ->AddInterface(WTF::BindRepeating(
          &MediaStreamDeviceObserver::BindMediaStreamDeviceObserverReceiver,
          WTF::Unretained(this)));
}

MediaStreamDeviceObserver::~MediaStreamDeviceObserver() {}

MediaStreamDevices MediaStreamDeviceObserver::GetNonScreenCaptureDevices() {
  MediaStreamDevices video_devices;
  for (const auto& stream_it : label_stream_map_) {
    for (const auto& video_device : stream_it.value.video_devices) {
      if (!IsScreenCaptureMediaType(video_device.type))
        video_devices.push_back(video_device);
    }
  }
  return video_devices;
}

void MediaStreamDeviceObserver::OnDeviceStopped(
    const String& label,
    const MediaStreamDevice& device) {
  DVLOG(1) << __func__ << " label=" << label << " device_id=" << device.id;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end()) {
    // This can happen if a user stops a device from JS at the same
    // time as the underlying media device is unplugged from the system.
    return;
  }
  Stream* stream = &it->value;
  if (IsAudioInputMediaType(device.type))
    RemoveStreamDeviceFromArray(device, &stream->audio_devices);
  else
    RemoveStreamDeviceFromArray(device, &stream->video_devices);

  if (stream->on_device_stopped_cb)
    stream->on_device_stopped_cb.Run(device);

  // |it| could have already been invalidated in the function call above. So we
  // need to check if |label| is still in |label_stream_map_| again.
  // Note: this is a quick fix to the crash caused by erasing the invalidated
  // iterator from |label_stream_map_| (https://crbug.com/616884). Future work
  // needs to be done to resolve this re-entrancy issue.
  it = label_stream_map_.find(label);
  if (it == label_stream_map_.end())
    return;
  stream = &it->value;
  if (stream->audio_devices.empty() && stream->video_devices.empty())
    label_stream_map_.erase(it);
}

void MediaStreamDeviceObserver::OnDeviceChanged(
    const String& label,
    const MediaStreamDevice& old_device,
    const MediaStreamDevice& new_device) {
  DVLOG(1) << __func__ << " old_device_id=" << old_device.id
           << " new_device_id=" << new_device.id;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end()) {
    // This can happen if a user stops a device from JS at the same
    // time as the underlying media device is unplugged from the system.
    return;
  }

  Stream* stream = &it->value;
  if (stream->on_device_changed_cb)
    stream->on_device_changed_cb.Run(old_device, new_device);

  // Update device list only for device changing. Removing device will be
  // handled in its own callback.
  if (old_device.type != mojom::MediaStreamType::NO_SERVICE &&
      new_device.type != mojom::MediaStreamType::NO_SERVICE) {
    if (RemoveStreamDeviceFromArray(old_device, &stream->audio_devices) ||
        RemoveStreamDeviceFromArray(old_device, &stream->video_devices)) {
      if (IsAudioInputMediaType(new_device.type))
        stream->audio_devices.push_back(new_device);
      else
        stream->video_devices.push_back(new_device);
    }
  }
}

void MediaStreamDeviceObserver::BindMediaStreamDeviceObserverReceiver(
    mojo::PendingReceiver<mojom::blink::MediaStreamDeviceObserver> receiver) {
  if (receiver_.is_bound())
    return;

  receiver_.Bind(std::move(receiver));
}

void MediaStreamDeviceObserver::AddStream(
    const String& label,
    const blink::MediaStreamDevices& audio_devices,
    const blink::MediaStreamDevices& video_devices,
    WebMediaStreamDeviceObserver::OnDeviceStoppedCb on_device_stopped_cb,
    WebMediaStreamDeviceObserver::OnDeviceChangedCb on_device_changed_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Stream stream;
  stream.on_device_stopped_cb = std::move(on_device_stopped_cb);
  stream.on_device_changed_cb = std::move(on_device_changed_cb);
  stream.audio_devices = audio_devices;
  stream.video_devices = video_devices;

  label_stream_map_.Set(label, stream);
}

void MediaStreamDeviceObserver::AddStream(const String& label,
                                          const MediaStreamDevice& device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Stream stream;
  if (IsAudioInputMediaType(device.type))
    stream.audio_devices.push_back(device);
  else if (IsVideoInputMediaType(device.type))
    stream.video_devices.push_back(device);
  else
    NOTREACHED();

  label_stream_map_.Set(label, stream);
}

bool MediaStreamDeviceObserver::RemoveStream(const String& label) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end())
    return false;

  label_stream_map_.erase(it);
  return true;
}

void MediaStreamDeviceObserver::RemoveStreamDevice(
    const MediaStreamDevice& device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Remove |device| from all streams in |label_stream_map_|.
  bool device_found = false;
  Vector<String> streams_to_remove;
  for (auto& entry : label_stream_map_) {
    MediaStreamDevices& audio_devices = entry.value.audio_devices;
    MediaStreamDevices& video_devices = entry.value.video_devices;

    if (RemoveStreamDeviceFromArray(device, &audio_devices) ||
        RemoveStreamDeviceFromArray(device, &video_devices)) {
      device_found = true;
      if (audio_devices.empty() && video_devices.empty())
        streams_to_remove.push_back(entry.key);
    }
  }
  DCHECK(device_found);
  for (const String& label : streams_to_remove)
    label_stream_map_.erase(label);
}

base::UnguessableToken MediaStreamDeviceObserver::GetAudioSessionId(
    const String& label) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end() || it->value.audio_devices.empty())
    return base::UnguessableToken();

  return it->value.audio_devices[0].session_id();
}

base::UnguessableToken MediaStreamDeviceObserver::GetVideoSessionId(
    const String& label) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end() || it->value.video_devices.empty())
    return base::UnguessableToken();

  return it->value.video_devices[0].session_id();
}

}  // namespace blink
