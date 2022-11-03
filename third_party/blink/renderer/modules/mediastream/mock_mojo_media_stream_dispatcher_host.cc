// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/mock_mojo_media_stream_dispatcher_host.h"

#include <utility>

#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"

namespace blink {

MockMojoMediaStreamDispatcherHost::~MockMojoMediaStreamDispatcherHost() {}

mojo::PendingRemote<mojom::blink::MediaStreamDispatcherHost>
MockMojoMediaStreamDispatcherHost::CreatePendingRemoteAndBind() {
  return receiver_.BindNewPipeAndPassRemote();
}

void MockMojoMediaStreamDispatcherHost::GenerateStreams(
    int32_t request_id,
    const StreamControls& controls,
    bool user_gesture,
    mojom::blink::StreamSelectionInfoPtr audio_stream_selection_info_ptr,
    GenerateStreamsCallback callback) {
  request_id_ = request_id;
  ++request_stream_counter_;
  stream_devices_ = blink::mojom::blink::StreamDevices();

  blink::mojom::StreamSelectionStrategy strategy =
      audio_stream_selection_info_ptr->strategy;
  if (controls.audio.requested() &&
      (strategy == blink::mojom::StreamSelectionStrategy::SEARCH_BY_DEVICE_ID ||
       strategy == blink::mojom::StreamSelectionStrategy::FORCE_NEW_STREAM)) {
    stream_devices_.audio_device = MediaStreamDevice(
        controls.audio.stream_type,
        controls.audio.device_id + session_id_.ToString(), "microphone");
    stream_devices_.audio_device.value().set_session_id(session_id_);
    stream_devices_.audio_device.value().matched_output_device_id =
        "associated_output_device_id" + session_id_.ToString();
  }

  if (controls.video.requested()) {
    stream_devices_.video_device = MediaStreamDevice(
        controls.video.stream_type,
        controls.video.device_id + session_id_.ToString(), "usb video camera");
    stream_devices_.video_device.value().video_facing =
        media::MEDIA_VIDEO_FACING_USER;
    stream_devices_.video_device.value().set_session_id(session_id_);
  }

  if (do_not_run_cb_) {
    generate_stream_cb_ = std::move(callback);
  } else {
    // TODO(crbug.com/1300883): Generalize to multiple streams.
    blink::mojom::blink::StreamDevicesSetPtr stream_devices_set =
        blink::mojom::blink::StreamDevicesSet::New();
    stream_devices_set->stream_devices.emplace_back(stream_devices_.Clone());
    std::move(callback).Run(mojom::blink::MediaStreamRequestResult::OK,
                            String("dummy") + String::Number(request_id_),
                            std::move(stream_devices_set),
                            /*pan_tilt_zoom_allowed=*/false);
  }
}

void MockMojoMediaStreamDispatcherHost::GetOpenDevice(
    int32_t request_id,
    const base::UnguessableToken&,
    const base::UnguessableToken&,
    GetOpenDeviceCallback callback) {
  request_id_ = request_id;
  ++request_stream_counter_;

  if (do_not_run_cb_) {
    get_open_device_cb_ = std::move(callback);
  } else {
    blink::mojom::blink::StreamDevicesSetPtr stream_devices_set =
        blink::mojom::blink::StreamDevicesSet::New();
    stream_devices_set->stream_devices.emplace_back(stream_devices_.Clone());
    if (stream_devices_.video_device) {
      std::move(callback).Run(mojom::blink::MediaStreamRequestResult::OK,
                              mojom::blink::GetOpenDeviceResponse::New(
                                  String("dummy") + String::Number(request_id_),
                                  *stream_devices_.video_device,
                                  /*pan_tilt_zoom_allowed=*/false));
    } else if (stream_devices_.audio_device) {
      std::move(callback).Run(mojom::blink::MediaStreamRequestResult::OK,
                              mojom::blink::GetOpenDeviceResponse::New(
                                  String("dummy") + String::Number(request_id_),
                                  *stream_devices_.audio_device,
                                  /*pan_tilt_zoom_allowed=*/false));
    } else {
      std::move(callback).Run(
          mojom::blink::MediaStreamRequestResult::INVALID_STATE, nullptr);
    }
  }
}

void MockMojoMediaStreamDispatcherHost::CancelRequest(int32_t request_id) {
  EXPECT_EQ(request_id, request_id_);
}

void MockMojoMediaStreamDispatcherHost::StopStreamDevice(
    const String& device_id,
    const absl::optional<base::UnguessableToken>& session_id) {
  if (stream_devices_.audio_device.has_value()) {
    const MediaStreamDevice& device = stream_devices_.audio_device.value();
    if (device.id == device_id.Utf8() && device.session_id() == session_id) {
      ++stop_audio_device_counter_;
      return;
    }
  }

  if (stream_devices_.video_device.has_value()) {
    const MediaStreamDevice& device = stream_devices_.video_device.value();
    if (device.id == device_id.Utf8() && device.session_id() == session_id) {
      ++stop_video_device_counter_;
      return;
    }
  }
  // Require that the device is found if a new session id has not been
  // requested.
  if (session_id == session_id_) {
    NOTREACHED();
  }
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
