// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/mock_mojo_media_stream_dispatcher_host.h"

#include <utility>

#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/text/format.h"

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
  const bool session_id_was_explicitly_reset =
      std::exchange(session_id_was_explicitly_reset_, false);

  if (controls.audio.requested() &&
      audio_stream_selection_info_ptr->is_search_by_session_id() &&
      !audio_stream_selection_info_ptr->get_search_by_session_id().is_null()) {
    base::UnguessableToken audio_session_id = session_id_;
    if (controls.audio.stream_type ==
        mojom::blink::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
      // Honor `session_id_map` from
      // UserMediaProcessor::DetermineExistingAudioSessionIds() without mutating
      // `session_id_` (so video devices are unaffected):
      // - If a matching active session ID was provided for the requested device
      //   (and the test did not explicitly call ResetSessionId()), reuse it.
      // - Otherwise, if an audio session is already active and `session_id_map`
      //   has no match for this request, allocate a new audio session ID to
      //   model opening a separate capture session.
      const auto& session_id_map =
          audio_stream_selection_info_ptr->get_search_by_session_id()
              ->session_id_map;
      // In production, the browser may select any eligible device from
      // `controls.audio.device_ids` and looks up that selected device ID in
      // `session_id_map`. This mock always opens `device_ids.front()` below,
      // so look up `device_ids.front()` here.
      String requested_device_id =
          controls.audio.device_ids.empty()
              ? String()
              : String::FromUtf8(controls.audio.device_ids.front());
      auto it = session_id_map.find(requested_device_id);
      if (!session_id_was_explicitly_reset && it != session_id_map.end() &&
          std::ranges::contains(allocated_audio_session_ids_, it->value)) {
        audio_session_id = it->value;
      } else if (!session_id_was_explicitly_reset &&
                 !allocated_audio_session_ids_.empty()) {
        audio_session_id = base::UnguessableToken::Create();
      }
      if (!std::ranges::contains(allocated_audio_session_ids_,
                                 audio_session_id)) {
        allocated_audio_session_ids_.push_back(audio_session_id);
      }
    }

    if (controls.audio.stream_type ==
        mojom::blink::MediaStreamType::DISPLAY_AUDIO_CAPTURE) {
      stream_devices_.audio_device = MediaStreamDevice(
          controls.audio.stream_type, "loopback", "System Audio");
    } else {
      stream_devices_.audio_device = MediaStreamDevice(
          controls.audio.stream_type, controls.audio.device_ids.front(),
          "microphone:" +
              MaybeAppendSessionId(controls.audio.device_ids.front(),
                                   audio_session_id));
    }
    stream_devices_.audio_device->set_session_id(audio_session_id);
    stream_devices_.audio_device->matched_output_device_id =
        MaybeAppendSessionId("associated_output_device_id", audio_session_id);
    if (audio_device_effects_.has_value()) {
      stream_devices_.audio_device->input.set_effects(*audio_device_effects_);
    }
  }

  if (controls.video.requested()) {
    if (controls.video.stream_type ==
        mojom::blink::MediaStreamType::DISPLAY_VIDEO_CAPTURE) {
      stream_devices_.video_device = MediaStreamDevice(
          controls.video.stream_type, "screen:1:0", "screen:1:0");
    } else {
      stream_devices_.video_device = MediaStreamDevice(
          controls.video.stream_type, controls.video.device_ids.front(),
          "camera:" + MaybeAppendSessionId(controls.video.device_ids.front(),
                                           session_id_));
    }
    stream_devices_.video_device->video_facing = media::MEDIA_VIDEO_FACING_USER;
    stream_devices_.video_device->set_session_id(session_id_);
  }

  if (do_not_run_cb_) {
    generate_stream_cb_ = std::move(callback);
  } else {
    // TODO(crbug.com/1300883): Generalize to multiple streams.
    blink::mojom::blink::StreamDevicesSetPtr stream_devices_set =
        blink::mojom::blink::StreamDevicesSet::New();
    stream_devices_set->stream_devices.emplace_back(stream_devices_.Clone());
    std::move(callback).Run(mojom::blink::MediaStreamRequestResult::OK,
                            Format("dummy{}", request_id_),
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
      std::move(callback).Run(
          mojom::blink::MediaStreamRequestResult::OK,
          mojom::blink::GetOpenDeviceResponse::New(
              Format("dummy{}", request_id_), *stream_devices_.video_device,
              /*pan_tilt_zoom_allowed=*/false));
    } else if (stream_devices_.audio_device) {
      std::move(callback).Run(
          mojom::blink::MediaStreamRequestResult::OK,
          mojom::blink::GetOpenDeviceResponse::New(
              Format("dummy{}", request_id_), *stream_devices_.audio_device,
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
    const std::optional<base::UnguessableToken>& session_id) {
  if (stream_devices_.audio_device.has_value()) {
    const MediaStreamDevice& device = stream_devices_.audio_device.value();
    // Accept either the most recently generated audio device session ID or any
    // previously allocated audio session ID when multiple split sessions are
    // stopped.
    if (device.id == device_id.Utf8()) {
      const bool removed_allocated_session =
          session_id.has_value() &&
          Erase(allocated_audio_session_ids_, *session_id) > 0;
      if (device.session_id() == session_id || removed_allocated_session) {
        ++stop_audio_device_counter_;
        return;
      }
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
  std::move(callback).Run(true /* success */, Format("dummy{}", request_id),
                          device);
}

std::string MockMojoMediaStreamDispatcherHost::MaybeAppendSessionId(
    std::string device_id,
    const base::UnguessableToken& session_id) {
  if (!append_session_id_to_device_ids_) {
    return device_id;
  }
  return device_id + session_id.ToString();
}

}  // namespace blink
