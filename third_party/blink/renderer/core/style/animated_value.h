// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANIMATED_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANIMATED_VALUE_H_

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/style/style_animated_sources.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Combines a value with the animated source and property driving it, so callers
// can track what updates it.
template <typename T>
struct AnimatedValue {
  DISALLOW_NEW();

  constexpr AnimatedValue() = default;
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr AnimatedValue(
      const T& value,
      AnimatedSource source = {},
      AnimatedSourceProperty property = AnimatedSourceProperty::kNumFields)
      : value(value), source(source), property(property) {}

  bool operator==(const AnimatedValue&) const = default;

  T value{};
  AnimatedSource source;
  AnimatedSourceProperty property = AnimatedSourceProperty::kNumFields;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANIMATED_VALUE_H_
