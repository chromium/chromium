// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_HALF_FLOAT_MAKER_H_
#define MEDIA_VIDEO_HALF_FLOAT_MAKER_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT HalfFloatMaker {
 public:
  virtual ~HalfFloatMaker();

  // Convert an array of short integers into an array of half-floats.
  // |src| is an array of integers in range 0 .. 2^{bits_per_channel} - 1
  // |num| is number of entries in input and output array.
  // The numbers stored in |dst| will be half floats in range 0.0..1.0
  virtual void MakeHalfFloats(const uint16_t* src,
                              size_t num,
                              uint16_t* dst) = 0;
  // The half-floats made needs by this class will be in the range
  // [Offset() .. Offset() + 1.0/Multiplier]. So if you want results
  // in the 0-1 range, you need to do:
  //   (half_float - Offset()) * Multiplier()
  // to each returned value.
  virtual float Offset() const = 0;
  virtual float Multiplier() const = 0;
  // Below is a factory method which can be used to create halffloatmaker.
  static std::unique_ptr<HalfFloatMaker> NewHalfFloatMaker(
      int bits_per_channel);
};

}  // namespace media
#endif  // MEDIA_VIDEO_HALF_FLOAT_MAKER_H_
