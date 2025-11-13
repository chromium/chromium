// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_ABR_ALGORITHM_H_
#define MEDIA_FORMATS_HLS_ABR_ALGORITHM_H_

#include <cstdint>

#include "media/base/media_export.h"

namespace media::hls {

// The HLS player uses an ABRAlgorithm to process historical network speeds for
// media content fetching and approximate the expected network speed for future
// requests. The default implementation, EwmaAbrAlgorithm uses two exponentially
// weighted moving averages and always selects the lowest of them. The
// FixedAbrAlgorithm is used for unit testing so that exact ABR measurements can
// be set.
class MEDIA_EXPORT ABRAlgorithm {
 public:
  virtual ~ABRAlgorithm() = default;

  // Ingests a new network speed measurement.
  virtual void UpdateNetworkSpeed(uint64_t network_bps) = 0;

  // Returns the calculated ABR speed in bits per second.
  virtual uint64_t GetABRSpeed() const = 0;
};

// An ABR algorithm that uses an exponentially weighted moving average.
class MEDIA_EXPORT EwmaAbrAlgorithm : public ABRAlgorithm {
 public:
  ~EwmaAbrAlgorithm() override;

  void UpdateNetworkSpeed(uint64_t network_bps) override;
  uint64_t GetABRSpeed() const override;

 private:
  uint64_t weighted_old_ = 0;
  uint64_t weighted_new_ = 0;
};

// A simple ABR algorithm for testing.
class MEDIA_EXPORT FixedAbrAlgorithm : public ABRAlgorithm {
 public:
  explicit FixedAbrAlgorithm(uint64_t network_bps);
  ~FixedAbrAlgorithm() override;

  void UpdateNetworkSpeed(uint64_t network_bps) override;
  uint64_t GetABRSpeed() const override;

 private:
  uint64_t fixed_bps_ = 0;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_ABR_ALGORITHM_H_
