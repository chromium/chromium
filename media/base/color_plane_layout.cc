// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/color_plane_layout.h"

namespace media {

ColorPlaneLayout::ColorPlaneLayout() = default;

ColorPlaneLayout::ColorPlaneLayout(int32_t stride, size_t offset, size_t size)
    : stride(stride), offset(offset), size(size) {}

ColorPlaneLayout::~ColorPlaneLayout() = default;

bool ColorPlaneLayout::operator==(const ColorPlaneLayout& rhs) const {
  return stride == rhs.stride && offset == rhs.offset && size == rhs.size;
}

bool ColorPlaneLayout::operator!=(const ColorPlaneLayout& rhs) const {
  return !(*this == rhs);
}

std::ostream& operator<<(std::ostream& ostream, const ColorPlaneLayout& plane) {
  ostream << "(" << plane.stride << ", " << plane.offset << ", " << plane.size
          << ")";
  return ostream;
}

}  // namespace media
