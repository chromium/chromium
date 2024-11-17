// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_block_flow_iterator.h"

#include <numeric>

#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_span.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/modules/accessibility/ax_debug_utils.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

namespace {

// Determines if a trailing space is required for a11y. An end-of-line text
// fragment does not include trailing whitespace since not rendered; however,
// we need to know there is a space there for editing purposes in a11y.
// A trailing whitespace is included if all of the following conditions are met:
//  * The line does not end in a forced linebreak (e.g. <br>)
//  * The position of the linebreak is immediately after the text fragment.
//  * The character immediately following the text fragment is a space.
//  * The trailing space was elided (not included in the next fragment)
//  * The trailing space is associated with the same layout object.
bool IncludeTrailingWhitespace(const WTF::String& text,
                               wtf_size_t offset,
                               const FragmentItem& item,
                               const AXBlockFlowData::Line& line) {
  if (line.forced_break) {
    return false;
  }

  if (line.break_index != offset + 1) {
    return false;
  }

  if (text[offset] != WTF::unicode::kSpaceCharacter) {
    return false;
  }

  if (item.Style().ShouldPreserveWhiteSpaces()) {
    return false;
  }

  const LayoutObject* layout_object = item.GetLayoutObject();
  const OffsetMapping* mapping = OffsetMapping::GetFor(layout_object);
  if (!mapping) {
    return false;
  }

  const base::span<const OffsetMappingUnit> mapping_units =
      mapping->GetMappingUnitsForTextContentOffsetRange(offset, offset + 1);
  if (mapping_units.begin() == mapping_units.end()) {
    return false;
  }
  const OffsetMappingUnit& mapping_unit = mapping_units.front();
  return mapping_unit.GetLayoutObject() == layout_object;
}

}  // end anonymous namespace

WTF::String AXBlockFlowData::GetText(wtf_size_t item_index) const {
  Position position = GetPosition(item_index);
  const PhysicalBoxFragment* box_fragment =
      BoxFragment(position.fragmentainer_index);
  const FragmentItems& fragment_items = *box_fragment->Items();
  const FragmentItem& item = fragment_items.Items()[position.item_index];
  if (item.Type() == FragmentItem::kGeneratedText) {
    return item.GeneratedText().ToString();
  }
  if (item.Type() != FragmentItem::kText) {
    return WTF::String();
  }
  wtf_size_t start_offset = item.TextOffset().start;
  wtf_size_t end_offset = item.TextOffset().end;
  wtf_size_t length = end_offset - start_offset;

  // TODO: handle first line text content.
  String full_text = fragment_items.Text(item.UsesFirstLineStyle());

  const FragmentProperties& fragment_properties =
      fragment_properties_[item_index];

  // If the text elided a trailing whitespace, we may need to reintroduce it.
  // Trailing whitespace is elided since not rendered; however, there may still
  // be required for a11y.
  if (fragment_properties.line_index) {
    const AXBlockFlowData::Line& line =
        lines_[fragment_properties.line_index.value()];
    if (IncludeTrailingWhitespace(full_text, end_offset, item, line)) {
      length++;
    }
  }

  return StringView(full_text, start_offset, length).ToString();
}

const AXBlockFlowData::FragmentProperties& AXBlockFlowData::GetProperties(
    wtf_size_t index) const {
  return fragment_properties_[index];
}

void AXBlockFlowData::Trace(Visitor* visitor) const {
  visitor->Trace(block_flow_container_);
  visitor->Trace(layout_fragment_map_);
  visitor->Trace(lines_);
  visitor->Trace(fragment_properties_);
}

AXBlockFlowData::AXBlockFlowData(LayoutBlockFlow* layout_block_flow)
    : block_flow_container_(layout_block_flow) {
#if DCHECK_IS_ON()
  // Launch with --vmodule=ax_debug_utils=2 to see a diagnostic dump of the
  // block fragmentation.
  DumpBlockFragmentationData(layout_block_flow);
#endif  // DCHECK_IS_ON()

  ProcessLayoutBlock(layout_block_flow);
}

AXBlockFlowData::~AXBlockFlowData() = default;

void AXBlockFlowData::ProcessLayoutBlock(LayoutBlockFlow* container) {
  wtf_size_t starting_fragment_index = 0;
  int container_fragment_count = container->PhysicalFragmentCount();
  // To compute hidden correctly, we need to walk the ancestor chain.
  if (container_fragment_count) {
    for (int fragment_index = 0; fragment_index < container_fragment_count;
         fragment_index++) {
      const PhysicalBoxFragment* fragment =
          container->GetPhysicalFragment(fragment_index);
      if (fragment->Items()) {
        wtf_size_t next_starting_index =
            starting_fragment_index + fragment->Items()->Size();
        fragment_properties_.resize(next_starting_index);
        ProcessBoxFragment(fragment, starting_fragment_index);
        starting_fragment_index = next_starting_index;
      }
    }
  }
  total_fragment_count_ = starting_fragment_index;
}

