// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_UTILITY_BARCODE_H_
#define MEDIA_CAST_TEST_UTILITY_BARCODE_H_

#include <stddef.h>

#include <vector>

#include "base/memory/ref_counted.h"

namespace media {
class VideoFrame;

namespace cast {
namespace test {
// Encode a resilient barcode into |frame| containing all the bits
// from |bits|.
bool EncodeBarcode(const std::vector<bool>& bits,
                   scoped_refptr<media::VideoFrame> output_frame);
// Decode a barcode (encoded by EncodeBarCode) into |output|.
// |output| should already be sized to contain the right number
// of bits.
bool DecodeBarcode(const media::VideoFrame& frame, std::vector<bool>* output);

// Convenience templates that allows you to encode/decode numeric
// types directly.
template<class T>
bool EncodeBarcode(T data, scoped_refptr<media::VideoFrame> output_frame) {
  std::vector<bool> bits(sizeof(T) * 8);
  for (size_t i = 0; i < bits.size(); i++) {
    bits[i] = ((data >> i) & 1) == 1;
  }
  return EncodeBarcode(bits, output_frame);
}

template <class T>
bool DecodeBarcode(const media::VideoFrame& output_frame, T* data) {
  std::vector<bool> bits(sizeof(T) * 8);
  bool ret = DecodeBarcode(output_frame, &bits);
  if (!ret) return false;
  *data = 0;
  for (size_t i = 0; i < bits.size(); i++) {
    if (bits[i]) {
      *data |= 1UL << i;
    }
  }
  return true;
}

}  // namespace test
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_UTILITY_BARCODE_H_
