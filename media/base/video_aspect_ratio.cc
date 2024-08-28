// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_aspect_ratio.h"

#include <cmath>
#include <limits>

namespace media {

VideoAspectRatio::VideoAspectRatio(Type type, int width, int height) {
  type_ = type;
  aspect_ratio_ = height ? static_cast<double>(width) / height : 0.0;
}

// static
VideoAspectRatio VideoAspectRatio::PAR(int width, int height) {
  return VideoAspectRatio(Type::kPixel, width, height);
}

// static
VideoAspectRatio VideoAspectRatio::DAR(int width, int height) {
  return VideoAspectRatio(Type::kDisplay, width, height);
}

VideoAspectRatio::VideoAspectRatio(const gfx::Rect& visible_rect,
                                   const gfx::Size& natural_size) {
  // The size of a pixel is:
  //   (natural_width / visible_width) by (natural_height / visible_height).
  // Both are multiplied by (visible_width * visible_height) to avoid division.
  // WARNING: Cast before multiply is necessary to prevent overflow.
  double w = static_cast<double>(visible_rect.height()) * natural_size.width();
  double h = static_cast<double>(visible_rect.width()) * natural_size.height();

  type_ = Type::kPixel;
  aspect_ratio_ = h != 0.0 ? w / h : 0.0;
}

bool VideoAspectRatio::operator==(const VideoAspectRatio& other) const {
  if (!IsValid() || !other.IsValid()) {
    return IsValid() == other.IsValid();
  }
  return type_ == other.type_ && aspect_ratio_ == other.aspect_ratio_;
}

bool VideoAspectRatio::IsValid() const {
  return std::isfinite(aspect_ratio_) && aspect_ratio_ > 0.0;
}

gfx::Size VideoAspectRatio::GetNaturalSize(
    const gfx::Rect& visible_rect) const {
  if (!IsValid() || visible_rect.IsEmpty())
    return visible_rect.size();

  // Cast up front to simplify expressions.
  // WARNING: Some aspect ratios can result in sizes that exceed INT_MAX.
  double w = visible_rect.width();
  double h = visible_rect.height();

  switch (type_) {
    case Type::kDisplay:
      if (aspect_ratio_ >= w / h) {
        // Display aspect is wider, grow width.
        w = h * aspect_ratio_;
      } else {
        // Display aspect is narrower, grow height.
        h = w / aspect_ratio_;
      }
      break;

    case Type::kPixel:
      if (aspect_ratio_ >= 1.0) {
        // Wide pixels, grow width.
        w = w * aspect_ratio_;
      } else {
        // Narrow pixels, grow height.
        h = h / aspect_ratio_;
      }
      break;
  }

  // A valid natural size is positive (at least 1 after truncation) and fits in
  // an int. Underflow should only be possible if the input sizes were already
  // invalid, because the mutations above only increase the size.
  w = std::round(w);
  h = std::round(h);
  if (w < 1.0 || w > std::numeric_limits<int>::max() || h < 1.0 ||
      h > std::numeric_limits<int>::max()) {
    return visible_rect.size();
  }
  return gfx::Size(static_cast<int>(w), static_cast<int>(h));
}

}  // namespace media
