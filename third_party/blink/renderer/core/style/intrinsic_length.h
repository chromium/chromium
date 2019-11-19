// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_INTRINSIC_LENGTH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_INTRINSIC_LENGTH_H_

#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class IntrinsicLength {
  DISALLOW_NEW();

  enum LengthType { kAuto, kLegacy, kSpecified };

 public:
  IntrinsicLength() : IntrinsicLength(kLegacy) {}

  static IntrinsicLength MakeAuto() { return IntrinsicLength(kAuto); }
  static IntrinsicLength MakeLegacy() { return IntrinsicLength(kLegacy); }
  static IntrinsicLength Make(const Length& length) {
    return IntrinsicLength(kSpecified, length);
  }

  bool IsAuto() const { return type_ == kAuto; }
  bool IsLegacy() const { return type_ == kLegacy; }
  const Length& GetLength() const {
    // We may still need to get the length for auto, so we only guard against
    // legacy.
    DCHECK(!IsLegacy());
    return length_;
  }

  bool operator==(const IntrinsicLength& other) const {
    return type_ == other.type_ && length_ == other.length_;
  }
  bool operator!=(const IntrinsicLength& other) const {
    return !(*this == other);
  }

 private:
  IntrinsicLength(LengthType type, const Length& length = Length())
      : type_(type), length_(length) {
    // If the type is specified, then length has to be fixed.
    // Otherwise, the length has to be the default value.
    DCHECK(type != kSpecified || length.IsFixed());
    DCHECK(type == kSpecified || length == Length());
  }

  LengthType type_;
  Length length_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_INTRINSIC_LENGTH_H_
