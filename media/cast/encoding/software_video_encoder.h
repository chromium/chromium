// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_ENCODING_SOFTWARE_VIDEO_ENCODER_H_
#define MEDIA_CAST_ENCODING_SOFTWARE_VIDEO_ENCODER_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"

namespace base {
class TimeTicks;
}

namespace media {
class VideoFrame;
}

namespace media {
namespace cast {

struct SenderEncodedFrame;

class SoftwareVideoEncoder {
 public:
  virtual ~SoftwareVideoEncoder() {}

  // Initialize the encoder before Encode() can be called. This method
  // must be called on the thread that Encode() is called.
  virtual void Initialize() = 0;

  // Encode a raw image (as a part of a video stream).
  virtual void Encode(scoped_refptr<media::VideoFrame> video_frame,
                      base::TimeTicks reference_time,
                      SenderEncodedFrame* encoded_frame) = 0;

  // Update the encoder with a new target bit rate.
  virtual void UpdateRates(uint32_t new_bitrate) = 0;

  // Set the next frame to be a key frame.
  virtual void GenerateKeyFrame() = 0;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_ENCODING_SOFTWARE_VIDEO_ENCODER_H_
