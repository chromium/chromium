// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUTLINE_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUTLINE_TYPE_H_

namespace blink {

// Outline styles
enum class NGOutlineType {
  kDontIncludeBlockVisualOverflow,       // Standard outline
  kIncludeBlockVisualOverflow,           // Focus outline
  kIncludeBlockVisualOverflowForAnchor,  // Focus outline for anchor
};

inline bool ShouldIncludeBlockVisualOverflow(NGOutlineType type) {
  return type == NGOutlineType::kIncludeBlockVisualOverflow ||
         type == NGOutlineType::kIncludeBlockVisualOverflowForAnchor;
}

inline bool ShouldIncludeBlockVisualOverflowForAnchorOnly(NGOutlineType type) {
  return type == NGOutlineType::kIncludeBlockVisualOverflowForAnchor;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUTLINE_TYPE_H_
