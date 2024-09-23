// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/mediastream/media_stream_device_observer.h"

#include <stddef.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "third_party/blink/public/platform/interface_registry.h"
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

MediaStreamDeviceObserver::MediaStreamDeviceObserver(LocalFrame* frame) {
  // There is no frame on unit tests.
  if (frame) {
    frame->GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
        &MediaStreamDeviceObserver::BindMediaStreamDeviceObserverReceiver,
        weak_factory_.GetWeakPtr()));
  }
}

MediaStreamDeviceObserver::~MediaStreamDeviceObserver() = default;

MediaStreamDevices MediaStreamDeviceObserver::GetNonScreenCaptureDevices() {
  MediaStreamDevices video_devices;
  for (const auto& stream_it : label_stream_map_) {
    for (const auto& stream : stream_it.value) {
      for (const auto& video_device : stream.video_devices) {
        if (!IsScreenCaptureMediaType(video_device.type))
          video_devices.push_back(video_device);
      }
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

  for (Stream& stream : it->value) {
    if (IsAudioInputMediaType(device.type)) {
      RemoveStreamDeviceFromArray(device, &stream.audio_devices);
    } else {
      RemoveStreamDeviceFromArray(device, &stream.video_devices);
    }
    if (stream.on_device_stopped_cb) {
      // Running `stream.on_device_stopped_cb` can destroy `this`. Use a weak
      // pointer to detect that condition, and stop processing if it happens.
      base::WeakPtr<MediaStreamDeviceObserver> weak_this =
          weak_factory_.GetWeakPtr();
      stream.on_device_stopped_cb.Run(device);
      if (!weak_this) {
        return;
      }
    }
  }

  // |it| could have already been invalidated in the function call above. So we
  // need to check if |label| is still in |label_stream_map_| again.
  // Note: this is a quick fix to the crash caused by erasing the invalidated
  // iterator from |label_stream_map_| (https://crbug.com/616884). Future work
  // needs to be done to resolve this re-entrancy issue.
  it = label_stream_map_.find(label);
  if (it == label_stream_map_.end()) {
    return;
  }

  Vector<Stream>& streams = it->value;
  auto stream_it = streams.begin();
  while (stream_it != it->value.end()) {
    Stream& stream = *stream_it;
    if (stream.audio_devices.empty() && stream.video_devices.empty()) {
      stream_it = it->value.erase(stream_it);
    } else {
      ++stream_it;
    }
  }

  if (it->value.empty())
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
  // OnDeviceChanged cannot only happen in combination with getAllScreensMedia,
  // which is the only API that handles multiple streams at once.
  DCHECK_EQ(1u, it->value.size());

  Stream* stream = &it->value[0];
  if (stream->on_device_changed_cb) {
    // Running `stream->on_device_changed_cb` can destroy `this`. Use a weak
    // pointer to detect that condition, and stop processing if it happens.
    base::WeakPtr<MediaStreamDeviceObserver> weak_this =
        weak_factory_.GetWeakPtr();
    stream->on_device_changed_cb.Run(old_device, new_device);
    if (!weak_this) {
      return;
    }
  }

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

void MediaStreamDeviceObserver::OnDeviceRequestStateChange(
    const String& label,
    const MediaStreamDevice& device,
    const mojom::blink::MediaStreamStateChange new_state) {
  DVLOG(1) << __func__ << " label=" << label << " device_id=" << device.id;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end()) {
    // This can happen if a user stops a device from JS at the same
    // time as the underlying media device is unplugged from the system.
    return;
  }

  for (Stream& stream : it->value) {
    if (stream.ContainsDevice(device) &&
        stream.on_device_request_state_change_cb) {
      stream.on_device_request_state_change_cb.Run(device, new_state);
      break;
    }
  }
}

void MediaStreamDeviceObserver::OnDeviceCaptureConfigurationChange(
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

  for (Stream& stream : it->value) {
    if (stream.ContainsDevice(device) &&
        stream.on_device_capture_configuration_change_cb) {
      stream.on_device_capture_configuration_change_cb.Run(device);
      break;
    }
  }
}

void MediaStreamDeviceObserver::OnDeviceCaptureHandleChange(
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
  // OnDeviceCaptureHandleChange cannot only happen in combination with
  // getAllScreensMedia, which is the only API that handles multiple streams
  // at once.
  DCHECK_EQ(1u, it->value.size());

  Stream* stream = &it->value[0];
  if (stream->on_device_capture_handle_change_cb) {
    stream->on_device_capture_handle_change_cb.Run(device);
  }
}

void MediaStreamDeviceObserver::OnZoomLevelChange(
    const String& label,
    const MediaStreamDevice& device,
    int zoom_level) {
  DVLOG(1) << __func__ << " label=" << label << " device_id=" << device.id;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK_GT(zoom_level, 0);

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end()) {
    return;
  }

  Vector<Stream>& streams = it->value;
  if (streams.size() != 1u) {
    return;
  }

  Stream* stream = &streams[0];
  if (!stream) {
    return;
  }

  if (stream->on_zoom_level_change_cb) {
    stream->on_zoom_level_change_cb.Run(device, zoom_level);
  }
#endif
}

void MediaStreamDeviceObserver::BindMediaStreamDeviceObserverReceiver(
    mojo::PendingReceiver<mojom::blink::MediaStreamDeviceObserver> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void MediaStreamDeviceObserver::AddStreams(
    const String& label,
    const mojom::blink::StreamDevicesSet& stream_devices_set,
    const WebMediaStreamDeviceObserver::StreamCallbacks& stream_callbacks) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Vector<Stream> streams;
  for (const mojom::blink::StreamDevicesPtr& stream_devices_ptr :
       stream_devices_set.stream_devices) {
    const mojom::blink::StreamDevices& stream_devices = *stream_devices_ptr;
    Stream stream;
    stream.on_device_stopped_cb = stream_callbacks.on_device_stopped_cb;
    stream.on_device_changed_cb = stream_callbacks.on_device_changed_cb;
    stream.on_device_request_state_change_cb =
        stream_callbacks.on_device_request_state_change_cb;
    stream.on_device_capture_configuration_change_cb =
        stream_callbacks.on_device_capture_configuration_change_cb;
    stream.on_device_capture_handle_change_cb =
        stream_callbacks.on_device_capture_handle_change_cb;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    stream.on_zoom_level_change_cb = stream_callbacks.on_zoom_level_change_cb;
#endif
    if (stream_devices.audio_device.has_value()) {
      stream.audio_devices.push_back(stream_devices.audio_device.value());
    }
    if (stream_devices.video_device.has_value()) {
      stream.video_devices.push_back(stream_devices.video_device.value());
    }
    streams.emplace_back(std::move(stream));
  }
  label_stream_map_.Set(label, streams);
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
    NOTREACHED_IN_MIGRATION();

  label_stream_map_.Set(label, Vector<Stream>{std::move(stream)});
}

bool MediaStreamDeviceObserver::RemoveStreams(const String& label) {
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
    for (auto stream_it = entry.value.begin();
         stream_it != entry.value.end();) {
      Stream& stream = *stream_it;
      MediaStreamDevices& audio_devices = stream.audio_devices;
      MediaStreamDevices& video_devices = stream.video_devices;
      if (RemoveStreamDeviceFromArray(device, &audio_devices) ||
          RemoveStreamDeviceFromArray(device, &video_devices)) {
        device_found = true;
      }
      if (audio_devices.empty() && video_devices.empty()) {
        stream_it = entry.value.erase(stream_it);
      } else {
        ++stream_it;
      }
    }

    if (device_found && entry.value.size() == 0) {
      streams_to_remove.push_back(entry.key);
    }
  }
  for (const String& label : streams_to_remove) {
    label_stream_map_.erase(label);
  }
}

base::UnguessableToken MediaStreamDeviceObserver::GetAudioSessionId(
    const String& label) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end() || it->value.empty() ||
      it->value[0].audio_devices.empty())
    return base::UnguessableToken();

  // It is assumed that all devices belong to the same request and
  // therefore have the same session id.
  return it->value[0].audio_devices[0].session_id();
}

base::UnguessableToken MediaStreamDeviceObserver::GetVideoSessionId(
    const String& label) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end() || it->value.empty() ||
      it->value[0].video_devices.empty())
    return base::UnguessableToken();

  // It is assumed that all devices belong to the same request and
  // therefore have the same session id.
  return it->value[0].video_devices[0].session_id();
}

bool MediaStreamDeviceObserver::Stream::ContainsDevice(
    const MediaStreamDevice& device) const {
  for (blink::MediaStreamDevice stream_device : audio_devices) {
    if (device.IsSameDevice(stream_device)) {
      return true;
    }
  }

  for (blink::MediaStreamDevice stream_device : video_devices) {
    if (device.IsSameDevice(stream_device)) {
      return true;
    }
  }

  return false;
}

}  // namespace blink
