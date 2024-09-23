// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/app_window/size_constraints.h"

#include <algorithm>

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {

SizeConstraints::SizeConstraints()
    : maximum_size_(kUnboundedSize, kUnboundedSize) {}

SizeConstraints::SizeConstraints(const gfx::Size& min_size,
                                 const gfx::Size& max_size)
    : minimum_size_(min_size), maximum_size_(max_size) {}

SizeConstraints::~SizeConstraints() = default;

// static
gfx::Size SizeConstraints::GetMinimumSizeSupportingRoundedCorners(
    const gfx::RoundedCornersF& radii) {
  return gfx::Size(std::max(radii.upper_left() + radii.upper_right(),
                            radii.lower_left() + radii.lower_right()),
                   std::max(radii.upper_left() + radii.lower_left(),
                            radii.upper_right() + radii.lower_right()));
}

// static
gfx::Size SizeConstraints::AddWindowToConstraints(
    const gfx::Size& size_constraints,
    const gfx::Insets& frame_insets,
    const gfx::RoundedCornersF& window_radii) {
  const gfx::Size minimum_size =
      GetMinimumSizeSupportingRoundedCorners(window_radii);
  return gfx::Size(
      size_constraints.width() == kUnboundedSize
          ? kUnboundedSize
          : std::max(minimum_size.width(),
                     size_constraints.width() + frame_insets.width()),
      size_constraints.height() == kUnboundedSize
          ? kUnboundedSize
          : std::max(minimum_size.height(),
                     size_constraints.height() + frame_insets.height()));
}

gfx::Size SizeConstraints::ClampSize(gfx::Size size) const {
  const gfx::Size max_size = GetMaximumSize();
  if (max_size.width() != kUnboundedSize) {
    size.set_width(std::min(size.width(), max_size.width()));
  }
  if (max_size.height() != kUnboundedSize) {
    size.set_height(std::min(size.height(), max_size.height()));
  }
  size.SetToMax(GetMinimumSize());
  return size;
}

bool SizeConstraints::HasMinimumSize() const {
  const gfx::Size min_size = GetMinimumSize();
  return min_size.width() != kUnboundedSize ||
         min_size.height() != kUnboundedSize;
}

bool SizeConstraints::HasMaximumSize() const {
  const gfx::Size max_size = GetMaximumSize();
  return max_size.width() != kUnboundedSize ||
         max_size.height() != kUnboundedSize;
}

bool SizeConstraints::HasFixedSize() const {
  return !GetMinimumSize().IsEmpty() && GetMinimumSize() == GetMaximumSize();
}

gfx::Size SizeConstraints::GetMinimumSize() const {
  return minimum_size_;
}

gfx::Size SizeConstraints::GetMaximumSize() const {
  return gfx::Size(
      maximum_size_.width() == kUnboundedSize
          ? kUnboundedSize
          : std::max(maximum_size_.width(), minimum_size_.width()),
      maximum_size_.height() == kUnboundedSize
          ? kUnboundedSize
          : std::max(maximum_size_.height(), minimum_size_.height()));
}

void SizeConstraints::set_minimum_size(const gfx::Size& min_size) {
  minimum_size_ = min_size;
}

void SizeConstraints::set_maximum_size(const gfx::Size& max_size) {
  maximum_size_ = max_size;
}

}  // namespace extensions
