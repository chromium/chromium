// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/svg_text_layout_algorithm.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text_path.h"
#include "third_party/blink/renderer/core/layout/svg/resolved_text_layout_attributes_iterator.h"
#include "third_party/blink/renderer/core/layout/svg/svg_inline_node_data.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_text_content_element.h"
#include "third_party/blink/renderer/platform/wtf/text/code_point_iterator.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

// See https://svgwg.org/svg2-draft/text.html#TextLayoutAlgorithm

SvgTextLayoutAlgorithm::SvgTextLayoutAlgorithm(InlineNode node,
                                               WritingMode writing_mode)
    : inline_node_(node),
      // 1.5. Let "horizontal" be a flag, true if the writing mode of ‘text’
      // is horizontal, false otherwise.
      horizontal_(IsHorizontalWritingMode(writing_mode)) {
  DCHECK(node.IsSvgText());
}

PhysicalSize SvgTextLayoutAlgorithm::Layout(
    const String& ifc_text_content,
    FragmentItemsBuilder::ItemWithOffsetList& items) {
  TRACE_EVENT0("blink", "SvgTextLayoutAlgorithm::Layout");
  // https://svgwg.org/svg2-draft/text.html#TextLayoutAlgorithm
  //
  // The major difference from the algorithm in the specification:
  // We handle only addressable characters. The size of "result",
  // "CSS_positions", and "resolved" is the number of addressable characters.

  // 1. Setup
  if (!Setup(ifc_text_content.length())) {
    return PhysicalSize();
  }

  // 2. Set flags and assign initial positions
  SetFlags(ifc_text_content, items);
  if (addressable_count_ == 0) {
    return PhysicalSize();
  }

  // 3. Resolve character positioning
  // This was already done in PrepareLayout() step. See
  // SvgTextLayoutAttributesBuilder.
  // Copy |rotate| and |anchored_chunk| fields.
  ResolvedTextLayoutAttributesIterator iterator(
      inline_node_.SvgCharacterDataList());
  for (wtf_size_t i = 0; i < result_.size(); ++i) {
    const SvgCharacterData& resolve = iterator.AdvanceTo(i);
    if (resolve.HasRotate()) {
      result_[i].rotate = resolve.rotate;
    }
    if (resolve.anchored_chunk) {
      result_[i].anchored_chunk = true;
    }
  }

  // 4. Adjust positions: dx, dy
  AdjustPositionsDxDy(items);

  // 5. Apply ‘textLength’ attribute
  ApplyTextLengthAttribute(items);

  // 6. Adjust positions: x, y
  AdjustPositionsXY(items);

  // 7. Apply anchoring
  ApplyAnchoring(items);

  // 8. Position on path
  PositionOnPath(items);

  return WriteBackToFragmentItems(items);
}

bool SvgTextLayoutAlgorithm::Setup(wtf_size_t approximate_count) {
  // 1.2. Let count be the number of DOM characters within the ‘text’ element's
  // subtree.
  // ==> We don't use |count|. We set |addressable_count_| in the step 2.

  // 1.3. Let result be an array of length count whose entries contain the
  // per-character information described above.
  // ... If result is empty, then return result.
  if (approximate_count == 0) {
    return false;
  }
  // ==> We don't fill |result| here. We do it in the step 2.
  result_.reserve(approximate_count);

  // 1.4. Let CSS_positions be an array of length count whose entries will be
  // filled with the x and y positions of the corresponding typographic
  // character in root. The array entries are initialized to (0, 0).
  // ==> We don't fill |CSS_positions| here. We do it in the step 2.
  css_positions_.reserve(approximate_count);
  return true;
}

