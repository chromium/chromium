// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/svg/ng_svg_text_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"

namespace blink {

namespace {

// This class wraps a sparse list, |Vector<std::pair<unsigned,
// NGSVGCharacterData>>|, so that it looks to have NGSVGCharacterData for
// any index.
//
// For example, if |resolved| contains the following pairs:
//     resolved[0]: (0, NGSVGCharacterData)
//     resolved[1]: (10, NGSVGCharacterData)
//     resolved[2]: (42, NGSVGCharacterData)
//
// AdvanceTo(0) returns the NGSVGCharacterData at [0].
// AdvanceTo(1 - 9) returns the default NGSVGCharacterData, which has no data.
// AdvanceTo(10) returns the NGSVGCharacterData at [1].
// AdvanceTo(11 - 41) returns the default NGSVGCharacterData.
// AdvanceTo(42) returns the NGSVGCharacterData at [2].
// AdvanceTo(43 or greater) returns the default NGSVGCharacterData.
class ResolvedIterator final {
 public:
  explicit ResolvedIterator(
      const Vector<std::pair<unsigned, NGSVGCharacterData>>& resolved)
      : resolved_(resolved) {}
  ResolvedIterator(const ResolvedIterator&) = delete;
  ResolvedIterator& operator=(const ResolvedIterator&) = delete;

  const NGSVGCharacterData& AdvanceTo(unsigned addressable_index) {
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
    index_ = std::distance(resolved_.begin(), it);
    return AdvanceTo(addressable_index);
  }

