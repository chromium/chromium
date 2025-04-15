// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/picture_in_picture_events_info_mojom_traits.h"

namespace mojo {

// static
media::mojom::AutoPipReason
EnumTraits<media::mojom::AutoPipReason,
           media::PictureInPictureEventsInfo::AutoPipReason>::
    ToMojom(media::PictureInPictureEventsInfo::AutoPipReason input) {
  switch (input) {
    case ::media::PictureInPictureEventsInfo::AutoPipReason::kUnknown:
      return media::mojom::AutoPipReason::kUnknown;
    case ::media::PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing:
      return media::mojom::AutoPipReason::kVideoConferencing;
    case ::media::PictureInPictureEventsInfo::AutoPipReason::kMediaPlayback:
      return media::mojom::AutoPipReason::kMediaPlayback;
  }
  NOTREACHED();
}

// static
bool EnumTraits<media::mojom::AutoPipReason,
                media::PictureInPictureEventsInfo::AutoPipReason>::
    FromMojom(media::mojom::AutoPipReason input,
              media::PictureInPictureEventsInfo::AutoPipReason* output) {
  switch (input) {
    case media::mojom::AutoPipReason::kUnknown:
      *output = ::media::PictureInPictureEventsInfo::AutoPipReason::kUnknown;
      return true;
    case media::mojom::AutoPipReason::kVideoConferencing:
      *output = ::media::PictureInPictureEventsInfo::AutoPipReason::
          kVideoConferencing;
      return true;
    case media::mojom::AutoPipReason::kMediaPlayback:
      *output =
          ::media::PictureInPictureEventsInfo::AutoPipReason::kMediaPlayback;
      return true;
  }
  NOTREACHED();
}

// static
bool StructTraits<media::mojom::AutoPipInfoDataView,
                  media::PictureInPictureEventsInfo::AutoPipInfo>::
    Read(media::mojom::AutoPipInfoDataView input,
         media::PictureInPictureEventsInfo::AutoPipInfo* output) {
  media::PictureInPictureEventsInfo::AutoPipReason auto_pip_reason;
  if (!input.ReadAutoPipReason(&auto_pip_reason)) {
    return false;
  }

  *output = media::PictureInPictureEventsInfo::AutoPipInfo();
  output->auto_pip_reason = auto_pip_reason;
  output->has_audio_focus = input.has_audio_focus();
  output->is_playing = input.is_playing();
  output->was_recently_audible = input.was_recently_audible();
  output->has_safe_url = input.has_safe_url();
  output->meets_media_engagement_conditions =
      input.meets_media_engagement_conditions();
  output->blocked_due_to_content_setting =
      input.blocked_due_to_content_setting();

  return true;
}

}  // namespace mojo
