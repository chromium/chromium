// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_MEDIA_STREAM_REGISTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_MEDIA_STREAM_REGISTRY_H_

#include <string>

#include "base/optional.h"
#include "third_party/blink/public/platform/web_media_stream.h"

namespace blink {

class VideoTrackAdapterSettings;

// This class encapsulates creation of a Blink MediaStream having inside the
// necessary Blink and Chromium, track and source. The Chrome Video source is
// a mock.
class MockMediaStreamRegistry final {
 public:
  MockMediaStreamRegistry();

  void Init();
  void AddVideoTrack(const std::string& track_id,
                     const VideoTrackAdapterSettings& adapter_settings,
                     const base::Optional<bool>& noise_reduction,
                     bool is_screen_cast,
                     double min_frame_rate);
  void AddVideoTrack(const std::string& track_id);
  void AddAudioTrack(const std::string& track_id);

  const WebMediaStream test_stream() const { return test_stream_; }

  void reset() { test_stream_.Reset(); }

 private:
  WebMediaStream test_stream_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_MEDIA_STREAM_REGISTRY_H_
