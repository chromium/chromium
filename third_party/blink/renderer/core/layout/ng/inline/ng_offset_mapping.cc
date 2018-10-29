// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/platform/text/character.h"

namespace blink {

namespace {

// Returns true if |node| has style 'display:inline' and can have descendants
// in the inline layout.
bool IsNonAtomicInline(const Node& node) {
  const LayoutObject* layout_object = node.GetLayoutObject();
  return layout_object && layout_object->IsInline() &&
         !layout_object->IsText() && !layout_object->IsAtomicInlineLevel();
}

Position CreatePositionForOffsetMapping(const Node& node, unsigned dom_offset) {
  if (node.IsTextNode())
    return Position(&node, dom_offset);
  // For non-text-anchored position, the offset must be either 0 or 1.
  DCHECK_LE(dom_offset, 1u);
  return dom_offset ? Position::AfterNode(node) : Position::BeforeNode(node);
}

std::pair<const Node&, unsigned> ToNodeOffsetPair(const Position& position) {
  DCHECK(NGOffsetMapping::AcceptsPosition(position)) << position;
  if (position.AnchorNode()->IsTextNode()) {
    if (position.IsOffsetInAnchor())
      return {*position.AnchorNode(), position.OffsetInContainerNode()};
    if (position.IsBeforeAnchor())
      return {*position.AnchorNode(), 0};
    DCHECK(position.IsAfterAnchor());
    return {*position.AnchorNode(), ToText(position.AnchorNode())->length()};
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
  if (!NGOffsetMapping::AcceptsPosition(position))
    return nullptr;
  const auto node_offset_pair = ToNodeOffsetPair(position);
  const LayoutObject* layout_object =
      AssociatedLayoutObjectOf(node_offset_pair.first, node_offset_pair.second);
  return layout_object->ContainingNGBlockFlow();
}

NGOffsetMappingUnit::NGOffsetMappingUnit(NGOffsetMappingUnitType type,
                                         const Node& node,
                                         unsigned dom_start,
                                         unsigned dom_end,
                                         unsigned text_content_start,
                                         unsigned text_content_end)
    : type_(type),
      owner_(&node),
      dom_start_(dom_start),
      dom_end_(dom_end),
      text_content_start_(text_content_start),
      text_content_end_(text_content_end) {}

NGOffsetMappingUnit::~NGOffsetMappingUnit() = default;

bool NGOffsetMappingUnit::Concatenate(const NGOffsetMappingUnit& other) {
  if (owner_ != other.owner_)
    return false;
  if (type_ != other.type_ || type_ == NGOffsetMappingUnitType::kExpanded)
    return false;
  if (dom_end_ != other.dom_start_)
    return false;
  if (text_content_end_ != other.text_content_start_)
    return false;
  // Don't merge first letter and remaining text
  if (const LayoutTextFragment* text_fragment =
          ToLayoutTextFragmentOrNull(owner_->GetLayoutObject())) {
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
  return layout_object && layout_object->IsInline();
}

// static
const NGOffsetMapping* NGOffsetMapping::GetFor(const Position& position) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return nullptr;
  if (!NGOffsetMapping::AcceptsPosition(position))
    return nullptr;
  return GetForContainingBlockFlow(NGInlineFormattingContextOf(position));
}

// static
const NGOffsetMapping* NGOffsetMapping::GetFor(
    const LayoutObject* layout_object) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return nullptr;
  if (!layout_object)
    return nullptr;
  return GetForContainingBlockFlow(layout_object->ContainingNGBlockFlow());
}

// static
const NGOffsetMapping* NGOffsetMapping::GetForContainingBlockFlow(
    LayoutBlockFlow* block_flow) {
  if (!block_flow || !block_flow->ChildrenInline())
    return nullptr;
  NGBlockNode block_node = NGBlockNode(block_flow);
  if (!block_node.CanUseNewLayout())
    return nullptr;
  NGLayoutInputNode node = block_node.FirstChild();
  if (node && node.IsInline())
    return ToNGInlineNode(node).ComputeOffsetMappingIfNeeded();
  return nullptr;
}

NGOffsetMapping::NGOffsetMapping(NGOffsetMapping&& other)
    : NGOffsetMapping(std::move(other.units_),
                      std::move(other.ranges_),
                      other.text_) {}

NGOffsetMapping::NGOffsetMapping(UnitVector&& units,
                                 RangeMap&& ranges,
                                 String text)
    : units_(std::move(units)), ranges_(std::move(ranges)), text_(text) {}

NGOffsetMapping::~NGOffsetMapping() = default;

const NGOffsetMappingUnit* NGOffsetMapping::GetMappingUnitForPosition(
    const Position& position) const {
  DCHECK(NGOffsetMapping::AcceptsPosition(position));
  DCHECK(!IsNonAtomicInline(*position.AnchorNode())) << position;
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

NGMappingUnitRange NGOffsetMapping::GetMappingUnitsForDOMRange(
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

  if (IsNonAtomicInline(node)) {
    if (start_offset == end_offset)
      return {};
    return {units_.begin() + range_start, units_.begin() + range_end};
  }

  if (range_start == range_end || units_[range_start].DOMStart() > end_offset ||
      units_[range_end - 1].DOMEnd() < start_offset)
    return {};

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

  return {result_begin, result_end};
}

NGMappingUnitRange NGOffsetMapping::GetMappingUnitsForTextContentOffsetRange(
    unsigned start,
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
  return {result_begin, result_end};
}

base::Optional<unsigned> NGOffsetMapping::GetTextContentOffset(
    const Position& position) const {
  DCHECK(NGOffsetMapping::AcceptsPosition(position)) << position;
  if (IsNonAtomicInline(*position.AnchorNode())) {
    auto iter = ranges_.find(position.AnchorNode());
    if (iter == ranges_.end())
      return base::nullopt;
    DCHECK_NE(iter->value.first, iter->value.second) << position;
    if (position.IsBeforeAnchor())
      return units_[iter->value.first].TextContentStart();
    return units_[iter->value.second - 1].TextContentEnd();
  }

  const NGOffsetMappingUnit* unit = GetMappingUnitForPosition(position);
  if (!unit)
    return base::nullopt;
  return unit->ConvertDOMOffsetToTextContent(ToNodeOffsetPair(position).second);
}

Position NGOffsetMapping::StartOfNextNonCollapsedContent(
    const Position& position) const {
  DCHECK(NGOffsetMapping::AcceptsPosition(position)) << position;
  DCHECK(!IsNonAtomicInline(*position.AnchorNode())) << position;
  const NGOffsetMappingUnit* unit = GetMappingUnitForPosition(position);
  if (!unit)
    return Position();

  const auto node_and_offset = ToNodeOffsetPair(position);
  const Node& node = node_and_offset.first;
  const unsigned offset = node_and_offset.second;
  while (unit != units_.end() && unit->GetOwner() == node) {
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
  DCHECK(!IsNonAtomicInline(*position.AnchorNode())) << position;
  const NGOffsetMappingUnit* unit = GetMappingUnitForPosition(position);
  if (!unit)
    return Position();

  const auto node_and_offset = ToNodeOffsetPair(position);
  const Node& node = node_and_offset.first;
  const unsigned offset = node_and_offset.second;
  while (unit->GetOwner() == node) {
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
  DCHECK(!IsNonAtomicInline(*position.AnchorNode())) << position;
  const NGOffsetMappingUnit* unit = GetMappingUnitForPosition(position);
  const unsigned offset = ToNodeOffsetPair(position).second;
  return unit && offset < unit->DOMEnd() &&
         unit->GetType() != NGOffsetMappingUnitType::kCollapsed;
}

bool NGOffsetMapping::IsAfterNonCollapsedContent(
    const Position& position) const {
  DCHECK(NGOffsetMapping::AcceptsPosition(position));
  DCHECK(!IsNonAtomicInline(*position.AnchorNode())) << position;
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
  DCHECK(!IsNonAtomicInline(*position.AnchorNode())) << position;
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
  if (result->TextContentStart() > offset)
    return {};
  const Node& node = result->GetOwner();
  const unsigned dom_offset =
      result->ConvertTextContentToFirstDOMOffset(offset);
  return CreatePositionForOffsetMapping(node, dom_offset);
}

Position NGOffsetMapping::GetLastPosition(unsigned offset) const {
  // Find the last unit where |unit.TextContentStart() <= offset|
  if (units_.IsEmpty() || units_.front().TextContentStart() > offset)
    return {};
  const NGOffsetMappingUnit* result =
      std::upper_bound(units_.begin(), units_.end(), offset,
                       [](unsigned offset, const NGOffsetMappingUnit& unit) {
                         return offset < unit.TextContentStart();
                       });
  DCHECK_NE(result, units_.begin());
  result = std::prev(result);
  if (result->TextContentEnd() < offset)
    return {};
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
