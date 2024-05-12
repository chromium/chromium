// Copyright 2024 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_BOX_EDGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_BOX_EDGE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

//
// Represents the CSS `text-box-edge` property.
// https://drafts.csswg.org/css-inline-3/#propdef-text-box-edge
//
class CORE_EXPORT TextBoxEdge {
  DISALLOW_NEW();

 public:
  // https://drafts.csswg.org/css-inline-3/#text-edges.
  enum class Type : uint8_t {
    kLeading,
    kText,
    kCap,
    kEx,
    kAlphabetic,

    // kIdeographic, not implemented.
    // kIdeographicInk, not implemented.
  };

  TextBoxEdge() : TextBoxEdge(Type::kLeading, Type::kLeading) {}
  explicit TextBoxEdge(Type over)
      : TextBoxEdge(over, ComputeMissingUnderEdge(over)) {}
  TextBoxEdge(Type over, Type under) : over_(over), under_(under) {}

  bool operator==(const TextBoxEdge& other) const {
    return over_ == other.Over() && under_ == other.Under();
  }
  bool operator!=(const TextBoxEdge& other) const { return !(*this == other); }

  const Type& Over() const { return over_; }
  const Type& Under() const { return under_; }

 private:
  static constexpr Type ComputeMissingUnderEdge(Type over) {
    switch (over) {
      case Type::kText:
      case Type::kLeading:
        return over;
      case Type::kCap:
      case Type::kEx:
        return Type::kText;
      case Type::kAlphabetic:
        NOTREACHED_NORETURN();
    }
  }

  Type over_;
  Type under_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_BOX_EDGE_H_
