// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/half_float_maker.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media {

HalfFloatMaker::~HalfFloatMaker() = default;

// By OR-ing with 0x3800, 10-bit numbers become half-floats in the
// range [0.5..1) and 9-bit numbers get the range [0.5..0.75).
//
// Half-floats are evaluated as:
// float value = pow(2.0, exponent - 25) * (0x400 + fraction);
//
// In our case the exponent is 14 (since we or with 0x3800) and
// pow(2.0, 14-25) * 0x400 evaluates to 0.5 (our offset) and
// pow(2.0, 14-25) * fraction is [0..0.49951171875] for 10-bit and
// [0..0.24951171875] for 9-bit.
//
// https://en.wikipedia.org/wiki/Half-precision_floating-point_format
class HalfFloatMaker_xor : public HalfFloatMaker {
 public:
  explicit HalfFloatMaker_xor(int bits_per_channel)
      : bits_per_channel_(bits_per_channel) {}
  float Offset() const override { return 0.5; }
  float Multiplier() const override {
    int max_input_value = (1 << bits_per_channel_) - 1;
    // 2 << 11 = 2048 would be 1.0 with our exponent.
    return 2048.0 / max_input_value;
  }
  void MakeHalfFloats(const uint16_t* src, size_t num, uint16_t* dst) override {
    // Micro-benchmarking indicates that the compiler does
    // a good enough job of optimizing this loop that trying
    // to manually operate on one uint64 at a time is not
    // actually helpful.
    // Note to future optimizers: Benchmark your optimizations!
    for (size_t i = 0; i < num; i++)
      dst[i] = src[i] | 0x3800;
  }

 private:
  int bits_per_channel_;
};

// Convert plane of 16 bit shorts to half floats using libyuv.
class HalfFloatMaker_libyuv : public HalfFloatMaker {
 public:
  explicit HalfFloatMaker_libyuv(int bits_per_channel) {
    int max_value = (1 << bits_per_channel) - 1;
    // For less than 15 bits, we can give libyuv a multiplier of
    // 1.0, which is faster on some platforms. If bits is 16 or larger,
    // a multiplier of 1.0 would cause overflows. However, a multiplier
    // of 1/max_value would cause subnormal floats, which perform
    // very poorly on some platforms.
    if (bits_per_channel <= 15) {
      libyuv_multiplier_ = 1.0f;
    } else {
      // This multiplier makes sure that we avoid subnormal values.
      libyuv_multiplier_ = 1.0f / 4096.0f;
    }
    resource_multiplier_ = 1.0f / libyuv_multiplier_ / max_value;
  }
  float Offset() const override { return 0.0f; }
  float Multiplier() const override { return resource_multiplier_; }
  void MakeHalfFloats(const uint16_t* src, size_t num, uint16_t* dst) override {
    // Source and dest stride can be zero since we're only copying
    // one row at a time.
    int stride = 0;
    int rows = 1;
    libyuv::HalfFloatPlane(src, stride, dst, stride, libyuv_multiplier_, num,
                           rows);
  }

 private:
  float libyuv_multiplier_;
  float resource_multiplier_;
};

std::unique_ptr<HalfFloatMaker> HalfFloatMaker::NewHalfFloatMaker(
    int bits_per_channel) {
  if (bits_per_channel < 11) {
    return std::unique_ptr<HalfFloatMaker>(
        new HalfFloatMaker_xor(bits_per_channel));
  } else {
    return std::unique_ptr<HalfFloatMaker>(
        new HalfFloatMaker_libyuv(bits_per_channel));
  }
}

}  // namespace media
