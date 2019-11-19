// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_H_

#include <memory>

#include "media/capture/capture_export.h"
#include "media/capture/video_capture_types.h"

namespace media {

class VideoCaptureBufferTracker;

class CAPTURE_EXPORT VideoCaptureBufferTrackerFactory {
 public:
  virtual ~VideoCaptureBufferTrackerFactory() {}
  virtual std::unique_ptr<VideoCaptureBufferTracker> CreateTracker(
      VideoCaptureBufferType buffer_type) = 0;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_H_
