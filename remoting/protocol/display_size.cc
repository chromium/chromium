// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/display_size.h"

#include "build/build_config.h"
#include "remoting/base/constants.h"

namespace remoting {

bool DisplaySize::operator==(const DisplaySize& other) {
  return other.width_dips_ == width_dips_ &&
         other.height_dips_ == height_dips_ && other.dpi_ == dpi_;
}

bool DisplaySize::operator!=(const DisplaySize& other) {
  return !(*this == other);
}

bool DisplaySize::IsEmpty() const {
  return width_dips_ == 0 || height_dips_ == 0;
}

int32_t DisplaySize::WidthAsPixels() const {
  return width_dips_ * GetScalingFactor();
}

int32_t DisplaySize::WidthAsDips() const {
  return width_dips_;
}

int32_t DisplaySize::HeightAsPixels() const {
  return height_dips_ * GetScalingFactor();
}

int32_t DisplaySize::HeightAsDips() const {
  return height_dips_;
}

uint32_t DisplaySize::GetDpi() const {
  return dpi_;
}

float DisplaySize::GetScalingFactor() const {
  return (float)dpi_ / (float)kDefaultDpi;
}

}  // namespace remoting

std::ostream& operator<<(std::ostream& out, const remoting::DisplaySize& size) {
  out << size.WidthAsDips() << "x" << size.HeightAsDips() << " @"
      << size.GetDpi();
  return out;
}
