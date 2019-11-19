// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/modules/mediastream/web_platform_media_stream_track.h"

namespace blink {

// static
WebPlatformMediaStreamTrack* WebPlatformMediaStreamTrack::GetTrack(
    const WebMediaStreamTrack& track) {
  return track.IsNull() ? nullptr : track.GetPlatformTrack();
}

WebPlatformMediaStreamTrack::WebPlatformMediaStreamTrack(bool is_local_track)
    : is_local_track_(is_local_track) {}

WebPlatformMediaStreamTrack::~WebPlatformMediaStreamTrack() {}

}  // namespace blink