// This function updates |result_|.
void SvgTextLayoutAlgorithm::SetFlags(
    const String& ifc_text_content,
    const FragmentItemsBuilder::ItemWithOffsetList& items) {
  // This function collects information per an "addressable" character in DOM
  // order. So we need to access FragmentItems in the logical order.
  Vector<wtf_size_t> sorted_item_indexes;
  sorted_item_indexes.reserve(items.size());
  for (wtf_size_t i = 0; i < items.size(); ++i) {
    if (items[i]->Type() == FragmentItem::kText) {
      sorted_item_indexes.push_back(i);
    }
  }
  if (inline_node_.IsBidiEnabled()) {
    base::ranges::sort(sorted_item_indexes, [&](wtf_size_t a, wtf_size_t b) {
      return items[a]->StartOffset() < items[b]->StartOffset();
    });
  }

  bool found_first_character = false;
  for (wtf_size_t i : sorted_item_indexes) {
    // Zero-length item is not addressable.
    if (items[i]->TextLength() == 0) {
      continue;
    }
    SvgPerCharacterInfo info;
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
    const FragmentItem& item = *items[info.item_index];
    const LogicalOffset logical_offset = items[info.item_index].offset;
    LayoutUnit ascent;
    if (const auto* font_data = item.ScaledFont().PrimaryFont()) {
      ascent = font_data->GetFontMetrics().FixedAscent(
          item.Style().GetFontBaseline());
    }
    gfx::PointF offset(logical_offset.inline_offset,
                       logical_offset.block_offset + ascent);
    if (!horizontal_) {
      offset.SetPoint(-offset.y(), offset.x());
    }
    css_positions_.push_back(offset);

    info.inline_size = horizontal_ ? item.Size().width : item.Size().height;
    result_.push_back(info);

    StringView item_string(ifc_text_content, item.StartOffset(),
                           item.TextLength());
    // 2.2. Set middle to true if the character at index i is the second or
    // later character that corresponds to a typographic character.
    WTF::CodePointIterator iterator = item_string.begin();
    const WTF::CodePointIterator end = item_string.end();
    for (++iterator; iterator != end; ++iterator) {
      SvgPerCharacterInfo middle_info;
      middle_info.middle = true;
      middle_info.item_index = info.item_index;
      result_.push_back(middle_info);
      css_positions_.push_back(css_positions_.back());
    }
  }
  addressable_count_ = result_.size();
}

void SvgTextLayoutAlgorithm::AdjustPositionsDxDy(
    const FragmentItemsBuilder::ItemWithOffsetList& items) {
  // 1. Let shift be the cumulative x and y shifts due to ‘x’ and ‘y’
  // attributes, initialized to (0,0).
  // TODO(crbug.com/1179585): Report a specification bug on "'x' and 'y'
  // attributes".
  gfx::PointF shift;
  // 2. For each array element with index i in result:
  ResolvedTextLayoutAttributesIterator iterator(
      inline_node_.SvgCharacterDataList());
  for (wtf_size_t i = 0; i < addressable_count_; ++i) {
    const SvgCharacterData& resolve = iterator.AdvanceTo(i);
    // https://github.com/w3c/svgwg/issues/846
    if (resolve.HasX()) {
      shift.set_x(0.0f);
    }
    if (resolve.HasY()) {
      shift.set_y(0.0f);
    }

    // If this character is the first one in a <textPath>, reset both of x
    // and y.
    if (IsFirstCharacterInTextPath(i)) {
      shift.set_x(0.0f);
      shift.set_y(0.0f);
    }

    // 2.1. If resolve_x[i] is unspecified, set it to 0. If resolve_y[i] is
    // unspecified, set it to 0.
    // https://github.com/w3c/svgwg/issues/271
    // 2.2. Let shift.x = shift.x + resolve_x[i] and
    // shift.y = shift.y + resolve_y[i].
    // https://github.com/w3c/svgwg/issues/271
    shift.Offset(resolve.HasDx() ? resolve.dx : 0.0f,
                 resolve.HasDy() ? resolve.dy : 0.0f);
    // 2.3. Let result[i].x = CSS_positions[i].x + shift.x and
    // result[i].y = CSS_positions[i].y + shift.y.
    const float scaling_factor = ScalingFactorAt(items, i);
    result_[i].x =
        ClampTo<float>(css_positions_[i].x() + shift.x() * scaling_factor);
    result_[i].y =
        ClampTo<float>(css_positions_[i].y() + shift.y() * scaling_factor);
  }
}

void SvgTextLayoutAlgorithm::ApplyTextLengthAttribute(
    const FragmentItemsBuilder::ItemWithOffsetList& items) {
  // Start indexes of the highest textLength elements which were already
  // handled by ResolveTextLength().
  Vector<wtf_size_t> resolved_descendant_node_starts;
  for (const auto& range : inline_node_.SvgTextLengthRangeList()) {
    ResolveTextLength(items, range, resolved_descendant_node_starts);
  }
}

