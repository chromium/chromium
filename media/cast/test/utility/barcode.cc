// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Routines for encoding and decoding a small number of bits into an image
// in a way that is decodable even after scaling/encoding/cropping.
//
// The encoding is very simple:
//
//   ####    ####    ########    ####        ####    ####
//   ####    ####    ########    ####        ####    ####
//   ####    ####    ########    ####        ####    ####
//   ####    ####    ########    ####        ####    ####
//   1   2   3   4   5   6   7   8   9   10  11  12  13  14
//   <-----start----><--one-bit-><-zero bit-><----stop---->
//
// We use a basic unit, depicted here as four characters wide.
// We start with 1u black 1u white 1u black 1u white. (1-4 above)
// From there on, a "one" bit is encoded as 2u black and 1u white,
// and a zero bit is encoded as 1u black and 2u white. After
// all the bits we end the pattern with the same pattern as the
// start of the pattern.

#include <algorithm>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/logging.h"
#include "media/base/video_frame.h"
#include "media/cast/test/utility/barcode.h"

namespace media {
namespace cast {
namespace test {

const int kBlackThreshold = 256 * 2 / 3;
const int kWhiteThreshold = 256 / 3;

bool EncodeBarcode(const std::vector<bool>& bits,
                   scoped_refptr<VideoFrame> output_frame) {
  DCHECK(output_frame->format() == PIXEL_FORMAT_YV12 ||
         output_frame->format() == PIXEL_FORMAT_I422 ||
         output_frame->format() == PIXEL_FORMAT_I420);
  int row_bytes = output_frame->row_bytes(VideoFrame::kYPlane);
  std::vector<unsigned char> bytes(row_bytes);
  for (int i = 0; i < row_bytes; i++) {
    bytes[i] = 255;
  }
  size_t units = bits.size() * 3 + 7;  // White or black bar where size matters.
  // We only use 60% of the image to make sure it works even if
  // the image gets cropped.
  size_t unit_size = row_bytes * 6 / 10 / units;
  if (unit_size < 1) return false;
  size_t bytes_required = unit_size * units;
  size_t padding = (row_bytes - bytes_required) / 2;
  unsigned char *pos = &bytes[padding];
  // Two leading black bars.
  memset(pos, 0, unit_size);
  pos += unit_size * 2;
  memset(pos, 0, unit_size);
  pos += unit_size * 2;
  for (size_t bit = 0; bit < bits.size(); bit++) {
    memset(pos, 0, bits[bit] ? unit_size * 2: unit_size);
    pos += unit_size * 3;
  }
  memset(pos, 0, unit_size);
  pos += unit_size * 2;
  memset(pos, 0, unit_size);
  pos += unit_size;
  DCHECK_LE(pos - &bytes.front(), row_bytes);

  // Now replicate this one row into all rows in kYPlane.
  for (int row = 0; row < output_frame->rows(VideoFrame::kYPlane); row++) {
    memcpy(output_frame->data(VideoFrame::kYPlane) +
           output_frame->stride(VideoFrame::kYPlane) * row,
           &bytes.front(),
           row_bytes);
  }
  return true;
}

namespace {
bool DecodeBarCodeRows(const VideoFrame& frame,
                       std::vector<bool>* output,
                       int min_row,
                       int max_row) {
  // Do a basic run-length encoding
  base::circular_deque<int> runs;
  bool is_black = true;
  int length = 0;
  for (int pos = 0; pos < frame.row_bytes(VideoFrame::kYPlane); pos++) {
    float value = 0.0;
    for (int row = min_row; row < max_row; row++) {
      value += frame.data(
          VideoFrame::kYPlane)[frame.stride(VideoFrame::kYPlane) * row + pos];
    }
    value /= max_row - min_row;
    if (is_black ? value > kBlackThreshold : value < kWhiteThreshold) {
      is_black = !is_black;
      runs.push_back(length);
      length = 1;
    } else {
      length++;
    }
  }
  runs.push_back(length);

  // Try decoding starting at each white-black transition.
  while (runs.size() >=  output->size() * 2 + 7) {
    base::circular_deque<int>::const_iterator i = runs.begin();
    double unit_size = (i[1] + i[2] + i[3] + i[4]) / 4;
    bool valid = true;
    if (i[0] > unit_size * 2 || i[0] < unit_size / 2) valid = false;
    if (i[1] > unit_size * 2 || i[1] < unit_size / 2) valid = false;
    if (i[2] > unit_size * 2 || i[2] < unit_size / 2) valid = false;
    if (i[3] > unit_size * 2 || i[3] < unit_size / 2) valid = false;
    i += 4;
    for (size_t bit = 0; valid && bit < output->size(); bit++) {
      if (i[0] > unit_size / 2 && i[0] <= unit_size * 1.5 &&
          i[1] > unit_size * 1.5 && i[1] <= unit_size * 3) {
        (*output)[bit] = false;
      } else if (i[1] > unit_size / 2 && i[1] <= unit_size * 1.5 &&
                 i[0] > unit_size * 1.5 && i[0] <= unit_size * 3) {
        (*output)[bit] = true;
      } else {
        // Not a valid code
        valid = false;
      }
      i += 2;
    }
    if (i[0] > unit_size * 2 || i[0] < unit_size / 2) valid = false;
    if (i[1] > unit_size * 2 || i[1] < unit_size / 2) valid = false;
    if (i[2] > unit_size * 2 || i[2] < unit_size / 2) valid = false;
    i += 3;
    DCHECK(i <= runs.end());
    if (valid) {
      // Decoding successful, return true
     return true;
    }
    runs.pop_front();
    runs.pop_front();
  }
  return false;
}

}  // namespace

// Note that "output" is assumed to be the right size already. This
// could be inferred from the data, but the decoding is more robust
// if we can assume that we know how many bits we want.
bool DecodeBarcode(const VideoFrame& frame, std::vector<bool>* output) {
  DCHECK(frame.format() == PIXEL_FORMAT_YV12 ||
         frame.format() == PIXEL_FORMAT_I422 ||
         frame.format() == PIXEL_FORMAT_I420);
  int rows = frame.rows(VideoFrame::kYPlane);
  // Middle 10 lines
  if (DecodeBarCodeRows(frame, output, std::max(0, rows / 2 - 5),
                        std::min(rows, rows / 2 + 5))) {
    return true;
  }

  // Top 5 lines
  if (DecodeBarCodeRows(frame, output, 0, std::min(5, rows))) {
    return true;
  }

  return false;
}

}  // namespace test
}  // namespace cast
}  // namespace media
