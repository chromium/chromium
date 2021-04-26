// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/svg/ng_svg_text_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_text_content_element.h"

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
  if (addressable_count_ == 0)
    return;

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
  ApplyTextLengthAttribute(items);

  // 6. Adjust positions: x, y
  AdjustPositionsXY(items);

  // 7. Apply anchoring
  ApplyAnchoring(items);

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
    const float width = horizontal_ ? info.inline_size : item->Size().width;
    const float height = horizontal_ ? item->Size().height : info.inline_size;
    FloatRect scaled_rect(*info.x, *info.y - ascent, width, height);
    const float scaling_factor = layout_object->ScalingFactor();
    DCHECK_NE(scaling_factor, 0.0f);
    PhysicalRect unscaled_rect(LayoutUnit(*info.x / scaling_factor),
                               LayoutUnit((*info.y - ascent) / scaling_factor),
                               LayoutUnit(width / scaling_factor),
                               LayoutUnit(height / scaling_factor));
    AffineTransform transform;
    if (info.length_adjust_scale != 1.0f) {
      // We'd like to scale only inline-size without moving inline position.
      if (horizontal_) {
        transform.SetMatrix(info.length_adjust_scale, 0, 0, 1,
                            *info.x - info.length_adjust_scale * *info.x, 0);
      } else {
        transform.SetMatrix(1, 0, 0, info.length_adjust_scale, 0,
                            *info.y - info.length_adjust_scale * *info.y);
      }
    }
    item.item.ConvertToSVGText(unscaled_rect, scaled_rect, transform);
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

    info.inline_size = horizontal_ ? item.Size().width : item.Size().height;
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

struct SVGTextLengthContext {
  DISALLOW_NEW();
  wtf_size_t start_index;
  Member<const LayoutObject> layout_object;
  float text_length;
  SVGLengthAdjustType length_adjust;

  void Trace(Visitor* visitor) const { visitor->Trace(layout_object); }
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::SVGTextLengthContext)

