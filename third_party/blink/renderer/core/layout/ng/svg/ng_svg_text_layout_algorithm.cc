// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/svg/ng_svg_text_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/svg_character_data.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length_list.h"
#include "third_party/blink/renderer/core/svg/svg_animated_number_list.h"
#include "third_party/blink/renderer/core/svg/svg_text_positioning_element.h"

namespace blink {

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
  SetFlags(items);

  // 3. Resolve character positioning
  // 3.1. Set up:
  // 3.1.1. Let resolve_x, resolve_y, resolve_dx, and resolve_dy be arrays
  // of length count whose entries are all initialized to "unspecified".
  Vector<SVGCharacterData> resolve(addressable_count_);
  // 3.1.2. Set "in_text_path" flag false
  bool in_text_path = false;
  // 3.1.3. Call the following procedure with the ‘text’ element node.
  wtf_size_t index = 0;
  ResolveCharacterPositioning(*inline_node_.GetLayoutBox(), items, in_text_path,
                              index, resolve);

  // 4. Adjust positions: dx, dy
  AdjustPositionsDxDy(items, resolve);

  // 5. Apply ‘textLength’ attribute
  // TODO(crbug.com/1179585): Implement this step.

  // 6. Adjust positions: x, y
  AdjustPositionsXY(items, resolve);

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

    // 2.2. Set middle to true if the character at index i is the second or
    // later character that corresponds to a typographic character.
    for (unsigned text_offset = item.StartOffset() + 1;
         text_offset < item.EndOffset(); ++text_offset) {
      NGSVGPerCharacterInfo middle_info;
      middle_info.middle = true;
      middle_info.item_index = info.item_index;
      result_.push_back(middle_info);
      css_positions_.push_back(css_positions_.back());
    }
  }
  addressable_count_ = result_.size();
}

// 3.2. Procedure: resolve character positioning:
//
// This function updates |result_|, |index|, and |resolve|.
//
// TODO(crbug.com/1179585): Accessing LayoutObject tree structure here is not
// appropriate. We should do this in PrepareLayout().
void NGSVGTextLayoutAlgorithm::ResolveCharacterPositioning(
    const LayoutObject& layout_object,
    const NGFragmentItemsBuilder::ItemWithOffsetList& items,
    bool in_text_path,
    wtf_size_t& index,
    Vector<SVGCharacterData>& resolve) {
  // 1. If node is a ‘text’ or ‘tspan’ node:
  if (const auto* text_position_element =
          DynamicTo<SVGTextPositioningElement>(layout_object.GetNode())) {
    SVGLengthContext context(text_position_element);
    // 1.2. Let x, y, dx, dy and rotate be the lists of values from the
    // corresponding attributes on node, or empty lists if the corresponding
    // attribute was not specified or was invalid.
    const SVGLengthList& x = *text_position_element->x()->CurrentValue();
    const SVGLengthList& y = *text_position_element->y()->CurrentValue();
    const SVGLengthList& dx = *text_position_element->dx()->CurrentValue();
    const SVGLengthList& dy = *text_position_element->dy()->CurrentValue();
    const SVGNumberList& rotate =
        *text_position_element->rotate()->CurrentValue();
    // 1.3. If "in_text_path" flag is false:
    //   * Let new_chunk_count = max(length of x, length of y).
    // Else:
    //   * If the "horizontal" flag is true:
    //     * Let new_chunk_count = length of x.
    //   * Else:
    //     * Let new_chunk_count = length of y.
    wtf_size_t new_chunk_count;
    if (!in_text_path) {
      new_chunk_count = std::max(x.length(), y.length());
    } else if (horizontal_) {
      new_chunk_count = x.length();
    } else {
      new_chunk_count = y.length();
    }

    // 1.5. Let i = 0 and j = 0
    // ==> In our implementation, 'i' and 'j' are equivalent because we store
    //     only addressable characters.
    DCHECK_EQ(result_.size(), addressable_count_);
    DCHECK_EQ(resolve.size(), addressable_count_);
    wtf_size_t i = 0;
    // 1.6. While j < length, do:
    while (
        index + i < addressable_count_ &&
        items[result_[index + i].item_index]->GetLayoutObject()->IsDescendantOf(
            &layout_object)) {
      // 1.6.1. If the "addressable" flag of result[index + j] is true, then:
      // ==> "addressable" is always true in our implementation.
      NGSVGPerCharacterInfo& info = result_[index + i];
      // 1.6.1.1. If i < new_check_count, then set the "anchored chunk" flag
      // of result[index + j] to true. Else set the flag to false.
      info.anchored_chunk = i < new_chunk_count;
      // 1.6.1.2. If i < length of x, then set resolve_x[index + j] to x[i].
      if (i < x.length())
        resolve[index + i].x = x.at(i)->Value(context);
      // 1.6.1.3. If "in_text_path" flag is true and the "horizontal" flag is
      // false, unset resolve_x[index].
      // TODO(crbug.com/1179585): Check if [index] is a specification bug?
      if (in_text_path && !horizontal_)
        resolve[index].x = SVGCharacterData::EmptyValue();
      // 1.6.1.4. If i < length of y, then set resolve_y[index + j] to y[i].
      if (i < y.length())
        resolve[index + i].y = y.at(i)->Value(context);
      // 1.6.1.5. If "in_text_path" flag is true and the "horizontal" flag is
      // true, unset resolve_y[index].
      // TODO(crbug.com/1179585): Check if [index] is a specification bug?
      if (in_text_path && horizontal_)
        resolve[index].y = SVGCharacterData::EmptyValue();
      // 1.6.1.6. If i < length of dx, then set resolve_dx[index + j] to dy[i].
      // TODO(crbug.com/1179585): Report a specification bug on "dy[i]".
      if (i < dx.length())
        resolve[index + i].dx = dx.at(i)->Value(context);
      // 1.6.1.7. If i < length of dy, then set resolve_dy[index + j] to dy[i].
      if (i < dy.length())
        resolve[index + i].dy = dy.at(i)->Value(context);
      // 1.6.1.8. If i < length of rotate, then set the angle value of
      // result[index + j] to rotate[i]. Otherwise, if rotate is not empty,
      // then set result[index + j] to result[index + j − 1].
      if (i < rotate.length())
        info.rotate = rotate.at(i)->Value();
      else if (rotate.length() > 0)
        info.rotate = result_[index + i - 1].rotate;
      // 1.6.1.9. Set i = i + 1.
      // 1.6.2. Set j = j + 1.
      ++i;
    }
  } else if (IsA<SVGTextPathElement>(layout_object.GetNode())) {
    // 2. If node is a ‘textPath’ node:
    // 2.2. Set the "anchored chunk" flag of result[index] to true.
    result_[index].anchored_chunk = true;
    // 2.3. Set in_text_path flag true.
    in_text_path = true;
  }

  // 3. For each child node child of node:
  // ==> We traverse LayoutObjects instead.
  for (const LayoutObject* child = layout_object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    // 3.1. Resolve glyph positioning of child.
    if (!IsA<Element>(child->GetNode()))
      continue;
    // To compute the index number of the first character in |child|, we
    // traverse LayoutObject for result_[i] until we find |child|.
    wtf_size_t i;
    for (i = index; i < addressable_count_; ++i) {
      const LayoutObject* item_layout_object =
          items[result_[i].item_index]->GetLayoutObject();
      if (!item_layout_object->IsDescendantOf(&layout_object))
        break;
      if (item_layout_object->IsDescendantOf(child)) {
        index = i;
        ResolveCharacterPositioning(*child, items, in_text_path, index,
                                    resolve);
        break;
      }
    }
  }

  // Updates |index| so that it points the first addressable character in the
  // next sibling of |layout_object|.
  while (index < addressable_count_ &&
         items[result_[index].item_index]->GetLayoutObject()->IsDescendantOf(
             &layout_object))
    ++index;
}

