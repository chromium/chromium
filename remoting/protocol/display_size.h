// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_DISPLAY_SIZE_H_
#define REMOTING_PROTOCOL_DISPLAY_SIZE_H_

#include <stddef.h>
#include <ostream>

#include "remoting/base/constants.h"

namespace remoting {

class DisplaySize {
 public:
  constexpr DisplaySize() : width_dips_(0), height_dips_(0), dpi_(0) {}

  constexpr DisplaySize(int32_t width_dips, int32_t height_dips, uint32_t dpi)
      : width_dips_(width_dips), height_dips_(height_dips), dpi_(dpi) {}

  ~DisplaySize() = default;

  // Static method to build a DisplaySize from pixels rather than DIPs.
  static constexpr DisplaySize FromPixels(int32_t width_px,
                                          int32_t height_px,
                                          uint32_t dpi) {
    const int32_t width_dips = width_px * ((float)kDefaultDpi / dpi);
    const int32_t height_dips = height_px * ((float)kDefaultDpi / dpi);
    return DisplaySize(width_dips, height_dips, dpi);
  }

  bool operator==(const DisplaySize& other);
  bool operator!=(const DisplaySize& other);

  bool IsEmpty() const;

  int32_t WidthAsDips() const;
  int32_t HeightAsDips() const;

  int32_t WidthAsPixels() const;
  int32_t HeightAsPixels() const;

  uint32_t GetDpi() const;
  float GetScalingFactor() const;

 private:
  int32_t width_dips_, height_dips_;
  uint32_t dpi_;
};

std::ostream& operator<<(std::ostream& out, const DisplaySize& size);

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_DISPLAY_SIZE_H_
