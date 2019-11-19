// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"

#include <algorithm>
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/platform/text/character.h"

namespace blink {

namespace {

// Note: LayoutFlowThread, used for multicol, can't provide offset mapping.
bool CanUseNGOffsetMapping(const LayoutObject& object) {
  return object.IsLayoutBlockFlow() && !object.IsLayoutFlowThread();
}

Position CreatePositionForOffsetMapping(const Node& node, unsigned dom_offset) {
  if (auto* text_node = DynamicTo<Text>(node)) {
    // 'text-transform' may make the rendered text length longer than the
    // original text node, in which case we clamp the offset to avoid crashing.
    // TODO(crbug.com/750990): Support 'text-transform' to remove this hack.
#if DCHECK_IS_ON()
    // Ensures that the clamping hack kicks in only with text-transform.
    if (node.ComputedStyleRef().TextTransform() == ETextTransform::kNone)
      DCHECK_LE(dom_offset, text_node->length());
#endif
    const unsigned clamped_offset = std::min(dom_offset, text_node->length());
    return Position(&node, clamped_offset);
  }
  // For non-text-anchored position, the offset must be either 0 or 1.
  DCHECK_LE(dom_offset, 1u);
  return dom_offset ? Position::AfterNode(node) : Position::BeforeNode(node);
}

std::pair<const Node&, unsigned> ToNodeOffsetPair(const Position& position) {
  DCHECK(NGOffsetMapping::AcceptsPosition(position)) << position;
  if (auto* text_node = DynamicTo<Text>(position.AnchorNode())) {
    if (position.IsOffsetInAnchor())
      return {*position.AnchorNode(), position.OffsetInContainerNode()};
    if (position.IsBeforeAnchor())
      return {*position.AnchorNode(), 0};
    DCHECK(position.IsAfterAnchor());
    return {*position.AnchorNode(), text_node->length()};
  }
  if (position.IsBeforeAnchor())
    return {*position.AnchorNode(), 0};
  return {*position.AnchorNode(), 1};
}

// TODO(xiaochengh): Introduce predicates for comparing Position and
// NGOffsetMappingUnit, to reduce position-offset conversion and ad-hoc
// predicates below.

}  // namespace

LayoutBlockFlow* NGInlineFormattingContextOf(const Position& position) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return nullptr;
  LayoutBlockFlow* block_flow =
      NGOffsetMapping::GetInlineFormattingContextOf(position);
  if (!block_flow || !block_flow->IsLayoutNGMixin())
    return nullptr;
  return block_flow;
}

// static
LayoutBlockFlow* NGOffsetMapping::GetInlineFormattingContextOf(
    const Position& position) {
  if (!AcceptsPosition(position))
    return nullptr;
  const auto node_offset_pair = ToNodeOffsetPair(position);
  const LayoutObject* layout_object =
      AssociatedLayoutObjectOf(node_offset_pair.first, node_offset_pair.second);
  if (!layout_object)
    return nullptr;
  return GetInlineFormattingContextOf(*layout_object);
}

NGOffsetMappingUnit::NGOffsetMappingUnit(NGOffsetMappingUnitType type,
                                         const LayoutObject& layout_object,
                                         unsigned dom_start,
                                         unsigned dom_end,
                                         unsigned text_content_start,
                                         unsigned text_content_end)
    : type_(type),
      layout_object_(&layout_object),
      dom_start_(dom_start),
      dom_end_(dom_end),
      text_content_start_(text_content_start),
      text_content_end_(text_content_end) {
  AssertValid();
}

void NGOffsetMappingUnit::AssertValid() const {
#if ENABLE_SECURITY_ASSERT
  SECURITY_DCHECK(dom_start_ <= dom_end_) << dom_start_ << " vs. " << dom_end_;
  SECURITY_DCHECK(text_content_start_ <= text_content_end_)
      << text_content_start_ << " vs. " << text_content_end_;
  if (layout_object_->IsText() &&
      !ToLayoutText(*layout_object_).IsWordBreak()) {
    const LayoutText& layout_text = ToLayoutText(*layout_object_);
    const unsigned text_start =
        AssociatedNode() ? layout_text.TextStartOffset() : 0;
    const unsigned text_end = text_start + layout_text.TextLength();
    SECURITY_DCHECK(dom_end_ >= text_start)
        << dom_end_ << " vs. " << text_start;
    SECURITY_DCHECK(dom_end_ <= text_end) << dom_end_ << " vs. " << text_end;
  } else {
    SECURITY_DCHECK(dom_start_ == 0) << dom_start_;
    SECURITY_DCHECK(dom_end_ == 1) << dom_end_;
  }
#endif
}

