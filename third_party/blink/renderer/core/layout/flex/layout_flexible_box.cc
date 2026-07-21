// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/flex/layout_flexible_box.h"

#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/layout/flex/flex_break_token_data.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"

namespace blink {

LayoutFlexibleBox::LayoutFlexibleBox(Element* element) : LayoutBlock(element) {}

namespace {

LogicalToPhysical<bool> GetOverflowConverter(const ComputedStyle& style) {
  const bool is_wrap_reverse = style.ResolvedIsFlexWrapReverse();
  const bool is_direction_reverse = style.ResolvedIsReverseFlexDirection();

  bool inline_start = false;
  bool inline_end = true;
  bool block_start = false;
  bool block_end = true;

  if (style.ResolvedIsColumnFlexDirection()) {
    if (is_direction_reverse) {
      std::swap(block_start, block_end);
    }
    if (is_wrap_reverse) {
      std::swap(inline_start, inline_end);
    }
  } else {
    if (is_direction_reverse) {
      std::swap(inline_start, inline_end);
    }
    if (is_wrap_reverse) {
      std::swap(block_start, block_end);
    }
  }

  return LogicalToPhysical(style.GetWritingDirection(), inline_start,
                           inline_end, block_start, block_end);
}

// Returns a column flex cross gap's index within its own flex line, relative to
// the current fragment.
wtf_size_t GapIndexWithinColumnFlexLine(const GapGeometry& gap_geometry,
                                        wtf_size_t gap_index,
                                        wtf_size_t absolute_flex_line_index) {
  // This mapping is only meaningful for column flex, where the cross gaps are
  // the row gaps.
  CHECK(gap_geometry.IsMainDirection(kForColumns));
  const Vector<MainGap>& main_gaps = gap_geometry.GetMainGaps();
  const wtf_size_t main_gap_count = main_gaps.size();
  if (absolute_flex_line_index < main_gap_count) {
    DCHECK_GE(gap_index,
              main_gaps[absolute_flex_line_index].GetCrossGapBeforeStart());
    return gap_index -
           main_gaps[absolute_flex_line_index].GetCrossGapBeforeStart();
  }
  // The cross gaps in the last flex line live in main gap's range of gaps after
  // itself.
  if (main_gap_count > 0 && main_gaps[main_gap_count - 1].HasCrossGapsAfter()) {
    DCHECK_GE(gap_index, main_gaps[main_gap_count - 1].GetCrossGapAfterStart());
    return gap_index - main_gaps[main_gap_count - 1].GetCrossGapAfterStart();
  }
  return gap_index;
}

}  // namespace

bool LayoutFlexibleBox::HasTopOverflow() const {
  return GetOverflowConverter(StyleRef()).Top();
}

bool LayoutFlexibleBox::HasLeftOverflow() const {
  return GetOverflowConverter(StyleRef()).Left();
}

// TODO(crbug.com/364348901): We should be able to remove this method entirely
// when the CustomizableSelect flag is removed or disabled, but it causes a
// crash in the switch-picker-appearance WPT.
bool LayoutFlexibleBox::IsChildAllowed(LayoutObject* object,
                                       const ComputedStyle& style) const {
  const auto* select = DynamicTo<HTMLSelectElement>(GetNode());
  // `style` has the wrong appearance value. `select->GetComputedStyle()` is up
  // to date.
  if (select && select->UsesMenuList() &&
      (!select->GetComputedStyle() ||
       !select->SupportsBaseAppearance(
           select->GetComputedStyle()->EffectiveAppearance()))) [[unlikely]] {
    // For a size=1 appearance:auto <select>, we only render the active option
    // label through the InnerElement. We do not allow adding layout objects
    // for options, optgroups, or any other child nodes in order to hide them
    // while still allowing them to have a ComputedStyle.
    return object->GetNode() == &select->InnerElement();
  }
  return LayoutBlock::IsChildAllowed(object, style);
}

void LayoutFlexibleBox::SetNeedsLayoutForDevtools() {
  SetNeedsLayout(layout_invalidation_reason::kDevtools);
  SetNeedsDevtoolsInfo(true);
}

const DevtoolsFlexInfo* LayoutFlexibleBox::FlexLayoutData() const {
  const wtf_size_t fragment_count = PhysicalFragmentCount();
  DCHECK_GE(fragment_count, 1u);
  // Currently, devtools data is on the first fragment of a fragmented flexbox.
  return GetLayoutResult(0)->FlexLayoutData();
}

wtf_size_t LayoutFlexibleBox::StitchedRowGapIndex(
    const PhysicalBoxFragment& fragment,
    wtf_size_t gap_index,
    std::optional<wtf_size_t> absolute_flex_line_index) const {
  NOT_DESTROYED();
  // This is only used for fragmented gap-decoration painting.
  CHECK(!fragment.IsOnlyForNode());
  const GapGeometry* gap_geometry = fragment.GetGapGeometry();
  if (!gap_geometry) {
    return gap_index;
  }

  // Row flex uses the fragment-relative gap index directly, because a row flex
  // container has a single row-gap sequence per fragment. Column flex adjusts
  // the index to be relative to its owning flex line, because each flex line
  // has its own cross-gap sequence.
  if (absolute_flex_line_index) {
    gap_index = GapIndexWithinColumnFlexLine(*gap_geometry, gap_index,
                                             *absolute_flex_line_index);
  }

  if (const auto* outgoing_break_token = fragment.GetBreakToken()) {
    if (const auto* flex_data =
            DynamicTo<FlexBreakTokenData>(outgoing_break_token->TokenData())) {
      if (!flex_data->gap_data.gap_data_for_rows.empty()) {
        return flex_data->GetFirstUnprocessedRowGapIndex(
                   absolute_flex_line_index) +
               gap_index;
      }
    }
  } else if (const auto* previous_break_token =
                 FindPreviousBreakToken(fragment)) {
    // Last fragments derive their start from the previous break token data.
    if (const auto* flex_data =
            DynamicTo<FlexBreakTokenData>(previous_break_token->TokenData())) {
      const Vector<FlexRowGapBreakTokenData>& row_gap_break_token_data =
          flex_data->gap_data.gap_data_for_rows;
      const wtf_size_t line = absolute_flex_line_index.value_or(0u);
      if (line < row_gap_break_token_data.size()) {
        return row_gap_break_token_data[line].first_row_gap_index +
               row_gap_break_token_data[line].row_gap_count + gap_index;
      }
    }
  }

  return gap_index;
}

}  // namespace blink
