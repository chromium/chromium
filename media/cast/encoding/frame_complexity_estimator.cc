// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/frame_complexity_estimator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "base/check_op.h"
#include "base/compiler_specific.h"

namespace media::cast {

namespace {

// The percentage of each frame to sample.  This value is based on an
// analysis that showed sampling 10% of the rows of a frame generated
// reasonably accurate results.
constexpr int kFrameSamplingPercentage = 10;

// The number of histogram buckets for quantization estimation. These
// histograms must encompass the range [-255, 255] (inclusive).
constexpr int kQuantizationHistogramSize = 511;

}  // namespace

FrameComplexityEstimator::FrameComplexityEstimator() = default;

FrameComplexityEstimator::~FrameComplexityEstimator() = default;

void FrameComplexityEstimator::Reset() {
  last_frame_pixel_buffer_.clear();
}

int FrameComplexityEstimator::GetSampleCountAndSetupBuffer(
    const gfx::Size& size) {
  const int rows_in_subset =
      std::max(1, size.height() * kFrameSamplingPercentage / 100);
  if (last_frame_size_ != size || last_frame_pixel_buffer_.empty()) {
    last_frame_pixel_buffer_.resize(size.width() * rows_in_subset);
    last_frame_size_ = size;
  }
  return rows_in_subset;
}

void FrameComplexityEstimator::PopulateHistogramForKeyFrame(
    const VideoFrame& frame,
    int rows_in_subset,
    base::span<int> histogram) {
  const gfx::Size size = frame.visible_rect().size();
  const int row_skip = size.height() / rows_in_subset;

  base::span<const uint8_t> y_plane =
      frame.GetVisiblePlaneData(VideoFrame::Plane::kY);
  const int stride = frame.stride(VideoFrame::Plane::kY);

  int y = 0;
  for (int i = 0; i < rows_in_subset; ++i, y += row_skip) {
    base::span<const uint8_t> row = y_plane.subspan(
        static_cast<size_t>(y * stride), static_cast<size_t>(size.width()));

    int left_hand_pixel_value = static_cast<int>(row[0]);
    for (size_t x = 1; x < row.size(); ++x) {
      const int right_hand_pixel_value = static_cast<int>(row[x]);
      const int difference = right_hand_pixel_value - left_hand_pixel_value;
      const int histogram_index = difference + 255;
      ++histogram[histogram_index];
      left_hand_pixel_value = right_hand_pixel_value;  // For next iteration.
    }

    // Copy the row of pixels into the buffer.  This will be used when
    // generating histograms for future delta frames.
    base::span<uint8_t> last_frame_row =
        base::span(last_frame_pixel_buffer_)
            .subspan(static_cast<size_t>(i * size.width()),
                     static_cast<size_t>(size.width()));
    last_frame_row.copy_from(row);
  }
}

void FrameComplexityEstimator::PopulateHistogramForDeltaFrame(
    const VideoFrame& frame,
    int rows_in_subset,
    base::span<int> histogram) {
  const gfx::Size size = frame.visible_rect().size();
  const int row_skip = size.height() / rows_in_subset;

  base::span<const uint8_t> y_plane =
      frame.GetVisiblePlaneData(VideoFrame::Plane::kY);
  const int stride = frame.stride(VideoFrame::Plane::kY);

  int y = 0;
  for (int i = 0; i < rows_in_subset; ++i, y += row_skip) {
    base::span<const uint8_t> row = y_plane.subspan(
        static_cast<size_t>(y * stride), static_cast<size_t>(size.width()));
    base::span<uint8_t> last_frame_row =
        base::span(last_frame_pixel_buffer_)
            .subspan(static_cast<size_t>(i * size.width()),
                     static_cast<size_t>(size.width()));

    for (size_t x = 0; x < row.size(); ++x) {
      const int difference =
          static_cast<int>(row[x]) - static_cast<int>(last_frame_row[x]);
      const int histogram_index = difference + 255;
      ++histogram[histogram_index];
    }

    // Copy the row of pixels into the buffer.  This will be used when
    // generating histograms for future delta frames.
    last_frame_row.copy_from(row);
  }
}

std::optional<double> FrameComplexityEstimator::EstimateForKeyFrame(
    const VideoFrame& frame) {
  if (!CanExamineFrame(frame)) {
    return std::nullopt;
  }

  // If the size of the frame is different from the last frame, allocate a new
  // buffer.  The buffer only needs to be a fraction of the size of the entire
  // frame, since the entropy analysis only examines a subset of each frame.
  const gfx::Size size = frame.visible_rect().size();
  const int rows_in_subset = GetSampleCountAndSetupBuffer(size);

  // Compute a histogram where each bucket represents the number of times two
  // neighboring pixels were different by a specific amount.
  std::array<int, kQuantizationHistogramSize> histogram{};
  PopulateHistogramForKeyFrame(frame, rows_in_subset, histogram);

  // Estimate a complexity value depending on the difference data in the
  // histogram and return it.
  const int num_samples = (size.width() - 1) * rows_in_subset;
  return ToComplexityRatio(ComputeEntropyFromHistogram(histogram, num_samples));
}

std::optional<double> FrameComplexityEstimator::EstimateForDeltaFrame(
    const VideoFrame& frame) {
  if (!CanExamineFrame(frame)) {
    return std::nullopt;
  }

  // If the size of the |frame| has changed, no difference can be examined.
  // In this case, process this frame as if it were a key frame.
  const gfx::Size& size = frame.visible_rect().size();
  if (last_frame_size_ != size || last_frame_pixel_buffer_.empty()) {
    return EstimateForKeyFrame(frame);
  }
  const int rows_in_subset = GetSampleCountAndSetupBuffer(size);

  // Compute a histogram where each bucket represents the number of times the
  // same pixel in this frame versus the last frame was different by a specific
  // amount.
  std::array<int, kQuantizationHistogramSize> histogram{};
  PopulateHistogramForDeltaFrame(frame, rows_in_subset, histogram);

  // Estimate a complexity value depending on the difference data in the
  // histogram and return it.
  const int num_samples = size.width() * rows_in_subset;
  return ToComplexityRatio(ComputeEntropyFromHistogram(histogram, num_samples));
}

// static
bool FrameComplexityEstimator::CanExamineFrame(const VideoFrame& frame) {
  DCHECK_EQ(8, VideoFrame::PlaneHorizontalBitsPerPixel(frame.format(),
                                                       VideoFrame::Plane::kY));
  return media::IsYuvPlanar(frame.format()) && !frame.visible_rect().IsEmpty();
}

// static
double FrameComplexityEstimator::ComputeEntropyFromHistogram(
    base::span<const int> histogram,
    int num_samples) {
  DCHECK_LT(0, num_samples);
  double entropy = 0.0;
  for (int value : histogram) {
    const double probability = static_cast<double>(value) / num_samples;
    if (probability > 0.0) {
      entropy = entropy - probability * std::log2(probability);
    }
  }
  return entropy;
}

// static
double FrameComplexityEstimator::ToComplexityRatio(double shannon_entropy) {
  DCHECK_GE(shannon_entropy, 0.0);

  // This math is based on an analysis of data produced by running a wide range
  // of mirroring content in a Cast streaming session on a Chromebook Pixel
  // (2013 edition).  The output from the Pixel's built-in hardware encoder was
  // compared to an identically-configured software implementation (libvpx)
  // running alongside.  Based on an analysis of the data, the following linear
  // mapping seems to produce reasonable estimates.
  // Entropy values typically peak around 7.5.
  constexpr double kEntropyAtMaxComplexity = 7.5;
  const double complexity = shannon_entropy / kEntropyAtMaxComplexity;
  return std::min<double>(1.0, complexity);
}

}  // namespace media::cast
