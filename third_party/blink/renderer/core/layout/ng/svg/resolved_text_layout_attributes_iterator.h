// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_RESOLVED_TEXT_LAYOUT_ATTRIBUTES_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_RESOLVED_TEXT_LAYOUT_ATTRIBUTES_ITERATOR_H_

#include <algorithm>
#include <iterator>
#include <utility>

#include "third_party/blink/renderer/core/layout/ng/svg/ng_svg_character_data.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// This class wraps a sparse list, |Vector<std::pair<unsigned,
// NGSvgCharacterData>>|, so that it looks to have NGSvgCharacterData for
// any index.
//
// For example, if |resolved| contains the following pairs:
//     resolved[0]: (0, NGSvgCharacterData)
//     resolved[1]: (10, NGSvgCharacterData)
//     resolved[2]: (42, NGSvgCharacterData)
//
// AdvanceTo(0) returns the NGSvgCharacterData at [0].
// AdvanceTo(1 - 9) returns the default NGSvgCharacterData, which has no data.
// AdvanceTo(10) returns the NGSvgCharacterData at [1].
// AdvanceTo(11 - 41) returns the default NGSvgCharacterData.
// AdvanceTo(42) returns the NGSvgCharacterData at [2].
// AdvanceTo(43 or greater) returns the default NGSvgCharacterData.
class ResolvedTextLayoutAttributesIterator final {
  USING_FAST_MALLOC(ResolvedTextLayoutAttributesIterator);

 public:
  explicit ResolvedTextLayoutAttributesIterator(
      const Vector<std::pair<unsigned, NGSvgCharacterData>>& resolved)
      : resolved_(resolved) {}
  ResolvedTextLayoutAttributesIterator(
      const ResolvedTextLayoutAttributesIterator&) = delete;
  ResolvedTextLayoutAttributesIterator& operator=(
      const ResolvedTextLayoutAttributesIterator&) = delete;

  const NGSvgCharacterData& AdvanceTo(unsigned addressable_index) {
    if (index_ >= resolved_.size())
      return default_data_;
    if (addressable_index < resolved_[index_].first)
      return default_data_;
    if (addressable_index == resolved_[index_].first)
      return resolved_[index_].second;
    auto* it = std::find_if(resolved_.begin() + index_, resolved_.end(),
                            [addressable_index](const auto& pair) {
                              return addressable_index <= pair.first;
                            });
    index_ = static_cast<wtf_size_t>(std::distance(resolved_.begin(), it));
    return AdvanceTo(addressable_index);
  }

 private:
  const NGSvgCharacterData default_data_;
  const Vector<std::pair<unsigned, NGSvgCharacterData>>& resolved_;
  wtf_size_t index_ = 0u;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_RESOLVED_TEXT_LAYOUT_ATTRIBUTES_ITERATOR_H_