void NGSVGTextLayoutAlgorithm::AdjustPositionsDxDy(
    const NGFragmentItemsBuilder::ItemWithOffsetList& items,
    Vector<SVGCharacterData>& resolve) {
  // 1. Let shift be the cumulative x and y shifts due to ‘x’ and ‘y’
  // attributes, initialized to (0,0).
  // TODO(crbug.com/1179585): Report a specification bug on "'x' and 'y'
  // attributes".
  FloatPoint shift;
  // 2. For each array element with index i in result:
  DCHECK_EQ(result_.size(), resolve.size());
  for (wtf_size_t i = 0; i < addressable_count_; ++i) {
    // 2.1. If resolve_x[i] is unspecified, set it to 0. If resolve_y[i] is
    // unspecified, set it to 0.
    // TODO(crbug.com/1179585): Report a specification bug on "resolve_x" and
    // "resolve_y".
    if (!resolve[i].HasDx())
      resolve[i].dx = 0.0;
    if (!resolve[i].HasDy())
      resolve[i].dy = 0.0;
    // 2.2. Let shift.x = shift.x + resolve_x[i] and
    // shift.y = shift.y + resolve_y[i].
    // TODO(crbug.com/1179585): Report a specification bug on "resolve_x" and
    // "resolve_y".
    shift.Move(resolve[i].dx, resolve[i].dy);
    // 2.3. Let result[i].x = CSS_positions[i].x + shift.x and
    // result[i].y = CSS_positions[i].y + shift.y.
    const float scaling_factor = ScalingFactorAt(items, i);
    result_[i].x = css_positions_[i].X() + shift.X() * scaling_factor;
    result_[i].y = css_positions_[i].Y() + shift.Y() * scaling_factor;
  }
}

void NGSVGTextLayoutAlgorithm::AdjustPositionsXY(
    const NGFragmentItemsBuilder::ItemWithOffsetList& items,
    const Vector<SVGCharacterData>& resolve) {
  // 1. Let shift be the current adjustment due to the ‘x’ and ‘y’ attributes,
  // initialized to (0,0).
  FloatPoint shift;
  // 2. Set index = 1.
  // 3. While index < count:
  // 3.5. Set index to index + 1.
  DCHECK_EQ(result_.size(), resolve.size());
  for (wtf_size_t i = 0; i < result_.size(); ++i) {
    const float scaling_factor = ScalingFactorAt(items, i);
    // 3.1. If resolved_x[index] is set, then let
    // shift.x = resolved_x[index] − result.x[index].
    if (resolve[i].HasX())
      shift.SetX(resolve[i].x * scaling_factor - *result_[i].x);
    // 3.2. If resolved_y[index] is set, then let
    // shift.y = resolved_y[index] − result.y[index].
    if (resolve[i].HasY())
      shift.SetY(resolve[i].y * scaling_factor - *result_[i].y);
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
