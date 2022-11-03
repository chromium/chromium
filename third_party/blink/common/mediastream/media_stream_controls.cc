// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/mediastream/media_stream_controls.h"

namespace blink {

const char kMediaStreamSourceTab[] = "tab";
const char kMediaStreamSourceScreen[] = "screen";
const char kMediaStreamSourceDesktop[] = "desktop";
const char kMediaStreamSourceSystem[] = "system";

TrackControls::TrackControls() {}

TrackControls::TrackControls(mojom::MediaStreamType type) : stream_type(type) {}

TrackControls::TrackControls(const TrackControls& other) = default;

TrackControls::~TrackControls() {}

StreamControls::StreamControls() {}

StreamControls::StreamControls(bool request_audio, bool request_video)
    : audio(request_audio ? mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE
                          : mojom::MediaStreamType::NO_SERVICE),
      video(request_video ? mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE
                          : mojom::MediaStreamType::NO_SERVICE) {}

StreamControls::~StreamControls() {}

}  // namespace blink