NGOffsetMappingUnit::~NGOffsetMappingUnit() = default;

const Node* NGOffsetMappingUnit::AssociatedNode() const {
  if (const auto* text_fragment = ToLayoutTextFragmentOrNull(layout_object_))
    return text_fragment->AssociatedTextNode();
  return layout_object_->GetNode();
}

const Node& NGOffsetMappingUnit::GetOwner() const {
  const Node* const node = AssociatedNode();
  DCHECK(node) << layout_object_;
  return *node;
}

bool NGOffsetMappingUnit::Concatenate(const NGOffsetMappingUnit& other) {
  if (layout_object_ != other.layout_object_)
    return false;
  if (type_ != other.type_ || type_ == NGOffsetMappingUnitType::kExpanded)
    return false;
  if (dom_end_ != other.dom_start_)
    return false;
  if (text_content_end_ != other.text_content_start_)
    return false;
  // Don't merge first letter and remaining text
  if (const LayoutTextFragment* text_fragment =
          ToLayoutTextFragmentOrNull(layout_object_)) {
    // TODO(layout-dev): Fix offset calculation for text-transform
    if (text_fragment->IsRemainingTextLayoutObject() &&
        other.dom_start_ == text_fragment->TextStartOffset())
      return false;
  }
  dom_end_ = other.dom_end_;
  text_content_end_ = other.text_content_end_;
  return true;
}

unsigned NGOffsetMappingUnit::ConvertDOMOffsetToTextContent(
    unsigned offset) const {
  DCHECK_GE(offset, dom_start_);
  DCHECK_LE(offset, dom_end_);
  // DOM start is always mapped to text content start.
  if (offset == dom_start_)
    return text_content_start_;
  // DOM end is always mapped to text content end.
  if (offset == dom_end_)
    return text_content_end_;
  // Handle collapsed mapping.
  if (text_content_start_ == text_content_end_)
    return text_content_start_;
  // Handle has identity mapping.
  return offset - dom_start_ + text_content_start_;
}

unsigned NGOffsetMappingUnit::ConvertTextContentToFirstDOMOffset(
    unsigned offset) const {
  DCHECK_GE(offset, text_content_start_);
  DCHECK_LE(offset, text_content_end_);
  // Always return DOM start for collapsed units.
  if (text_content_start_ == text_content_end_)
    return dom_start_;
  // Handle identity mapping.
  if (type_ == NGOffsetMappingUnitType::kIdentity)
    return dom_start_ + offset - text_content_start_;
  // Handle expanded mapping.
  return offset < text_content_end_ ? dom_start_ : dom_end_;
}

unsigned NGOffsetMappingUnit::ConvertTextContentToLastDOMOffset(
    unsigned offset) const {
  DCHECK_GE(offset, text_content_start_);
  DCHECK_LE(offset, text_content_end_);
  // Always return DOM end for collapsed units.
  if (text_content_start_ == text_content_end_)
    return dom_end_;
  // In a non-collapsed unit, mapping between DOM and text content offsets is
  // one-to-one. Reuse existing code.
  return ConvertTextContentToFirstDOMOffset(offset);
}

// static
bool NGOffsetMapping::AcceptsPosition(const Position& position) {
  if (position.IsNull())
    return false;
  if (position.AnchorNode()->IsTextNode()) {
    // Position constructor should have rejected other anchor types.
    DCHECK(position.IsOffsetInAnchor() || position.IsBeforeAnchor() ||
           position.IsAfterAnchor());
    return true;
  }
  if (!position.IsBeforeAnchor() && !position.IsAfterAnchor())
    return false;
  const LayoutObject* layout_object = position.AnchorNode()->GetLayoutObject();
  if (!layout_object || !layout_object->IsInline())
    return false;
  return layout_object->IsText() || layout_object->IsAtomicInlineLevel();
}

// static
const NGOffsetMapping* NGOffsetMapping::GetFor(const Position& position) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return nullptr;
  if (!NGOffsetMapping::AcceptsPosition(position))
    return nullptr;
  LayoutBlockFlow* context = NGInlineFormattingContextOf(position);
  if (!context)
    return nullptr;
  return NGInlineNode::GetOffsetMapping(context);
}

