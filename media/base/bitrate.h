// Copyright 2021 The Chromium Authors. All rights reserved.
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
  // Indicates whether constant bitrate (CBR) or variable bitrate (VBR) should
  // be used for encoding.
  enum class Mode { kConstant, kVariable };

  // Required by Mojo for serialization and de-serialization. Creates an
  // invalid constant bitrate with |target_| and |peak_| set to 0u. Prefer
  // to use the Bitrate::ConstantBitrate() method.
  constexpr Bitrate() = default;

  constexpr Bitrate(const Bitrate& other) = default;
  constexpr Bitrate& operator=(const Bitrate& other) = default;

  static constexpr Bitrate ConstantBitrate(uint32_t target_bitrate) {
    return Bitrate(Mode::kConstant, target_bitrate, 0u);
  }
  static constexpr Bitrate VariableBitrate(uint32_t target_bitrate,
                                           uint32_t peak_bitrate) {
    return Bitrate(Mode::kVariable, target_bitrate, peak_bitrate);
  }
  bool operator==(const Bitrate& right) const;
  bool operator!=(const Bitrate& right) const;

  // Accessor for |mode_|.
  constexpr Mode mode() const { return mode_; }

  // Accessor for |target_|.
  constexpr uint32_t target() const { return target_; }

  // Accessor for |peak_|. Returns 0 if |mode_| is
  // Mode::kConstantBitrate.
  uint32_t peak() const;

  std::string ToString() const;

 private:
  constexpr Bitrate(Mode mode, uint32_t target_bitrate, uint32_t peak_bitrate)
      : mode_(mode), target_(target_bitrate), peak_(peak_bitrate) {}

  // These member variables cannot be const (despite the intent that we do not
  // change them after creation) because we must have an assignment operator for
  // Mojo, and const member variables are incompatible with an assignment
  // operator.

  // The bitrate mode.
  Mode mode_ = Mode::kConstant;

  // Target bitrate for the stream in bits per second.
  uint32_t target_ = 0u;

  // For use with Mode::kVariable. Peak bitrate in bits per second.
  uint32_t peak_ = 0u;
};

}  // namespace media

#endif  // MEDIA_BASE_BITRATE_H_