// The implementation of step 2 of "Procedure: resolve text length"
// in "5. Apply 'textLength' attribute".
//
// This function is called for elements with textLength in the order of
// closed tags. e.g.
//     <text textLength="...">
//       <tspan textLength="...">...</tspan>
//       <tspan textLength="...">...</tspan>
//     </text>
//    1. Called for the first <tspan>.
//    2. Called for the second <tspan>.
//    3. Called for the <text>.
void SvgTextLayoutAlgorithm::ResolveTextLength(
    const FragmentItemsBuilder::ItemWithOffsetList& items,
    const SvgTextContentRange& range,
    Vector<wtf_size_t>& resolved_descendant_node_starts) {
  const unsigned i = range.start_index;
  const unsigned j_plus_1 = range.end_index + 1;
  auto* element = To<SVGTextContentElement>(range.layout_object->GetNode());
  const float text_length = ClampTo<float>(
      element->textLength()->CurrentValue()->Value(SVGLengthContext(element)) *
      ScalingFactorAt(items, i));
  const SVGLengthAdjustType length_adjust =
      element->lengthAdjust()->CurrentEnumValue();

  // 2.1. Let a = +Infinity and b = −Infinity.
  float min_position = std::numeric_limits<float>::infinity();
  float max_position = -std::numeric_limits<float>::infinity();

  // 2.2. Let i and j be the global index of the first character and last
  // characters in node, respectively.
  // ==> They are computed in TextLayoutAttributeBuilder.

  // 2.3. For each index k in the range [i, j] where the "addressable" flag of
  // result[k] is true:
  for (wtf_size_t k = i; k < j_plus_1; ++k) {
    // 2.3.1. If the character at k is a linefeed or carriage return, return. No
    // adjustments due to ‘textLength’ are made to a node with a forced line
    // break.
    // ==> We don't support white-space:pre yet. crbug.com/366558.

    // 2.3.2. Let pos = the x coordinate of the position in result[k], if the
    // "horizontal" flag is true, and the y coordinate otherwise.
    float min_char_pos = horizontal_ ? *result_[k].x : *result_[k].y;

    // 2.3.3. Let advance = the advance of the typographic character
    // corresponding to character k.
    float inline_size = result_[k].inline_size;
    // 2.3.4. Set a = min(a, pos, pos + advance).
    min_position = std::min(min_position, min_char_pos);
    // 2.3.5. Set b = max(b, pos, pos + advance).
    max_position = std::max(max_position, min_char_pos + inline_size);
  }
  // 2.4. If a != +Infinity then:
  if (min_position == std::numeric_limits<float>::infinity()) {
    return;
  }
  // 2.4.1. Find the distance delta = ‘textLength’ computed value − (b − a).
  const float delta = text_length - (max_position - min_position);

  float shift;
  if (length_adjust == kSVGLengthAdjustSpacingAndGlyphs) {
    // If the target range contains no glyphs, we do nothing.
    if (min_position >= max_position) {
      return;
    }
    float length_adjust_scale = text_length / (max_position - min_position);
    for (wtf_size_t k = i; k < j_plus_1; ++k) {
      SvgPerCharacterInfo& info = result_[k];
      float original_x = *info.x;
      float original_y = *info.y;
      if (horizontal_) {
        *info.x = min_position + (*info.x - min_position) * length_adjust_scale;
      } else {
        *info.y = min_position + (*info.y - min_position) * length_adjust_scale;
      }
      info.text_length_shift_x += *info.x - original_x;
      info.text_length_shift_y += *info.y - original_y;
      if (!info.middle && !info.text_length_resolved) {
        info.length_adjust_scale = length_adjust_scale;
        info.inline_size *= length_adjust_scale;
      }
      info.text_length_resolved = true;
    }
    shift = delta;
  } else {
    // 2.4.2. Find n, the total number of typographic characters in this node
    // including any descendant nodes that are not resolved descendant nodes or
    // within a resolved descendant node.
    auto n = base::ranges::count_if(
        base::span(result_).subspan(i, j_plus_1 - i), [](const auto& info) {
          return !info.middle && !info.text_length_resolved;
        });
    // 2.4.3. Let n = n + number of resolved descendant nodes − 1.
    n += base::ranges::count_if(resolved_descendant_node_starts,
                                [i, j_plus_1](const auto& start_index) {
                                  return i <= start_index &&
                                         start_index < j_plus_1;
                                }) -
         1;
    // 2.4.4. Find the per-character adjustment small-delta = delta/n.
    // character_delta should be 0 if n==0 because it means we have no
    // adjustable characters for this textLength.
    float character_delta = n != 0 ? delta / n : 0;
    // 2.4.5. Let shift = 0.
    shift = 0.0f;
    // 2.4.6. For each index k in the range [i,j]:
    //  ==> This loop should run in visual order.
    Vector<wtf_size_t> visual_indexes;
    visual_indexes.reserve(j_plus_1 - i);
    for (wtf_size_t k = i; k < j_plus_1; ++k) {
      visual_indexes.push_back(k);
    }
    if (inline_node_.IsBidiEnabled()) {
      std::sort(visual_indexes.begin(), visual_indexes.end(),
                [&](wtf_size_t a, wtf_size_t b) {
                  return result_[a].item_index < result_[b].item_index;
                });
    }

    for (wtf_size_t k : visual_indexes) {
      SvgPerCharacterInfo& info = result_[k];
      // 2.4.6.1. Add shift to the x coordinate of the position in result[k], if
      // the "horizontal" flag is true, and to the y coordinate otherwise.
      if (horizontal_) {
        *info.x += shift;
        info.text_length_shift_x += shift;
      } else {
        *info.y += shift;
        info.text_length_shift_y += shift;
      }
      // 2.4.6.2. If the "middle" flag for result[k] is not true and k is not a
      // character in a resolved descendant node other than the first character
      // then shift = shift + small-delta.
      if (!info.middle && (base::Contains(resolved_descendant_node_starts, k) ||
                           !info.text_length_resolved)) {
        shift += character_delta;
      }
      info.text_length_resolved = true;
    }
  }
  // We should shift characters until the end of this text chunk.
  // Note: This is not defined by the algorithm. But it seems major SVG
  // engines work so.
  for (wtf_size_t k = j_plus_1; k < result_.size(); ++k) {
    if (result_[k].anchored_chunk) {
      break;
    }
    if (horizontal_) {
      *result_[k].x += shift;
    } else {
      *result_[k].y += shift;
    }
  }

  // Remove resolved_descendant_node_starts entries for descendant nodes,
  // and register an entry for this node.
  auto new_end =
      std::remove_if(resolved_descendant_node_starts.begin(),
                     resolved_descendant_node_starts.end(),
                     [i, j_plus_1](const auto& start_index) {
                       return i <= start_index && start_index < j_plus_1;
                     });
  resolved_descendant_node_starts.erase(new_end,
                                        resolved_descendant_node_starts.end());
  resolved_descendant_node_starts.push_back(i);
}

