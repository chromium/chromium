// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Observer class of Sinkfilter. The implementor of this class receive video
// frames from the SinkFilter DirectShow filter.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_SINK_FILTER_OBSERVER_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_SINK_FILTER_OBSERVER_WIN_H_

#include <stdint.h>
#include "media/capture/video_capture_types.h"

namespace media {

class SinkFilterObserver {
 public:
  // SinkFilter will call this function with all frames delivered to it.
  // |buffer| is only valid during this function call.
  virtual void FrameReceived(const uint8_t* buffer,
                             int length,
                             const VideoCaptureFormat& format,
                             base::TimeDelta timestamp,
                             bool flip_y) = 0;

  virtual void FrameDropped(VideoCaptureFrameDropReason reason) = 0;

 protected:
  virtual ~SinkFilterObserver();
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_SINK_FILTER_OBSERVER_WIN_H_
