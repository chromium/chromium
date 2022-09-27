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
  // Style data for aspect-ratio: none | <length> | auto && <length>. none is
  // represented as an empty absl::optional.
  StyleIntrinsicLength(bool has_auto, const Length& length)
      : has_auto_(has_auto), length_(length) {}

  StyleIntrinsicLength() = default;

  bool HasAuto() const { return has_auto_; }

  const Length& GetLength() const { return length_; }

  bool operator==(const StyleIntrinsicLength& o) const {
    return has_auto_ == o.has_auto_ && length_ == o.length_;
  }

  bool operator!=(const StyleIntrinsicLength& o) const { return !(*this == o); }

 private:
  bool has_auto_ = false;
  Length length_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_INTRINSIC_LENGTH_H_
