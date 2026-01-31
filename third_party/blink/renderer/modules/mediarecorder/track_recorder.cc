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

  if (EqualIgnoringASCIICase(type, "video/mp4")) {
    return MediaTrackContainerType::kVideoMp4;
  } else if (EqualIgnoringASCIICase(type, "video/webm")) {
    return MediaTrackContainerType::kVideoWebM;
  } else if (EqualIgnoringASCIICase(type, "video/x-matroska") ||
             EqualIgnoringASCIICase(type, "video/matroska")) {
    return MediaTrackContainerType::kVideoMatroska;
  } else if (EqualIgnoringASCIICase(type, "audio/mp4")) {
    return MediaTrackContainerType::kAudioMp4;
  } else if (EqualIgnoringASCIICase(type, "audio/webm")) {
    return MediaTrackContainerType::kAudioWebM;
  } else if (EqualIgnoringASCIICase(type, "audio/x-matroska") ||
             EqualIgnoringASCIICase(type, "audio/matroska")) {
    return MediaTrackContainerType::kAudioMatroska;
  }

  return MediaTrackContainerType::kNone;
}

}  // namespace blink.
