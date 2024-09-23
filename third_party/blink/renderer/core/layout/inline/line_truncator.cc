// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/line_truncator.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/layout/inline/inline_box_state.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_result.h"
#include "third_party/blink/renderer/core/layout/inline/line_info.h"
#include "third_party/blink/renderer/core/layout/inline/logical_line_item.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

namespace {

bool IsLeftMostOffset(const ShapeResult& shape_result, unsigned offset) {
  if (shape_result.IsRtl())
    return offset == shape_result.NumCharacters();
  return offset == 0;
}

bool IsRightMostOffset(const ShapeResult& shape_result, unsigned offset) {
  if (shape_result.IsRtl())
    return offset == 0;
  return offset == shape_result.NumCharacters();
}

}  // namespace

LineTruncator::LineTruncator(const LineInfo& line_info)
    : line_style_(&line_info.LineStyle()),
      available_width_(line_info.AvailableWidth() - line_info.TextIndent()),
      line_direction_(line_info.BaseDirection()) {}

const ComputedStyle& LineTruncator::EllipsisStyle() const {
  // The ellipsis is styled according to the line style.
  // https://drafts.csswg.org/css-ui/#ellipsing-details
  DCHECK(line_style_);
  return *line_style_;
}

void LineTruncator::SetupEllipsis() {
  const Font& font = EllipsisStyle().GetFont();
  ellipsis_font_data_ = font.PrimaryFont();
  DCHECK(ellipsis_font_data_);
  ellipsis_text_ =
      ellipsis_font_data_ && ellipsis_font_data_->GlyphForCharacter(
                                 kHorizontalEllipsisCharacter)
          ? String(&kHorizontalEllipsisCharacter, 1u)
          : String(u"...");
  HarfBuzzShaper shaper(ellipsis_text_);
  ellipsis_shape_result_ =
      ShapeResultView::Create(shaper.Shape(&font, line_direction_));
  ellipsis_width_ = ellipsis_shape_result_->SnappedWidth();
}

LayoutUnit LineTruncator::PlaceEllipsisNextTo(
    LogicalLineItems* line_box,
    LogicalLineItem* ellipsized_child) {
  // Create the ellipsis, associating it with the ellipsized child.
  DCHECK(ellipsized_child->HasInFlowFragment());
  const LayoutObject* ellipsized_layout_object =
      ellipsized_child->GetMutableLayoutObject();
  DCHECK(ellipsized_layout_object);
  DCHECK(ellipsized_layout_object->IsInline());
  DCHECK(ellipsized_layout_object->IsText() ||
         ellipsized_layout_object->IsAtomicInlineLevel());

  // Now the offset of the ellpisis is determined. Place the ellpisis into the
  // line box.
  LayoutUnit ellipsis_inline_offset =
      IsLtr(line_direction_)
          ? ellipsized_child->InlineOffset() + ellipsized_child->inline_size
          : ellipsized_child->InlineOffset() - ellipsis_width_;
  FontHeight ellipsis_metrics;
  DCHECK(ellipsis_font_data_);
  if (ellipsis_font_data_) {
    ellipsis_metrics = ellipsis_font_data_->GetFontMetrics().GetFontHeight(
        line_style_->GetFontBaseline());
  }

  DCHECK(ellipsis_text_);
  DCHECK(ellipsis_shape_result_);
  line_box->AddChild(
      *ellipsized_layout_object, StyleVariant::kEllipsis,
      ellipsis_shape_result_, ellipsis_text_,
      LogicalRect(ellipsis_inline_offset, -ellipsis_metrics.ascent,
                  ellipsis_width_, ellipsis_metrics.LineHeight()),
      /* bidi_level */ 0);
  return ellipsis_inline_offset;
}

