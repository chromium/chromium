// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/mock_mojo_media_stream_dispatcher_host.h"

#include <utility>

#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"

namespace blink {

MockMojoMediaStreamDispatcherHost::MockMojoMediaStreamDispatcherHost() {}

MockMojoMediaStreamDispatcherHost::~MockMojoMediaStreamDispatcherHost() {}

mojo::PendingRemote<mojom::blink::MediaStreamDispatcherHost>
MockMojoMediaStreamDispatcherHost::CreatePendingRemoteAndBind() {
  return receiver_.BindNewPipeAndPassRemote();
}

void MockMojoMediaStreamDispatcherHost::GenerateStream(
    int32_t request_id,
    const StreamControls& controls,
    bool user_gesture,
    mojom::blink::StreamSelectionInfoPtr audio_stream_selection_info_ptr,
    GenerateStreamCallback callback) {
  request_id_ = request_id;
  audio_devices_.clear();
  video_devices_.clear();
  ++request_stream_counter_;

  blink::mojom::StreamSelectionStrategy strategy =
      audio_stream_selection_info_ptr->strategy;
  if (controls.audio.requested &&
      (strategy == blink::mojom::StreamSelectionStrategy::SEARCH_BY_DEVICE_ID ||
       strategy == blink::mojom::StreamSelectionStrategy::FORCE_NEW_STREAM)) {
    MediaStreamDevice audio_device;
    audio_device.id = controls.audio.device_id + session_id_.ToString();
    audio_device.name = "microphone";
    audio_device.type = controls.audio.stream_type;
    audio_device.set_session_id(session_id_);
    audio_device.matched_output_device_id =
        "associated_output_device_id" + session_id_.ToString();
    audio_devices_.push_back(std::move(audio_device));
  }

  if (controls.video.requested) {
    MediaStreamDevice video_device;
    video_device.id = controls.video.device_id + session_id_.ToString();
    video_device.name = "usb video camera";
    video_device.type = controls.video.stream_type;
    video_device.video_facing = media::MEDIA_VIDEO_FACING_USER;
    video_device.set_session_id(session_id_);
    video_devices_.push_back(video_device);
  }

  if (do_not_run_cb_) {
    generate_stream_cb_ = std::move(callback);
  } else {
    std::move(callback).Run(mojom::blink::MediaStreamRequestResult::OK,
                            String("dummy") + String::Number(request_id_),
                            audio_devices_, video_devices_);
  }
}

void MockMojoMediaStreamDispatcherHost::CancelRequest(int32_t request_id) {
  EXPECT_EQ(request_id, request_id_);
}

void MockMojoMediaStreamDispatcherHost::StopStreamDevice(
    const String& device_id,
    const base::Optional<base::UnguessableToken>& session_id) {
  for (const MediaStreamDevice& device : audio_devices_) {
    if (device.id == device_id.Utf8() && device.session_id() == session_id) {
      ++stop_audio_device_counter_;
      return;
    }
  }
  for (const MediaStreamDevice& device : video_devices_) {
    if (device.id == device_id.Utf8() && device.session_id() == session_id) {
      ++stop_video_device_counter_;
      return;
    }
  }
  NOTREACHED();
}

void MockMojoMediaStreamDispatcherHost::OpenDevice(
    int32_t request_id,
    const String& device_id,
    mojom::blink::MediaStreamType type,
    OpenDeviceCallback callback) {
  MediaStreamDevice device;
  device.id = device_id.Utf8();
  device.type = type;
  device.set_session_id(session_id_);
  std::move(callback).Run(true /* success */,
                          "dummy" + String::Number(request_id), device);
}

}  // namespace blink
