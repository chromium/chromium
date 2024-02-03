// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "third_party/blink/public/common/interest_group/ad_display_size.h"

namespace blink {

AdSize::AdSize() = default;

AdSize::AdSize(double width,
               LengthUnit width_units,
               double height,
               LengthUnit height_units)
    : width(width),
      width_units(width_units),
      height(height),
      height_units(height_units) {}

AdSize::AdSize(const AdSize&) = default;

AdSize::AdSize(AdSize&&) = default;

AdSize& AdSize::operator=(const AdSize&) = default;

AdSize& AdSize::operator=(AdSize&&) = default;

bool AdSize::operator==(const AdSize& other) const {
  return std::tie(width, width_units, height, height_units) ==
         std::tie(other.width, other.width_units, other.height,
                  other.height_units);
}

bool AdSize::operator!=(const AdSize& other) const {
  return !(*this == other);
}

bool AdSize::operator<(const AdSize& other) const {
  return std::tie(width, width_units, height, height_units) <
         std::tie(other.width, other.width_units, other.height,
                  other.height_units);
}

AdSize::~AdSize() = default;

AdDescriptor::AdDescriptor() = default;

AdDescriptor::AdDescriptor(GURL url, std::optional<AdSize> size)
    : url(url), size(size) {}

AdDescriptor::AdDescriptor(const AdDescriptor&) = default;

AdDescriptor::AdDescriptor(AdDescriptor&&) = default;

AdDescriptor& AdDescriptor::operator=(const AdDescriptor&) = default;

AdDescriptor& AdDescriptor::operator=(AdDescriptor&&) = default;

bool AdDescriptor::operator==(const AdDescriptor& other) const {
  return std::tie(url, size) == std::tie(other.url, other.size);
}

bool AdDescriptor::operator!=(const AdDescriptor& other) const {
  return !(*this == other);
}

AdDescriptor::~AdDescriptor() = default;

}  // namespace blink