void SvgTextLayoutAlgorithm::AdjustPositionsXY(
    const FragmentItemsBuilder::ItemWithOffsetList& items) {
  // This function moves characters to
  //   <position specified by x/y attributes>
  //   + <shift specified by dx/dy attributes>
  //   + <baseline-shift done in the inline layout>
  // css_positions_[i].y() for horizontal_ or css_positions_[i].x() for
  // !horizontal_ represents baseline-shift because the block offsets of the
  // normal baseline is 0.

  // 1. Let shift be the current adjustment due to the ‘x’ and ‘y’ attributes,
  // initialized to (0,0).
  gfx::PointF shift;
  // 2. Set index = 1.
  // 3. While index < count:
  // 3.5. Set index to index + 1.
  ResolvedTextLayoutAttributesIterator iterator(
      inline_node_.SvgCharacterDataList());
  for (wtf_size_t i = 0; i < result_.size(); ++i) {
    const float scaling_factor = ScalingFactorAt(items, i);
    const SvgCharacterData& resolve = iterator.AdvanceTo(i);
    // 3.1. If resolved_x[index] is set, then let
    // shift.x = resolved_x[index] − result.x[index].
    // https://github.com/w3c/svgwg/issues/845
    if (resolve.HasX()) {
      shift.set_x(resolve.x * scaling_factor - css_positions_[i].x() -
                  result_[i].text_length_shift_x);
      // Take into account of baseline-shift.
      if (!horizontal_) {
        shift.set_x(shift.x() + css_positions_[i].x());
      }
      shift.set_x(ClampTo<float>(shift.x()));
    }
    // 3.2. If resolved_y[index] is set, then let
    // shift.y = resolved_y[index] − result.y[index].
    // https://github.com/w3c/svgwg/issues/845
    if (resolve.HasY()) {
      shift.set_y(resolve.y * scaling_factor - css_positions_[i].y() -
                  result_[i].text_length_shift_y);
      // Take into account of baseline-shift.
      if (horizontal_) {
        shift.set_y(shift.y() + css_positions_[i].y());
      }
      shift.set_y(ClampTo<float>(shift.y()));
    }

    // If this character is the first one in a <textPath>, reset the
    // block-direction shift.
    if (IsFirstCharacterInTextPath(i)) {
      if (horizontal_) {
        shift.set_y(0.0f);
      } else {
        shift.set_x(0.0f);
      }
    }

    // 3.3. Let result.x[index] = result.x[index] + shift.x and
    // result.y[index] = result.y[index] + shift.y.
    result_[i].x = *result_[i].x + shift.x();
    result_[i].y = *result_[i].y + shift.y();
    // 3.4. If the "middle" and "anchored chunk" flags of result[index] are
    // both true, then:
    if (result_[i].middle && result_[i].anchored_chunk) {
      // 3.4.1. Set the "anchored chunk" flag of result[index] to false.
      result_[i].anchored_chunk = false;
      // 3.4.2. If index + 1 < count, then set the "anchored chunk" flag of
      // result[index + 1] to true.
      if (i + 1 < result_.size()) {
        result_[i + 1].anchored_chunk = true;
      }
    }
  }
}