// static
const NGOffsetMapping* NGOffsetMapping::GetFor(
    const LayoutObject* layout_object) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return nullptr;
  if (!layout_object)
    return nullptr;
  LayoutBlockFlow* context = layout_object->ContainingNGBlockFlow();
  if (!context)
    return nullptr;
  return NGInlineNode::GetOffsetMapping(context);
}

// static
LayoutBlockFlow* NGOffsetMapping::GetInlineFormattingContextOf(
    const LayoutObject& object) {
  for (LayoutObject* runner = object.Parent(); runner;
       runner = runner->Parent()) {
    if (!CanUseNGOffsetMapping(*runner))
      continue;
    return To<LayoutBlockFlow>(runner);
  }
  return nullptr;
}

NGOffsetMapping::NGOffsetMapping(UnitVector&& units,
                                 RangeMap&& ranges,
                                 String text)
    : units_(std::move(units)), ranges_(std::move(ranges)), text_(text) {
#if ENABLE_SECURITY_ASSERT
  for (const auto& unit : units_) {
    SECURITY_DCHECK(unit.TextContentStart() <= text.length())
        << unit.TextContentStart() << "<=" << text.length();
    SECURITY_DCHECK(unit.TextContentEnd() <= text.length())
        << unit.TextContentEnd() << "<=" << text.length();
    unit.AssertValid();
  }
  for (const auto& pair : ranges) {
    SECURITY_DCHECK(pair.value.first < units_.size())
        << pair.value.first << "<" << units_.size();
    SECURITY_DCHECK(pair.value.second < units_.size())
        << pair.value.second << "<" << units_.size();
  }
#endif
}

NGOffsetMapping::~NGOffsetMapping() = default;

const NGOffsetMappingUnit* NGOffsetMapping::GetMappingUnitForPosition(
    const Position& position) const {
  DCHECK(NGOffsetMapping::AcceptsPosition(position));
  const auto node_and_offset = ToNodeOffsetPair(position);
  const Node& node = node_and_offset.first;
  const unsigned offset = node_and_offset.second;
  unsigned range_start;
  unsigned range_end;
  std::tie(range_start, range_end) = ranges_.at(&node);
  if (range_start == range_end || units_[range_start].DOMStart() > offset)
    return nullptr;
  // Find the last unit where unit.dom_start <= offset
  const NGOffsetMappingUnit* unit = std::prev(std::upper_bound(
      units_.begin() + range_start, units_.begin() + range_end, offset,
      [](unsigned offset, const NGOffsetMappingUnit& unit) {
        return offset < unit.DOMStart();
      }));
  if (unit->DOMEnd() < offset)
    return nullptr;
  return unit;
}

NGOffsetMapping::UnitVector NGOffsetMapping::GetMappingUnitsForDOMRange(
    const EphemeralRange& range) const {
  DCHECK(NGOffsetMapping::AcceptsPosition(range.StartPosition()));
  DCHECK(NGOffsetMapping::AcceptsPosition(range.EndPosition()));
  DCHECK_EQ(range.StartPosition().AnchorNode(),
            range.EndPosition().AnchorNode());
  const Node& node = *range.StartPosition().AnchorNode();
  const unsigned start_offset = ToNodeOffsetPair(range.StartPosition()).second;
  const unsigned end_offset = ToNodeOffsetPair(range.EndPosition()).second;
  unsigned range_start;
  unsigned range_end;
  std::tie(range_start, range_end) = ranges_.at(&node);

  if (range_start == range_end || units_[range_start].DOMStart() > end_offset ||
      units_[range_end - 1].DOMEnd() < start_offset)
    return UnitVector();

  // Find the first unit where unit.dom_end >= start_offset
  const NGOffsetMappingUnit* result_begin = std::lower_bound(
      units_.begin() + range_start, units_.begin() + range_end, start_offset,
      [](const NGOffsetMappingUnit& unit, unsigned offset) {
        return unit.DOMEnd() < offset;
      });

  // Find the next of the last unit where unit.dom_start <= end_offset
  const NGOffsetMappingUnit* result_end =
      std::upper_bound(result_begin, units_.begin() + range_end, end_offset,
                       [](unsigned offset, const NGOffsetMappingUnit& unit) {
                         return offset < unit.DOMStart();
                       });

  UnitVector result;
  for (const auto& unit : base::make_span(result_begin, result_end)) {
    // If the unit isn't fully within the range, create a new unit that's
    // within the range.
    const unsigned clamped_start = std::max(unit.DOMStart(), start_offset);
    const unsigned clamped_end = std::min(unit.DOMEnd(), end_offset);
    DCHECK_LE(clamped_start, clamped_end);
    const unsigned clamped_text_content_start =
        unit.ConvertDOMOffsetToTextContent(clamped_start);
    const unsigned clamped_text_content_end =
        unit.ConvertDOMOffsetToTextContent(clamped_end);
    result.emplace_back(unit.GetType(), unit.GetLayoutObject(), clamped_start,
                        clamped_end, clamped_text_content_start,
                        clamped_text_content_end);
  }
  return result;
}

