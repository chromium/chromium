// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_ENCODER_BITRATE_FILTER_H_
#define REMOTING_CODEC_ENCODER_BITRATE_FILTER_H_

#include "remoting/base/weighted_samples.h"

namespace remoting {

// Receives bandwidth estimations, frame size, etc and decide the best bitrate
// for encoder.
class EncoderBitrateFilter final {
 public:
  explicit EncoderBitrateFilter(int minimum_bitrate_kbps_per_megapixel);
  ~EncoderBitrateFilter();

  void SetBandwidthEstimateKbps(int bandwidth_kbps);
  void SetFrameSize(int width, int height);
  int GetTargetBitrateKbps() const;

 private:
  const int minimum_bitrate_kbps_per_megapixel_;
  // This is the minimum number to avoid returning unreasonable value from
  // GetTargetBitrateKbps(). It roughly equals to the minimum bitrate of a 780 x
  // 512 screen for VP8, or 1024 x 558 screen for H264.
  int minimum_bitrate_kbps_ = 1000;
  WeightedSamples bandwidth_kbps_;
  int bitrate_kbps_ = minimum_bitrate_kbps_;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_ENCODER_BITRATE_FILTER_H_
