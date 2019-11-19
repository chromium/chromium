// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_truncator.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_box_state.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

NGLineTruncator::NGLineTruncator(const NGLineInfo& line_info)
    : line_style_(&line_info.LineStyle()),
      available_width_(line_info.AvailableWidth()),
      line_direction_(line_info.BaseDirection()) {}

LayoutUnit NGLineTruncator::TruncateLine(
    LayoutUnit line_width,
    NGLineBoxFragmentBuilder::ChildList* line_box,
    NGInlineLayoutStateStack* box_states) {
  // Shape the ellipsis and compute its inline size.
  // The ellipsis is styled according to the line style.
  // https://drafts.csswg.org/css-ui/#ellipsing-details
  const ComputedStyle* ellipsis_style = line_style_.get();
  const Font& font = ellipsis_style->GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);
  String ellipsis_text =
      font_data && font_data->GlyphForCharacter(kHorizontalEllipsisCharacter)
          ? String(&kHorizontalEllipsisCharacter, 1)
          : String(u"...");
  HarfBuzzShaper shaper(ellipsis_text);
  scoped_refptr<ShapeResultView> ellipsis_shape_result =
      ShapeResultView::Create(shaper.Shape(&font, line_direction_).get());
  LayoutUnit ellipsis_width = ellipsis_shape_result->SnappedWidth();

  // Loop children from the logical last to the logical first to determine where
  // to place the ellipsis. Children maybe truncated or moved as part of the
  // process.
  NGLineBoxFragmentBuilder::Child* ellpisized_child = nullptr;
  scoped_refptr<const NGPhysicalTextFragment> truncated_fragment;
  if (IsLtr(line_direction_)) {
    NGLineBoxFragmentBuilder::Child* first_child = line_box->FirstInFlowChild();
    for (auto it = line_box->rbegin(); it != line_box->rend(); it++) {
      auto& child = *it;
      if (EllipsizeChild(line_width, ellipsis_width, &child == first_child,
                         &child, &truncated_fragment)) {
        ellpisized_child = &child;
        break;
      }
    }
  } else {
    NGLineBoxFragmentBuilder::Child* first_child = line_box->LastInFlowChild();
    for (auto& child : *line_box) {
      if (EllipsizeChild(line_width, ellipsis_width, &child == first_child,
                         &child, &truncated_fragment)) {
        ellpisized_child = &child;
        break;
      }
    }
  }

  // Abort if ellipsis could not be placed.
  if (!ellpisized_child)
    return line_width;

  // Truncate the text fragment if needed.
  if (truncated_fragment) {
    DCHECK(ellpisized_child->fragment);
    // In order to preserve layout information before truncated, hide the
    // original fragment and insert a truncated one.
    size_t child_index_to_truncate = ellpisized_child - line_box->begin();
    line_box->InsertChild(child_index_to_truncate + 1);
    box_states->ChildInserted(child_index_to_truncate + 1);
    NGLineBoxFragmentBuilder::Child* child_to_truncate =
        &(*line_box)[child_index_to_truncate];
    ellpisized_child = std::next(child_to_truncate);
    *ellpisized_child = *child_to_truncate;
    HideChild(child_to_truncate);
    LayoutUnit new_inline_size = line_style_->IsHorizontalWritingMode()
                                     ? truncated_fragment->Size().width
                                     : truncated_fragment->Size().height;
    DCHECK_LE(new_inline_size, ellpisized_child->inline_size);
    if (UNLIKELY(IsRtl(line_direction_))) {
      ellpisized_child->offset.inline_offset +=
          ellpisized_child->inline_size - new_inline_size;
    }
    ellpisized_child->inline_size = new_inline_size;
    ellpisized_child->fragment = std::move(truncated_fragment);
  }

  // Create the ellipsis, associating it with the ellipsized child.
  LayoutObject* ellipsized_layout_object =
      ellpisized_child->PhysicalFragment()->GetMutableLayoutObject();
  DCHECK(ellipsized_layout_object && ellipsized_layout_object->IsInline() &&
         (ellipsized_layout_object->IsText() ||
          ellipsized_layout_object->IsAtomicInlineLevel()));
  NGTextFragmentBuilder builder(line_style_->GetWritingMode());
  builder.SetText(ellipsized_layout_object, ellipsis_text, ellipsis_style,
                  true /* is_ellipsis_style */,
                  std::move(ellipsis_shape_result));

  // Now the offset of the ellpisis is determined. Place the ellpisis into the
  // line box.
  LayoutUnit ellipsis_inline_offset =
      IsLtr(line_direction_)
          ? ellpisized_child->offset.inline_offset +
                ellpisized_child->inline_size
          : ellpisized_child->offset.inline_offset - ellipsis_width;
  FontBaseline baseline_type = line_style_->GetFontBaseline();
  NGLineHeightMetrics ellipsis_metrics(font_data->GetFontMetrics(),
                                       baseline_type);
  line_box->AddChild(
      builder.ToTextFragment(),
      LogicalOffset{ellipsis_inline_offset, -ellipsis_metrics.ascent},
      ellipsis_width, 0);
  return std::max(ellipsis_inline_offset + ellipsis_width, line_width);
}