base::span<const NGOffsetMappingUnit> NGOffsetMapping::GetMappingUnitsForNode(
    const Node& node) const {
  const auto it = ranges_.find(&node);
  if (it == ranges_.end()) {
    NOTREACHED() << node;
    return {};
  }
  return base::make_span(units_.begin() + it->value.first,
                         units_.begin() + it->value.second);
}

base::span<const NGOffsetMappingUnit>
NGOffsetMapping::GetMappingUnitsForLayoutObject(
    const LayoutObject& layout_object) const {
  const auto* begin =
      std::find_if(units_.begin(), units_.end(),
                   [&layout_object](const NGOffsetMappingUnit& unit) {
                     return unit.GetLayoutObject() == layout_object;
                   });
  DCHECK_NE(begin, units_.end());
  const auto* end =
      std::find_if(std::next(begin), units_.end(),
                   [&layout_object](const NGOffsetMappingUnit& unit) {
                     return unit.GetLayoutObject() != layout_object;
                   });
  DCHECK_LT(begin, end);
  return base::make_span(begin, end);
}

base::span<const NGOffsetMappingUnit>
NGOffsetMapping::GetMappingUnitsForTextContentOffsetRange(unsigned start,
                                                          unsigned end) const {
  DCHECK_LE(start, end);
  if (units_.front().TextContentStart() >= end ||
      units_.back().TextContentEnd() <= start)
    return {};

  // Find the first unit where unit.text_content_end > start
  const NGOffsetMappingUnit* result_begin =
      std::lower_bound(units_.begin(), units_.end(), start,
                       [](const NGOffsetMappingUnit& unit, unsigned offset) {
                         return unit.TextContentEnd() <= offset;
                       });
  if (result_begin == units_.end() || result_begin->TextContentStart() >= end)
    return {};

  // Find the next of the last unit where unit.text_content_start < end
  const NGOffsetMappingUnit* result_end =
      std::upper_bound(units_.begin(), units_.end(), end,
                       [](unsigned offset, const NGOffsetMappingUnit& unit) {
                         return offset <= unit.TextContentStart();
                       });
  return base::make_span(result_begin, result_end);
}

base::Optional<unsigned> NGOffsetMapping::GetTextContentOffset(
    const Position& position) const {
  DCHECK(NGOffsetMapping::AcceptsPosition(position)) << position;
  const NGOffsetMappingUnit* unit = GetMappingUnitForPosition(position);
  if (!unit)
    return base::nullopt;
  return unit->ConvertDOMOffsetToTextContent(ToNodeOffsetPair(position).second);
}

Position NGOffsetMapping::StartOfNextNonCollapsedContent(
    const Position& position) const {
  DCHECK(NGOffsetMapping::AcceptsPosition(position)) << position;
  const NGOffsetMappingUnit* unit = GetMappingUnitForPosition(position);
  if (!unit)
    return Position();

  const auto node_and_offset = ToNodeOffsetPair(position);
  const Node& node = node_and_offset.first;
  const unsigned offset = node_and_offset.second;
  while (unit != units_.end() && unit->AssociatedNode() == node) {
    if (unit->DOMEnd() > offset &&
        unit->GetType() != NGOffsetMappingUnitType::kCollapsed) {
      const unsigned result = std::max(offset, unit->DOMStart());
      return CreatePositionForOffsetMapping(node, result);
    }
    ++unit;
  }
  return Position();
}

Position NGOffsetMapping::EndOfLastNonCollapsedContent(
    const Position& position) const {
  DCHECK(NGOffsetMapping::AcceptsPosition(position)) << position;
  const NGOffsetMappingUnit* unit = GetMappingUnitForPosition(position);
  if (!unit)
    return Position();

  const auto node_and_offset = ToNodeOffsetPair(position);
  const Node& node = node_and_offset.first;
  const unsigned offset = node_and_offset.second;
  while (unit->AssociatedNode() == node) {
    if (unit->DOMStart() < offset &&
        unit->GetType() != NGOffsetMappingUnitType::kCollapsed) {
      const unsigned result = std::min(offset, unit->DOMEnd());
      return CreatePositionForOffsetMapping(node, result);
    }
    if (unit == units_.begin())
      break;
    --unit;
  }
  return Position();
}

