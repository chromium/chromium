// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_IMAGE_QUALITY_METRICS_H_
#define MEDIA_GPU_TEST_IMAGE_QUALITY_METRICS_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"

namespace media {

class VideoFrame;

namespace test {

// Compare each byte of two VideoFrames, |frame1| and |frame2|, allowing an
// error up to |tolerance|. Return the number of bytes of which the difference
// is more than |tolerance|.
size_t CompareFramesWithErrorDiff(const VideoFrame& frame1,
                                  const VideoFrame& frame2,
                                  uint8_t tolerance);

// Compute PSNR (Peak Signal-to-Noise Ratio) and SSIM (Structural SIMilarity)
// from |frame1| and |frame2|. These metrics give an estimate of the similarity
// of two images, and can be used as an indication of image quality when
// compared to a baseline.
// The VideoFrames must be memory-based. Note: since these functions leverage
// libyuv::I420(Ssim|Psnr), I420 VideoFrames are created from |frame1| and
// |frame2| if they are not I420.
double ComputePSNR(const VideoFrame& frame1, const VideoFrame& frame2);
double ComputeSSIM(const VideoFrame& frame1, const VideoFrame& frame2);

// Compute the log likelihood ratio between a golden frame and a test frame.
// This metric performs a statistical analysis on the distribution of colors in
// each frame, and looks for anomalies consistent with encoding or decoding
// bugs. More details on this algorithm can be found here:
// go/log-likelihood-artifact-detection
double ComputeLogLikelihoodRatio(scoped_refptr<const VideoFrame> golden_frame,
                                 scoped_refptr<const VideoFrame> test_frame);

double ComputeAR30PSNR(const uint32_t* frame1_data,
                       size_t frame1_stride,
                       const uint32_t* frame2_data,
                       size_t frame2_stride,
                       size_t width,
                       size_t height);

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_IMAGE_QUALITY_METRICS_H_
