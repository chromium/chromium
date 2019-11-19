// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping_builder.h"

#include <utility>
#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"

namespace blink {

namespace {

// Returns 0 unless |layout_object| is the remaining text of a node styled with
// ::first-letter, in which case it returns the start offset of the remaining
// text. When ::first-letter is applied to generated content, e.g. ::before,
// remaining part contains text for remaining part only instead of all text.
unsigned GetAssociatedStartOffset(const LayoutObject* layout_object) {
  const auto* text_fragment = ToLayoutTextFragmentOrNull(layout_object);
  if (!text_fragment || !text_fragment->AssociatedTextNode())
    return 0;
  return text_fragment->Start();
}

}  // namespace

NGOffsetMappingBuilder::NGOffsetMappingBuilder() = default;

NGOffsetMappingBuilder::SourceNodeScope::SourceNodeScope(
    NGOffsetMappingBuilder* builder,
    const LayoutObject* node)
    : builder_(builder),
      layout_object_auto_reset_(&builder->current_layout_object_, node),
      appended_length_auto_reset_(&builder->current_offset_,
                                  GetAssociatedStartOffset(node)) {
  builder_->has_open_unit_ = false;
#if DCHECK_IS_ON()
  if (!builder_->current_layout_object_)
    return;
  // We allow at most one scope with non-null node at any time.
  DCHECK(!builder->has_nonnull_node_scope_);
  builder->has_nonnull_node_scope_ = true;
#endif
}

NGOffsetMappingBuilder::SourceNodeScope::~SourceNodeScope() {
  builder_->has_open_unit_ = false;
#if DCHECK_IS_ON()
  if (builder_->current_layout_object_)
    builder_->has_nonnull_node_scope_ = false;
#endif
}

void NGOffsetMappingBuilder::ReserveCapacity(unsigned capacity) {
  unit_ranges_.ReserveCapacityForSize(capacity);
  mapping_units_.ReserveCapacity(capacity * 1.5);
}

void NGOffsetMappingBuilder::AppendIdentityMapping(unsigned length) {
  DCHECK_GT(length, 0u);
  const unsigned dom_start = current_offset_;
  const unsigned dom_end = dom_start + length;
  const unsigned text_content_start = destination_length_;
  const unsigned text_content_end = text_content_start + length;
  current_offset_ += length;
  destination_length_ += length;

  if (!current_layout_object_)
    return;

  if (has_open_unit_ &&
      mapping_units_.back().GetType() == NGOffsetMappingUnitType::kIdentity) {
    DCHECK_EQ(mapping_units_.back().GetLayoutObject(), current_layout_object_);
    DCHECK_EQ(mapping_units_.back().DOMEnd(), dom_start);
    mapping_units_.back().dom_end_ += length;
    mapping_units_.back().text_content_end_ += length;
    return;
  }

  mapping_units_.emplace_back(NGOffsetMappingUnitType::kIdentity,
                              *current_layout_object_, dom_start, dom_end,
                              text_content_start, text_content_end);
  has_open_unit_ = true;
}

void NGOffsetMappingBuilder::AppendCollapsedMapping(unsigned length) {
  DCHECK_GT(length, 0u);
  const unsigned dom_start = current_offset_;
  const unsigned dom_end = dom_start + length;
  const unsigned text_content_start = destination_length_;
  const unsigned text_content_end = text_content_start;
  current_offset_ += length;

  if (!current_layout_object_)
    return;

  if (has_open_unit_ &&
      mapping_units_.back().GetType() == NGOffsetMappingUnitType::kCollapsed) {
    DCHECK_EQ(mapping_units_.back().GetLayoutObject(), current_layout_object_);
    DCHECK_EQ(mapping_units_.back().DOMEnd(), dom_start);
    mapping_units_.back().dom_end_ += length;
    return;
  }

  mapping_units_.emplace_back(NGOffsetMappingUnitType::kCollapsed,
                              *current_layout_object_, dom_start, dom_end,
                              text_content_start, text_content_end);
  has_open_unit_ = true;
}

void NGOffsetMappingBuilder::CollapseTrailingSpace(unsigned space_offset) {
  DCHECK_LT(space_offset, destination_length_);
  --destination_length_;

  NGOffsetMappingUnit* container_unit = nullptr;
  for (unsigned i = mapping_units_.size(); i;) {
    NGOffsetMappingUnit& unit = mapping_units_[--i];
    if (unit.TextContentStart() > space_offset) {
      --unit.text_content_start_;
      --unit.text_content_end_;
      continue;
    }
    container_unit = &unit;
    break;
  }

  if (!container_unit || container_unit->TextContentEnd() <= space_offset)
    return;

  // container_unit->TextContentStart()
  // <= space_offset <
  // container_unit->TextContentEnd()
  DCHECK_EQ(NGOffsetMappingUnitType::kIdentity, container_unit->GetType());
  const LayoutObject& layout_object = container_unit->GetLayoutObject();
  unsigned dom_offset = container_unit->DOMStart();
  unsigned text_content_offset = container_unit->TextContentStart();
  unsigned offset_to_collapse = space_offset - text_content_offset;

  Vector<NGOffsetMappingUnit, 3> new_units;
  if (offset_to_collapse) {
    new_units.emplace_back(NGOffsetMappingUnitType::kIdentity, layout_object,
                           dom_offset, dom_offset + offset_to_collapse,
                           text_content_offset,
                           text_content_offset + offset_to_collapse);
    dom_offset += offset_to_collapse;
    text_content_offset += offset_to_collapse;
  }
  new_units.emplace_back(NGOffsetMappingUnitType::kCollapsed, layout_object,
                         dom_offset, dom_offset + 1, text_content_offset,
                         text_content_offset);
  ++dom_offset;
  if (dom_offset < container_unit->DOMEnd()) {
    new_units.emplace_back(NGOffsetMappingUnitType::kIdentity, layout_object,
                           dom_offset, container_unit->DOMEnd(),
                           text_content_offset,
                           container_unit->TextContentEnd() - 1);
  }

  // TODO(xiaochengh): Optimize if this becomes performance bottleneck.
  unsigned position = std::distance(mapping_units_.begin(), container_unit);
  mapping_units_.EraseAt(position);
  mapping_units_.InsertVector(position, new_units);
  unsigned new_unit_end = position + new_units.size();
  while (new_unit_end && new_unit_end < mapping_units_.size() &&
         mapping_units_[new_unit_end - 1].Concatenate(
             mapping_units_[new_unit_end])) {
    mapping_units_.EraseAt(new_unit_end);
  }
  while (position && position < mapping_units_.size() &&
         mapping_units_[position - 1].Concatenate(mapping_units_[position])) {
    mapping_units_.EraseAt(position);
  }
}

void NGOffsetMappingBuilder::RestoreTrailingCollapsibleSpace(
    const LayoutText& layout_text,
    unsigned offset) {
  ++destination_length_;
  for (auto& unit : base::Reversed(mapping_units_)) {
    if (unit.text_content_end_ < offset) {
      // There are no collapsed unit.
      NOTREACHED();
      return;
    }
    if (unit.text_content_start_ != offset ||
        unit.text_content_end_ != offset ||
        unit.layout_object_ != layout_text) {
      ++unit.text_content_start_;
      ++unit.text_content_end_;
      continue;
    }
    DCHECK_EQ(unit.type_, NGOffsetMappingUnitType::kCollapsed);
    const unsigned original_dom_end = unit.dom_end_;
    unit.type_ = NGOffsetMappingUnitType::kIdentity;
    unit.dom_end_ = unit.dom_start_ + 1;
    unit.text_content_end_ = unit.text_content_start_ + 1;
    if (original_dom_end - unit.dom_start_ == 1)
      return;
    // When we collapsed multiple spaces, e.g. <b>   </b>.
    mapping_units_.insert(
        std::distance(mapping_units_.begin(), &unit) + 1,
        NGOffsetMappingUnit(NGOffsetMappingUnitType::kCollapsed, layout_text,
                            unit.dom_end_, original_dom_end,
                            unit.text_content_end_, unit.text_content_end_));
    return;
  }
  NOTREACHED();
  return;
}

void NGOffsetMappingBuilder::SetDestinationString(String string) {
  DCHECK_EQ(destination_length_, string.length());
  destination_string_ = string;
}

std::unique_ptr<NGOffsetMapping> NGOffsetMappingBuilder::Build() {
  // All mapping units are already built. Scan them to build mapping ranges.
  for (unsigned range_start = 0; range_start < mapping_units_.size();) {
    const LayoutObject& layout_object =
        mapping_units_[range_start].GetLayoutObject();
    unsigned range_end = range_start + 1;
    const Node* node = mapping_units_[range_start].AssociatedNode();
    if (node) {
      while (range_end < mapping_units_.size() &&
             mapping_units_[range_end].AssociatedNode() == node)
        ++range_end;
      // Units of the same node should be consecutive in the mapping function,
      // If not, the layout structure should be already broken.
      DCHECK(!unit_ranges_.Contains(node)) << node;
      unit_ranges_.insert(node, std::make_pair(range_start, range_end));
    } else {
      while (range_end < mapping_units_.size() &&
             mapping_units_[range_end].GetLayoutObject() == layout_object)
        ++range_end;
    }
    range_start = range_end;
  }

  return std::make_unique<NGOffsetMapping>(
      std::move(mapping_units_), std::move(unit_ranges_), destination_string_);
}

}  // namespace blink
