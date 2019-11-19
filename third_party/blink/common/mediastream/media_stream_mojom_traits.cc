// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/mediastream/media_stream_mojom_traits.h"

#include "base/logging.h"
#include "media/base/ipc/media_param_traits.h"
#include "media/capture/mojom/video_capture_types_mojom_traits.h"
#include "media/mojo/mojom/display_media_information.mojom.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::MediaStreamDeviceDataView,
                  blink::MediaStreamDevice>::
    Read(blink::mojom::MediaStreamDeviceDataView input,
         blink::MediaStreamDevice* out) {
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
  base::Optional<base::UnguessableToken> session_id;
  if (input.ReadSessionId(&session_id)) {
    out->set_session_id(session_id ? *session_id : base::UnguessableToken());
  } else {
    return false;
  }
  if (!input.ReadDisplayMediaInfo(&out->display_media_info))
    return false;
  return true;
}

// static
bool StructTraits<blink::mojom::TrackControlsDataView, blink::TrackControls>::
    Read(blink::mojom::TrackControlsDataView input, blink::TrackControls* out) {
  out->requested = input.requested();
  if (!input.ReadStreamType(&out->stream_type))
    return false;
  if (!input.ReadDeviceId(&out->device_id))
    return false;
  return true;
}

// static
bool StructTraits<blink::mojom::StreamControlsDataView, blink::StreamControls>::
    Read(blink::mojom::StreamControlsDataView input,
         blink::StreamControls* out) {
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
