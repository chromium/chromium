// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_track_platform.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"

namespace blink {

// static
MediaStreamTrackPlatform* MediaStreamTrackPlatform::GetTrack(
    const WebMediaStreamTrack& track) {
  if (track.IsNull())
    return nullptr;

  MediaStreamComponent& component = *track;
  return component.GetPlatformTrack();
}

MediaStreamTrackPlatform::MediaStreamTrackPlatform(bool is_local_track)
    : is_local_track_(is_local_track) {}

MediaStreamTrackPlatform::~MediaStreamTrackPlatform() {}

MediaStreamTrackPlatform::CaptureHandle
MediaStreamTrackPlatform::GetCaptureHandle() {
  return MediaStreamTrackPlatform::CaptureHandle();
}

}  // namespace blink
