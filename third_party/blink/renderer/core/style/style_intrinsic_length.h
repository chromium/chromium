// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_INTRINSIC_LENGTH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_INTRINSIC_LENGTH_H_

#include "base/types/strong_alias.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Style data for `contain-intrinsic-size`:
//   `[ auto | from-element ]? [ none | <length [0,∞]> ]`.
// https://drafts.csswg.org/css-sizing-4/#intrinsic-size-override
class StyleIntrinsicLength {
  DISALLOW_NEW();

 public:
  struct Options {
    bool has_auto = false;
  };

  // Create data for `auto? [ none | <length [0,∞]> ]`.
  explicit StyleIntrinsicLength(const std::optional<Length>& length,
                                Options options = {.has_auto = false})
      : StyleIntrinsicLength(options.has_auto, false, length) {}

  // Create data for `from-element [ none | <length [0,∞]> ]`.
  static StyleIntrinsicLength CreateFromElement(
      const std::optional<Length>& length) {
    return {false, true, length};
  }

  StyleIntrinsicLength() = default;

  // This returns true if the value is "none" without auto. It's not named
  // "IsNone" to avoid confusion with "auto none" grammar.
  bool IsNoOp() const {
    return !has_auto_ && !is_from_element_ && !length_.has_value();
  }

  bool HasAuto() const { return has_auto_; }
  bool IsFromElement() const { return is_from_element_; }

  void SetHasAuto() { has_auto_ = true; }

  const std::optional<Length>& GetLength() const { return length_; }

  bool operator==(const StyleIntrinsicLength& o) const {
    return has_auto_ == o.has_auto_ && is_from_element_ == o.is_from_element_ &&
           length_ == o.length_;
  }

 private:
  StyleIntrinsicLength(bool has_auto,
                       bool is_from_element,
                       const std::optional<Length>& length)
      : has_auto_(has_auto),
        is_from_element_(is_from_element),
        length_(length) {}

  bool has_auto_ = false;
  bool is_from_element_ = false;
  std::optional<Length> length_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_INTRINSIC_LENGTH_H_
