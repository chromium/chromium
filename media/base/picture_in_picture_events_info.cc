// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/picture_in_picture_events_info.h"

#include <array>
#include <string>
#include <string_view>

#include "base/notreached.h"
#include "base/strings/stringprintf.h"

namespace media {

PictureInPictureEventsInfo::PictureInPictureEventsInfo() = default;

PictureInPictureEventsInfo::~PictureInPictureEventsInfo() = default;

// static
std::string PictureInPictureEventsInfo::AutoPipReasonToString(
    AutoPipReason auto_pip_reason) {
  switch (auto_pip_reason) {
    case AutoPipReason::kUnknown:
      return "Unknown";
    case AutoPipReason::kVideoConferencing:
      return "VideoConferencing";
    case AutoPipReason::kMediaPlayback:
      return "MediaPlayback";
    case AutoPipReason::kBrowserInitiated:
      return "BrowserInitiated";
  }

  NOTREACHED() << "Invalid auto_pip_reason provided: "
               << static_cast<int>(auto_pip_reason);
}

// static
std::string PictureInPictureEventsInfo::AutoPipInfoToString(
    AutoPipInfo auto_pip_info) {
  constexpr std::array<std::string_view, 2> bool_to_string{"false", "true"};
  return base::StringPrintf(
      "{reason: %s, has audio focus: %s, is_playing: %s, was recently audible: "
      "%s, has safe url: %s, meets media engagement conditions: %s, blocked "
      "due to content setting: %s}",
      AutoPipReasonToString(auto_pip_info.auto_pip_reason),
      bool_to_string[auto_pip_info.has_audio_focus],
      bool_to_string[auto_pip_info.is_playing],
      bool_to_string[auto_pip_info.was_recently_audible],
      bool_to_string[auto_pip_info.has_safe_url],
      bool_to_string[auto_pip_info.meets_media_engagement_conditions],
      bool_to_string[auto_pip_info.blocked_due_to_content_setting]);
}

}  // namespace media
