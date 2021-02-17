// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_SIGNAL_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_SIGNAL_OBSERVER_H_

#include "third_party/blink/renderer/platform/heap/impl/garbage_collected.h"

namespace blink {

class MediaStreamVideoTrackSignalObserver : public GarbageCollectedMixin {
 public:
  virtual ~MediaStreamVideoTrackSignalObserver() = default;
  virtual void SetMinimumFrameRate(double) = 0;
  virtual void RequestFrame() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_SIGNAL_OBSERVER_H_
