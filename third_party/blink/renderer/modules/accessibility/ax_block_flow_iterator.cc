// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_block_flow_iterator.h"

#include <numeric>
#include <optional>
#include <utility>

#include "base/check_deref.h"
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
#include "third_party/blink/renderer/platform/text/writing_direction_mode.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

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
bool IncludeTrailingWhitespace(const String& text,
                               wtf_size_t offset,
                               const FragmentItem& item,
                               const AXBlockFlowData::Line& line) {
  if (line.forced_break) {
    return false;
  }

  if (line.break_index != offset + 1) {
    return false;
  }

  if (text[offset] != uchar::kSpace) {
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

template <typename T>
T MoveIndexToNeighbor(const bool forward, const T& index) {
  return forward ? index + 1 : index - 1;
}

}  // end anonymous namespace

LayoutText* AXBlockFlowData::GetFirstLetterPseudoLayoutText(
    FragmentIndex index) const {
  const FragmentItem* item = ItemAt(index);
  const auto it = layout_fragment_map_.find(item->GetLayoutObject());
  if (it == layout_fragment_map_.end() || it->value != index) {
    return nullptr;
  }

  const LayoutText* layout_text =
      DynamicTo<LayoutText>(item->GetLayoutObject());
  if (!layout_text) {
    return nullptr;
  }
  Node* node = layout_text->GetNode();
  if (!node) {
    return nullptr;
  }
  if (const auto* layout_text_containing_letter =
          DynamicTo<LayoutText>(node->GetLayoutObject())) {
    return layout_text_containing_letter->GetFirstLetterPart();
  }
  return nullptr;
}

String AXBlockFlowData::GetText(wtf_size_t item_index) const {
  Position position = GetPosition(item_index);
  const PhysicalBoxFragment* box_fragment =
      BoxFragment(position.fragmentainer_index);
  const FragmentItems& fragment_items = *box_fragment->Items();
  const FragmentItem& item = fragment_items.Items()[position.item_index];
  if (item.Type() == FragmentItem::kGeneratedText) {
    return item.GeneratedText().ToString();
  }
  if (item.Type() != FragmentItem::kText) {
    return String();
  }
  wtf_size_t start_offset = item.TextOffset().start;
  wtf_size_t end_offset = item.TextOffset().end;
  wtf_size_t length = end_offset - start_offset;

  const String& full_text = fragment_items.Text(item.UsesFirstLineStyle());

  // If the text elided a trailing whitespace, we may need to reintroduce it.
  // Trailing whitespace is elided since not rendered; however, there may still
  // be required for a11y.
  if (const auto line = GetLine(item_index)) {
    if (IncludeTrailingWhitespace(full_text, end_offset, item, line.value())) {
      length++;
    }
  }

  if (LayoutText* first_letter = GetFirstLetterPseudoLayoutText(item_index)) {
    // When the CSS first-letter pseudoselector is used, the LayoutText for the
    // first letter is excluded from the accessibility tree, so we need to
    // prepend its text here.
    // TODO(accessibility): investigate and implement the correct solution for
    // this, as this is not serializing always the correct style info.
    return StrCat({first_letter->TransformedText().SimplifyWhiteSpace(),
                   StringView(full_text, start_offset, length)});
  }

  return StringView(full_text, start_offset, length).ToString();
}

std::optional<AXBlockFlowData::Line> AXBlockFlowData::GetLine(
    wtf_size_t index) const {
  for (const Line& line : lines_) {
    if (index >= line.start_index && index < line.start_index + line.length) {
      return line;
    }
  }
  return std::nullopt;
}

AXBlockFlowData::Neighbor AXBlockFlowData::ComputeNeighborOnLine(
    FragmentIndex index,
    bool forward) const {
  CHECK(index < Size());
  Position position = GetPosition(index);
  const PhysicalBoxFragment* box_fragment =
      BoxFragment(position.fragmentainer_index);
  const FragmentItems& fragment_items = CHECK_DEREF(box_fragment->Items());
  const auto& items_boundary =
      forward ? fragment_items.Items().end() : fragment_items.Items().begin();
  index = MoveIndexToNeighbor(forward, index);
  for (auto it = fragment_items.Items().begin() +
                 MoveIndexToNeighbor(forward, position.item_index);
       it != items_boundary; it = MoveIndexToNeighbor(forward, it),
            index = MoveIndexToNeighbor(forward, index)) {
    switch (it->Type()) {
      case FragmentItem::kText:
      case FragmentItem::kGeneratedText:
        if (!it->GetLayoutObject()) [[unlikely]] {
          // A generated text fragment item may not have a backing LayoutObject.
          // For regular text, we expect them to always have one.
          CHECK_EQ(it->Type(), FragmentItem::kGeneratedText);
        } else if (it->GetLayoutObject()->IsText()) {
          // TODO(accessibility): Investigate when an item can be text, but its
          // LayoutObject is not marked as such.
          return std::make_pair(it->GetLayoutObject(), index);
        }
        break;

      case FragmentItem::kLine:
        return FailureReason::kAtLineBoundary;

      case FragmentItem::kBox:
        if (it->BoxFragment()) {
          // TODO(accessibility): Add a test that exercises this branch.
          // TODO(crbug.com/399204651): Implement navigating into separate
          // PhysicalBox
          // fragments.
          return FailureReason::kAtBoxFragment;
        }
        // Inline-box continues on to the next/previous item.
        break;

      case FragmentItem::kInvalid:
        NOTREACHED();
    }
  }

  return FailureReason::kAtLineBoundary;
}

void AXBlockFlowData::Trace(Visitor* visitor) const {
  visitor->Trace(block_flow_container_);
  visitor->Trace(layout_fragment_map_);
  visitor->Trace(lines_);
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

  // Keep track of the current outermost line index, as ruby content may have
  // multiple nested lines within the flattened list. When checking if a
  // fragment is within line boundaries, this makes sure we are comparing
  // against the outermost line.
  // For example:
  // ++1. Line (5) <-- line within paragraph
  // ++2. Box (2) <-- <ruby> tag
  // ++3. Text "ruby base" <-- base text
  // ++4. Line (2)  <-- <rt> tag
  // ++5. Text "ruby text" <-- ruby annotation
  //
  // Then item 5 is on a separate nested line containing the annotation. For the
  // purpose of next-previous on-line, we want to be using the line within the
  // paragraph (item 1).
  std::optional<size_t> current_outermost_line_index;
  for (auto it = items->Items().begin(); it != items->Items().end();
       it++, fragment_index++) {
    const LayoutObject* layout_object = it->GetLayoutObject();
    auto range_it = layout_fragment_map_.find(layout_object);
    if (range_it == layout_fragment_map_.end()) {
      layout_fragment_map_.insert(layout_object, fragment_index);
    }

    bool on_current_line =
        current_outermost_line_index
            ? OnLine(lines_[current_outermost_line_index.value()],
                     fragment_index)
            : false;
    if (it->Type() == FragmentItem::kLine) {
      if (!on_current_line) {
        // We are moving to a new line that is not nested in the previous line.
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
        current_outermost_line_index = lines_.size() - 1;
      }
    }

    // TODO(crbug.com/399204651): Implement navigating into separate PhysicalBox
    // fragments.
  }
}

bool AXBlockFlowData::OnLine(const Line& line, wtf_size_t index) const {
  // The fragment is on the current line if within the line's boundaries.
  return line.start_index < index && line.start_index + line.length > index;
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
      if (!fragment || !fragment->HasItems()) {
        // A BlockFlow may have a physical box but no fragments when the
        // BlockFlow has no text content, but is still rendered. For example:
        //  <div style="margin-top:30px;">
        //    <div style="float:left;"></div>
        //    <div>PASS</div>
        //  </div>
        continue;
      }

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
  CHECK(index < Size());

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

const String& AXBlockFlowIterator::GetText() {
  DCHECK(current_index_) << "Must call Next to set initial iterator position "
                            "before calling GetText";

  if (text_) {
    return text_.value();
  }

  text_ = block_flow_data_->GetText(current_index_.value());
  return text_.value();
}

// static
String AXBlockFlowIterator::GetTextForTesting(
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

void AXBlockFlowIterator::GetCharacterLayoutPixelOffsets(Vector<int>& offsets) {
  DCHECK(current_index_) << "Must call Next to set initial iterator position "
                            "before calling GetCharacterOffsets";

  const wtf_size_t length = GetText().length();
  offsets.resize(length);
  const FragmentItem& item =
      CHECK_DEREF(block_flow_data_->ItemAt(current_index_.value()));
  const ShapeResultView* shape_result_view = item.TextShapeResult();
  if (!shape_result_view) {
    // When |fragment_| for BR, we don't have shape result.
    // "aom-computed-boolean-properties.html" reaches here.
    return;
  }

    ShapeResult* shape_result = shape_result_view->CreateShapeResult();
    if (!shape_result) {
      return;
    }
      Vector<CharacterRange> ranges;
      shape_result->IndividualCharacterRanges(&ranges);
      float width_so_far = 0.0;
      for (wtf_size_t i = 0; i < offsets.size(); ++i) {
        if (i < ranges.size()) {
          // The shaper can fail to return glyph metrics for all characters (see
          // crbug.com/613915 and crbug.com/615661) so add empty ranges to
          // ensure all characters have an associated range. This means that if
          // there is no range value, we assume 0 and just add the previous
          // offset.
          width_so_far += ranges[i].Width();
        }
        offsets[i] = roundf(width_so_far);
      }
}

AXBlockFlowData::Neighbor AXBlockFlowIterator::NextOnLineAsIndex() {
  DCHECK(current_index_)
      << "Must call Next to set initial iterator position before calling "
      << "NextOnLine";
  return block_flow_data_->ComputeNeighborOnLine(current_index_.value(),
                                                 /*forward=*/true);
}

AXBlockFlowData::Neighbor AXBlockFlowIterator::PreviousOnLineAsIndex() {
  DCHECK(current_index_)
      << "Must call Next to set initial iterator position before calling "
      << "PreviousOnLine";
  return block_flow_data_->ComputeNeighborOnLine(current_index_.value(),
                                                 /*forward=*/false);
}

const std::optional<AXBlockFlowIterator::MapKey>
AXBlockFlowIterator::NextOnLine() {
  DCHECK(current_index_)
      << "Must call Next to set initial iterator position before calling "
      << "NextOnLine";

  auto result_or_status = NextOnLineAsIndex();
  AXBlockFlowData::TextFragmentKey* index =
      std::get_if<AXBlockFlowData::TextFragmentKey>(&result_or_status);
  if (!index) {
    return std::nullopt;
  }
  const FragmentItem* item = block_flow_data_->ItemAt(index->second);
  if (!item || !item->GetLayoutObject() || !item->GetLayoutObject()->IsText()) {
    return std::nullopt;
  }

  const AXBlockFlowData::Position position =
      block_flow_data_->GetPosition(index->second);
  const PhysicalBoxFragment* box_fragment =
      block_flow_data_->BoxFragment(position.fragmentainer_index);
  return MapKey(box_fragment->Items(), position.item_index);
}

const std::optional<AXBlockFlowIterator::MapKey>
AXBlockFlowIterator::PreviousOnLine() {
  DCHECK(current_index_)
      << "Must call Next to set initial iterator position before calling "
      << "PreviousOnLine";
  auto result_or_status = PreviousOnLineAsIndex();
  AXBlockFlowData::TextFragmentKey* index =
      std::get_if<AXBlockFlowData::TextFragmentKey>(&result_or_status);
  if (!index) {
    return std::nullopt;
  }
  const FragmentItem* item = block_flow_data_->ItemAt(index->second);
  if (!item || !item->GetLayoutObject() || !item->GetLayoutObject()->IsText()) {
    return std::nullopt;
  }

  const AXBlockFlowData::Position position =
      block_flow_data_->GetPosition(index->second);
  const PhysicalBoxFragment* box_fragment =
      block_flow_data_->BoxFragment(position.fragmentainer_index);
  return MapKey(box_fragment->Items(), position.item_index);
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

PhysicalRect AXBlockFlowIterator::LocalBounds() const {
  DCHECK(current_index_) << "Must call Next to set initial iterator position "
                            "before calling LocalBounds";
  return block_flow_data_->ItemAt(current_index_.value())
      ->RectInContainerFragment();
}

TextOffsetRange AXBlockFlowIterator::TextOffset() const {
  return block_flow_data_->ItemAt(current_index_.value())->TextOffset();
}

PhysicalDirection AXBlockFlowIterator::GetDirection() const {
  auto* layout_text = To<LayoutText>(layout_object_);
  return WritingDirectionMode(layout_text->Style()->GetWritingMode(),
                              block_flow_data_->ItemAt(current_index_.value())
                                  ->ResolvedDirection())
      .InlineEnd();
}

bool AXBlockFlowIterator::IsLineBreak() const {
  const FragmentItem* item = block_flow_data_->ItemAt(current_index_.value());
  return item->IsText() && item->IsLineBreak();
}

}  // namespace blink
