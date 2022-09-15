// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/encoder_bitrate_filter.h"

#include <algorithm>
#include <cstdlib>

namespace remoting {

namespace {

// Only update encoder bitrate when bandwidth changes by more than 33%. This
// value is chosen such that the codec is notified about significant changes in
// bandwidth, while ignoring bandwidth estimate noise. This is necessary because
// the encoder drops quality every time it's being reconfigured. When using VP8
// encoder in realtime mode, the encoded frame size correlates very poorly with
// the target bitrate, so it's not necessary to set target bitrate to match
// bandwidth exactly. Send bitrate is controlled more precisely by adjusting
// time intervals between frames (i.e. FPS).
constexpr int kEncoderBitrateChangePercentage = 33;

// We use a WeightedSamples to analyze the bandwidth to avoid a sharp change to
// significantly impact the image quality. By using the weight factor as 0.95,
// the weight of a bandwidth estimate one second ago (30 frames before) drops to
// ~21.5%.
constexpr double kBandwidthWeightFactor = 0.95;

}  // namespace

EncoderBitrateFilter::EncoderBitrateFilter(
    int minimum_bitrate_kbps_per_megapixel)
    : minimum_bitrate_kbps_per_megapixel_(minimum_bitrate_kbps_per_megapixel),
      bandwidth_kbps_(kBandwidthWeightFactor) {}

EncoderBitrateFilter::~EncoderBitrateFilter() = default;

void EncoderBitrateFilter::SetBandwidthEstimateKbps(int bandwidth_kbps) {
  bandwidth_kbps_.Record(bandwidth_kbps);
  int current_kbps = bandwidth_kbps_.WeightedAverage();
  if (std::abs(current_kbps - bitrate_kbps_) >
      bitrate_kbps_ * kEncoderBitrateChangePercentage / 100) {
    bitrate_kbps_ = current_kbps;
  }
}

void EncoderBitrateFilter::SetFrameSize(int width, int height) {
  minimum_bitrate_kbps_ =
      std::max(static_cast<int64_t>(minimum_bitrate_kbps_per_megapixel_) *
                   width * height / 1000000,
               static_cast<int64_t>(minimum_bitrate_kbps_));
}

int EncoderBitrateFilter::GetTargetBitrateKbps() const {
  return bitrate_kbps_;
}

}  // namespace remoting
