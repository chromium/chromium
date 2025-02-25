// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/picture_in_picture_events_info.h"

#include <string>

#include "base/notreached.h"

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
  }

  NOTREACHED() << "Invalid auto_pip_reason provided: "
               << static_cast<int>(auto_pip_reason);
}

}  // namespace media
