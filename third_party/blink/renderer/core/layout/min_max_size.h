// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MIN_MAX_SIZE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MIN_MAX_SIZE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

// A struct that holds a pair of two sizes, a "min" size and a "max" size.
// Useful for holding a {min,max}-content size pair or a
// {min,max}-{width,height}.
struct CORE_EXPORT MinMaxSize {
  LayoutUnit min_size;
  LayoutUnit max_size;

  // Make sure that our min/max sizes are at least as large as |other|.
  void Encompass(const MinMaxSize& other) {
    min_size = std::max(min_size, other.min_size);
    max_size = std::max(max_size, other.max_size);
  }

  // Make sure that our min/max sizes are at least as large as |value|.
  void Encompass(LayoutUnit value) {
    min_size = std::max(min_size, value);
    max_size = std::max(max_size, value);
  }

  // Make sure that our min/max sizes aren't larger than |value|.
  void Constrain(LayoutUnit value) {
    min_size = std::min(min_size, value);
    max_size = std::min(max_size, value);
  }

  // Interprets the sizes as a min-content/max-content pair and computes the
  // "shrink-to-fit" size based on them for the given available size.
  LayoutUnit ShrinkToFit(LayoutUnit available_size) const {
    DCHECK_GE(max_size, min_size);
    return std::min(max_size, std::max(min_size, available_size));
  }

  // Interprets the sizes as a {min-max}-size pair and clamps the given input
  // size to that.
  LayoutUnit ClampSizeToMinAndMax(LayoutUnit size) const {
    return std::max(min_size, std::min(size, max_size));
  }

  bool operator==(const MinMaxSize& other) const {
    return min_size == other.min_size && max_size == other.max_size;
  }

  void operator=(LayoutUnit value) { min_size = max_size = value; }
  MinMaxSize& operator+=(MinMaxSize extra) {
    min_size += extra.min_size;
    max_size += extra.max_size;
    return *this;
  }
  MinMaxSize& operator+=(const LayoutUnit length) {
    min_size += length;
    max_size += length;
    return *this;
  }
  MinMaxSize& operator-=(const LayoutUnit length) {
    min_size -= length;
    max_size -= length;
    return *this;
  }
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const MinMaxSize&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MIN_MAX_SIZE_H_
