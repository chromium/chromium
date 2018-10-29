// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/media_stream_device_observer.h"

#include <stddef.h>

#include <utility>

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "content/child/child_thread_impl.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/media/stream/media_stream_dispatcher_eventhandler.h"
#include "url/origin.h"

namespace content {

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
  base::WeakPtr<MediaStreamDispatcherEventHandler> handler;
  MediaStreamDevices audio_devices;
  MediaStreamDevices video_devices;
};

MediaStreamDeviceObserver::MediaStreamDeviceObserver(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame), binding_(this) {
  registry_.AddInterface(base::Bind(
      &MediaStreamDeviceObserver::BindMediaStreamDeviceObserverRequest,
      base::Unretained(this)));
}

MediaStreamDeviceObserver::~MediaStreamDeviceObserver() {}

MediaStreamDevices MediaStreamDeviceObserver::GetNonScreenCaptureDevices() {
  MediaStreamDevices video_devices;
  for (const auto& stream_it : label_stream_map_) {
    for (const auto& video_device : stream_it.second.video_devices) {
      if (!IsScreenCaptureMediaType(video_device.type))
        video_devices.push_back(video_device);
    }
  }
  return video_devices;
}

void MediaStreamDeviceObserver::OnInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

void MediaStreamDeviceObserver::OnDestruct() {
  // Do not self-destruct. UserMediaClientImpl owns |this|.
}

void MediaStreamDeviceObserver::OnDeviceStopped(
    const std::string& label,
    const MediaStreamDevice& device) {
  DVLOG(1) << __func__ << " label=" << label << " device_id=" << device.id;
  DCHECK(thread_checker_.CalledOnValidThread());

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end()) {
    // This can happen if a user happen stop a the device from JS at the same
    // time as the underlying media device is unplugged from the system.
    return;
  }
  Stream* stream = &it->second;
  if (IsAudioInputMediaType(device.type))
    RemoveStreamDeviceFromArray(device, &stream->audio_devices);
  else
    RemoveStreamDeviceFromArray(device, &stream->video_devices);

  if (stream->handler.get())
    stream->handler->OnDeviceStopped(device);

  // |it| could have already been invalidated in the function call above. So we
  // need to check if |label| is still in |label_stream_map_| again.
  // Note: this is a quick fix to the crash caused by erasing the invalidated
  // iterator from |label_stream_map_| (crbug.com/616884). Future work needs to
  // be done to resolve this re-entrancy issue.
  it = label_stream_map_.find(label);
  if (it == label_stream_map_.end())
    return;
  stream = &it->second;
  if (stream->audio_devices.empty() && stream->video_devices.empty())
    label_stream_map_.erase(it);
}

void MediaStreamDeviceObserver::BindMediaStreamDeviceObserverRequest(
    mojom::MediaStreamDeviceObserverRequest request) {
  binding_.Bind(std::move(request));
}

void MediaStreamDeviceObserver::AddStream(
    const std::string& label,
    const MediaStreamDevices& audio_devices,
    const MediaStreamDevices& video_devices,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());

  Stream stream;
  stream.handler = event_handler;
  stream.audio_devices = audio_devices;
  stream.video_devices = video_devices;

  label_stream_map_[label] = stream;
}

void MediaStreamDeviceObserver::AddStream(const std::string& label,
                                          const MediaStreamDevice& device) {
  DCHECK(thread_checker_.CalledOnValidThread());

  Stream stream;
  if (IsAudioInputMediaType(device.type))
    stream.audio_devices.push_back(device);
  else if (IsVideoInputMediaType(device.type))
    stream.video_devices.push_back(device);
  else
    NOTREACHED();

  label_stream_map_[label] = stream;
}

bool MediaStreamDeviceObserver::RemoveStream(const std::string& label) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end())
    return false;

  label_stream_map_.erase(it);
  return true;
}

void MediaStreamDeviceObserver::RemoveStreamDevice(
    const MediaStreamDevice& device) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Remove |device| from all streams in |label_stream_map_|.
  bool device_found = false;
  auto stream_it = label_stream_map_.begin();
  while (stream_it != label_stream_map_.end()) {
    MediaStreamDevices& audio_devices = stream_it->second.audio_devices;
    MediaStreamDevices& video_devices = stream_it->second.video_devices;

    if (RemoveStreamDeviceFromArray(device, &audio_devices) ||
        RemoveStreamDeviceFromArray(device, &video_devices)) {
      device_found = true;
      if (audio_devices.empty() && video_devices.empty()) {
        label_stream_map_.erase(stream_it++);
        continue;
      }
    }
    ++stream_it;
  }
  DCHECK(device_found);
}

int MediaStreamDeviceObserver::audio_session_id(const std::string& label) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end() || it->second.audio_devices.empty())
    return MediaStreamDevice::kNoId;

  return it->second.audio_devices[0].session_id;
}

int MediaStreamDeviceObserver::video_session_id(const std::string& label) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end() || it->second.video_devices.empty())
    return MediaStreamDevice::kNoId;

  return it->second.video_devices[0].session_id;
}

}  // namespace content
