// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BITRATE_H_
#define MEDIA_BASE_BITRATE_H_

#include <stdint.h>
#include <string>

#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT Bitrate {
 public:
  // Indicates whether
  // - constant bitrate (CBR)
  // - variable bitrate (VBR)
  // - or external rate control
  // should be used for encoding.
  enum class Mode { kConstant, kVariable, kExternal };

  // Required by Mojo for serialization and de-serialization. Creates an
  // invalid constant bitrate with |target_| and |peak_| set to 0u. Prefer
  // to use the Bitrate::ConstantBitrate() method.
  constexpr Bitrate() = default;

  constexpr Bitrate(const Bitrate& other) = default;
  constexpr Bitrate& operator=(const Bitrate& other) = default;

  // Do not use int or uint64_t variations of these. If you have a signed
  // or 64-bit value you want to use as input, you must explicitly convert to
  // uint32_t before calling. This is intended to prevent implicit and unsafe
  // type conversion.
  static constexpr Bitrate ConstantBitrate(uint32_t target_bps) {
    return Bitrate(Mode::kConstant, target_bps, 0);
  }
  static Bitrate VariableBitrate(uint32_t target_bps, uint32_t peak_bps);
  static Bitrate ExternalRateControl();

  // Deleted variants: you must SAFELY convert to uint32_t before calling.
  // See base/numerics/safe_conversions.h for functions to safely convert
  // between types.
  static Bitrate ConstantBitrate(int target_bps) = delete;
  static Bitrate VariableBitrate(int target_bps, int peak_bps) = delete;
  static Bitrate VariableBitrate(int target_bps, uint32_t peak_bps) = delete;
  static Bitrate VariableBitrate(uint32_t target_bps, int peak_bps) = delete;
  static Bitrate ConstantBitrate(uint64_t target_bps) = delete;
  static Bitrate VariableBitrate(uint64_t target_bps,
                                 uint64_t peak_bps) = delete;
  static Bitrate VariableBitrate(uint64_t target_bps,
                                 uint32_t peak_bps) = delete;
  static Bitrate VariableBitrate(uint32_t target_bps,
                                 uint64_t peak_bps) = delete;

  bool operator==(const Bitrate& right) const;
  bool operator!=(const Bitrate& right) const;

  // Accessor for |mode_|.
  constexpr Mode mode() const { return mode_; }

  // Accessor for |target_|.
  constexpr uint32_t target_bps() const { return target_bps_; }

  // Accessor for |peak_|. Returns 0 if |mode_| is
  // Mode::kConstantBitrate.
  uint32_t peak_bps() const;

  std::string ToString() const;

 private:
  constexpr Bitrate(Mode mode, uint32_t target_bps, uint32_t peak_bps)
      : mode_(mode), target_bps_(target_bps), peak_bps_(peak_bps) {}

  // These member variables cannot be const (despite the intent that we do not
  // change them after creation) because we must have an assignment operator for
  // Mojo, and const member variables are incompatible with an assignment
  // operator.

  // The bitrate mode.
  Mode mode_ = Mode::kConstant;

  // Target bitrate for the stream in bits per second.
  uint32_t target_bps_ = 0u;

  // For use with Mode::kVariable. Peak bitrate in bits per second.
  uint32_t peak_bps_ = 0u;
};

}  // namespace media

#endif  // MEDIA_BASE_BITRATE_H_