bool NGOffsetMapping::IsBeforeNonCollapsedContent(
    const Position& position) const {
  DCHECK(NGOffsetMapping::AcceptsPosition(position));
  const NGOffsetMappingUnit* unit = GetMappingUnitForPosition(position);
  const unsigned offset = ToNodeOffsetPair(position).second;
  return unit && offset < unit->DOMEnd() &&
         unit->GetType() != NGOffsetMappingUnitType::kCollapsed;
}

bool NGOffsetMapping::IsAfterNonCollapsedContent(
    const Position& position) const {
  DCHECK(NGOffsetMapping::AcceptsPosition(position));
  const auto node_and_offset = ToNodeOffsetPair(position);
  const Node& node = node_and_offset.first;
  const unsigned offset = node_and_offset.second;
  if (!offset)
    return false;
  // In case we have one unit ending at |offset| and another starting at
  // |offset|, we need to find the former. Hence, search with |offset - 1|.
  const NGOffsetMappingUnit* unit = GetMappingUnitForPosition(
      CreatePositionForOffsetMapping(node, offset - 1));
  return unit && offset > unit->DOMStart() &&
         unit->GetType() != NGOffsetMappingUnitType::kCollapsed;
}

base::Optional<UChar> NGOffsetMapping::GetCharacterBefore(
    const Position& position) const {
  DCHECK(NGOffsetMapping::AcceptsPosition(position));
  base::Optional<unsigned> text_content_offset = GetTextContentOffset(position);
  if (!text_content_offset || !*text_content_offset)
    return base::nullopt;
  return text_[*text_content_offset - 1];
}

Position NGOffsetMapping::GetFirstPosition(unsigned offset) const {
  // Find the first unit where |unit.TextContentEnd() >= offset|
  if (units_.IsEmpty() || units_.back().TextContentEnd() < offset)
    return {};
  const NGOffsetMappingUnit* result =
      std::lower_bound(units_.begin(), units_.end(), offset,
                       [](const NGOffsetMappingUnit& unit, unsigned offset) {
                         return unit.TextContentEnd() < offset;
                       });
  DCHECK_NE(result, units_.end());
  // Skip CSS generated content, e.g. "content" property in ::before/::after.
  while (!result->AssociatedNode()) {
    result = std::next(result);
    if (result == units_.end() || result->TextContentStart() > offset)
      return {};
  }
  const Node& node = result->GetOwner();
  const unsigned dom_offset =
      result->ConvertTextContentToFirstDOMOffset(offset);
  return CreatePositionForOffsetMapping(node, dom_offset);
}

const NGOffsetMappingUnit* NGOffsetMapping::GetLastMappingUnit(
    unsigned offset) const {
  // Find the last unit where |unit.TextContentStart() <= offset|
  if (units_.IsEmpty() || units_.front().TextContentStart() > offset)
    return nullptr;
  const NGOffsetMappingUnit* result =
      std::upper_bound(units_.begin(), units_.end(), offset,
                       [](unsigned offset, const NGOffsetMappingUnit& unit) {
                         return offset < unit.TextContentStart();
                       });
  DCHECK_NE(result, units_.begin());
  result = std::prev(result);
  if (result->TextContentEnd() < offset)
    return nullptr;
  return result;
}

Position NGOffsetMapping::GetLastPosition(unsigned offset) const {
  const NGOffsetMappingUnit* result = GetLastMappingUnit(offset);
  if (!result)
    return {};
  // Skip CSS generated content, e.g. "content" property in ::before/::after.
  while (!result->AssociatedNode()) {
    if (result == units_.begin())
      return {};
    result = std::prev(result);
    if (result->TextContentEnd() < offset)
      return {};
  }
  const Node& node = result->GetOwner();
  const unsigned dom_offset = result->ConvertTextContentToLastDOMOffset(offset);
  return CreatePositionForOffsetMapping(node, dom_offset);
}

bool NGOffsetMapping::HasBidiControlCharactersOnly(unsigned start,
                                                   unsigned end) const {
  DCHECK_LE(start, end);
  DCHECK_LE(end, text_.length());
  for (unsigned i = start; i < end; ++i) {
    if (!Character::IsBidiControl(text_[i]))
      return false;
  }
  return true;
}

}  // namespace blink
