// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/mediarecorder/track_recorder.h"

#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// static.
MediaTrackContainerType GetMediaContainerTypeFromString(const String& type) {
  if (type.empty()) {
    return MediaTrackContainerType::kNone;
  }

  if (EqualIgnoringAsciiCase(type, "video/mp4")) {
    return MediaTrackContainerType::kVideoMp4;
  } else if (EqualIgnoringAsciiCase(type, "video/webm")) {
    return MediaTrackContainerType::kVideoWebM;
  } else if (EqualIgnoringAsciiCase(type, "video/x-matroska") ||
             EqualIgnoringAsciiCase(type, "video/matroska")) {
    return MediaTrackContainerType::kVideoMatroska;
  } else if (EqualIgnoringAsciiCase(type, "audio/mp4")) {
    return MediaTrackContainerType::kAudioMp4;
  } else if (EqualIgnoringAsciiCase(type, "audio/webm")) {
    return MediaTrackContainerType::kAudioWebM;
  } else if (EqualIgnoringAsciiCase(type, "audio/x-matroska") ||
             EqualIgnoringAsciiCase(type, "audio/matroska")) {
    return MediaTrackContainerType::kAudioMatroska;
  }

  return MediaTrackContainerType::kNone;
}

}  // namespace blink.