namespace blink {

void NGSVGTextLayoutAlgorithm::ApplyTextLengthAttribute(
    const NGFragmentItemsBuilder::ItemWithOffsetList& items) {
  // TODO(crbug.com/1179585): Traversing LayoutObject ancestors in a layout
  // algorithm is not preferable. We should consider moving this part to
  // NGSVGTextLayoutAttributesBuilder.

  VectorOf<SVGTextLengthContext> context_stack;
  // Start indexes of the highest textLength elements which were already
  // handled by ResolveTextLength().
  Vector<wtf_size_t> resolved_descendant_node_starts;
  const LayoutObject* last_parent = nullptr;
  for (wtf_size_t index = 0; index < result_.size(); ++index) {
    const LayoutObject* layout_object =
        items[result_[index].item_index]->GetLayoutObject();
    DCHECK(IsA<LayoutText>(layout_object));
    if (last_parent == layout_object->Parent())
      continue;
    last_parent = layout_object->Parent();
    HeapVector<SVGTextLengthContext> text_length_ancestors =
        CollectTextLengthAncestors(items, index, layout_object);

    // Find a common part of context_stack and text_length_ancestors.
    wtf_size_t common_size = 0;
    for (const auto& ancestor : text_length_ancestors) {
      if (common_size >= context_stack.size())
        break;
      if (context_stack[common_size].layout_object != ancestor.layout_object)
        break;
      ++common_size;
    }
    // Pop uncommon part of context_stack.
    while (context_stack.size() > common_size) {
      ResolveTextLength(items, context_stack.back(), index,
                        resolved_descendant_node_starts);
      context_stack.pop_back();
    }
    // Push uncommon part of text_length_ancestors.
    for (wtf_size_t p = common_size; p < text_length_ancestors.size(); ++p) {
      SVGTextLengthContext context = text_length_ancestors[p];
      context.start_index = index;
      context_stack.push_back(context);
    }
  }
  while (!context_stack.IsEmpty()) {
    ResolveTextLength(items, context_stack.back(), result_.size(),
                      resolved_descendant_node_starts);
    context_stack.pop_back();
  }
}

// Collects ancestors with a valid textLength attribute up until the IFC.
// The result is a list of pairs of scaled textLength value and LayoutObject
// in the reversed order of distance from the specified |layout_object|.
HeapVector<SVGTextLengthContext>
NGSVGTextLayoutAlgorithm::CollectTextLengthAncestors(
    const NGFragmentItemsBuilder::ItemWithOffsetList& items,
    wtf_size_t index,
    const LayoutObject* layout_object) const {
  DCHECK(layout_object);
  VectorOf<SVGTextLengthContext> ancestors;
  do {
    layout_object = layout_object->Parent();
    if (auto* element =
            DynamicTo<SVGTextContentElement>(layout_object->GetNode())) {
      if (element->TextLengthIsSpecifiedByUser()) {
        float text_length = element->textLength()->CurrentValue()->Value(
            SVGLengthContext(element));
        // text_length is 0.0 if the textLength attribute has an invalid
        // string. Legacy SVG <text> skips textLength processing if the
        // attribute is "0" or invalid. Firefox skips textLength processing if
        // textLength value is smaller than the intrinsic width of the text.
        // This code follows the legacy behavior.
        if (text_length > 0.0f) {
          ancestors.push_back(SVGTextLengthContext{
              WTF::kNotFound, layout_object,
              text_length * ScalingFactorAt(items, index),
              element->lengthAdjust()->CurrentEnumValue()});
        }
      }
    }
  } while (layout_object != inline_node_.GetLayoutBlockFlow());
  ancestors.Reverse();
  return ancestors;
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
void NGSVGTextLayoutAlgorithm::ResolveTextLength(
    const NGFragmentItemsBuilder::ItemWithOffsetList& items,
    const SVGTextLengthContext& length_context,
    wtf_size_t j_plus_1,
    Vector<wtf_size_t>& resolved_descendant_node_starts) {
  const wtf_size_t i = length_context.start_index;

  // 2.1. Let a = +Infinity and b = −Infinity.
  float min_position = std::numeric_limits<float>::infinity();
  float max_position = -std::numeric_limits<float>::infinity();

  // 2.2. Let i and j be the global index of the first character and last
  // characters in node, respectively.
  // ==> They are computed in ApplyTextLengthAttribute().

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
  if (min_position == std::numeric_limits<float>::infinity())
    return;
  // 2.4.1. Find the distance delta = ‘textLength’ computed value − (b − a).
  const float delta =
      length_context.text_length - (max_position - min_position);

  float shift;
  if (length_context.length_adjust == kSVGLengthAdjustSpacingAndGlyphs) {
    float length_adjust_scale =
        length_context.text_length / (max_position - min_position);
    for (wtf_size_t k = i; k < j_plus_1; ++k) {
      NGSVGPerCharacterInfo& info = result_[k];
      if (horizontal_)
        *info.x = min_position + (*info.x - min_position) * length_adjust_scale;
      else
        *info.y = min_position + (*info.y - min_position) * length_adjust_scale;
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
    auto n = std::count_if(result_.begin() + i, result_.begin() + j_plus_1,
                           [](const auto& info) {
                             return !info.middle && !info.text_length_resolved;
                           });
    // 2.4.3. Let n = n + number of resolved descendant nodes − 1.
    n += std::count_if(resolved_descendant_node_starts.begin(),
                       resolved_descendant_node_starts.end(),
                       [i, j_plus_1](const auto& start_index) {
                         return i <= start_index && start_index < j_plus_1;
                       }) -
         1;
    // 2.4.4. Find the per-character adjustment small-delta = delta/n.
    float character_delta = n != 0 ? delta / n : delta;
    // 2.4.5. Let shift = 0.
    shift = 0.0f;
    // 2.4.6. For each index k in the range [i,j]:
    for (wtf_size_t k = i; k < j_plus_1; ++k) {
      NGSVGPerCharacterInfo& info = result_[k];
      // 2.4.6.1. Add shift to the x coordinate of the position in result[k], if
      // the "horizontal" flag is true, and to the y coordinate otherwise.
      if (horizontal_)
        *info.x += shift;
      else
        *info.y += shift;
      // 2.4.6.2. If the "middle" flag for result[k] is not true and k is not a
      // character in a resolved descendant node other than the first character
      // then shift = shift + small-delta.
      if (!info.middle &&
          (std::any_of(resolved_descendant_node_starts.begin(),
                       resolved_descendant_node_starts.end(),
                       [k](auto offset) { return offset == k; }) ||
           !info.text_length_resolved))
        shift += character_delta;
      info.text_length_resolved = true;
    }
  }
  // We should shift characters until the end of this text chunk.
  // Note: This is not defined by the algorithm. But it seems major SVG
  // engines work so.
  for (wtf_size_t k = j_plus_1; k < result_.size(); ++k) {
    if (result_[k].anchored_chunk)
      break;
    if (horizontal_)
      *result_[k].x += shift;
    else
      *result_[k].y += shift;
  }

  // Remove resolved_descendant_node_starts entries for descendant nodes,
  // and register an entry for this node.
  auto* new_end =
      std::remove_if(resolved_descendant_node_starts.begin(),
                     resolved_descendant_node_starts.end(),
                     [i, j_plus_1](const auto& start_index) {
                       return i <= start_index && start_index < j_plus_1;
                     });
  resolved_descendant_node_starts.erase(new_end,
                                        resolved_descendant_node_starts.end());
  resolved_descendant_node_starts.push_back(i);
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

void NGSVGTextLayoutAlgorithm::ApplyAnchoring(
    const NGFragmentItemsBuilder::ItemWithOffsetList& items) {
  DCHECK_GT(result_.size(), 0u);
  DCHECK(result_[0].anchored_chunk);
  // 1. For each slice result[i..j] (inclusive of both i and j), where:
  //  * the "anchored chunk" flag of result[i] is true,
  //  * the "anchored chunk" flags of result[k] where i < k ≤ j are false, and
  //  * j = count − 1 or the "anchored chunk" flag of result[j + 1] is true;
  wtf_size_t i = 0;
  while (i < result_.size()) {
    auto* next_anchor =
        std::find_if(result_.begin() + i + 1, result_.end(),
                     [](const auto& info) { return info.anchored_chunk; });
    wtf_size_t j = std::distance(result_.begin(), next_anchor) - 1;

    // 1.1. Let a = +Infinity and b = −Infinity.
    // ==> 'a' is left/top of characters. 'b' is right/top of characters.
    float a = std::numeric_limits<float>::infinity();
    float b = -std::numeric_limits<float>::infinity();
    // 1.2. For each index k in the range [i, j] where the "addressable" flag
    // of result[k] is true:
    for (wtf_size_t k = i; k <= j; ++k) {
      // 1.2.1. Let pos = the x coordinate of the position in result[k], if
      // the "horizontal" flag is true, and the y coordinate otherwise.
      float pos = horizontal_ ? *result_[k].x : *result_[k].y;
      // 2.2.2. Let advance = the advance of the typographic character
      // corresponding to character k.
      float advance = result_[k].inline_size;
      // 2.2.3. Set a = min(a, pos, pos + advance).
      a = std::min(a, pos);
      // 2.2.4. Set b = max(b, pos, pos + advance).
      b = std::max(b, pos + advance);
    }

    // 1.3. if a != +Infinity, then:
    if (a != std::numeric_limits<float>::infinity()) {
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
          NOTREACHED();
          FALLTHROUGH;
        case ETextAnchor::kStart:
          shift = is_ltr ? shift - a : shift - b;
          break;
        case ETextAnchor::kEnd:
          shift = is_ltr ? shift - b : shift - a;
          break;
        case ETextAnchor::kMiddle:
          shift = shift - (a + b) / 2;
          break;
      }

      // 1.3.3. For each index k in the range [i, j]:
      for (wtf_size_t k = i; k <= j; ++k) {
        // 1.3.3.1. Add shift to the x coordinate of the position in result[k],
        // if the "horizontal" flag is true, and to the y coordinate otherwise.
        if (horizontal_)
          *result_[k].x += shift;
        else
          *result_[k].y += shift;
      }
    }
    i = j + 1;
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
