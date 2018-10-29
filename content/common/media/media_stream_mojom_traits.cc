// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/media_stream_mojom_traits.h"

#include "base/logging.h"
#include "media/base/ipc/media_param_traits.h"
#include "media/capture/mojom/video_capture_types_mojom_traits.h"
#include "media/mojo/interfaces/display_media_information.mojom.h"

namespace mojo {

// static
content::mojom::MediaStreamType
EnumTraits<content::mojom::MediaStreamType, content::MediaStreamType>::ToMojom(
    content::MediaStreamType type) {
  switch (type) {
    case content::MediaStreamType::MEDIA_NO_SERVICE:
      return content::mojom::MediaStreamType::MEDIA_NO_SERVICE;
    case content::MediaStreamType::MEDIA_DEVICE_AUDIO_CAPTURE:
      return content::mojom::MediaStreamType::MEDIA_DEVICE_AUDIO_CAPTURE;
    case content::MediaStreamType::MEDIA_DEVICE_VIDEO_CAPTURE:
      return content::mojom::MediaStreamType::MEDIA_DEVICE_VIDEO_CAPTURE;
    case content::MediaStreamType::MEDIA_GUM_TAB_AUDIO_CAPTURE:
      return content::mojom::MediaStreamType::MEDIA_GUM_TAB_AUDIO_CAPTURE;
    case content::MediaStreamType::MEDIA_GUM_TAB_VIDEO_CAPTURE:
      return content::mojom::MediaStreamType::MEDIA_GUM_TAB_VIDEO_CAPTURE;
    case content::MediaStreamType::MEDIA_GUM_DESKTOP_VIDEO_CAPTURE:
      return content::mojom::MediaStreamType::MEDIA_GUM_DESKTOP_VIDEO_CAPTURE;
    case content::MediaStreamType::MEDIA_GUM_DESKTOP_AUDIO_CAPTURE:
      return content::mojom::MediaStreamType::MEDIA_GUM_DESKTOP_AUDIO_CAPTURE;
    case content::MediaStreamType::MEDIA_DISPLAY_VIDEO_CAPTURE:
      return content::mojom::MediaStreamType::MEDIA_DISPLAY_VIDEO_CAPTURE;
    case content::MediaStreamType::NUM_MEDIA_TYPES:
      return content::mojom::MediaStreamType::NUM_MEDIA_TYPES;
  }
  NOTREACHED();
  return content::mojom::MediaStreamType::MEDIA_NO_SERVICE;
}

// static
bool EnumTraits<content::mojom::MediaStreamType, content::MediaStreamType>::
    FromMojom(content::mojom::MediaStreamType input,
              content::MediaStreamType* out) {
  switch (input) {
    case content::mojom::MediaStreamType::MEDIA_NO_SERVICE:
      *out = content::MediaStreamType::MEDIA_NO_SERVICE;
      return true;
    case content::mojom::MediaStreamType::MEDIA_DEVICE_AUDIO_CAPTURE:
      *out = content::MediaStreamType::MEDIA_DEVICE_AUDIO_CAPTURE;
      return true;
    case content::mojom::MediaStreamType::MEDIA_DEVICE_VIDEO_CAPTURE:
      *out = content::MediaStreamType::MEDIA_DEVICE_VIDEO_CAPTURE;
      return true;
    case content::mojom::MediaStreamType::MEDIA_GUM_TAB_AUDIO_CAPTURE:
      *out = content::MediaStreamType::MEDIA_GUM_TAB_AUDIO_CAPTURE;
      return true;
    case content::mojom::MediaStreamType::MEDIA_GUM_TAB_VIDEO_CAPTURE:
      *out = content::MediaStreamType::MEDIA_GUM_TAB_VIDEO_CAPTURE;
      return true;
    case content::mojom::MediaStreamType::MEDIA_GUM_DESKTOP_VIDEO_CAPTURE:
      *out = content::MediaStreamType::MEDIA_GUM_DESKTOP_VIDEO_CAPTURE;
      return true;
    case content::mojom::MediaStreamType::MEDIA_GUM_DESKTOP_AUDIO_CAPTURE:
      *out = content::MediaStreamType::MEDIA_GUM_DESKTOP_AUDIO_CAPTURE;
      return true;
    case content::mojom::MediaStreamType::MEDIA_DISPLAY_VIDEO_CAPTURE:
      *out = content::MediaStreamType::MEDIA_DISPLAY_VIDEO_CAPTURE;
      return true;
    case content::mojom::MediaStreamType::NUM_MEDIA_TYPES:
      *out = content::MediaStreamType::NUM_MEDIA_TYPES;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
content::mojom::MediaStreamRequestResult
EnumTraits<content::mojom::MediaStreamRequestResult,
           content::MediaStreamRequestResult>::
    ToMojom(content::MediaStreamRequestResult result) {
  switch (result) {
    case content::MediaStreamRequestResult::MEDIA_DEVICE_OK:
      return content::mojom::MediaStreamRequestResult::OK;
    case content::MediaStreamRequestResult::MEDIA_DEVICE_PERMISSION_DENIED:
      return content::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
    case content::MediaStreamRequestResult::MEDIA_DEVICE_PERMISSION_DISMISSED:
      return content::mojom::MediaStreamRequestResult::PERMISSION_DISMISSED;
    case content::MediaStreamRequestResult::MEDIA_DEVICE_INVALID_STATE:
      return content::mojom::MediaStreamRequestResult::INVALID_STATE;
    case content::MediaStreamRequestResult::MEDIA_DEVICE_NO_HARDWARE:
      return content::mojom::MediaStreamRequestResult::NO_HARDWARE;
    case content::MediaStreamRequestResult::
        MEDIA_DEVICE_INVALID_SECURITY_ORIGIN:
      return content::mojom::MediaStreamRequestResult::INVALID_SECURITY_ORIGIN;
    case content::MediaStreamRequestResult::MEDIA_DEVICE_TAB_CAPTURE_FAILURE:
      return content::mojom::MediaStreamRequestResult::TAB_CAPTURE_FAILURE;
    case content::MediaStreamRequestResult::MEDIA_DEVICE_SCREEN_CAPTURE_FAILURE:
      return content::mojom::MediaStreamRequestResult::SCREEN_CAPTURE_FAILURE;
    case content::MediaStreamRequestResult::MEDIA_DEVICE_CAPTURE_FAILURE:
      return content::mojom::MediaStreamRequestResult::CAPTURE_FAILURE;
    case content::MediaStreamRequestResult::
        MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED:
      return content::mojom::MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED;
    case content::MediaStreamRequestResult::
        MEDIA_DEVICE_TRACK_START_FAILURE_AUDIO:
      return content::mojom::MediaStreamRequestResult::
          TRACK_START_FAILURE_AUDIO;
    case content::MediaStreamRequestResult::
        MEDIA_DEVICE_TRACK_START_FAILURE_VIDEO:
      return content::mojom::MediaStreamRequestResult::
          TRACK_START_FAILURE_VIDEO;
    case content::MediaStreamRequestResult::MEDIA_DEVICE_NOT_SUPPORTED:
      return content::mojom::MediaStreamRequestResult::NOT_SUPPORTED;
    case content::MediaStreamRequestResult::MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN:
      return content::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN;
    case content::MediaStreamRequestResult::MEDIA_DEVICE_KILL_SWITCH_ON:
      return content::mojom::MediaStreamRequestResult::KILL_SWITCH_ON;
    default:
      break;
  }
  NOTREACHED();
  return content::mojom::MediaStreamRequestResult::OK;
}

// static
bool EnumTraits<content::mojom::MediaStreamRequestResult,
                content::MediaStreamRequestResult>::
    FromMojom(content::mojom::MediaStreamRequestResult input,
              content::MediaStreamRequestResult* out) {
  switch (input) {
    case content::mojom::MediaStreamRequestResult::OK:
      *out = content::MediaStreamRequestResult::MEDIA_DEVICE_OK;
      return true;
    case content::mojom::MediaStreamRequestResult::PERMISSION_DENIED:
      *out = content::MediaStreamRequestResult::MEDIA_DEVICE_PERMISSION_DENIED;
      return true;
    case content::mojom::MediaStreamRequestResult::PERMISSION_DISMISSED:
      *out =
          content::MediaStreamRequestResult::MEDIA_DEVICE_PERMISSION_DISMISSED;
      return true;
    case content::mojom::MediaStreamRequestResult::INVALID_STATE:
      *out = content::MediaStreamRequestResult::MEDIA_DEVICE_INVALID_STATE;
      return true;
    case content::mojom::MediaStreamRequestResult::NO_HARDWARE:
      *out = content::MediaStreamRequestResult::MEDIA_DEVICE_NO_HARDWARE;
      return true;
    case content::mojom::MediaStreamRequestResult::INVALID_SECURITY_ORIGIN:
      *out = content::MediaStreamRequestResult::
          MEDIA_DEVICE_INVALID_SECURITY_ORIGIN;
      return true;
    case content::mojom::MediaStreamRequestResult::TAB_CAPTURE_FAILURE:
      *out =
          content::MediaStreamRequestResult::MEDIA_DEVICE_TAB_CAPTURE_FAILURE;
      return true;
    case content::mojom::MediaStreamRequestResult::SCREEN_CAPTURE_FAILURE:
      *out = content::MediaStreamRequestResult::
          MEDIA_DEVICE_SCREEN_CAPTURE_FAILURE;
      return true;
    case content::mojom::MediaStreamRequestResult::CAPTURE_FAILURE:
      *out = content::MediaStreamRequestResult::MEDIA_DEVICE_CAPTURE_FAILURE;
      return true;
    case content::mojom::MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED:
      *out = content::MediaStreamRequestResult::
          MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED;
      return true;
    case content::mojom::MediaStreamRequestResult::TRACK_START_FAILURE_AUDIO:
      *out = content::MediaStreamRequestResult::
          MEDIA_DEVICE_TRACK_START_FAILURE_AUDIO;
      return true;
    case content::mojom::MediaStreamRequestResult::TRACK_START_FAILURE_VIDEO:
      *out = content::MediaStreamRequestResult::
          MEDIA_DEVICE_TRACK_START_FAILURE_VIDEO;
      return true;
    case content::mojom::MediaStreamRequestResult::NOT_SUPPORTED:
      *out = content::MediaStreamRequestResult::MEDIA_DEVICE_NOT_SUPPORTED;
      return true;
    case content::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN:
      *out = content::MediaStreamRequestResult::
          MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN;
      return true;
    case content::mojom::MediaStreamRequestResult::KILL_SWITCH_ON:
      *out = content::MediaStreamRequestResult::MEDIA_DEVICE_KILL_SWITCH_ON;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<content::mojom::MediaStreamDeviceDataView,
                  content::MediaStreamDevice>::
    Read(content::mojom::MediaStreamDeviceDataView input,
         content::MediaStreamDevice* out) {
  if (!input.ReadType(&out->type))
    return false;
  if (!input.ReadId(&out->id))
    return false;
  if (!input.ReadVideoFacing(&out->video_facing))
    return false;
  if (!input.ReadGroupId(&out->group_id))
    return false;
  if (!input.ReadMatchedOutputDeviceId(&out->matched_output_device_id))
    return false;
  if (!input.ReadName(&out->name))
    return false;
  if (!input.ReadInput(&out->input))
    return false;
  out->session_id = input.session_id();
  if (!input.ReadCameraCalibration(&out->camera_calibration))
    return false;
  if (!input.ReadDisplayMediaInfo(&out->display_media_info))
    return false;
  return true;
}

// static
bool StructTraits<
    content::mojom::TrackControlsDataView,
    content::TrackControls>::Read(content::mojom::TrackControlsDataView input,
                                  content::TrackControls* out) {
  out->requested = input.requested();
  if (!input.ReadStreamType(&out->stream_type))
    return false;
  if (!input.ReadDeviceId(&out->device_id))
    return false;
  return true;
}

// static
bool StructTraits<
    content::mojom::StreamControlsDataView,
    content::StreamControls>::Read(content::mojom::StreamControlsDataView input,
                                   content::StreamControls* out) {
  if (!input.ReadAudio(&out->audio))
    return false;
  if (!input.ReadVideo(&out->video))
    return false;
#if DCHECK_IS_ON()
  if (input.hotword_enabled() || input.disable_local_echo())
    DCHECK(out->audio.requested);
#endif
  out->hotword_enabled = input.hotword_enabled();
  out->disable_local_echo = input.disable_local_echo();
  return true;
}

}  // namespace mojo