void SvgTextLayoutAlgorithm::ApplyAnchoring(
    const FragmentItemsBuilder::ItemWithOffsetList& items) {
  DCHECK_GT(result_.size(), 0u);
  DCHECK(result_[0].anchored_chunk);
  // 1. For each slice result[i..j] (inclusive of both i and j), where:
  //  * the "anchored chunk" flag of result[i] is true,
  //  * the "anchored chunk" flags of result[k] where i < k ≤ j are false, and
  //  * j = count − 1 or the "anchored chunk" flag of result[j + 1] is true;
  wtf_size_t i = 0;
  while (i < result_.size()) {
    const wtf_size_t start_index = i + 1;
    auto result_range = base::span(result_).subspan(start_index);
    auto next_anchor = base::ranges::find_if(
        result_range, [](const auto& info) { return info.anchored_chunk; });
    wtf_size_t j =
        start_index + static_cast<wtf_size_t>(
                          std::distance(result_range.begin(), next_anchor) - 1);

    const auto& text_path_ranges = inline_node_.SvgTextPathRangeList();
    const auto text_path_iter =
        base::ranges::find_if(text_path_ranges, [i](const auto& range) {
          return range.start_index <= i && i <= range.end_index;
        });
    if (text_path_iter != text_path_ranges.end()) {
      // Anchoring should be scoped within the <textPath>.
      // Non-anchored text following <textPath> will be handled in
      // PositionOnPath().
      // This affects the third test in svg/batik/text/textOnPath2.svg.
      j = std::min(j, text_path_iter->end_index);
    }

    // 1.1. Let a = +Infinity and b = −Infinity.
    // ==> 'a' is left/top of characters. 'b' is right/top of characters.
    float min_position = std::numeric_limits<float>::infinity();
    float max_position = -std::numeric_limits<float>::infinity();
    // 1.2. For each index k in the range [i, j] where the "addressable" flag
    // of result[k] is true:
    for (wtf_size_t k = i; k <= j; ++k) {
      // The code in this block is simpler than the specification because
      // min_char_pos is always smaller edge of the character though
      // result[k].x/y in the specification is not.

      // 1.2.1. Let pos = the x coordinate of the position in result[k], if
      // the "horizontal" flag is true, and the y coordinate otherwise.
      const float min_char_pos = horizontal_ ? *result_[k].x : *result_[k].y;
      // 2.2.2. Let advance = the advance of the typographic character
      // corresponding to character k.
      const float inline_size = result_[k].inline_size;
      // 2.2.3. Set a = min(a, pos, pos + advance).
      min_position = std::min(min_position, min_char_pos);
      // 2.2.4. Set b = max(b, pos, pos + advance).
      max_position = std::max(max_position, min_char_pos + inline_size);
    }

    // 1.3. if a != +Infinity, then:
    if (min_position != std::numeric_limits<float>::infinity()) {
      // 1.3.1. Let shift be the x coordinate of result[i], if the "horizontal"
      // flag is true, and the y coordinate otherwise.
      float shift = horizontal_ ? *result_[i].x : *result_[i].y;

      // 1.3.2. Adjust shift based on the value of text-anchor and direction
      // of the element the character at index i is in:
      //  -> (start, ltr) or (end, rtl)
      //       Set shift = shift − a.
      //  -> (start, rtl) or (end, ltr)
      //       Set shift = shift − b.
      //  -> (middle, ltr) or (middle, rtl)
      //       Set shift = shift − (a + b) / 2.
      const ComputedStyle& style = items[result_[i].item_index]->Style();
      const bool is_ltr = style.IsLeftToRightDirection();
      switch (style.TextAnchor()) {
        default:
          NOTREACHED_IN_MIGRATION();
          [[fallthrough]];
        case ETextAnchor::kStart:
          shift = is_ltr ? shift - min_position : shift - max_position;
          break;
        case ETextAnchor::kEnd:
          shift = is_ltr ? shift - max_position : shift - min_position;
          break;
        case ETextAnchor::kMiddle:
          shift = shift - (min_position + max_position) / 2;
          break;
      }

      // 1.3.3. For each index k in the range [i, j]:
      for (wtf_size_t k = i; k <= j; ++k) {
        // 1.3.3.1. Add shift to the x coordinate of the position in result[k],
        // if the "horizontal" flag is true, and to the y coordinate otherwise.
        if (horizontal_) {
          *result_[k].x += shift;
        } else {
          *result_[k].y += shift;
        }
      }
    }
    i = j + 1;
  }
}

