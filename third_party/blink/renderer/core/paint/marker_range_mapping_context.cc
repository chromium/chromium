// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/marker_range_mapping_context.h"

#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/editing/position.h"

namespace blink {

MarkerRangeMappingContext::DOMToTextContentOffsetMapper::
    DOMToTextContentOffsetMapper(const Text& text_node) {
  units_ = GetMappingUnits(text_node.GetLayoutObject());
  units_begin_ = units_.begin();
  DCHECK(units_.size());
}

base::span<const OffsetMappingUnit>
MarkerRangeMappingContext::DOMToTextContentOffsetMapper::GetMappingUnits(
    const LayoutObject* layout_object) {
  const OffsetMapping* const offset_mapping =
      OffsetMapping::GetFor(layout_object);
  DCHECK(offset_mapping);
  if (RuntimeEnabledFeatures::PaintHighlightsForFirstLetterEnabled()) {
    return offset_mapping->GetMappingUnitsForNode(*layout_object->GetNode());
  } else {
    return offset_mapping->GetMappingUnitsForLayoutObject(*layout_object);
  }
}

unsigned
MarkerRangeMappingContext::DOMToTextContentOffsetMapper::GetTextContentOffset(
    unsigned dom_offset) const {
  auto unit = FindUnit(units_begin_, dom_offset);
  // Update the cached search starting point.
  units_begin_ = unit;
  // Since the unit range only covers the fragment, map anything that falls
  // outside of that range to the start/end.
  if (dom_offset < unit->DOMStart()) {
    return unit->TextContentStart();
  }
  if (dom_offset > unit->DOMEnd()) {
    return unit->TextContentEnd();
  }
  return unit->ConvertDOMOffsetToTextContent(dom_offset);
}

unsigned MarkerRangeMappingContext::DOMToTextContentOffsetMapper::
    GetTextContentOffsetNoCache(unsigned dom_offset) const {
  auto unit = FindUnit(units_begin_, dom_offset);
  // Since the unit range only covers the fragment, map anything that falls
  // outside of that range to the start/end.
  if (dom_offset < unit->DOMStart()) {
    return unit->TextContentStart();
  }
  if (dom_offset > unit->DOMEnd()) {
    return unit->TextContentEnd();
  }
  return unit->ConvertDOMOffsetToTextContent(dom_offset);
}

// Find the mapping unit for `dom_offset`, starting from `begin`.
base::span<const OffsetMappingUnit>::iterator
MarkerRangeMappingContext::DOMToTextContentOffsetMapper::FindUnit(
    base::span<const OffsetMappingUnit>::iterator begin,
    unsigned dom_offset) const {
  if (dom_offset <= begin->DOMEnd()) {
    return begin;
  }
  return std::prev(
      std::upper_bound(begin, units_.end(), dom_offset,
                       [](unsigned offset, const OffsetMappingUnit& unit) {
                         return offset < unit.DOMStart();
                       }));
}

std::optional<TextOffsetRange> MarkerRangeMappingContext::GetTextContentOffsets(
    const DocumentMarker& marker) const {
  if (marker.EndOffset() <= fragment_dom_range_.start ||
      marker.StartOffset() >= fragment_dom_range_.end) {
    return std::nullopt;
  }

  // Clamp the marker to the fragment in DOM space
  const unsigned start_dom_offset =
      std::max(marker.StartOffset(), fragment_dom_range_.start);
  const unsigned end_dom_offset =
      std::min(marker.EndOffset(), fragment_dom_range_.end);
  const unsigned text_content_start =
      mapper_.GetTextContentOffset(start_dom_offset);
  const unsigned text_content_end =
      mapper_.GetTextContentOffsetNoCache(end_dom_offset);
  return TextOffsetRange(text_content_start, text_content_end);
}

}  // namespace blink
