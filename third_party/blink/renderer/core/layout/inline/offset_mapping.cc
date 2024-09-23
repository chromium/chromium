// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"

#include <algorithm>

#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/platform/text/character.h"

namespace blink {

namespace {

// Note: LayoutFlowThread, used for multicol, can't provide offset mapping.
bool CanUseOffsetMapping(const LayoutObject& object) {
  return object.IsLayoutBlockFlow() && !object.IsLayoutFlowThread();
}

Position CreatePositionForOffsetMapping(const Node& node, unsigned dom_offset) {
  if (auto* text_node = DynamicTo<Text>(node)) {
    // 'text-transform' may make the rendered text length longer than the
    // original text node, in which case we clamp the offset to avoid crashing.
    // TODO(crbug.com/750990): Support 'text-transform' to remove this hack.
#if DCHECK_IS_ON()
    // Ensures that the clamping hack kicks in only with text-transform.
    if (node.GetLayoutObject()->StyleRef().TextTransform() ==
        ETextTransform::kNone) {
      DCHECK_LE(dom_offset, text_node->length());
    }
#endif
    const unsigned clamped_offset = std::min(dom_offset, text_node->length());
    return Position(&node, clamped_offset);
  }
  // For non-text-anchored position, the offset must be either 0 or 1.
  DCHECK_LE(dom_offset, 1u);
  return dom_offset ? Position::AfterNode(node) : Position::BeforeNode(node);
}

std::pair<const Node&, unsigned> ToNodeOffsetPair(const Position& position) {
  DCHECK(OffsetMapping::AcceptsPosition(position)) << position;
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
// OffsetMappingUnit, to reduce position-offset conversion and ad-hoc
// predicates below.

}  // namespace

LayoutBlockFlow* NGInlineFormattingContextOf(const Position& position) {
  LayoutBlockFlow* block_flow =
      OffsetMapping::GetInlineFormattingContextOf(position);
  if (!block_flow || !block_flow->IsLayoutNGObject())
    return nullptr;
  return block_flow;
}

// static
LayoutBlockFlow* OffsetMapping::GetInlineFormattingContextOf(
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

OffsetMappingUnit::OffsetMappingUnit(OffsetMappingUnitType type,
                                     const LayoutObject& layout_object,
                                     unsigned dom_start,
                                     unsigned dom_end,
                                     unsigned text_content_start,
                                     unsigned text_content_end)
    : type_(type),
      // Use atomic construction to allow for concurrently marking
      // OffsetMappingUnit.
      layout_object_(&layout_object,
                     Member<const LayoutObject>::AtomicInitializerTag{}),
      dom_start_(dom_start),
      dom_end_(dom_end),
      text_content_start_(text_content_start),
      text_content_end_(text_content_end) {
  AssertValid();
}

void OffsetMappingUnit::AssertValid() const {
#if ENABLE_SECURITY_ASSERT
  SECURITY_DCHECK(dom_start_ <= dom_end_) << dom_start_ << " vs. " << dom_end_;
  SECURITY_DCHECK(text_content_start_ <= text_content_end_)
      << text_content_start_ << " vs. " << text_content_end_;
  if (layout_object_->IsText() &&
      !To<LayoutText>(*layout_object_).IsWordBreak()) {
    const auto& layout_text = To<LayoutText>(*layout_object_);
    const unsigned text_start =
        AssociatedNode() ? layout_text.TextStartOffset() : 0;
    SECURITY_DCHECK(dom_end_ >= text_start)
        << dom_end_ << " vs. " << text_start;
  } else {
    SECURITY_DCHECK(dom_start_ == 0) << dom_start_;
    SECURITY_DCHECK(dom_end_ == 1) << dom_end_;
  }
#endif
}

const Node* OffsetMappingUnit::AssociatedNode() const {
  if (const auto* text_fragment =
          DynamicTo<LayoutTextFragment>(layout_object_.Get()))
    return text_fragment->AssociatedTextNode();
  return layout_object_->GetNode();
}

const Node& OffsetMappingUnit::GetOwner() const {
  const Node* const node = AssociatedNode();
  DCHECK(node) << layout_object_;
  return *node;
}

bool OffsetMappingUnit::Concatenate(const OffsetMappingUnit& other) {
  if (layout_object_ != other.layout_object_)
    return false;
  if (type_ != other.type_)
    return false;
  if (dom_end_ != other.dom_start_)
    return false;
  if (text_content_end_ != other.text_content_start_)
    return false;
  // Don't merge first letter and remaining text
  if (const auto* text_fragment =
          DynamicTo<LayoutTextFragment>(layout_object_.Get())) {
    // TODO(layout-dev): Fix offset calculation for text-transform
    if (text_fragment->IsRemainingTextLayoutObject() &&
        other.dom_start_ == text_fragment->TextStartOffset())
      return false;
  }
  dom_end_ = other.dom_end_;
  text_content_end_ = other.text_content_end_;
  return true;
}

unsigned OffsetMappingUnit::ConvertDOMOffsetToTextContent(
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

unsigned OffsetMappingUnit::ConvertTextContentToFirstDOMOffset(
    unsigned offset) const {
  DCHECK_GE(offset, text_content_start_);
  DCHECK_LE(offset, text_content_end_);
  // Always return DOM start for collapsed units.
  if (text_content_start_ == text_content_end_)
    return dom_start_;
  // Handle identity mapping.
  if (type_ == OffsetMappingUnitType::kIdentity) {
    return dom_start_ + offset - text_content_start_;
  }
  // Handle expanded mapping.
  return offset < text_content_end_ ? dom_start_ : dom_end_;
}

unsigned OffsetMappingUnit::ConvertTextContentToLastDOMOffset(
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
bool OffsetMapping::AcceptsPosition(const Position& position) {
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
const OffsetMapping* OffsetMapping::GetFor(const Position& position) {
  return ForceGetFor(position);
}

const OffsetMapping* OffsetMapping::ForceGetFor(const Position& position) {
  if (!OffsetMapping::AcceptsPosition(position)) {
    return nullptr;
  }
  LayoutBlockFlow* context =
      OffsetMapping::GetInlineFormattingContextOf(position);
  if (!context)
    return nullptr;
  return InlineNode::GetOffsetMapping(context);
}

// static
const OffsetMapping* OffsetMapping::GetFor(const LayoutObject* layout_object) {
  if (!layout_object)
    return nullptr;
  LayoutBlockFlow* context = layout_object->FragmentItemsContainer();
  if (!context)
    return nullptr;
  return InlineNode::GetOffsetMapping(context);
}

// static
LayoutBlockFlow* OffsetMapping::GetInlineFormattingContextOf(
    const LayoutObject& object) {
  for (LayoutObject* runner = object.Parent(); runner;
       runner = runner->Parent()) {
    if (!CanUseOffsetMapping(*runner)) {
      continue;
    }
    return To<LayoutBlockFlow>(runner);
  }
  return nullptr;
}

OffsetMapping::OffsetMapping(UnitVector&& units, RangeMap&& ranges, String text)
    : units_(std::move(units)), ranges_(std::move(ranges)), text_(text) {
#if ENABLE_SECURITY_ASSERT
  for (const auto& unit : units_) {
    SECURITY_DCHECK(unit.TextContentStart() <= text.length())
        << unit.TextContentStart() << "<=" << text.length();
    SECURITY_DCHECK(unit.TextContentEnd() <= text.length())
        << unit.TextContentEnd() << "<=" << text.length();
    unit.AssertValid();
  }
  for (const auto& pair : ranges_) {
    SECURITY_DCHECK(pair.value.first < units_.size())
        << pair.value.first << "<" << units_.size();
    SECURITY_DCHECK(pair.value.second <= units_.size())
        << pair.value.second << "<=" << units_.size();
  }
#endif
}

OffsetMapping::~OffsetMapping() = default;

const OffsetMappingUnit* OffsetMapping::GetMappingUnitForPosition(
    const Position& position) const {
  DCHECK(OffsetMapping::AcceptsPosition(position));
  const auto node_and_offset = ToNodeOffsetPair(position);
  const Node& node = node_and_offset.first;
  const unsigned offset = node_and_offset.second;
  unsigned range_start;
  unsigned range_end;
  auto it = ranges_.find(&node);
  std::tie(range_start, range_end) =
      it != ranges_.end() ? it->value : std::pair<unsigned, unsigned>(0, 0);
  if (range_start == range_end || units_[range_start].DOMStart() > offset)
    return nullptr;
  // Find the last unit where unit.dom_start <= offset
  auto unit = std::prev(std::upper_bound(
      units_.begin() + range_start, units_.begin() + range_end, offset,
      [](unsigned offset, const OffsetMappingUnit& unit) {
        return offset < unit.DOMStart();
      }));
  if (unit->DOMEnd() < offset)
    return nullptr;
  return &*unit;
}

OffsetMapping::UnitVector OffsetMapping::GetMappingUnitsForDOMRange(
    const EphemeralRange& range) const {
  DCHECK(OffsetMapping::AcceptsPosition(range.StartPosition()));
  DCHECK(OffsetMapping::AcceptsPosition(range.EndPosition()));
  DCHECK_EQ(range.StartPosition().AnchorNode(),
            range.EndPosition().AnchorNode());
  const Node& node = *range.StartPosition().AnchorNode();
  const unsigned start_offset = ToNodeOffsetPair(range.StartPosition()).second;
  const unsigned end_offset = ToNodeOffsetPair(range.EndPosition()).second;
  unsigned range_start;
  unsigned range_end;
  auto it = ranges_.find(&node);
  std::tie(range_start, range_end) =
      it != ranges_.end() ? it->value : std::pair<unsigned, unsigned>(0, 0);

  if (range_start == range_end || units_[range_start].DOMStart() > end_offset ||
      units_[range_end - 1].DOMEnd() < start_offset)
    return UnitVector();

  // Find the first unit where unit.dom_end >= start_offset
  auto result_begin = std::lower_bound(
      units_.begin() + range_start, units_.begin() + range_end, start_offset,
      [](const OffsetMappingUnit& unit, unsigned offset) {
        return unit.DOMEnd() < offset;
      });

  // Find the next of the last unit where unit.dom_start <= end_offset
  auto result_end =
      std::upper_bound(result_begin, units_.begin() + range_end, end_offset,
                       [](unsigned offset, const OffsetMappingUnit& unit) {
                         return offset < unit.DOMStart();
                       });

  UnitVector result;
  result.reserve(base::checked_cast<wtf_size_t>(result_end - result_begin));
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

base::span<const OffsetMappingUnit> OffsetMapping::GetMappingUnitsForNode(
    const Node& node) const {
  const auto it = ranges_.find(&node);
  if (it == ranges_.end()) {
    return {};
  }
  return base::make_span(units_.begin() + it->value.first,
                         units_.begin() + it->value.second);
}

base::span<const OffsetMappingUnit>
OffsetMapping::GetMappingUnitsForLayoutObject(
    const LayoutObject& layout_object) const {
  const auto begin = base::ranges::find(units_, layout_object,
                                        &OffsetMappingUnit::GetLayoutObject);
  CHECK_NE(begin, units_.end());
  const auto end =
      std::find_if(std::next(begin), units_.end(),
                   [&layout_object](const OffsetMappingUnit& unit) {
                     return unit.GetLayoutObject() != layout_object;
                   });
  DCHECK_LT(begin, end);
  return base::make_span(begin, end);
}

base::span<const OffsetMappingUnit>
OffsetMapping::GetMappingUnitsForTextContentOffsetRange(unsigned start,
                                                        unsigned end) const {
  DCHECK_LE(start, end);
  if (units_.front().TextContentStart() >= end ||
      units_.back().TextContentEnd() <= start)
    return {};

  // Find the first unit where unit.text_content_end > start
  auto result_begin =
      std::lower_bound(units_.begin(), units_.end(), start,
                       [](const OffsetMappingUnit& unit, unsigned offset) {
                         return unit.TextContentEnd() <= offset;
                       });
  if (result_begin == units_.end() || result_begin->TextContentStart() >= end)
    return {};

  // Find the next of the last unit where unit.text_content_start < end
  auto result_end =
      std::upper_bound(units_.begin(), units_.end(), end,
                       [](unsigned offset, const OffsetMappingUnit& unit) {
                         return offset <= unit.TextContentStart();
                       });
  return base::make_span(result_begin, result_end);
}

std::optional<unsigned> OffsetMapping::GetTextContentOffset(
    const Position& position) const {
  DCHECK(OffsetMapping::AcceptsPosition(position)) << position;
  const OffsetMappingUnit* unit = GetMappingUnitForPosition(position);
  if (!unit)
    return std::nullopt;
  return unit->ConvertDOMOffsetToTextContent(ToNodeOffsetPair(position).second);
}

Position OffsetMapping::StartOfNextNonCollapsedContent(
    const Position& position) const {
  DCHECK(OffsetMapping::AcceptsPosition(position)) << position;
  const OffsetMappingUnit* unit = GetMappingUnitForPosition(position);
  if (!unit)
    return Position();

  const auto node_and_offset = ToNodeOffsetPair(position);
  const Node& node = node_and_offset.first;
  const unsigned offset = node_and_offset.second;
  while (unit != units_.data() + units_.size() &&
         unit->AssociatedNode() == node) {
    if (unit->DOMEnd() > offset &&
        unit->GetType() != OffsetMappingUnitType::kCollapsed) {
      const unsigned result = std::max(offset, unit->DOMStart());
      return CreatePositionForOffsetMapping(node, result);
    }
    ++unit;
  }
  return Position();
}

Position OffsetMapping::EndOfLastNonCollapsedContent(
    const Position& position) const {
  DCHECK(OffsetMapping::AcceptsPosition(position)) << position;
  const OffsetMappingUnit* unit = GetMappingUnitForPosition(position);
  if (!unit)
    return Position();

  const auto node_and_offset = ToNodeOffsetPair(position);
  const Node& node = node_and_offset.first;
  const unsigned offset = node_and_offset.second;
  while (unit->AssociatedNode() == node) {
    if (unit->DOMStart() < offset &&
        unit->GetType() != OffsetMappingUnitType::kCollapsed) {
      const unsigned result = std::min(offset, unit->DOMEnd());
      return CreatePositionForOffsetMapping(node, result);
    }
    if (unit == units_.data()) {
      break;
    }
    --unit;
  }
  return Position();
}

bool OffsetMapping::IsBeforeNonCollapsedContent(
    const Position& position) const {
  DCHECK(OffsetMapping::AcceptsPosition(position));
  const OffsetMappingUnit* unit = GetMappingUnitForPosition(position);
  const unsigned offset = ToNodeOffsetPair(position).second;
  return unit && offset < unit->DOMEnd() &&
         unit->GetType() != OffsetMappingUnitType::kCollapsed;
}

bool OffsetMapping::IsAfterNonCollapsedContent(const Position& position) const {
  DCHECK(OffsetMapping::AcceptsPosition(position));
  const auto node_and_offset = ToNodeOffsetPair(position);
  const Node& node = node_and_offset.first;
  const unsigned offset = node_and_offset.second;
  if (!offset)
    return false;
  // In case we have one unit ending at |offset| and another starting at
  // |offset|, we need to find the former. Hence, search with |offset - 1|.
  const OffsetMappingUnit* unit = GetMappingUnitForPosition(
      CreatePositionForOffsetMapping(node, offset - 1));
  return unit && offset > unit->DOMStart() &&
         unit->GetType() != OffsetMappingUnitType::kCollapsed;
}

std::optional<UChar> OffsetMapping::GetCharacterBefore(
    const Position& position) const {
  DCHECK(OffsetMapping::AcceptsPosition(position));
  std::optional<unsigned> text_content_offset = GetTextContentOffset(position);
  if (!text_content_offset || !*text_content_offset)
    return std::nullopt;
  return text_[*text_content_offset - 1];
}

Position OffsetMapping::GetFirstPosition(unsigned offset) const {
  // Find the first unit where |unit.TextContentEnd() >= offset|
  if (units_.empty() || units_.back().TextContentEnd() < offset)
    return {};
  auto result =
      std::lower_bound(units_.begin(), units_.end(), offset,
                       [](const OffsetMappingUnit& unit, unsigned offset) {
                         return unit.TextContentEnd() < offset;
                       });
  CHECK_NE(result, units_.end());
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

const OffsetMappingUnit* OffsetMapping::GetFirstMappingUnit(
    unsigned offset) const {
  // Find the first unit where |unit.TextContentEnd() <= offset|
  if (units_.empty() || units_.front().TextContentStart() > offset)
    return nullptr;
  auto result =
      std::lower_bound(units_.begin(), units_.end(), offset,
                       [](const OffsetMappingUnit& unit, unsigned offset) {
                         return unit.TextContentEnd() < offset;
                       });
  if (result == units_.end())
    return nullptr;
  auto next_unit = std::next(result);
  if (next_unit != units_.end() && next_unit->TextContentStart() == offset) {
    // For offset=2, returns [1] instead of [0].
    // For offset=3, returns [3] instead of [2],
    // in below example:
    //  text_content = "ab\ncd"
    //  offset mapping unit:
    //   [0] I DOM:0-2 TC:0-2 "ab"
    //   [1] C DOM:2-3 TC:2-2
    //   [2] I DOM:3-4 TC:2-3 "\n"
    //   [3] C DOM:4-5 TC:3-3
    //   [4] I DOM:5-7 TC:3-5 "cd"
    return &*next_unit;
  }
  return &*result;
}

const OffsetMappingUnit* OffsetMapping::GetLastMappingUnit(
    unsigned offset) const {
  // Find the last unit where |unit.TextContentStart() <= offset|
  if (units_.empty() || units_.front().TextContentStart() > offset)
    return nullptr;
  auto result =
      std::upper_bound(units_.begin(), units_.end(), offset,
                       [](unsigned offset, const OffsetMappingUnit& unit) {
                         return offset < unit.TextContentStart();
                       });
  CHECK_NE(result, units_.begin());
  result = std::prev(result);
  if (result->TextContentEnd() < offset)
    return nullptr;
  return &*result;
}

Position OffsetMapping::GetLastPosition(unsigned offset) const {
  const OffsetMappingUnit* result = GetLastMappingUnit(offset);
  if (!result)
    return {};
  // Skip CSS generated content, e.g. "content" property in ::before/::after.
  while (!result->AssociatedNode()) {
    if (result == units_.data()) {
      return {};
    }
    result = std::prev(result);
    if (result->TextContentEnd() < offset)
      return {};
  }
  const Node& node = result->GetOwner();
  const unsigned dom_offset = result->ConvertTextContentToLastDOMOffset(offset);
  return CreatePositionForOffsetMapping(node, dom_offset);
}

bool OffsetMapping::HasBidiControlCharactersOnly(unsigned start,
                                                 unsigned end) const {
  DCHECK_LE(start, end);
  DCHECK_LE(end, text_.length());
  for (unsigned i = start; i < end; ++i) {
    if (!Character::IsBidiControl(text_[i]))
      return false;
  }
  return true;
}

unsigned OffsetMapping::LayoutObjectConverter::TextContentOffset(
    unsigned offset) const {
  auto iter = offset >= last_offset_ ? last_unit_ : units_.begin();
  if (offset >= iter->DOMEnd()) {
    iter = base::ranges::find_if(
        iter, units_.end(), [offset](const OffsetMappingUnit& unit) {
          return unit.DOMStart() <= offset && offset < unit.DOMEnd();
        });
  }
  CHECK(iter != units_.end());
  last_unit_ = iter;
  last_offset_ = offset;
  return iter->ConvertDOMOffsetToTextContent(offset);
}

void OffsetMappingUnit::Trace(Visitor* visitor) const {
  visitor->Trace(layout_object_);
}

}  // namespace blink