wtf_size_t LineTruncator::AddTruncatedChild(
    wtf_size_t source_index,
    bool leave_one_character,
    LayoutUnit position,
    TextDirection edge,
    LogicalLineItems* line_box,
    InlineLayoutStateStack* box_states) {
  LogicalLineItems& line = *line_box;
  const LogicalLineItem& source_item = line[source_index];
  DCHECK(source_item.shape_result);
  const ShapeResult* shape_result =
      source_item.shape_result->CreateShapeResult();
  unsigned text_offset = shape_result->OffsetToFit(position, edge);
  if (IsLtr(edge) ? IsLeftMostOffset(*shape_result, text_offset)
                  : IsRightMostOffset(*shape_result, text_offset)) {
    if (!leave_one_character)
      return kDidNotAddChild;
    text_offset =
        shape_result->OffsetToFit(shape_result->PositionForOffset(
                                      IsRtl(edge) == shape_result->IsRtl()
                                          ? 1
                                          : shape_result->NumCharacters() - 1),
                                  edge);
  }

  const wtf_size_t new_index = line.size();
  line.AddChild(TruncateText(source_item, *shape_result, text_offset, edge));
  box_states->ChildInserted(new_index);
  return new_index;
}

LayoutUnit LineTruncator::TruncateLine(LayoutUnit line_width,
                                       LogicalLineItems* line_box,
                                       InlineLayoutStateStack* box_states) {
  // Shape the ellipsis and compute its inline size.
  SetupEllipsis();

  // Loop children from the logical last to the logical first to determine where
  // to place the ellipsis. Children maybe truncated or moved as part of the
  // process.
  LogicalLineItem* ellipsized_child = nullptr;
  std::optional<LogicalLineItem> truncated_child;
  if (IsLtr(line_direction_)) {
    LogicalLineItem* first_child = line_box->FirstInFlowChild();
    for (auto& child : base::Reversed(*line_box)) {
      if (EllipsizeChild(line_width, ellipsis_width_, &child == first_child,
                         &child, &truncated_child)) {
        ellipsized_child = &child;
        break;
      }
    }
  } else {
    LogicalLineItem* first_child = line_box->LastInFlowChild();
    for (auto& child : *line_box) {
      if (EllipsizeChild(line_width, ellipsis_width_, &child == first_child,
                         &child, &truncated_child)) {
        ellipsized_child = &child;
        break;
      }
    }
  }

  // Abort if ellipsis could not be placed.
  if (!ellipsized_child)
    return line_width;

  // Truncate the text fragment if needed.
  if (truncated_child) {
    // In order to preserve layout information before truncated, hide the
    // original fragment and insert a truncated one.
    unsigned child_index_to_truncate =
        base::checked_cast<unsigned>(ellipsized_child - &*line_box->begin());
    line_box->InsertChild(child_index_to_truncate + 1,
                          std::move(*truncated_child));
    box_states->ChildInserted(child_index_to_truncate + 1);
    LogicalLineItem* child_to_truncate = &(*line_box)[child_index_to_truncate];
    ellipsized_child = std::next(child_to_truncate);

    HideChild(child_to_truncate);
    DCHECK_LE(ellipsized_child->inline_size, child_to_truncate->inline_size);
    if (IsRtl(line_direction_)) [[unlikely]] {
      ellipsized_child->rect.offset.inline_offset +=
          child_to_truncate->inline_size - ellipsized_child->inline_size;
    }
  }

  // Create the ellipsis, associating it with the ellipsized child.
  LayoutUnit ellipsis_inline_offset =
      PlaceEllipsisNextTo(line_box, ellipsized_child);
  return std::max(ellipsis_inline_offset + ellipsis_width_, line_width);
}

