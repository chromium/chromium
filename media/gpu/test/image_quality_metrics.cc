// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <utility>

#include "base/logging.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv/compare.h"
#include "ui/gfx/geometry/point.h"

#define ASSERT_TRUE_OR_RETURN(predicate, return_value) \
  do {                                                 \
    if (!(predicate)) {                                \
      ADD_FAILURE();                                   \
      return (return_value);                           \
    }                                                  \
  } while (0)

namespace media {
namespace test {
namespace {
// The metrics of the similarity of two images.
enum SimilarityMetrics {
  PSNR,  // Peak Signal-to-Noise Ratio. For detail see
         // https://en.wikipedia.org/wiki/Peak_signal-to-noise_ratio
  SSIM,  // Structural Similarity. For detail see
         // https://en.wikipedia.org/wiki/Structural_similarity
};

double ComputeSimilarity(const VideoFrame* frame1,
                         const VideoFrame* frame2,
                         SimilarityMetrics mode) {
  ASSERT_TRUE_OR_RETURN(frame1->IsMappable() && frame2->IsMappable(),
                        std::numeric_limits<std::size_t>::max());
  ASSERT_TRUE_OR_RETURN(
      frame1->visible_rect().size() == frame2->visible_rect().size(),
      std::numeric_limits<std::size_t>::max());
  // These are used, only if frames are converted to I420, for keeping converted
  // frames alive until the end of function.
  scoped_refptr<VideoFrame> converted_frame1;
  scoped_refptr<VideoFrame> converted_frame2;

  if (frame1->format() != PIXEL_FORMAT_I420) {
    converted_frame1 = ConvertVideoFrame(frame1, PIXEL_FORMAT_I420);
    frame1 = converted_frame1.get();
  }
  if (frame2->format() != PIXEL_FORMAT_I420) {
    converted_frame2 = ConvertVideoFrame(frame2, PIXEL_FORMAT_I420);
    frame2 = converted_frame2.get();
  }

  decltype(&libyuv::I420Psnr) metric_func = nullptr;
  switch (mode) {
    case SimilarityMetrics::PSNR:
      metric_func = &libyuv::I420Psnr;
      break;
    case SimilarityMetrics::SSIM:
      metric_func = &libyuv::I420Ssim;
      break;
  }
  ASSERT_TRUE_OR_RETURN(metric_func, std::numeric_limits<double>::max());

  return metric_func(
      frame1->visible_data(0), frame1->stride(0), frame1->visible_data(1),
      frame1->stride(1), frame1->visible_data(2), frame1->stride(2),
      frame2->visible_data(0), frame2->stride(0), frame2->visible_data(1),
      frame2->stride(1), frame2->visible_data(2), frame2->stride(2),
      frame1->visible_rect().width(), frame1->visible_rect().height());
}
}  // namespace

size_t CompareFramesWithErrorDiff(const VideoFrame& frame1,
                                  const VideoFrame& frame2,
                                  uint8_t tolerance) {
  ASSERT_TRUE_OR_RETURN(frame1.IsMappable() && frame2.IsMappable(),
                        std::numeric_limits<std::size_t>::max());
  ASSERT_TRUE_OR_RETURN(frame1.format() == frame2.format(),
                        std::numeric_limits<std::size_t>::max());
  ASSERT_TRUE_OR_RETURN(
      frame1.visible_rect().size() == frame2.visible_rect().size(),
      std::numeric_limits<std::size_t>::max());
  size_t diff_cnt = 0;

  const VideoPixelFormat format = frame1.format();
  const size_t num_planes = VideoFrame::NumPlanes(format);
  const gfx::Size& visible_size = frame1.visible_rect().size();
  for (size_t i = 0; i < num_planes; ++i) {
    const uint8_t* data1 = frame1.visible_data(i);
    const int stride1 = frame1.stride(i);
    const uint8_t* data2 = frame2.visible_data(i);
    const int stride2 = frame2.stride(i);
    const size_t rows = VideoFrame::Rows(i, format, visible_size.height());
    const int row_bytes = VideoFrame::RowBytes(i, format, visible_size.width());
    for (size_t r = 0; r < rows; ++r) {
      for (int c = 0; c < row_bytes; c++) {
        uint8_t b1 = data1[(stride1 * r) + c];
        uint8_t b2 = data2[(stride2 * r) + c];
        uint8_t diff = std::max(b1, b2) - std::min(b1, b2);
        diff_cnt += diff > tolerance;
      }
    }
  }
  return diff_cnt;
}

double ComputePSNR(const VideoFrame& frame1, const VideoFrame& frame2) {
  return ComputeSimilarity(&frame1, &frame2, SimilarityMetrics::PSNR);
}

double ComputeSSIM(const VideoFrame& frame1, const VideoFrame& frame2) {
  return ComputeSimilarity(&frame1, &frame2, SimilarityMetrics::SSIM);
}
}  // namespace test
}  // namespace media
