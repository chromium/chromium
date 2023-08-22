// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_IOS_PIXEL_BUFFER_ROTATOR_H_
#define MEDIA_CAPTURE_VIDEO_IOS_PIXEL_BUFFER_ROTATOR_H_

#import <VideoToolbox/VideoToolbox.h>

#include "base/apple/scoped_cftyperef.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/apple/pixel_buffer_pool.h"

namespace media {

// The PixelBufferRotator rotates a source pixel buffer and writes the output to
// the destination pixel buffer. This class uses VTPixelRotationSessionRef[1]
//
// For 90 and 270 degree rotations, the width and height of destination must be
// the inverse of the source buffer. For 180 degree rotations, the dimensions of
// the source and destination buffers must match
//
// [1]
// https://developer.apple.com/documentation/videotoolbox/vtpixelrotationsessionref?language=objc
class CAPTURE_EXPORT PixelBufferRotator {
 public:
  PixelBufferRotator();
  ~PixelBufferRotator();

  // Rotates a source pixel buffer and writes the output to the destination
  // pixel buffer.
  bool Rotate(CVPixelBufferRef source,
              CVPixelBufferRef destination,
              int rotation);

 private:
  base::apple::ScopedCFTypeRef<VTPixelRotationSessionRef> rotation_session_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_IOS_PIXEL_BUFFER_ROTATOR_H_