// This function was designed to work only with <input type=file>.
// We assume the line box contains:
//     (Optional) children without in-flow fragments
//     Children with in-flow fragments, and
//     (Optional) children without in-flow fragments
// in this order, and the children with in-flow fragments have no padding,
// no border, and no margin.
// Children with IsPlaceholder() can appear anywhere.
LayoutUnit LineTruncator::TruncateLineInTheMiddle(
    LayoutUnit line_width,
    LogicalLineItems* line_box,
    InlineLayoutStateStack* box_states) {
  // Shape the ellipsis and compute its inline size.
  SetupEllipsis();

  LogicalLineItems& line = *line_box;
  wtf_size_t initial_index_left = kNotFound;
  wtf_size_t initial_index_right = kNotFound;
  for (wtf_size_t i = 0; i < line_box->size(); ++i) {
    auto& child = line[i];
    if (child.IsPlaceholder())
      continue;
    if (!child.shape_result) {
      if (initial_index_right != kNotFound)
        break;
      continue;
    }
    // Skip pseudo elements like ::before.
    if (!child.GetNode())
      continue;

    if (initial_index_left == kNotFound)
      initial_index_left = i;
    initial_index_right = i;
  }
  // There are no truncatable children.
  if (initial_index_left == kNotFound)
    return line_width;
  DCHECK_NE(initial_index_right, kNotFound);
  DCHECK(line[initial_index_left].HasInFlowFragment());
  DCHECK(line[initial_index_right].HasInFlowFragment());

  // line[]:
  //     s s s p f f p f f s s
  //             ^       ^
  // initial_index_left  |
  //                     initial_index_right
  //   s: child without in-flow fragment
  //   p: placeholder child
  //   f: child with in-flow fragment

  const LayoutUnit static_width_left = line[initial_index_left].InlineOffset();
  LayoutUnit static_width_right = LayoutUnit(0);
  if (initial_index_right + 1 < line.size()) {
    const LogicalLineItem& item = line[initial_index_right];
    LayoutUnit truncatable_right = item.InlineOffset() + item.inline_size;
    // |line_width| and/or truncatable_right might be saturated.
    if (line_width <= truncatable_right) {
      return line_width;
    }
    // We can do nothing if the right-side static item sticks out to the both
    // sides.
    if (truncatable_right < 0) {
      return line_width;
    }
    static_width_right = line_width - truncatable_right;
  }
  const LayoutUnit available_width =
      available_width_ - static_width_left - static_width_right;
  if (available_width <= ellipsis_width_)
    return line_width;
  LayoutUnit available_width_left = (available_width - ellipsis_width_) / 2;
  LayoutUnit available_width_right = available_width_left;

  // Children for ellipsis and truncated fragments will have index which
  // is >= new_child_start.
  const wtf_size_t new_child_start = line.size();

  wtf_size_t index_left = initial_index_left;
  wtf_size_t index_right = initial_index_right;

  if (IsLtr(line_direction_)) {
    // Find truncation point at the left, truncate, and add an ellipsis.
    while (available_width_left >= line[index_left].inline_size) {
      available_width_left -= line[index_left++].inline_size;
      if (index_left >= line.size()) {
        // We have a logic bug. Do nothing.
        return line_width;
      }
    }
    DCHECK_LE(index_left, index_right);
    DCHECK(!line[index_left].IsPlaceholder());
    wtf_size_t new_index = AddTruncatedChild(
        index_left, index_left == initial_index_left, available_width_left,
        TextDirection::kLtr, line_box, box_states);
    if (new_index == kDidNotAddChild) {
      DCHECK_GT(index_left, initial_index_left);
      DCHECK_GT(index_left, 0u);
      wtf_size_t i = index_left;
      while (!line[--i].HasInFlowFragment())
        DCHECK(line[i].IsPlaceholder());
      PlaceEllipsisNextTo(line_box, &line[i]);
      available_width_right += available_width_left;
    } else {
      PlaceEllipsisNextTo(line_box, &line[new_index]);
      available_width_right +=
          available_width_left -
          line[new_index].inline_size.ClampNegativeToZero();
    }

    // Find truncation point at the right.
    while (available_width_right >= line[index_right].inline_size) {
      available_width_right -= line[index_right].inline_size;
      if (index_right == 0) {
        // We have a logic bug. We proceed anyway because |line| was already
        // modified.
        break;
      }
      --index_right;
    }
    LayoutUnit new_modified_right_offset =
        line[line.size() - 1].InlineOffset() + ellipsis_width_;
    DCHECK_LE(index_left, index_right);
    DCHECK(!line[index_right].IsPlaceholder());
    if (available_width_right > 0) {
      new_index = AddTruncatedChild(
          index_right, false,
          line[index_right].inline_size - available_width_right,
          TextDirection::kRtl, line_box, box_states);
      if (new_index != kDidNotAddChild) {
        line[new_index].rect.offset.inline_offset = new_modified_right_offset;
        new_modified_right_offset += line[new_index].inline_size;
      }
    }
    // Shift unchanged children at the right of the truncated child.
    // It's ok to modify existing children's offsets because they are not
    // web-exposed.
    LayoutUnit offset_diff = line[index_right].InlineOffset() +
                             line[index_right].inline_size -
                             new_modified_right_offset;
    for (wtf_size_t i = index_right + 1; i < new_child_start; ++i)
      line[i].rect.offset.inline_offset -= offset_diff;
    line_width -= offset_diff;

  } else {
    // Find truncation point at the right, truncate, and add an ellipsis.
    while (available_width_right >= line[index_right].inline_size) {
      available_width_right -= line[index_right].inline_size;
      if (index_right == 0) {
        // We have a logic bug. Do nothing.
        return line_width;
      }
      --index_right;
    }
    DCHECK_LE(index_left, index_right);
    DCHECK(!line[index_right].IsPlaceholder());
    wtf_size_t new_index =
        AddTruncatedChild(index_right, index_right == initial_index_right,
                          line[index_right].inline_size - available_width_right,
                          TextDirection::kRtl, line_box, box_states);
    if (new_index == kDidNotAddChild) {
      DCHECK_LT(index_right, initial_index_right);
      wtf_size_t i = index_right;
      while (!line[++i].HasInFlowFragment())
        DCHECK(line[i].IsPlaceholder());
      PlaceEllipsisNextTo(line_box, &line[i]);
      available_width_left += available_width_right;
    } else {
      line[new_index].rect.offset.inline_offset +=
          line[index_right].inline_size - line[new_index].inline_size;
      PlaceEllipsisNextTo(line_box, &line[new_index]);
      available_width_left += available_width_right -
                              line[new_index].inline_size.ClampNegativeToZero();
    }
    LayoutUnit ellipsis_offset = line[line.size() - 1].InlineOffset();

    // Find truncation point at the left.
    while (available_width_left >= line[index_left].inline_size) {
      available_width_left -= line[index_left++].inline_size;
      if (index_left >= line.size()) {
        // We have a logic bug. We proceed anyway because |line| was already
        // modified.
        break;
      }
    }
    DCHECK_LE(index_left, index_right);
    DCHECK(!line[index_left].IsPlaceholder());
    if (available_width_left > 0) {
      new_index = AddTruncatedChild(index_left, false, available_width_left,
                                    TextDirection::kLtr, line_box, box_states);
      if (new_index != kDidNotAddChild) {
        line[new_index].rect.offset.inline_offset =
            ellipsis_offset - line[new_index].inline_size;
      }
    }

    // Shift unchanged children at the left of the truncated child.
    // It's ok to modify existing children's offsets because they are not
    // web-exposed.
    LayoutUnit offset_diff =
        line[line.size() - 1].InlineOffset() - line[index_left].InlineOffset();
    for (wtf_size_t i = index_left; i > 0; --i)
      line[i - 1].rect.offset.inline_offset += offset_diff;
    line_width -= offset_diff;
  }
  // Hide left/right truncated children and children between them.
  for (wtf_size_t i = index_left; i <= index_right; ++i) {
    if (line[i].HasInFlowFragment())
      HideChild(&line[i]);
  }

  return line_width;
}

