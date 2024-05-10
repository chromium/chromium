// Copyright 2024 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_BOX_EDGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_BOX_EDGE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CORE_EXPORT TextBoxEdge {
  DISALLOW_NEW();

 public:
  // https://drafts.csswg.org/css-inline-3/#text-edges.
  enum class TextBoxEdgeType : uint8_t {
    kLeading,
    kText,
    kCap,
    kEx,
    kAlphabetic,

    // kIdeographic, not implemented.
    // kIdeographicInk, not implemented.
  };

  explicit TextBoxEdge(TextBoxEdgeType over)
      : TextBoxEdge(over, ComputedMissingUnderEdge(over)) {}

  TextBoxEdge(TextBoxEdgeType over, TextBoxEdgeType under)
      : over_(over), under_(under) {}

  bool operator==(const TextBoxEdge& other) const {
    return over_ == other.Over() && under_ == other.Under();
  }
  bool operator!=(const TextBoxEdge& other) const { return !(*this == other); }

  const TextBoxEdgeType& Over() const { return over_; }
  const TextBoxEdgeType& Under() const { return under_; }

 private:
  static TextBoxEdgeType ComputedMissingUnderEdge(TextBoxEdgeType over) {
    switch (over) {
      case TextBoxEdgeType::kText:
      case TextBoxEdgeType::kLeading:
        return over;
      case TextBoxEdgeType::kCap:
      case TextBoxEdgeType::kEx:
        return TextBoxEdgeType::kText;
      case TextBoxEdgeType::kAlphabetic:
        NOTREACHED_NORETURN();
    }
  }

  TextBoxEdgeType over_;
  TextBoxEdgeType under_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_BOX_EDGE_H_
