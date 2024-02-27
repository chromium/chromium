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

  explicit TextBoxEdge(const TextBoxEdgeType& x)
      : over_(x), under_(ComputedMissingUnderEdge()) {}

  TextBoxEdge(const TextBoxEdgeType& x, const TextBoxEdgeType& y)
      : over_(x), under_(y) {}

  bool operator==(const TextBoxEdge& o) const {
    return over_ == o.Over() && under_ == o.Under();
  }
  bool operator!=(const TextBoxEdge& o) const { return !(*this == o); }

  const TextBoxEdgeType& Over() const { return over_; }
  const TextBoxEdgeType& Under() const { return under_; }

 private:
  TextBoxEdgeType ComputedMissingUnderEdge() const {
    switch (over_) {
      case TextBoxEdgeType::kText:
      case TextBoxEdgeType::kLeading:
        return over_;
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