// Hide this child from being painted. Leaves a hidden fragment so that layout
// queries such as |offsetWidth| work as if it is not truncated.
void LineTruncator::HideChild(LogicalLineItem* child) {
  DCHECK(child->HasInFlowFragment());

  if (const LayoutResult* layout_result = child->layout_result) {
    // Need to propagate OOF descendants in this inline-block child.
    const auto& fragment =
        To<PhysicalBoxFragment>(layout_result->GetPhysicalFragment());
    if (fragment.HasOutOfFlowPositionedDescendants())
      return;

    // Truncate this object. Atomic inline is monolithic.
    DCHECK(fragment.IsMonolithic());
    LayoutObject* layout_object = fragment.GetMutableLayoutObject();
    DCHECK(layout_object);
    DCHECK(layout_object->IsAtomicInlineLevel());
    layout_object->SetIsTruncated(true);
    return;
  }

  if (child->inline_item) {
    child->is_hidden_for_paint = true;
    return;
  }

  NOTREACHED_IN_MIGRATION();
}

// Return the offset to place the ellipsis.
//
// This function may truncate or move the child so that the ellipsis can fit.
bool LineTruncator::EllipsizeChild(
    LayoutUnit line_width,
    LayoutUnit ellipsis_width,
    bool is_first_child,
    LogicalLineItem* child,
    std::optional<LogicalLineItem>* truncated_child) {
  DCHECK(truncated_child && !*truncated_child);

  // Leave out-of-flow children as is.
  if (!child->HasInFlowFragment())
    return false;

  // Inline boxes should not be ellipsized. Usually they will be created in the
  // later phase, but empty inline box are already created.
  if (child->IsInlineBox())
    return false;

  // Can't place ellipsis if this child is completely outside of the box.
  LayoutUnit child_inline_offset =
      IsLtr(line_direction_)
          ? child->InlineOffset()
          : line_width - (child->InlineOffset() + child->inline_size);
  LayoutUnit space_for_child = available_width_ - child_inline_offset;
  if (space_for_child <= 0) {
    // This child is outside of the content box, but we still need to hide it.
    // When the box has paddings, this child outside of the content box maybe
    // still inside of the clipping box.
    if (!is_first_child)
      HideChild(child);
    return false;
  }

  // At least part of this child is in the box.
  // If |child| can fit in the space, truncate this line at the end of |child|.
  space_for_child -= ellipsis_width;
  if (space_for_child >= child->inline_size)
    return true;

  // If not all of this child can fit, try to truncate.
  if (TruncateChild(space_for_child, is_first_child, *child, truncated_child))
    return true;

  // This child is partially in the box, but it can't be truncated to fit. It
  // should not be visible because earlier sibling will be truncated.
  if (!is_first_child)
    HideChild(child);
  return false;
}

