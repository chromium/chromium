// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/mediastream/media_stream_mojom_traits.h"

#include "base/check_op.h"
#include "media/base/ipc/media_param_traits.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/mojom/video_capture_types_mojom_traits.h"
#include "media/mojo/mojom/display_media_information.mojom.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "third_party/blink/public/common/mediastream/media_device_id.h"

namespace {
const size_t kMaxDeviceIdCount = 100;
const size_t kMaxDeviceIdSize = 500;

}  // namespace

namespace mojo {

// static
bool StructTraits<blink::mojom::MediaStreamDeviceDataView,
                  blink::MediaStreamDevice>::
    Read(blink::mojom::MediaStreamDeviceDataView input,
         blink::MediaStreamDevice* out) {
  if (!input.ReadType(&out->type)) {
    return false;
  }
  if (!input.ReadId(&out->id)) {
    return false;
  }
  out->display_id = input.display_id();
  if (!input.ReadVideoFacing(&out->video_facing)) {
    return false;
  }
  if (!input.ReadGroupId(&out->group_id)) {
    return false;
  }
  if (!input.ReadMatchedOutputDeviceId(&out->matched_output_device_id)) {
    return false;
  }
  if (!input.ReadName(&out->name)) {
    return false;
  }
  if (!input.ReadInput(&out->input)) {
    return false;
  }
  std::optional<base::UnguessableToken> session_id;
  if (input.ReadSessionId(&session_id)) {
    out->set_session_id(session_id ? *session_id : base::UnguessableToken());
  } else {
    return false;
  }
  if (!input.ReadDisplayMediaInfo(&out->display_media_info)) {
    return false;
  }
  return true;
}

// static
bool StructTraits<blink::mojom::TrackControlsDataView, blink::TrackControls>::
    Read(blink::mojom::TrackControlsDataView input, blink::TrackControls* out) {
  if (!input.ReadStreamType(&out->stream_type)) {
    return false;
  }
  if (!input.ReadDeviceIds(&out->device_ids)) {
    return false;
  }
  if (out->device_ids.size() > kMaxDeviceIdCount) {
    return false;
  }
  for (const auto& device_id : out->device_ids) {
    if (out->stream_type ==
            blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE ||
        out->stream_type ==
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
      if (!blink::IsValidMediaDeviceId(device_id)) {
        return false;
      }
    } else {
      if (device_id.size() > kMaxDeviceIdSize) {
        return false;
      }
    }
  }
  return true;
}

// static
bool StructTraits<blink::mojom::StreamControlsDataView, blink::StreamControls>::
    Read(blink::mojom::StreamControlsDataView input,
         blink::StreamControls* out) {
  if (!input.ReadAudio(&out->audio)) {
    return false;
  }
  if (!input.ReadVideo(&out->video)) {
    return false;
  }
  DCHECK(out->audio.requested() ||
         (!input.hotword_enabled() && !input.disable_local_echo() &&
          !input.suppress_local_audio_playback()));
  out->hotword_enabled = input.hotword_enabled();
  out->disable_local_echo = input.disable_local_echo();
  out->suppress_local_audio_playback = input.suppress_local_audio_playback();
  out->exclude_system_audio = input.exclude_system_audio();
  out->exclude_self_browser_surface = input.exclude_self_browser_surface();
  out->request_pan_tilt_zoom_permission =
      input.request_pan_tilt_zoom_permission();
  out->request_all_screens = input.request_all_screens();
  out->preferred_display_surface = input.preferred_display_surface();
  out->dynamic_surface_switching_requested =
      input.dynamic_surface_switching_requested();
  out->exclude_monitor_type_surfaces = input.exclude_monitor_type_surfaces();
  return true;
}

}  // namespace mojo
