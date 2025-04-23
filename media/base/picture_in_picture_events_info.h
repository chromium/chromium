// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PICTURE_IN_PICTURE_EVENTS_INFO_H_
#define MEDIA_BASE_PICTURE_IN_PICTURE_EVENTS_INFO_H_

#include <string>

#include "base/functional/bind.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT PictureInPictureEventsInfo {
 public:
  PictureInPictureEventsInfo();
  ~PictureInPictureEventsInfo();

  // These values represent the reason for entering picture in picture
  // automatically and are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  enum class AutoPipReason {
    // The reason for entering auto picture in picture is not known, or auto
    // picture in picture has not been triggered.
    kUnknown = 0,

    // Entered auto picture in picture due to video conferencing (usage of
    // camera or microphone).
    kVideoConferencing = 1,

    // Entered auto picture in picture due to media playback.
    kMediaPlayback = 2,

    kMaxValue = kMediaPlayback,
  };

  struct MEDIA_EXPORT AutoPipInfo {
    AutoPipReason auto_pip_reason = AutoPipReason::kUnknown;
    bool has_audio_focus = false;
    bool is_playing = false;
    bool was_recently_audible = false;
    bool has_safe_url = false;
    bool meets_media_engagement_conditions = false;
    bool blocked_due_to_content_setting = false;
  };

  using AutoPipReasonCallback = base::RepeatingCallback<AutoPipReason(void)>;

  static std::string AutoPipReasonToString(AutoPipReason auto_pip_reason);
  static std::string AutoPipInfoToString(AutoPipInfo auto_pip_info);
};
}  // namespace media

#endif  // MEDIA_BASE_PICTURE_IN_PICTURE_EVENTS_INFO_H_