void SvgTextLayoutAlgorithm::PositionOnPath(
    const FragmentItemsBuilder::ItemWithOffsetList& items) {
  const auto& ranges = inline_node_.SvgTextPathRangeList();
  if (ranges.empty()) {
    return;
  }

  wtf_size_t range_index = 0;
  wtf_size_t in_path_index = WTF::kNotFound;
  std::unique_ptr<PathPositionMapper> path_mapper;

  // 2. Set the "in path" flag to false.
  bool in_path = false;
  // 3. Set the "after path" flag to false.
  bool after_path = false;
  // 4. Let path_end be an offset for characters that follow a ‘textPath’
  // element. Set path_end to (0,0).
  float path_end_x = 0.0f;
  float path_end_y = 0.0f;
  // 1. Set index = 0.
  // 5. While index < count:
  // 5.3. Set index = index + 1.
  for (unsigned index = 0; index < result_.size(); ++index) {
    auto& info = result_[index];
    // 5.1. If the character at index i is within a ‘textPath’ element and
    // corresponds to a typographic character, then:
    if (range_index < ranges.size() &&
        index >= ranges[range_index].start_index &&
        index <= ranges[range_index].end_index) {
      if (!in_path || in_path_index != range_index) {
        path_mapper =
            To<LayoutSVGTextPath>(ranges[range_index].layout_object.Get())
                ->LayoutPath();
      }
      // 5.1.1. Set "in path" flag to true.
      in_path = true;
      in_path_index = range_index;
      info.in_text_path = true;
      // 5.1.2. If the "middle" flag of result[index] is false, then:
      if (!info.middle) {
        const float scaling_factor = ScalingFactorAt(items, index);
        // 5.1.2.1. Let path be the equivalent path of the basic shape element
        // referenced by the ‘textPath’ element, or an empty path if the
        // reference is invalid.
        if (!path_mapper) {
          info.hidden = true;
        } else {
          // 5.1.2.2. If the ‘side’ attribute of the ‘textPath’ element is
          // 'right', then reverse path.
          // ==> We don't support 'side' attribute yet.

          // 5.1.2.4. Let offset be the value of the ‘textPath’ element's
          // ‘startOffset’ attribute, adjusted due to any ‘pathLength’
          // attribute on the referenced element.
          const float offset = path_mapper->StartOffset();

          // 5.1.2.5. Let advance = the advance of the typographic character
          // corresponding to character k.
          // 5.1.2.6. Let (x, y) and angle be the position and angle in
          // result[index].
          // 5.1.2.7. Let mid be a coordinate value depending on the value of
          // the "horizontal" flag:
          //   -> true
          //      mid is x + advance / 2 + offset
          //   -> false
          //      mid is y + advance / 2 + offset
          const float mid =
              ((horizontal_ ? *info.x : *info.y) + info.inline_size / 2) /
                  scaling_factor +
              offset;

          // 5.1.2.3. Let length be the length of path.
          // 5.1.2.9. If path is a closed subpath depending on the values of
          // text-anchor and direction of the element the character at index is
          // in:
          //   -> (start, ltr) or (end, rtl)
          //      If mid−offset < 0 or mid−offset > length, set the "hidden"
          //      flag of result[index] to true.
          //   -> (middle, ltr) or (middle, rtl)
          //      If mid−offset < −length/2 or mid−offset > length/2, set the
          //      "hidden" flag of result[index] to true.
          //   -> (start, rtl) or (end, ltr)
          //      If mid−offset < −length or mid−offset > 0, set the "hidden"
          //      flag of result[index] to true.
          //
          // ==> Major browsers don't support the special handling for closed
          //     paths.

          // 5.1.2.10. If the hidden flag is false:
          if (!info.hidden) {
            PointAndTangent point_tangent;
            PathPositionMapper::PositionType position_type =
                path_mapper->PointAndNormalAtLength(mid, point_tangent);
            if (position_type != PathPositionMapper::kOnPath) {
              info.hidden = true;
            }
            point_tangent.tangent_in_degrees += info.rotate.value_or(0.0f);
            if (!horizontal_) {
              point_tangent.tangent_in_degrees -= 90;
            }
            info.rotate = point_tangent.tangent_in_degrees;
            if (*info.rotate == 0.0f) {
              if (horizontal_) {
                info.x = point_tangent.point.x() * scaling_factor -
                         info.inline_size / 2;
                info.y = point_tangent.point.y() * scaling_factor + *info.y;
              } else {
                info.x = point_tangent.point.x() * scaling_factor + *info.x;
                info.y = point_tangent.point.y() * scaling_factor -
                         info.inline_size / 2;
              }
            } else {
              // Unlike the specification, we just set result[index].x/y to the
              // point along the path. The character is moved by an
              // AffineTransform produced from baseline_shift and inline_size/2.
              // See |FragmentItem::BuildSVGTransformForTextPath()|.
              info.baseline_shift = horizontal_ ? *info.y : *info.x;
              info.x = point_tangent.point.x() * scaling_factor;
              info.y = point_tangent.point.y() * scaling_factor;
            }
            info.x = ClampTo<float>(*info.x);
            info.y = ClampTo<float>(*info.y);
          }
        }
      } else {
        // 5.1.3. Otherwise, the "middle" flag of result[index] is true:
        // 5.1.3.1. Set the position and angle values of result[index] to those
        // in result[index − 1].
        info.x = *result_[index - 1].x;
        info.y = *result_[index - 1].y;
        info.rotate = result_[index - 1].rotate;
      }
    } else {
      // 5.2. If the character at index i is not within a ‘textPath’ element
      // and corresponds to a typographic character, then:
      // 5.2.1. If the "in path" flag is true:
      if (in_path) {
        // 5.2.1.1. Set the "in path" flag to false.
        in_path = false;
        // 5.2.1.2. Set the "after path" flag to true.
        after_path = true;
        // 5.2.1.3. Set path_end equal to the end point of the path referenced
        // by ‘textPath’ − the position of result[index].
        //
        // ==> This is not compatible with the legacy layout, in which text
        // following <textPath> is placed on the end of the last character
        // in the <textPath>. However, the specification asks the new behavior
        // explicitly. See the figure before
        // https://svgwg.org/svg2-draft/text.html#TextRenderingOrder .
        // This affects svg/batik/text/{textOnPath,textOnPath2}.svg.
        if (path_mapper) {
          const float scaling_factor = ScalingFactorAt(items, index);
          PointAndTangent point_tangent;
          path_mapper->PointAndNormalAtLength(path_mapper->length(),
                                              point_tangent);
          path_end_x = ClampTo<float>(point_tangent.point.x() * scaling_factor -
                                      *info.x);
          path_end_y = ClampTo<float>(point_tangent.point.y() * scaling_factor -
                                      *info.y);
        } else {
          // The 'current text position' should be at the next to the last
          // drawn character.
          auto result_range = base::span(result_).subspan(index);
          auto reverse_result_range = base::Reversed(result_range);
          const auto iter = base::ranges::find_if(
              reverse_result_range,
              [](const auto& info) { return !info.hidden && !info.middle; });
          if (iter != reverse_result_range.end()) {
            if (horizontal_) {
              path_end_x = *iter->x + iter->inline_size;
              path_end_y = *iter->y;
            } else {
              path_end_x = *iter->x;
              path_end_y = *iter->y + iter->inline_size;
            }
          } else {
            path_end_x = 0.0f;
            path_end_y = 0.0f;
          }
          path_end_x -= *info.x;
          path_end_y -= *info.y;
        }
      }
      // 5.2.2. If the "after path" is true.
      if (after_path) {
        // 5.2.2.1. If anchored chunk of result[index] is true, set the
        // "after path" flag to false.
        if (info.anchored_chunk) {
          after_path = false;
        } else {
          // 5.2.2.2. Else, let result.x[index] = result.x[index] + path_end.x
          // and result.y[index] = result.y[index] + path_end.y.
          *info.x += path_end_x;
          *info.y += path_end_y;
        }
      }
    }
    if (range_index < ranges.size() && index == ranges[range_index].end_index) {
      ++range_index;
    }
  }
}

