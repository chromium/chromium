// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_VIDEO_STATS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_VIDEO_STATS_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_track_platform.h"

namespace blink {

class MODULES_EXPORT MediaStreamTrackVideoStats : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  MediaStreamTrackVideoStats();

  void setStats(MediaStreamTrackPlatform::VideoFrameStats);

  uint64_t deliveredFrames() const;
  uint64_t discardedFrames() const;
  uint64_t totalFrames() const;

 private:
  MediaStreamTrackPlatform::VideoFrameStats stats_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_VIDEO_STATS_H_