// Truncate the specified child. Returns true if truncated successfully, false
// otherwise.
//
// Note that this function may return true even if it can't fit the child when
// |is_first_child|, because the spec defines that the first character or atomic
// inline-level element on a line must be clipped rather than ellipsed.
// https://drafts.csswg.org/css-ui/#text-overflow
bool LineTruncator::TruncateChild(
    LayoutUnit space_for_child,
    bool is_first_child,
    const LogicalLineItem& child,
    std::optional<LogicalLineItem>* truncated_child) {
  DCHECK(truncated_child && !*truncated_child);

  // If the space is not enough, try the next child.
  if (space_for_child <= 0 && !is_first_child)
    return false;

  // Only text fragments can be truncated.
  if (!child.shape_result)
    return is_first_child;

  // TODO(layout-dev): Add support for OffsetToFit to ShapeResultView to avoid
  // this copy.
  const ShapeResult* shape_result = child.shape_result->CreateShapeResult();
  DCHECK(shape_result);
  const TextOffsetRange original_offset = child.text_offset;
  // Compute the offset to truncate.
  unsigned offset_to_fit = shape_result->OffsetToFit(
      IsLtr(line_direction_) ? space_for_child
                             : shape_result->Width() - space_for_child,
      line_direction_);
  DCHECK_LE(offset_to_fit, original_offset.Length());
  if (!offset_to_fit || offset_to_fit == original_offset.Length()) {
    if (!is_first_child)
      return false;
    offset_to_fit = !offset_to_fit ? 1 : offset_to_fit - 1;
  }
  *truncated_child =
      TruncateText(child, *shape_result, offset_to_fit, line_direction_);
  return true;
}

LogicalLineItem LineTruncator::TruncateText(const LogicalLineItem& item,
                                            const ShapeResult& shape_result,
                                            unsigned offset_to_fit,
                                            TextDirection direction) {
  const TextOffsetRange new_text_offset =
      direction == shape_result.Direction()
          ? TextOffsetRange(item.StartOffset(),
                            item.StartOffset() + offset_to_fit)
          : TextOffsetRange(item.StartOffset() + offset_to_fit,
                            item.EndOffset());
  const ShapeResultView* new_shape_result = ShapeResultView::Create(
      &shape_result, new_text_offset.start, new_text_offset.end);
  DCHECK(item.inline_item);
  return LogicalLineItem(item, new_shape_result, new_text_offset);
}

}  // namespace blink