 private:
  const NGSVGCharacterData default_data_;
  const Vector<std::pair<unsigned, NGSVGCharacterData>>& resolved_;
  unsigned index_ = 0u;
};

unsigned NextCodePointOffset(StringView string, unsigned offset) {
  ++offset;
  if (offset < string.length() && U16_IS_LEAD(string[offset - 1]) &&
      U16_IS_TRAIL(string[offset]))
    ++offset;
  return offset;
}

}  // anonymous namespace

// See https://svgwg.org/svg2-draft/text.html#TextLayoutAlgorithm

NGSVGTextLayoutAlgorithm::NGSVGTextLayoutAlgorithm(NGInlineNode node,
                                                   WritingMode writing_mode)
    : inline_node_(node),
      // 1.5. Let "horizontal" be a flag, true if the writing mode of ‘text’
      // is horizontal, false otherwise.
      horizontal_(IsHorizontalWritingMode(writing_mode)) {
  DCHECK(node.IsSVGText());
}

void NGSVGTextLayoutAlgorithm::Layout(
    const String& ifc_text_content,
    NGFragmentItemsBuilder::ItemWithOffsetList& items) {
  // https://svgwg.org/svg2-draft/text.html#TextLayoutAlgorithm
  //
  // The major difference from the algorithm in the specification:
  // We handle only addressable characters. The size of "result",
  // "CSS_positions", and "resolved" is the number of addressable characters.

  // 1. Setup
  if (!Setup(items.size()))
    return;

  // 2. Set flags and assign initial positions
  SetFlags(ifc_text_content, items);

  // 3. Resolve character positioning
  // This was already done in PrepareLayout() step. See
  // NGSVGTextLayoutAttributesBuilder.
  // Copy |rotate| and |anchored_chunk| fields.
  ResolvedIterator iterator(inline_node_.SVGCharacterDataList());
  for (wtf_size_t i = 0; i < result_.size(); ++i) {
    const NGSVGCharacterData& resolve = iterator.AdvanceTo(i);
    result_[i].rotate = resolve.rotate;
    if (resolve.anchored_chunk)
      result_[i].anchored_chunk = true;
  }

  // 4. Adjust positions: dx, dy
  AdjustPositionsDxDy(items);

  // 5. Apply ‘textLength’ attribute
  // TODO(crbug.com/1179585): Implement this step.

  // 6. Adjust positions: x, y
  AdjustPositionsXY(items);

  // 7. Apply anchoring
  // TODO(crbug.com/1179585): Implement this step.

  // 8. Position on path
  // TODO(crbug.com/1179585): Implement this step.

  // Write back the result to NGFragmentItems.
  for (const NGSVGPerCharacterInfo& info : result_) {
    if (info.middle)
      continue;
    NGFragmentItemsBuilder::ItemWithOffset& item = items[info.item_index];
    const auto* layout_object =
        To<LayoutSVGInlineText>(item->GetLayoutObject());
    // TODO(crbug.com/1179585): Supports vertical flow.
    LayoutUnit ascent = layout_object->ScaledFont()
                            .PrimaryFont()
                            ->GetFontMetrics()
                            .FixedAscent();
    FloatRect scaled_rect(*info.x, *info.y - ascent, item->Size().width,
                          item->Size().height);
    const float scaling_factor = layout_object->ScalingFactor();
    DCHECK_NE(scaling_factor, 0.0f);
    PhysicalRect unscaled_rect(
        LayoutUnit(*info.x / scaling_factor),
        LayoutUnit((*info.y - ascent) / scaling_factor),
        LayoutUnit(item->Size().width / scaling_factor),
        LayoutUnit(item->Size().height / scaling_factor));
    item.item.ConvertToSVGText(unscaled_rect, scaled_rect);
  }
}

bool NGSVGTextLayoutAlgorithm::Setup(wtf_size_t approximate_count) {
  // 1.2. Let count be the number of DOM characters within the ‘text’ element's
  // subtree.
  // ==> We don't use |count|. We set |addressable_count_| in the step 2.

  // 1.3. Let result be an array of length count whose entries contain the
  // per-character information described above.
  // ... If result is empty, then return result.
  if (approximate_count == 0)
    return false;
  // ==> We don't fill |result| here. We do it in the step 2.
  result_.ReserveCapacity(approximate_count);

  // 1.4. Let CSS_positions be an array of length count whose entries will be
  // filled with the x and y positions of the corresponding typographic
  // character in root. The array entries are initialized to (0, 0).
  // ==> We don't fill |CSS_positions| here. We do it in the step 2.
  css_positions_.ReserveCapacity(approximate_count);
  return true;
}

// This function updates |result_|.
void NGSVGTextLayoutAlgorithm::SetFlags(
    const String& ifc_text_content,
    const NGFragmentItemsBuilder::ItemWithOffsetList& items) {
  // This function collects information per an "addressable" character in DOM
  // order. So we need to access NGFragmentItems in the logical order.
  Vector<wtf_size_t> sorted_item_indexes;
  sorted_item_indexes.ReserveCapacity(items.size());
  for (wtf_size_t i = 0; i < items.size(); ++i) {
    if (items[i]->Type() == NGFragmentItem::kText)
      sorted_item_indexes.push_back(i);
  }
  if (inline_node_.IsBidiEnabled()) {
    std::sort(sorted_item_indexes.data(),
              sorted_item_indexes.data() + sorted_item_indexes.size(),
              [&](wtf_size_t a, wtf_size_t b) {
                return items[a]->StartOffset() < items[b]->StartOffset();
              });
  }

  bool found_first_character = false;
  for (wtf_size_t i : sorted_item_indexes) {
    NGSVGPerCharacterInfo info;
    info.item_index = i;
    // 2.3. If the character at index i corresponds to a typographic
    // character at the beginning of a line, then set the "anchored chunk"
    // flag of result[i] to true.
    if (!found_first_character) {
      found_first_character = true;
      info.anchored_chunk = true;
    }
    // 2.4. If addressable is true and middle is false then set
    // CSS_positions[i] to the position of the corresponding typographic
    // character as determined by the CSS renderer.
    const NGFragmentItem& item = *items[info.item_index];
    PhysicalOffset offset = item.OffsetInContainerFragment();
    const auto* layout_svg_inline =
        To<LayoutSVGInlineText>(item.GetLayoutObject());
    LayoutUnit ascent = layout_svg_inline->ScaledFont()
                            .PrimaryFont()
                            ->GetFontMetrics()
                            .FixedAscent();
    // TODO(crbug.com/1179585): Supports vertical flow.
    css_positions_.push_back(FloatPoint(offset.left, offset.top + ascent));
    result_.push_back(info);

    StringView item_string(ifc_text_content, item.StartOffset(),
                           item.TextLength());
    // 2.2. Set middle to true if the character at index i is the second or
    // later character that corresponds to a typographic character.
    for (unsigned text_offset = NextCodePointOffset(item_string, 0);
         text_offset < item_string.length();
         text_offset = NextCodePointOffset(item_string, text_offset)) {
      NGSVGPerCharacterInfo middle_info;
      middle_info.middle = true;
      middle_info.item_index = info.item_index;
      result_.push_back(middle_info);
      css_positions_.push_back(css_positions_.back());
    }
  }
  addressable_count_ = result_.size();
}

void NGSVGTextLayoutAlgorithm::AdjustPositionsDxDy(
    const NGFragmentItemsBuilder::ItemWithOffsetList& items) {
  // 1. Let shift be the cumulative x and y shifts due to ‘x’ and ‘y’
  // attributes, initialized to (0,0).
  // TODO(crbug.com/1179585): Report a specification bug on "'x' and 'y'
  // attributes".
  FloatPoint shift;
  // 2. For each array element with index i in result:
  ResolvedIterator iterator(inline_node_.SVGCharacterDataList());
  for (wtf_size_t i = 0; i < addressable_count_; ++i) {
    const NGSVGCharacterData& resolve = iterator.AdvanceTo(i);
    // 2.1. If resolve_x[i] is unspecified, set it to 0. If resolve_y[i] is
    // unspecified, set it to 0.
    // https://github.com/w3c/svgwg/issues/271
    // 2.2. Let shift.x = shift.x + resolve_x[i] and
    // shift.y = shift.y + resolve_y[i].
    // https://github.com/w3c/svgwg/issues/271
    shift.Move(resolve.HasDx() ? resolve.dx : 0.0f,
               resolve.HasDy() ? resolve.dy : 0.0f);
    // 2.3. Let result[i].x = CSS_positions[i].x + shift.x and
    // result[i].y = CSS_positions[i].y + shift.y.
    const float scaling_factor = ScalingFactorAt(items, i);
    result_[i].x = css_positions_[i].X() + shift.X() * scaling_factor;
    result_[i].y = css_positions_[i].Y() + shift.Y() * scaling_factor;
  }
}

void NGSVGTextLayoutAlgorithm::AdjustPositionsXY(
    const NGFragmentItemsBuilder::ItemWithOffsetList& items) {
  // 1. Let shift be the current adjustment due to the ‘x’ and ‘y’ attributes,
  // initialized to (0,0).
  FloatPoint shift;
  // 2. Set index = 1.
  // 3. While index < count:
  // 3.5. Set index to index + 1.
  ResolvedIterator iterator(inline_node_.SVGCharacterDataList());
  for (wtf_size_t i = 0; i < result_.size(); ++i) {
    const float scaling_factor = ScalingFactorAt(items, i);
    const NGSVGCharacterData& resolve = iterator.AdvanceTo(i);
    // 3.1. If resolved_x[index] is set, then let
    // shift.x = resolved_x[index] − result.x[index].
    if (resolve.HasX())
      shift.SetX(resolve.x * scaling_factor - *result_[i].x);
    // 3.2. If resolved_y[index] is set, then let
    // shift.y = resolved_y[index] − result.y[index].
    if (resolve.HasY())
      shift.SetY(resolve.y * scaling_factor - *result_[i].y);
    // 3.3. Let result.x[index] = result.x[index] + shift.x and
    // result.y[index] = result.y[index] + shift.y.
    result_[i].x = *result_[i].x + shift.X();
    result_[i].y = *result_[i].y + shift.Y();
    // 3.4. If the "middle" and "anchored chunk" flags of result[index] are
    // both true, then:
    if (result_[i].middle && result_[i].anchored_chunk) {
      // 3.4.1. Set the "anchored chunk" flag of result[index] to false.
      result_[i].anchored_chunk = false;
      // 3.4.2. If index + 1 < count, then set the "anchored chunk" flag of
      // result[index + 1] to true.
      if (i + 1 < result_.size())
        result_[i + 1].anchored_chunk = true;
    }
  }
}

float NGSVGTextLayoutAlgorithm::ScalingFactorAt(
    const NGFragmentItemsBuilder::ItemWithOffsetList& items,
    wtf_size_t addressable_index) const {
  return To<LayoutSVGInlineText>(
             items[result_[addressable_index].item_index]->GetLayoutObject())
      ->ScalingFactor();
}

}  // namespace blink