void AXBlockFlowData::ProcessBoxFragment(const PhysicalBoxFragment* fragment,
                                         wtf_size_t starting_fragment_index) {
  const FragmentItems* items = fragment->Items();
  if (!items) {
    return;
  }

  wtf_size_t fragment_index = starting_fragment_index;
  std::optional<wtf_size_t> previous_on_line;
  for (auto it = items->Items().begin(); it != items->Items().end();
       it++, fragment_index++) {
    const LayoutObject* layout_object = it->GetLayoutObject();
    auto range_it = layout_fragment_map_.find(layout_object);
    if (range_it == layout_fragment_map_.end()) {
      layout_fragment_map_.insert(layout_object, fragment_index);
    }

    bool on_current_line = OnCurrentLine(fragment_index);
    if (it->Type() == FragmentItem::kLine) {
      wtf_size_t length = it->DescendantsCount();
      const InlineBreakToken* break_token = it->GetInlineBreakToken();
      bool forced_break = break_token && break_token->IsForcedBreak();
      std::optional<wtf_size_t> break_index;
      if (break_token) {
        break_index = break_token->StartTextOffset();
      }
      lines_.push_back<Line>({.start_index = fragment_index,
                              .length = length,
                              .forced_break = forced_break,
                              .break_index = break_index});
      if (!on_current_line) {
        previous_on_line = std::nullopt;
      }
      on_current_line = true;
    }

    FragmentProperties& properties = fragment_properties_[fragment_index];
    if (on_current_line) {
      properties.line_index = lines_.size() - 1;
    }

    if (it->Type() == FragmentItem::kText ||
        it->Type() == FragmentItem::kGeneratedText) {
      if (previous_on_line) {
        properties.previous_on_line = previous_on_line;
        fragment_properties_[previous_on_line.value()].next_on_line =
            fragment_index;
      }
      previous_on_line = fragment_index;
    }

    // TODO (accessibility): Handle box fragments with children stored in a
    // separate physical box fragment. We should be able to process these in
    // a single pass and make AXNodeObject::NextOnLine a trivial lookup.
  }
}

bool AXBlockFlowData::OnCurrentLine(wtf_size_t index) const {
  if (lines_.empty()) {
    return false;
  }

  // The fragment is on the current line if within the line's boundaries.
  const Line& candidate = lines_.back();
  return candidate.start_index < index &&
         candidate.start_index + candidate.length > index;
}

const std::optional<wtf_size_t> AXBlockFlowData::FindFirstFragment(
    const LayoutObject* layout_object) const {
  auto it = layout_fragment_map_.find(layout_object);
  if (it != layout_fragment_map_.end()) {
    return it->value;
  }
  return std::nullopt;
}

const AXBlockFlowData::Position AXBlockFlowData::GetPosition(
    wtf_size_t index) const {
  wtf_size_t container_fragment_count =
      block_flow_container_->PhysicalFragmentCount();

  if (container_fragment_count) {
    for (wtf_size_t fragment_index = 0;
         fragment_index < container_fragment_count; fragment_index++) {
      const PhysicalBoxFragment* fragment =
          block_flow_container_->GetPhysicalFragment(fragment_index);
      wtf_size_t size = fragment->Items()->Size();
      if (index < size) {
        return {.fragmentainer_index = fragment_index, .item_index = index};
      }
      index -= size;
    }
  }
  return {container_fragment_count, 0};
}

const FragmentItem* AXBlockFlowData::ItemAt(wtf_size_t index) const {
  if (index >= Size()) {
    return nullptr;
  }

  Position position = GetPosition(index);
  const PhysicalBoxFragment* box_fragment =
      block_flow_container_->GetPhysicalFragment(position.fragmentainer_index);
  return &box_fragment->Items()->Items()[position.item_index];
}

const PhysicalBoxFragment* AXBlockFlowData::BoxFragment(
    wtf_size_t index) const {
  return block_flow_container_->GetPhysicalFragment(index);
}

AXBlockFlowIterator::AXBlockFlowIterator(const AXObject* object)
    : block_flow_data_(object->AXObjectCache().GetBlockFlowData(object)),
      layout_object_(object->GetLayoutObject()) {
  start_index_ = block_flow_data_->FindFirstFragment(layout_object_);
}