// Hide this child from being painted. Leaves a hidden fragment so that layout
// queries such as |offsetWidth| work as if it is not truncated.
void NGLineTruncator::HideChild(NGLineBoxFragmentBuilder::Child* child) {
  DCHECK(child->HasInFlowFragment());

  if (const NGPhysicalTextFragment* text = child->fragment.get()) {
    child->fragment = text->CloneAsHiddenForPaint();
    return;
  }

  if (const NGLayoutResult* layout_result = child->layout_result.get()) {
    // Need to propagate OOF descendants in this inline-block child.
    const auto& fragment =
        To<NGPhysicalBoxFragment>(layout_result->PhysicalFragment());
    if (fragment.HasOutOfFlowPositionedDescendants())
      return;

    // If this child has self painting layer, not producing fragments will not
    // suppress painting because layers are painted separately. Move it out of
    // the clipping area.
    if (fragment.HasSelfPaintingLayer()) {
      // |available_width_| may not be enough when the containing block has
      // paddings, because clipping is at the content box but ellipsizing is at
      // the padding box. Just move to the max because we don't know paddings,
      // and max should do what we need.
      child->offset.inline_offset = LayoutUnit::NearlyMax();
      return;
    }

    child->layout_result = fragment.CloneAsHiddenForPaint();
    return;
  }

  NOTREACHED();
}

// Return the offset to place the ellipsis.
//
// This function may truncate or move the child so that the ellipsis can fit.
bool NGLineTruncator::EllipsizeChild(
    LayoutUnit line_width,
    LayoutUnit ellipsis_width,
    bool is_first_child,
    NGLineBoxFragmentBuilder::Child* child,
    scoped_refptr<const NGPhysicalTextFragment>* truncated_fragment) {
  DCHECK(truncated_fragment && !*truncated_fragment);

  // Leave out-of-flow children as is.
  if (!child->HasInFlowFragment())
    return false;

  // Inline boxes should not be ellipsized. Usually they will be created in the
  // later phase, but empty inline box are already created.
  if (child->layout_result &&
      child->layout_result->PhysicalFragment().IsInlineBox())
    return false;

  // Can't place ellipsis if this child is completely outside of the box.
  LayoutUnit child_inline_offset =
      IsLtr(line_direction_)
          ? child->offset.inline_offset
          : line_width - (child->offset.inline_offset + child->inline_size);
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
  // If not all of this child can fit, try to truncate.
  space_for_child -= ellipsis_width;
  if (space_for_child < child->inline_size &&
      !TruncateChild(space_for_child, is_first_child, *child,
                     truncated_fragment)) {
    // This child is partially in the box, but it should not be visible because
    // earlier sibling will be truncated and ellipsized.
    if (!is_first_child)
      HideChild(child);
    return false;
  }

  return true;
}

// Truncate the specified child. Returns true if truncated successfully, false
// otherwise.
//
// Note that this function may return true even if it can't fit the child when
// |is_first_child|, because the spec defines that the first character or atomic
// inline-level element on a line must be clipped rather than ellipsed.
// https://drafts.csswg.org/css-ui/#text-overflow
bool NGLineTruncator::TruncateChild(
    LayoutUnit space_for_child,
    bool is_first_child,
    const NGLineBoxFragmentBuilder::Child& child,
    scoped_refptr<const NGPhysicalTextFragment>* truncated_fragment) {
  DCHECK(truncated_fragment && !*truncated_fragment);

  // If the space is not enough, try the next child.
  if (space_for_child <= 0 && !is_first_child)
    return false;

  // Only text fragments can be truncated.
  if (!child.fragment)
    return is_first_child;
  auto& fragment = To<NGPhysicalTextFragment>(*child.fragment);

  // No need to truncate empty results.
  if (!fragment.TextShapeResult())
    return is_first_child;

  // TODO(layout-dev): Add support for OffsetToFit to ShapeResultView to avoid
  // this copy.
  scoped_refptr<blink::ShapeResult> shape_result =
      fragment.TextShapeResult()->CreateShapeResult();
  if (!shape_result)
    return is_first_child;

  // Compute the offset to truncate.
  unsigned new_length = shape_result->OffsetToFit(
      IsLtr(line_direction_) ? space_for_child
                             : shape_result->Width() - space_for_child,
      line_direction_);
  DCHECK_LE(new_length, fragment.TextLength());
  if (!new_length || new_length == fragment.TextLength()) {
    if (!is_first_child)
      return false;
    new_length = !new_length ? 1 : new_length - 1;
  }

  // Truncate the text fragment.
  *truncated_fragment =
      line_direction_ == shape_result->Direction()
          ? fragment.TrimText(fragment.StartOffset(),
                              fragment.StartOffset() + new_length)
          : fragment.TrimText(fragment.StartOffset() + new_length,
                              fragment.EndOffset());
  return true;
}

}  // namespace blink
