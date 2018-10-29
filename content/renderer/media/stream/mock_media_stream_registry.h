// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_MOCK_MEDIA_STREAM_REGISTRY_H_
#define CONTENT_RENDERER_MEDIA_STREAM_MOCK_MEDIA_STREAM_REGISTRY_H_

#include <string>

#include "base/optional.h"
#include "content/renderer/media/stream/media_stream_registry_interface.h"

namespace content {

class VideoTrackAdapterSettings;

// This class encapsulates creation of a Blink MediaStream having inside the
// necessary Blink and Chromium, track and source. The Chrome Video source is
// a mock.
class MockMediaStreamRegistry final : public MediaStreamRegistryInterface {
 public:
  MockMediaStreamRegistry();

  void Init(const std::string& stream_url);
  void AddVideoTrack(const std::string& track_id,
                     const VideoTrackAdapterSettings& adapter_settings,
                     const base::Optional<bool>& noise_reduction,
                     bool is_screen_cast,
                     double min_frame_rate);
  void AddVideoTrack(const std::string& track_id);
  void AddAudioTrack(const std::string& track_id);
  blink::WebMediaStream GetMediaStream(const std::string& url) override;

  const blink::WebMediaStream test_stream() const { return test_stream_; }

  void reset() { test_stream_.Reset(); }

 private:
  blink::WebMediaStream test_stream_;
  std::string stream_url_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_STREAM_MOCK_MEDIA_STREAM_REGISTRY_H_
