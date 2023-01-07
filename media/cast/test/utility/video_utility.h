// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_UTILITY_VIDEO_UTILITY_H_
#define MEDIA_CAST_TEST_UTILITY_VIDEO_UTILITY_H_

// Utility functions for video testing.

#include "media/base/video_frame.h"

namespace media {
namespace cast {

// Compute and return PSNR between two frames.
double I420PSNR(const media::VideoFrame& frame1,
                const media::VideoFrame& frame2);

// Compute and return SSIM between two frames.
double I420SSIM(const media::VideoFrame& frame1,
                const media::VideoFrame& frame2);

// Populate a video |frame| with a plaid pattern, cycling from the given
// |start_value|.
// Width, height and stride should be set in advance.
// Memory is allocated within the function.
void PopulateVideoFrame(VideoFrame* frame, int start_value);

// Populate a video frame with noise.
void PopulateVideoFrameWithNoise(VideoFrame* frame);

// Populate a video frame from a file.
// Returns true if frame was populated, false if not (EOF).
bool PopulateVideoFrameFromFile(VideoFrame* frame, FILE* video_file);

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_UTILITY_VIDEO_UTILITY_H_
