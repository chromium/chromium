// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_PICTURE_IN_PICTURE_EVENTS_INFO_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_PICTURE_IN_PICTURE_EVENTS_INFO_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "media/base/picture_in_picture_events_info.h"
#include "media/mojo/mojom/media_types.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<media::mojom::AutoPipReason,
                  media::PictureInPictureEventsInfo::AutoPipReason> {
  static media::mojom::AutoPipReason ToMojom(
      media::PictureInPictureEventsInfo::AutoPipReason input);

  static bool FromMojom(
      media::mojom::AutoPipReason,
      media::PictureInPictureEventsInfo::AutoPipReason* output);
};

template <>
struct StructTraits<media::mojom::AutoPipInfoDataView,
                    media::PictureInPictureEventsInfo::AutoPipInfo> {
  static media::PictureInPictureEventsInfo::AutoPipReason auto_pip_reason(
      const media::PictureInPictureEventsInfo::AutoPipInfo& input) {
    return input.auto_pip_reason;
  }

  static bool has_audio_focus(
      const media::PictureInPictureEventsInfo::AutoPipInfo& input) {
    return input.has_audio_focus;
  }

  static bool is_playing(
      const media::PictureInPictureEventsInfo::AutoPipInfo& input) {
    return input.is_playing;
  }

  static bool was_recently_audible(
      const media::PictureInPictureEventsInfo::AutoPipInfo& input) {
    return input.was_recently_audible;
  }

  static bool has_safe_url(
      const media::PictureInPictureEventsInfo::AutoPipInfo& input) {
    return input.has_safe_url;
  }

  static bool meets_media_engagement_conditions(
      const media::PictureInPictureEventsInfo::AutoPipInfo& input) {
    return input.meets_media_engagement_conditions;
  }

  static bool blocked_due_to_content_setting(
      const media::PictureInPictureEventsInfo::AutoPipInfo& input) {
    return input.blocked_due_to_content_setting;
  }

  static bool Read(media::mojom::AutoPipInfoDataView input,
                   media::PictureInPictureEventsInfo::AutoPipInfo* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_PICTURE_IN_PICTURE_EVENTS_INFO_MOJOM_TRAITS_H_
