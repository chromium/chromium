// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_ENCODING_FRAME_COMPLEXITY_ESTIMATOR_H_
#define MEDIA_CAST_ENCODING_FRAME_COMPLEXITY_ESTIMATOR_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "media/base/video_frame.h"
#include "ui/gfx/geometry/size.h"

namespace media::cast {

// Estimates the visual complexity of a VideoFrame based on Shannon Entropy of
// pixel differences. This provides a codec-agnostic metric in the range
// [0.0, 1.0], where 0.0 represents a uniform frame (e.g., solid color) and 1.0
// represents maximum complexity (e.g., random noise). This metric is used to
// estimate the "lossiness" of the encoded frame when the actual encoder does
// not expose its internal quantizer choice.
//
// Supported frame types:
// - Format: Planar YUV formats (e.g., I420, NV12, I422, I444) where the Y
//   (luma) plane has 8 bits per pixel. Complexity is estimated solely based
//   on the Y plane.
// - Storage: Must be CPU-accessible (in system memory) to allow direct pixel
//   access.
class FrameComplexityEstimator {
 public:
  FrameComplexityEstimator();

  FrameComplexityEstimator(const FrameComplexityEstimator&) = delete;
  FrameComplexityEstimator& operator=(const FrameComplexityEstimator&) = delete;

  ~FrameComplexityEstimator();

  // Discard any state related to the processing of prior frames.
  void Reset();

  // Estimates complexity for a key frame (spatial complexity).
  std::optional<double> EstimateForKeyFrame(const VideoFrame& frame);

  // Estimates complexity for a delta frame (temporal/spatial complexity).
  std::optional<double> EstimateForDeltaFrame(const VideoFrame& frame);

 private:
  // Returns true if the frame is in planar YUV format.
  static bool CanExamineFrame(const VideoFrame& frame);

  // Returns a value in the range [0,log2(num_buckets)], the Shannon Entropy
  // based on the probabilities of values falling within each of the buckets of
  // the given |histogram|.
  static double ComputeEntropyFromHistogram(base::span<const int> histogram,
                                            int num_samples);

  // Maps the calculated Shannon entropy to a normalized [0.0, 1.0] complexity
  // ratio.
  static double ToComplexityRatio(double shannon_entropy);

  // A cache of a subset of rows of pixels from the last frame examined.  This
  // is used to compute the entropy of the difference between frames, which in
  // turn is used to compute the entropy and complexity.
  //
  // Memory overhead: This buffer stores a 10% subsample of the rows of the
  // Y (luma) plane of the previous frame. For a YUV 4:2:0 frame, this
  // represents approximately 6.7% of the size of the source frame (10% of the
  // luma plane).
  std::vector<uint8_t> last_frame_pixel_buffer_;
  gfx::Size last_frame_size_;

  // Prepares the `last_frame_pixel_buffer_` for the given frame size and
  // returns the number of rows that should be sampled.
  int GetSampleCountAndSetupBuffer(const gfx::Size& size);

  // Computes the pixel difference histogram for a key frame, which compares
  // neighboring pixels within the same frame.
  void PopulateHistogramForKeyFrame(const VideoFrame& frame,
                                    int rows_in_subset,
                                    base::span<int> histogram);

  // Computes the pixel difference histogram for a delta frame, which compares
  // the pixels of this frame with the corresponding pixels of the last frame.
  void PopulateHistogramForDeltaFrame(const VideoFrame& frame,
                                      int rows_in_subset,
                                      base::span<int> histogram);
};

}  // namespace media::cast

#endif  // MEDIA_CAST_ENCODING_FRAME_COMPLEXITY_ESTIMATOR_H_
