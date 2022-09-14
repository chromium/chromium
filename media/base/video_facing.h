// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FACING_H_
#define MEDIA_BASE_VIDEO_FACING_H_

namespace media {

// Facing mode for video capture.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
enum VideoFacingMode {
  MEDIA_VIDEO_FACING_NONE = 0,
  MEDIA_VIDEO_FACING_USER,
  MEDIA_VIDEO_FACING_ENVIRONMENT,

  NUM_MEDIA_VIDEO_FACING_MODES
};

// Clients interested in video capture events can implement this interface
// and register the observers to MediaStreamManager or VideoCaptureManager.
class VideoCaptureObserver {
 public:
  virtual ~VideoCaptureObserver() {}
  virtual void OnVideoCaptureStarted(VideoFacingMode facing) = 0;
  virtual void OnVideoCaptureStopped(VideoFacingMode facing) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FACING_H_
