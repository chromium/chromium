// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_INTRINSIC_LENGTH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_INTRINSIC_LENGTH_H_

#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class StyleIntrinsicLength {
  DISALLOW_NEW();

 public:
  // Style data for contain-intrinsic-size:
  //  none | <length> | auto && <length> | auto && none.
  StyleIntrinsicLength(bool has_auto, const absl::optional<double>& length)
      : has_auto_(has_auto),
        length_(length ? absl::optional<LayoutUnit>(LayoutUnit(*length))
                       : absl::nullopt) {}

  StyleIntrinsicLength() = default;

  // This returns true if the value is "none" without auto. It's not named
  // "IsNone" to avoid confusion with "auto none" grammar.
  bool IsNoOp() const { return !has_auto_ && !length_.has_value(); }

  bool HasAuto() const { return has_auto_; }

  const absl::optional<LayoutUnit>& GetLength() const { return length_; }

  bool operator==(const StyleIntrinsicLength& o) const {
    return has_auto_ == o.has_auto_ && length_ == o.length_;
  }

  bool operator!=(const StyleIntrinsicLength& o) const { return !(*this == o); }

 private:
  bool has_auto_ = false;
  absl::optional<LayoutUnit> length_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_INTRINSIC_LENGTH_H_