bool AXBlockFlowIterator::Next() {
  text_.reset();
  character_widths_.reset();

  if (!start_index_) {
    return false;
  }

  if (!current_index_) {
    current_index_ = start_index_.value();
    return true;
  }

  wtf_size_t delta = block_flow_data_->ItemAt(current_index_.value())
                         ->DeltaToNextForSameLayoutObject();
  if (delta) {
    current_index_ = current_index_.value() + delta;
    return true;
  }

  return false;
}

const WTF::String& AXBlockFlowIterator::GetText() {
  DCHECK(current_index_) << "Must call Next to set initial iterator position "
                            "before calling GetText";

  if (text_) {
    return text_.value();
  }

  text_ = block_flow_data_->GetText(current_index_.value());
  return text_.value();
}

// static
WTF::String AXBlockFlowIterator::GetTextForTesting(
    AXBlockFlowIterator::MapKey map_key) {
  const FragmentItems* items = map_key.first;
  wtf_size_t index = map_key.second;
  const FragmentItem item = items->Items()[index];
  wtf_size_t start_offset = item.TextOffset().start;
  wtf_size_t end_offset = item.TextOffset().end;
  wtf_size_t length = end_offset - start_offset;
  String full_text = items->Text(/*first_line=*/false);
  return StringView(full_text, start_offset, length).ToString();
}

const std::vector<int>& AXBlockFlowIterator::GetCharacterLayoutPixelOffsets() {
  DCHECK(current_index_) << "Must call Next to set initial iterator position "
                            "before calling GetCharacterOffsets";

  if (character_widths_) {
    return character_widths_.value();
  }

  wtf_size_t length = GetText().length();
  const FragmentItem& item = *block_flow_data_->ItemAt(current_index_.value());
  const ShapeResultView* shape_result_view = item.TextShapeResult();
  float width_tally = 0;
  if (shape_result_view) {
    ShapeResult* shape_result = shape_result_view->CreateShapeResult();
    if (shape_result) {
      Vector<CharacterRange> ranges;
      shape_result->IndividualCharacterRanges(&ranges);
      character_widths_ = std::vector<int>();
      character_widths_->reserve(std::max(ranges.size(), length));
      character_widths_->resize(0);
      for (const auto& range : ranges) {
        width_tally += range.Width();
        character_widths_->push_back(roundf(width_tally));
      }
    }
  }
  // Pad with zero-width characters to the required length.
  for (wtf_size_t i = character_widths_->size(); i < length; i++) {
    character_widths_->push_back(width_tally);
  }
  return character_widths_.value();
}

const std::optional<AXBlockFlowIterator::MapKey>
AXBlockFlowIterator::NextOnLine() {
  DCHECK(current_index_)
      << "Must call Next to set initial iterator position before calling "
      << "NextOnLine";

  const AXBlockFlowData::FragmentProperties& properties =
      block_flow_data_->GetProperties(current_index_.value());
  if (properties.next_on_line) {
    const AXBlockFlowData::Position position =
        block_flow_data_->GetPosition(properties.next_on_line.value());
    const PhysicalBoxFragment* box_fragment =
        block_flow_data_->BoxFragment(position.fragmentainer_index);
    return MapKey(box_fragment->Items(), position.item_index);
  }

  return std::nullopt;
}

const std::optional<AXBlockFlowIterator::MapKey>
AXBlockFlowIterator::PreviousOnLine() {
  DCHECK(current_index_)
      << "Must call Next to set initial iterator position before calling "
      << "PreviousOnLine";

  const AXBlockFlowData::FragmentProperties& properties =
      block_flow_data_->GetProperties(current_index_.value());
  if (properties.previous_on_line) {
    const AXBlockFlowData::Position position =
        block_flow_data_->GetPosition(properties.previous_on_line.value());
    const PhysicalBoxFragment* box_fragment =
        block_flow_data_->BoxFragment(position.fragmentainer_index);
    return MapKey(box_fragment->Items(), position.item_index);
  }

  return std::nullopt;
}

const AXBlockFlowIterator::MapKey AXBlockFlowIterator::GetMapKey() const {
  DCHECK(current_index_)
      << "Must call Next to set initial iterator position before calling "
      << "GetMapKey";

  const AXBlockFlowData::Position position =
      block_flow_data_->GetPosition(current_index_.value());
  const PhysicalBoxFragment* box_fragment =
      block_flow_data_->BoxFragment(position.fragmentainer_index);
  return MapKey(box_fragment->Items(), position.item_index);
}

}  // namespace blink