PhysicalSize SvgTextLayoutAlgorithm::WriteBackToFragmentItems(
    FragmentItemsBuilder::ItemWithOffsetList& items) {
  gfx::RectF unscaled_visual_rect;
  for (const SvgPerCharacterInfo& info : result_) {
    if (info.middle) {
      continue;
    }
    FragmentItemsBuilder::ItemWithOffset& item = items[info.item_index];
    const auto* layout_object =
        To<LayoutSVGInlineText>(item->GetLayoutObject());
    LayoutUnit ascent;
    LayoutUnit descent;
    if (const auto* font_data = layout_object->ScaledFont().PrimaryFont()) {
      const auto& font_metrics = font_data->GetFontMetrics();
      const auto font_baseline = item->Style().GetFontBaseline();
      ascent = font_metrics.FixedAscent(font_baseline);
      descent = font_metrics.FixedDescent(font_baseline);
    }
    float x = *info.x;
    float y = *info.y;
    float width;
    float height;
    if (horizontal_) {
      y -= ascent;
      width = info.inline_size;
      height = item->Size().height;
    } else {
      x -= descent;
      width = item->Size().width;
      height = info.inline_size;
    }
    // Clamp values in order to avoid infinity values.
    gfx::RectF scaled_rect(ClampTo<float>(x), ClampTo<float>(y),
                           ClampTo<float>(width), ClampTo<float>(height));
    const float scaling_factor = layout_object->ScalingFactor();
    DCHECK_NE(scaling_factor, 0.0f);
    gfx::RectF unscaled_rect = gfx::ScaleRect(scaled_rect, 1 / scaling_factor);
    auto* data = MakeGarbageCollected<SvgFragmentData>();
    data->rect = scaled_rect;
    data->length_adjust_scale = info.length_adjust_scale;
    data->angle = info.rotate.value_or(0.0f);
    data->baseline_shift = info.baseline_shift;
    data->in_text_path = info.in_text_path;
    item.item.SetSvgFragmentData(
        data, PhysicalRect::EnclosingRect(unscaled_rect), info.hidden);

    gfx::RectF transformd_rect = scaled_rect;
    if (item.item.HasSvgTransformForBoundingBox()) {
      transformd_rect =
          item.item.BuildSvgTransformForBoundingBox().MapRect(transformd_rect);
    }
    transformd_rect.Scale(1 / scaling_factor);
    unscaled_visual_rect.Union(transformd_rect);
  }
  if (items[0]->Type() == FragmentItem::kLine) {
    items[0].item.SetSvgLineLocalRect(
        PhysicalRect(gfx::ToEnclosingRect(unscaled_visual_rect)));
  }
  // |items| should not have kLine items other than the first one.
  DCHECK(base::ranges::find(base::span(items).subspan(1u), FragmentItem::kLine,
                            &FragmentItem::Type) ==
         base::span(items).subspan(1u).end());
  return {LayoutUnit(unscaled_visual_rect.right()),
          LayoutUnit(unscaled_visual_rect.bottom())};
}

float SvgTextLayoutAlgorithm::ScalingFactorAt(
    const FragmentItemsBuilder::ItemWithOffsetList& items,
    wtf_size_t addressable_index) const {
  return items[result_[addressable_index].item_index]->SvgScalingFactor();
}

bool SvgTextLayoutAlgorithm::IsFirstCharacterInTextPath(
    wtf_size_t index) const {
  if (!result_[index].anchored_chunk) {
    return false;
  }
  // This implementation is O(N) where N is the number of <textPath>s in
  // a <text>. If this function is a performance bottleneck, we should add
  // |first_in_text_path| flag to SvgCharacterData.
  return base::Contains(inline_node_.SvgTextPathRangeList(), index,
                        &SvgTextContentRange::start_index);
}

}  // namespace blink
