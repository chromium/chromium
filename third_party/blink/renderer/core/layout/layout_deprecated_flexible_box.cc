/*
 * This file is part of the layout object implementation for KHTML.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/layout/layout_deprecated_flexible_box.h"

#include <algorithm>
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/layout/text_run_constructor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

class FlexBoxIterator {
 public:
  FlexBoxIterator(LayoutDeprecatedFlexibleBox* parent)
      : box_(parent), largest_ordinal_(1) {
    if (box_->StyleRef().BoxOrient() == EBoxOrient::kHorizontal &&
        !box_->StyleRef().IsLeftToRightDirection())
      forward_ = box_->StyleRef().BoxDirection() != EBoxDirection::kNormal;
    else
      forward_ = box_->StyleRef().BoxDirection() == EBoxDirection::kNormal;
    if (!forward_) {
      // No choice, since we're going backwards, we have to find out the highest
      // ordinal up front.
      LayoutBox* child = box_->FirstChildBox();
      while (child) {
        if (child->StyleRef().BoxOrdinalGroup() > largest_ordinal_)
          largest_ordinal_ = child->StyleRef().BoxOrdinalGroup();
        child = child->NextSiblingBox();
      }
    }

    Reset();
  }

  void Reset() {
    current_child_ = nullptr;
    natural_current_child_ = nullptr;
    ordinal_iteration_ = -1;
  }

  LayoutBox* First() {
    Reset();
    return Next();
  }

  LayoutBox* Next() {
    do {
      if (!current_child_) {
        ++ordinal_iteration_;

        if (!ordinal_iteration_) {
          current_ordinal_ = forward_ ? 1 : largest_ordinal_;
        } else {
          if (static_cast<size_t>(ordinal_iteration_) >=
              ordinal_values_.size() + 1)
            return nullptr;

          // Only copy+sort the values once per layout even if the iterator is
          // reset.
          if (ordinal_values_.size() != sorted_ordinal_values_.size()) {
            CopyToVector(ordinal_values_, sorted_ordinal_values_);
            std::sort(sorted_ordinal_values_.begin(),
                      sorted_ordinal_values_.end());
          }
          current_ordinal_ =
              forward_ ? sorted_ordinal_values_[ordinal_iteration_ - 1]
                       : sorted_ordinal_values_[sorted_ordinal_values_.size() -
                                                ordinal_iteration_];
        }

        current_child_ =
            forward_ ? box_->FirstChildBox() : box_->LastChildBox();
      } else {
        current_child_ = forward_ ? current_child_->NextSiblingBox()
                                  : current_child_->PreviousSiblingBox();
      }

      if (current_child_ && NotFirstOrdinalValue())
        ordinal_values_.insert(current_child_->StyleRef().BoxOrdinalGroup());
    } while (!current_child_ || (!current_child_->IsAnonymous() &&
                                 current_child_->StyleRef().BoxOrdinalGroup() !=
                                     current_ordinal_));

    // This peice of code just exists for detecting if this iterator actually
    // does something other than returning the default order.
    if (!natural_current_child_)
      natural_current_child_ = box_->FirstChildBox();
    else
      natural_current_child_ = natural_current_child_->NextSiblingBox();

    if (natural_current_child_ != current_child_) {
      UseCounter::Count(box_->GetDocument(),
                        WebFeature::kWebkitBoxNotDefaultOrder);
    }

    return current_child_;
  }

 private:
  bool NotFirstOrdinalValue() {
    unsigned first_ordinal_value = forward_ ? 1 : largest_ordinal_;
    return current_ordinal_ == first_ordinal_value &&
           current_child_->StyleRef().BoxOrdinalGroup() != first_ordinal_value;
  }

  LayoutDeprecatedFlexibleBox* box_;
  LayoutBox* current_child_;
  LayoutBox* natural_current_child_;
  bool forward_;
  unsigned current_ordinal_;
  unsigned largest_ordinal_;
  HashSet<unsigned> ordinal_values_;
  Vector<unsigned> sorted_ordinal_values_;
  int ordinal_iteration_;
};

// Helper methods for obtaining the last line, computing line counts and heights
// for line counts
// (crawling into blocks).
static bool ShouldCheckLines(LayoutBlockFlow* block_flow) {
  return !block_flow->IsFloatingOrOutOfFlowPositioned() &&
         block_flow->StyleRef().Height().IsAuto();
}

static int GetHeightForLineCount(const LayoutBlockFlow* block_flow,
                                 int line_count,
                                 bool include_bottom,
                                 int& count) {
  if (block_flow->ChildrenInline()) {
    for (RootInlineBox* box = block_flow->FirstRootBox(); box;
         box = box->NextRootBox()) {
      if (++count == line_count)
        return (box->LineBottomWithLeading() +
                (include_bottom ? (block_flow->BorderBottom() +
                                   block_flow->PaddingBottom())
                                : LayoutUnit()))
            .ToInt();
    }
    return -1;
  }

  LayoutBox* normal_flow_child_without_lines = nullptr;
  for (LayoutBox* obj = block_flow->FirstChildBox(); obj;
       obj = obj->NextSiblingBox()) {
    auto* block_flow = DynamicTo<LayoutBlockFlow>(obj);
    if (block_flow && ShouldCheckLines(block_flow)) {
      int result = GetHeightForLineCount(block_flow, line_count, false, count);
      if (result != -1)
        return (result + obj->Location().Y() +
                (include_bottom ? (block_flow->BorderBottom() +
                                   block_flow->PaddingBottom())
                                : LayoutUnit()))
            .ToInt();
    } else if (!obj->IsFloatingOrOutOfFlowPositioned()) {
      normal_flow_child_without_lines = obj;
    }
  }
  if (normal_flow_child_without_lines && line_count == 0)
    return (normal_flow_child_without_lines->Location().Y() +
            normal_flow_child_without_lines->Size().Height())
        .ToInt();

  return -1;
}

static RootInlineBox* LineAtIndex(const LayoutBlockFlow* block_flow, int i) {
  DCHECK_GE(i, 0);

  if (block_flow->ChildrenInline()) {
    for (RootInlineBox* box = block_flow->FirstRootBox(); box;
         box = box->NextRootBox()) {
      if (!i--)
        return box;
    }
    return nullptr;
  }
  for (LayoutObject* child = block_flow->FirstChild(); child;
       child = child->NextSibling()) {
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
    if (!child_block_flow)
      continue;
    if (!ShouldCheckLines(child_block_flow))
      continue;
    if (RootInlineBox* box = LineAtIndex(child_block_flow, i))
      return box;
  }

  return nullptr;
}

static int LineCount(const LayoutBlockFlow* block_flow,
                     const RootInlineBox* stop_root_inline_box = nullptr,
                     bool* found = nullptr) {
  int count = 0;
  if (block_flow->ChildrenInline()) {
    for (RootInlineBox* box = block_flow->FirstRootBox(); box;
         box = box->NextRootBox()) {
      count++;
      if (box == stop_root_inline_box) {
        if (found)
          *found = true;
        break;
      }
    }
    return count;
  }
  for (LayoutObject* obj = block_flow->FirstChild(); obj;
       obj = obj->NextSibling()) {
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(obj);
    if (!child_block_flow)
      continue;
    if (!ShouldCheckLines(child_block_flow))
      continue;
    bool recursive_found = false;
    count +=
        LineCount(child_block_flow, stop_root_inline_box, &recursive_found);
    if (recursive_found) {
      if (found)
        *found = true;
      break;
    }
  }
  return count;
}

static void ClearTruncation(LayoutBlockFlow* block_flow) {
  if (block_flow->ChildrenInline() && block_flow->HasMarkupTruncation()) {
    block_flow->SetHasMarkupTruncation(false);
    for (RootInlineBox* box = block_flow->FirstRootBox(); box;
         box = box->NextRootBox())
      box->ClearTruncation();
    return;
  }
  for (LayoutObject* obj = block_flow->FirstChild(); obj;
       obj = obj->NextSibling()) {
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(obj);
    if (!child_block_flow)
      continue;
    if (ShouldCheckLines(child_block_flow))
      ClearTruncation(child_block_flow);
  }
}

LayoutDeprecatedFlexibleBox::LayoutDeprecatedFlexibleBox(Element& element)
    : LayoutBlock(&element) {
  DCHECK(!ChildrenInline());
  stretching_children_ = false;
  if (!IsAnonymous()) {
    const KURL& url = GetDocument().Url();
    if (url.ProtocolIs("chrome")) {
      UseCounter::Count(GetDocument(), WebFeature::kDeprecatedFlexboxChrome);
    } else if (url.ProtocolIs("chrome-extension")) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kDeprecatedFlexboxChromeExtension);
    } else {
      UseCounter::Count(GetDocument(),
                        WebFeature::kDeprecatedFlexboxWebContent);
    }
  }
}

LayoutDeprecatedFlexibleBox::~LayoutDeprecatedFlexibleBox() = default;

static LayoutUnit MarginWidthForChild(LayoutBox* child) {
  // A margin basically has three types: fixed, percentage, and auto (variable).
  // Auto and percentage margins simply become 0 when computing min/max width.
  // Fixed margins can be added in as is.
  const Length& margin_left = child->StyleRef().MarginLeft();
  const Length& margin_right = child->StyleRef().MarginRight();
  LayoutUnit margin;
  if (margin_left.IsFixed())
    margin += margin_left.Value();
  if (margin_right.IsFixed())
    margin += margin_right.Value();
  return margin;
}

static LayoutUnit WidthForChild(LayoutBox* child) {
  if (child->HasOverrideLogicalWidth())
    return child->OverrideLogicalWidth();
  return child->LogicalWidth();
}

static LayoutUnit ContentWidthForChild(LayoutBox* child) {
  // TODO(rego): Shouldn't we subtract the scrollbar width too?
  return (WidthForChild(child) - child->BorderAndPaddingLogicalWidth())
      .ClampNegativeToZero();
}

static LayoutUnit HeightForChild(LayoutBox* child) {
  if (child->HasOverrideLogicalHeight())
    return child->OverrideLogicalHeight();
  return child->LogicalHeight();
}

static LayoutUnit ContentHeightForChild(LayoutBox* child) {
  // TODO(rego): Shouldn't we subtract the scrollbar height too?
  return (HeightForChild(child) - child->BorderAndPaddingLogicalHeight())
      .ClampNegativeToZero();
}

void LayoutDeprecatedFlexibleBox::StyleWillChange(
    StyleDifference diff,
    const ComputedStyle& new_style) {
  const ComputedStyle* old_style = Style();
  if (old_style && old_style->HasLineClamp() && !new_style.HasLineClamp())
    ClearLineClamp();

  LayoutBlock::StyleWillChange(diff, new_style);
}

void LayoutDeprecatedFlexibleBox::ComputeIntrinsicLogicalWidths(
    LayoutUnit& min_logical_width,
    LayoutUnit& max_logical_width) const {
  if (IsVertical()) {
    for (LayoutBox* child = FirstChildBox(); child;
         child = child->NextSiblingBox()) {
      if (child->IsOutOfFlowPositioned())
        continue;

      LayoutUnit margin = MarginWidthForChild(child);
      LayoutUnit width = child->MinPreferredLogicalWidth() + margin;
      min_logical_width = std::max(width, min_logical_width);

      width = child->MaxPreferredLogicalWidth() + margin;
      max_logical_width = std::max(width, max_logical_width);
    }
  } else {
    for (LayoutBox* child = FirstChildBox(); child;
         child = child->NextSiblingBox()) {
      if (child->IsOutOfFlowPositioned())
        continue;

      LayoutUnit margin = MarginWidthForChild(child);
      min_logical_width += child->MinPreferredLogicalWidth() + margin;
      max_logical_width += child->MaxPreferredLogicalWidth() + margin;
    }
  }

  max_logical_width = std::max(min_logical_width, max_logical_width);

  LayoutUnit scrollbar_width(ScrollbarLogicalWidth());
  max_logical_width += scrollbar_width;
  min_logical_width += scrollbar_width;
}

void LayoutDeprecatedFlexibleBox::UpdateBlockLayout(bool relayout_children) {
  DCHECK(NeedsLayout());

  UseCounter::Count(GetDocument(), WebFeature::kWebkitBoxLayout);

  if (StyleRef().BoxAlign() != ComputedStyleInitialValues::InitialBoxAlign())
    UseCounter::Count(GetDocument(), WebFeature::kWebkitBoxAlignNotInitial);

  if (StyleRef().BoxDirection() !=
      ComputedStyleInitialValues::InitialBoxDirection())
    UseCounter::Count(GetDocument(), WebFeature::kWebkitBoxDirectionNotInitial);

  if (StyleRef().BoxPack() != ComputedStyleInitialValues::InitialBoxPack())
    UseCounter::Count(GetDocument(), WebFeature::kWebkitBoxPackNotInitial);

  if (!FirstChildBox()) {
    UseCounter::Count(GetDocument(), WebFeature::kWebkitBoxNoChildren);
  } else if (!FirstChildBox()->NextSiblingBox()) {
    UseCounter::Count(GetDocument(), WebFeature::kWebkitBoxOneChild);

    auto* first_child_block_flow = DynamicTo<LayoutBlockFlow>(FirstChildBox());
    if (first_child_block_flow && first_child_block_flow->ChildrenInline()) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kWebkitBoxOneChildIsLayoutBlockFlowInline);
    }
  } else {
    UseCounter::Count(GetDocument(), WebFeature::kWebkitBoxManyChildren);
  }

  if (!relayout_children && SimplifiedLayout())
    return;

  {
    // LayoutState needs this deliberate scope to pop before paint invalidation.
    LayoutState state(*this);

    LayoutSize previous_size = Size();

    UpdateLogicalWidth();
    UpdateLogicalHeight();

    TextAutosizer::LayoutScope text_autosizer_layout_scope(this);

    if (previous_size != Size() ||
        (Parent()->StyleRef().IsDeprecatedWebkitBox() &&
         Parent()->StyleRef().BoxOrient() == EBoxOrient::kHorizontal &&
         Parent()->StyleRef().BoxAlign() == EBoxAlignment::kStretch))
      relayout_children = true;

    SetHeight(LayoutUnit());

    stretching_children_ = false;

    if (IsHorizontal()) {
      UseCounter::Count(GetDocument(), WebFeature::kWebkitBoxLayoutHorizontal);
      LayoutHorizontalBox(relayout_children);
    } else {
      UseCounter::Count(GetDocument(), WebFeature::kWebkitBoxLayoutVertical);
      LayoutVerticalBox(relayout_children);
    }

    LayoutUnit old_client_after_edge = ClientLogicalBottom();
    UpdateLogicalHeight();

    if (previous_size.Height() != Size().Height())
      relayout_children = true;

    LayoutPositionedObjects(relayout_children || IsDocumentElement());

    ComputeLayoutOverflow(old_client_after_edge);
  }

  UpdateAfterLayout();

  ClearNeedsLayout();
}

// The first walk over our kids is to find out if we have any flexible children.
static void GatherFlexChildrenInfo(FlexBoxIterator& iterator,
                                   Document& document,
                                   bool relayout_children,
                                   bool& have_flex) {
  for (LayoutBox* child = iterator.First(); child; child = iterator.Next()) {
    if (child->StyleRef().BoxFlex() !=
        ComputedStyleInitialValues::InitialBoxFlex())
      UseCounter::Count(document, WebFeature::kWebkitBoxChildFlexNotInitial);

    if (child->StyleRef().BoxOrdinalGroup() !=
        ComputedStyleInitialValues::InitialBoxOrdinalGroup()) {
      UseCounter::Count(document,
                        WebFeature::kWebkitBoxChildOrdinalGroupNotInitial);
    }

    // Check to see if this child flexes.
    if (!child->IsOutOfFlowPositioned() && child->StyleRef().BoxFlex() > 0.0f) {
      // We always have to lay out flexible objects again, since the flex
      // distribution
      // may have changed, and we need to reallocate space.
      child->ClearOverrideSize();
      if (!relayout_children)
        child->SetChildNeedsLayout(kMarkOnlyThis);
      have_flex = true;
    }
  }
}

void LayoutDeprecatedFlexibleBox::LayoutHorizontalBox(bool relayout_children) {
  LayoutUnit to_add =
      BorderBottom() + PaddingBottom() + HorizontalScrollbarHeight();
  LayoutUnit y_pos = BorderTop() + PaddingTop();
  LayoutUnit x_pos = BorderLeft() + PaddingLeft();
  bool height_specified = false;
  bool paginated = View()->GetLayoutState()->IsPaginated();
  LayoutUnit old_height;

  LayoutUnit remaining_space;

  FlexBoxIterator iterator(this);
  bool have_flex = false, flexing_children = false;
  GatherFlexChildrenInfo(iterator, GetDocument(), relayout_children, have_flex);

  PaintLayerScrollableArea::DelayScrollOffsetClampScope delay_clamp_scope;

  // We do 2 passes.  The first pass is simply to lay everyone out at
  // their preferred widths.  The second pass handles flexing the children.
  do {
    // Reset our height.
    SetHeight(y_pos);

    x_pos = BorderLeft() + PaddingLeft();

    // Our first pass is done without flexing.  We simply lay the children
    // out within the box.  We have to do a layout first in order to determine
    // our box's intrinsic height.
    LayoutUnit max_ascent;
    LayoutUnit max_descent;
    for (LayoutBox* child = iterator.First(); child; child = iterator.Next()) {
      if (child->IsOutOfFlowPositioned())
        continue;

      SubtreeLayoutScope layout_scope(*child);
      // TODO(jchaffraix): It seems incorrect to check isAtomicInlineLevel in
      // this file.
      // We probably want to check if the element is replaced.
      if (relayout_children || (child->IsAtomicInlineLevel() &&
                                (child->StyleRef().Width().IsPercentOrCalc() ||
                                 child->StyleRef().Height().IsPercentOrCalc())))
        layout_scope.SetChildNeedsLayout(child);

      // Compute the child's vertical margins.
      child->ComputeAndSetBlockDirectionMargins(this);

      if (!child->NeedsLayout())
        MarkChildForPaginationRelayoutIfNeeded(*child, layout_scope);

      // Now do the layout.
      child->LayoutIfNeeded();

      // Update our height and overflow height.
      if (StyleRef().BoxAlign() == EBoxAlignment::kBaseline) {
        LayoutUnit ascent(child->FirstLineBoxBaseline());
        if (ascent == -1)
          ascent = child->Size().Height() + child->MarginBottom();
        ascent += child->MarginTop();
        LayoutUnit descent =
            (child->Size().Height() + child->MarginHeight()) - ascent;

        // Update our maximum ascent.
        max_ascent = std::max(max_ascent, ascent);

        // Update our maximum descent.
        max_descent = std::max(max_descent, descent);

        // Now update our height.
        SetHeight(std::max(y_pos + max_ascent + max_descent, Size().Height()));
      } else {
        SetHeight(std::max(Size().Height(), y_pos + child->Size().Height() +
                                                child->MarginHeight()));
      }

      if (paginated)
        UpdateFragmentationInfoForChild(*child);
    }

    if (!iterator.First() && HasLineIfEmpty()) {
      SetHeight(Size().Height() +
                LineHeight(true,
                           StyleRef().IsHorizontalWritingMode()
                               ? kHorizontalLine
                               : kVerticalLine,
                           kPositionOfInteriorLineBoxes));
    }

    SetHeight(Size().Height() + to_add);

    old_height = Size().Height();
    UpdateLogicalHeight();

    relayout_children = false;
    if (old_height != Size().Height())
      height_specified = true;

    // Now that our height is actually known, we can place our boxes.
    stretching_children_ = (StyleRef().BoxAlign() == EBoxAlignment::kStretch);
    for (LayoutBox* child = iterator.First(); child; child = iterator.Next()) {
      if (child->IsOutOfFlowPositioned()) {
        child->ContainingBlock()->InsertPositionedObject(child);
        PaintLayer* child_layer = child->Layer();
        child_layer->SetStaticInlinePosition(x_pos);
        if (child_layer->StaticBlockPosition() != y_pos) {
          child_layer->SetStaticBlockPosition(y_pos);
          if (child->StyleRef().HasStaticBlockPosition(
                  StyleRef().IsHorizontalWritingMode()))
            child->SetChildNeedsLayout(kMarkOnlyThis);
        }
        continue;
      }

      SubtreeLayoutScope layout_scope(*child);

      // We need to see if this child's height will change, since we make block
      // elements fill the height of a containing box by default. We cannot
      // actually *set* the new height here, though. Need to do that from
      // within layout, or we won't be able to detect the change and duly
      // notify any positioned descendants that are affected by it.
      LayoutUnit old_child_height = child->LogicalHeight();
      LogicalExtentComputedValues computed_values;
      child->ComputeLogicalHeight(child->LogicalHeight(), child->LogicalTop(),
                                  computed_values);
      LayoutUnit new_child_height = computed_values.extent_;
      if (old_child_height != new_child_height)
        layout_scope.SetChildNeedsLayout(child);

      if (!child->NeedsLayout())
        MarkChildForPaginationRelayoutIfNeeded(*child, layout_scope);

      child->LayoutIfNeeded();

      // We can place the child now, using our value of box-align.
      x_pos += child->MarginLeft();
      LayoutUnit child_y = y_pos;
      switch (StyleRef().BoxAlign()) {
        case EBoxAlignment::kCenter:
          child_y += child->MarginTop() +
                     ((ContentHeight() -
                       (child->Size().Height() + child->MarginHeight())) /
                      2)
                         .ClampNegativeToZero();
          break;
        case EBoxAlignment::kBaseline: {
          LayoutUnit ascent(child->FirstLineBoxBaseline());
          if (ascent == -1)
            ascent = child->Size().Height() + child->MarginBottom();
          ascent += child->MarginTop();
          child_y += child->MarginTop() + (max_ascent - ascent);
          break;
        }
        case EBoxAlignment::kEnd:
          child_y +=
              ContentHeight() - child->MarginBottom() - child->Size().Height();
          break;
        default:  // BSTART
          child_y += child->MarginTop();
          break;
      }

      PlaceChild(child, LayoutPoint(x_pos, child_y));

      x_pos += child->Size().Width() + child->MarginRight();
    }

    remaining_space = Size().Width() - BorderRight() - PaddingRight() -
                      VerticalScrollbarWidth() - x_pos;

    stretching_children_ = false;
    if (flexing_children) {
      have_flex = false;  // We're done.
    } else if (have_flex) {
      // We have some flexible objects.  See if we need to grow/shrink them at
      // all.
      if (!remaining_space)
        break;

      // Allocate the remaining space among the flexible objects.
      bool expanding = remaining_space > 0;
      do {
        // Flexing consists of multiple passes, since we have to change
        // ratios every time an object hits its max/min-width For a given
        // pass, we always start off by computing the totalFlex of all
        // objects that can grow/shrink at all, and computing the allowed
        // growth before an object hits its min/max width (and thus forces a
        // totalFlex recomputation).
        LayoutUnit remaining_space_at_beginning = remaining_space;
        float total_flex = 0.0f;
        for (LayoutBox* child = iterator.First(); child;
             child = iterator.Next()) {
          if (AllowedChildFlex(child, expanding))
            total_flex += child->StyleRef().BoxFlex();
        }
        LayoutUnit space_available_this_pass = remaining_space;
        for (LayoutBox* child = iterator.First(); child;
             child = iterator.Next()) {
          LayoutUnit allowed_flex = AllowedChildFlex(child, expanding);
          if (allowed_flex) {
            LayoutUnit projected_flex =
                (allowed_flex == LayoutUnit::Max())
                    ? allowed_flex
                    : LayoutUnit(allowed_flex *
                                 (total_flex / child->StyleRef().BoxFlex()));
            space_available_this_pass =
                expanding ? std::min(space_available_this_pass, projected_flex)
                          : std::max(space_available_this_pass, projected_flex);
          }
        }

        // If we can't grow/shrink anymore, break.
        if (!space_available_this_pass || total_flex == 0.0f)
          break;

        // Now distribute the space to objects.
        for (LayoutBox* child = iterator.First();
             child && space_available_this_pass && total_flex;
             child = iterator.Next()) {
          if (AllowedChildFlex(child, expanding)) {
            LayoutUnit space_add =
                LayoutUnit(space_available_this_pass *
                           (child->StyleRef().BoxFlex() / total_flex));
            if (space_add) {
              child->SetOverrideLogicalWidth(WidthForChild(child) + space_add);
              flexing_children = true;
              relayout_children = true;
            }

            space_available_this_pass -= space_add;
            remaining_space -= space_add;

            total_flex -= child->StyleRef().BoxFlex();
          }
        }
        if (remaining_space == remaining_space_at_beginning) {
          // This is not advancing, avoid getting stuck by distributing the
          // remaining pixels.
          LayoutUnit space_add = LayoutUnit(remaining_space > 0 ? 1 : -1);
          for (LayoutBox* child = iterator.First(); child && remaining_space;
               child = iterator.Next()) {
            if (AllowedChildFlex(child, expanding)) {
              child->SetOverrideLogicalWidth(WidthForChild(child) + space_add);
              flexing_children = true;
              relayout_children = true;
              remaining_space -= space_add;
            }
          }
        }
      } while (AbsoluteValue(remaining_space) >= 1);

      // We didn't find any children that could grow.
      if (have_flex && !flexing_children)
        have_flex = false;
    }
  } while (have_flex);

  if (remaining_space > 0 && ((StyleRef().IsLeftToRightDirection() &&
                               StyleRef().BoxPack() != EBoxPack::kStart) ||
                              (!StyleRef().IsLeftToRightDirection() &&
                               StyleRef().BoxPack() != EBoxPack::kEnd))) {
    // Children must be repositioned.
    LayoutUnit offset;
    if (StyleRef().BoxPack() == EBoxPack::kJustify) {
      // Determine the total number of children.
      int total_children = 0;
      for (LayoutBox* child = iterator.First(); child;
           child = iterator.Next()) {
        if (child->IsOutOfFlowPositioned())
          continue;
        ++total_children;
      }

      // Iterate over the children and space them out according to the
      // justification level.
      if (total_children > 1) {
        --total_children;
        bool first_child = true;
        for (LayoutBox* child = iterator.First(); child;
             child = iterator.Next()) {
          if (child->IsOutOfFlowPositioned())
            continue;

          if (first_child) {
            first_child = false;
            continue;
          }

          offset += remaining_space / total_children;
          remaining_space -= (remaining_space / total_children);
          --total_children;

          PlaceChild(child,
                     child->Location() + LayoutSize(offset, LayoutUnit()));
        }
      }
    } else {
      if (StyleRef().BoxPack() == EBoxPack::kCenter)
        offset += remaining_space / 2;
      else  // END for LTR, START for RTL
        offset += remaining_space;
      for (LayoutBox* child = iterator.First(); child;
           child = iterator.Next()) {
        if (child->IsOutOfFlowPositioned())
          continue;

        PlaceChild(child, child->Location() + LayoutSize(offset, LayoutUnit()));
      }
    }
  }

  // So that the computeLogicalHeight in layoutBlock() knows to relayout
  // positioned objects because of a height change, we revert our height back
  // to the intrinsic height before returning.
  if (height_specified)
    SetHeight(old_height);
}

void LayoutDeprecatedFlexibleBox::LayoutVerticalBox(bool relayout_children) {
  LayoutUnit y_pos = BorderTop() + PaddingTop();
  LayoutUnit to_add =
      BorderBottom() + PaddingBottom() + HorizontalScrollbarHeight();
  bool height_specified = false;
  bool paginated = View()->GetLayoutState()->IsPaginated();
  LayoutUnit old_height;

  LayoutUnit remaining_space;

  FlexBoxIterator iterator(this);
  bool have_flex = false, flexing_children = false;
  GatherFlexChildrenInfo(iterator, GetDocument(), relayout_children, have_flex);

  // We confine the line clamp ugliness to vertical flexible boxes (thus keeping
  // it out of
  // mainstream block layout); this is not really part of the XUL box model.
  if (StyleRef().HasLineClamp())
    ApplyLineClamp(iterator, relayout_children);

  PaintLayerScrollableArea::DelayScrollOffsetClampScope delay_clamp_scope;

  // We do 2 passes.  The first pass is simply to lay everyone out at
  // their preferred widths.  The second pass handles flexing the children.
  // Our first pass is done without flexing.  We simply lay the children
  // out within the box.
  do {
    SetHeight(BorderTop() + PaddingTop());
    LayoutUnit min_height = Size().Height() + to_add;

    for (LayoutBox* child = iterator.First(); child; child = iterator.Next()) {
      if (child->IsOutOfFlowPositioned()) {
        child->ContainingBlock()->InsertPositionedObject(child);
        PaintLayer* child_layer = child->Layer();
        child_layer->SetStaticInlinePosition(BorderStart() + PaddingStart());
        if (child_layer->StaticBlockPosition() != Size().Height()) {
          child_layer->SetStaticBlockPosition(Size().Height());
          if (child->StyleRef().HasStaticBlockPosition(
                  StyleRef().IsHorizontalWritingMode()))
            child->SetChildNeedsLayout(kMarkOnlyThis);
        }
        continue;
      }

      SubtreeLayoutScope layout_scope(*child);
      if (!StyleRef().HasLineClamp() &&
          (relayout_children ||
           (child->IsAtomicInlineLevel() &&
            (child->StyleRef().Width().IsPercentOrCalc() ||
             child->StyleRef().Height().IsPercentOrCalc()))))
        layout_scope.SetChildNeedsLayout(child);

      // Compute the child's vertical margins.
      child->ComputeAndSetBlockDirectionMargins(this);

      // Add in the child's marginTop to our height.
      SetHeight(Size().Height() + child->MarginTop());

      if (!child->NeedsLayout())
        MarkChildForPaginationRelayoutIfNeeded(*child, layout_scope);

      // Now do a layout.
      child->LayoutIfNeeded();

      // We can place the child now, using our value of box-align.
      LayoutUnit child_x = BorderLeft() + PaddingLeft();
      switch (StyleRef().BoxAlign()) {
        case EBoxAlignment::kCenter:
        case EBoxAlignment::kBaseline:  // Baseline just maps to center for
                                        // vertical boxes
          child_x += child->MarginLeft() +
                     ((ContentWidth() -
                       (child->Size().Width() + child->MarginWidth())) /
                      2)
                         .ClampNegativeToZero();
          break;
        case EBoxAlignment::kEnd:
          if (!StyleRef().IsLeftToRightDirection()) {
            child_x += child->MarginLeft();
          } else {
            child_x +=
                ContentWidth() - child->MarginRight() - child->Size().Width();
          }
          break;
        default:  // BSTART/BSTRETCH
          if (StyleRef().IsLeftToRightDirection()) {
            child_x += child->MarginLeft();
          } else {
            child_x +=
                ContentWidth() - child->MarginRight() - child->Size().Width();
          }
          break;
      }

      // Place the child.
      PlaceChild(child, LayoutPoint(child_x, Size().Height()));
      SetHeight(Size().Height() + child->Size().Height() +
                child->MarginBottom());

      if (paginated)
        UpdateFragmentationInfoForChild(*child);
    }

    y_pos = Size().Height();

    if (!iterator.First() && HasLineIfEmpty()) {
      SetHeight(Size().Height() +
                LineHeight(true,
                           StyleRef().IsHorizontalWritingMode()
                               ? kHorizontalLine
                               : kVerticalLine,
                           kPositionOfInteriorLineBoxes));
    }

    SetHeight(Size().Height() + to_add);

    // Negative margins can cause our height to shrink below our minimal height
    // (border/padding).  If this happens, ensure that the computed height is
    // increased to the minimal height.
    if (Size().Height() < min_height)
      SetHeight(min_height);

    // Now we have to calc our height, so we know how much space we have
    // remaining.
    old_height = Size().Height();
    UpdateLogicalHeight();
    if (old_height != Size().Height())
      height_specified = true;

    remaining_space = Size().Height() - BorderBottom() - PaddingBottom() -
                      HorizontalScrollbarHeight() - y_pos;

    if (flexing_children) {
      have_flex = false;  // We're done.
    } else if (have_flex) {
      // We have some flexible objects.  See if we need to grow/shrink them at
      // all.
      if (!remaining_space)
        break;

      // Allocate the remaining space among the flexible objects.
      bool expanding = remaining_space > 0;
      do {
        // Flexing consists of multiple passes, since we have to change
        // ratios every time an object hits its max/min-width For a given
        // pass, we always start off by computing the totalFlex of all
        // objects that can grow/shrink at all, and computing the allowed
        // growth before an object hits its min/max width (and thus forces a
        // totalFlex recomputation).
        LayoutUnit remaining_space_at_beginning = remaining_space;
        float total_flex = 0.0f;
        for (LayoutBox* child = iterator.First(); child;
             child = iterator.Next()) {
          if (AllowedChildFlex(child, expanding))
            total_flex += child->StyleRef().BoxFlex();
        }
        LayoutUnit space_available_this_pass = remaining_space;
        for (LayoutBox* child = iterator.First(); child;
             child = iterator.Next()) {
          LayoutUnit allowed_flex = AllowedChildFlex(child, expanding);
          if (allowed_flex) {
            LayoutUnit projected_flex =
                (allowed_flex == LayoutUnit::Max())
                    ? allowed_flex
                    : static_cast<LayoutUnit>(
                          allowed_flex *
                          (total_flex / child->StyleRef().BoxFlex()));
            space_available_this_pass =
                expanding ? std::min(space_available_this_pass, projected_flex)
                          : std::max(space_available_this_pass, projected_flex);
          }
        }

        // If we can't grow/shrink anymore, break.
        if (!space_available_this_pass || total_flex == 0.0f)
          break;

        // Now distribute the space to objects.
        for (LayoutBox* child = iterator.First();
             child && space_available_this_pass && total_flex;
             child = iterator.Next()) {
          if (AllowedChildFlex(child, expanding)) {
            LayoutUnit space_add = static_cast<LayoutUnit>(
                space_available_this_pass *
                (child->StyleRef().BoxFlex() / total_flex));
            if (space_add) {
              child->SetOverrideLogicalHeight(HeightForChild(child) +
                                              space_add);
              flexing_children = true;
              relayout_children = true;
            }

            space_available_this_pass -= space_add;
            remaining_space -= space_add;

            total_flex -= child->StyleRef().BoxFlex();
          }
        }
        if (remaining_space == remaining_space_at_beginning) {
          // This is not advancing, avoid getting stuck by distributing the
          // remaining pixels.
          LayoutUnit space_add = LayoutUnit(remaining_space > 0 ? 1 : -1);
          for (LayoutBox* child = iterator.First(); child && remaining_space;
               child = iterator.Next()) {
            if (AllowedChildFlex(child, expanding)) {
              child->SetOverrideLogicalHeight(HeightForChild(child) +
                                              space_add);
              flexing_children = true;
              relayout_children = true;
              remaining_space -= space_add;
            }
          }
        }
      } while (AbsoluteValue(remaining_space) >= 1);

      // We didn't find any children that could grow.
      if (have_flex && !flexing_children)
        have_flex = false;
    }
  } while (have_flex);

  if (StyleRef().BoxPack() != EBoxPack::kStart && remaining_space > 0) {
    // Children must be repositioned.
    LayoutUnit offset;
    if (StyleRef().BoxPack() == EBoxPack::kJustify) {
      // Determine the total number of children.
      int total_children = 0;
      for (LayoutBox* child = iterator.First(); child;
           child = iterator.Next()) {
        if (child->IsOutOfFlowPositioned())
          continue;

        ++total_children;
      }

      // Iterate over the children and space them out according to the
      // justification level.
      if (total_children > 1) {
        --total_children;
        bool first_child = true;
        for (LayoutBox* child = iterator.First(); child;
             child = iterator.Next()) {
          if (child->IsOutOfFlowPositioned())
            continue;

          if (first_child) {
            first_child = false;
            continue;
          }

          offset += remaining_space / total_children;
          remaining_space -= (remaining_space / total_children);
          --total_children;
          PlaceChild(child,
                     child->Location() + LayoutSize(LayoutUnit(), offset));
        }
      }
    } else {
      if (StyleRef().BoxPack() == EBoxPack::kCenter)
        offset += remaining_space / 2;
      else  // END
        offset += remaining_space;
      for (LayoutBox* child = iterator.First(); child;
           child = iterator.Next()) {
        if (child->IsOutOfFlowPositioned())
          continue;
        PlaceChild(child, child->Location() + LayoutSize(LayoutUnit(), offset));
      }
    }
  }

  // So that the computeLogicalHeight in layoutBlock() knows to relayout
  // positioned objects because of a height change, we revert our height back
  // to the intrinsic height before returning.
  if (height_specified)
    SetHeight(old_height);
}

void LayoutDeprecatedFlexibleBox::ApplyLineClamp(FlexBoxIterator& iterator,
                                                 bool relayout_children) {
  UseCounter::Count(GetDocument(), WebFeature::kLineClamp);
  UseCounter::Count(GetDocument(), WebFeature::kWebkitBoxLineClamp);

  LayoutBox* child = FirstChildBox();
  if (!child) {
    UseCounter::Count(GetDocument(), WebFeature::kWebkitBoxLineClampNoChildren);
  } else if (!child->NextSiblingBox()) {
    UseCounter::Count(GetDocument(), WebFeature::kWebkitBoxLineClampOneChild);

    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
    if (child_block_flow && child_block_flow->ChildrenInline()) {
      UseCounter::Count(
          GetDocument(),
          WebFeature::kWebkitBoxLineClampOneChildIsLayoutBlockFlowInline);
    }
  } else {
    UseCounter::Count(GetDocument(),
                      WebFeature::kWebkitBoxLineClampManyChildren);
  }

  int max_line_count = 0;
  for (LayoutBox* child = iterator.First(); child; child = iterator.Next()) {
    if (child->IsOutOfFlowPositioned())
      continue;

    child->ClearOverrideSize();
    if (relayout_children ||
        (child->IsAtomicInlineLevel() &&
         (child->StyleRef().Width().IsPercentOrCalc() ||
          child->StyleRef().Height().IsPercentOrCalc())) ||
        (child->StyleRef().Height().IsAuto() && child->IsLayoutBlock())) {
      child->SetChildNeedsLayout(kMarkOnlyThis);

      // Dirty all the positioned objects.
      auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
      if (child_block_flow) {
        child_block_flow->MarkPositionedObjectsForLayout();
        ClearTruncation(child_block_flow);
      }
    }
    child->LayoutIfNeeded();
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
    if (child->StyleRef().Height().IsAuto() && child_block_flow) {
      max_line_count = std::max(max_line_count, LineCount(child_block_flow));
    }
  }

  // Get the number of lines and then alter all block flow children with auto
  // height to use the
  // specified height. We always try to leave room for at least one line.
  int num_visible_lines = StyleRef().LineClamp();
  DCHECK_GT(num_visible_lines, 0);

  if (num_visible_lines >= max_line_count)
    return;

  for (LayoutBox* child = iterator.First(); child; child = iterator.Next()) {
    auto* block_child = DynamicTo<LayoutBlockFlow>(child);
    if (child->IsOutOfFlowPositioned() ||
        !child->StyleRef().Height().IsAuto() || !block_child)
      continue;

    int line_count = blink::LineCount(block_child);
    if (line_count <= num_visible_lines)
      continue;

    int dummy_count = 0;
    LayoutUnit new_height(GetHeightForLineCount(block_child, num_visible_lines,
                                                true, dummy_count));
    if (new_height == child->Size().Height())
      continue;

    child->SetOverrideLogicalHeight(new_height);
    child->ForceLayout();

    // FIXME: For now don't support RTL.
    if (StyleRef().Direction() != TextDirection::kLtr)
      continue;

    // Get the last line
    RootInlineBox* last_line = LineAtIndex(block_child, line_count - 1);
    if (!last_line)
      continue;

    RootInlineBox* last_visible_line =
        LineAtIndex(block_child, num_visible_lines - 1);
    if (!last_visible_line)
      continue;

    DEFINE_STATIC_LOCAL(AtomicString, ellipsis_str,
                        (&kHorizontalEllipsisCharacter, 1));
    const Font& font = Style(num_visible_lines == 1)->GetFont();
    float total_width =
        font.Width(ConstructTextRun(font, &kHorizontalEllipsisCharacter, 1,
                                    StyleRef(), StyleRef().Direction()));

    // See if this width can be accommodated on the last visible line
    LineLayoutBlockFlow dest_block = last_visible_line->Block();
    LineLayoutBlockFlow src_block = last_line->Block();

    // FIXME: Directions of src/destBlock could be different from our direction
    // and from one another.
    if (!src_block.StyleRef().IsLeftToRightDirection())
      continue;

    bool left_to_right = dest_block.StyleRef().IsLeftToRightDirection();
    if (!left_to_right)
      continue;

    LayoutUnit block_right_edge = dest_block.LogicalRightOffsetForLine(
        last_visible_line->Y(), kDoNotIndentText);
    if (!last_visible_line->LineCanAccommodateEllipsis(
            left_to_right, block_right_edge,
            last_visible_line->X() + last_visible_line->LogicalWidth(),
            LayoutUnit(total_width)))
      continue;

    UseCounter::Count(GetDocument(),
                      WebFeature::kWebkitBoxLineClampDoesSomething);

    // Let the truncation code kick in.
    // FIXME: the text alignment should be recomputed after the width changes
    // due to truncation.
    LayoutUnit block_left_edge = dest_block.LogicalLeftOffsetForLine(
        last_visible_line->Y(), kDoNotIndentText);
    InlineBox* box_truncation_starts_at = nullptr;
    last_visible_line->PlaceEllipsis(ellipsis_str, left_to_right,
                                     block_left_edge, block_right_edge,
                                     LayoutUnit(total_width), LayoutUnit(),
                                     &box_truncation_starts_at, ForceEllipsis);
    dest_block.SetHasMarkupTruncation(true);
  }
}

void LayoutDeprecatedFlexibleBox::ClearLineClamp() {
  FlexBoxIterator iterator(this);
  for (LayoutBox* child = iterator.First(); child; child = iterator.Next()) {
    if (child->IsOutOfFlowPositioned())
      continue;

    child->ClearOverrideSize();
    if ((child->IsAtomicInlineLevel() &&
         (child->StyleRef().Width().IsPercentOrCalc() ||
          child->StyleRef().Height().IsPercentOrCalc())) ||
        (child->StyleRef().Height().IsAuto() && child->IsLayoutBlock())) {
      child->SetChildNeedsLayout();

      auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
      if (child_block_flow) {
        child_block_flow->MarkPositionedObjectsForLayout();
        ClearTruncation(child_block_flow);
      }
    }
  }
}

void LayoutDeprecatedFlexibleBox::PlaceChild(LayoutBox* child,
                                             const LayoutPoint& location) {
  // FIXME Investigate if this can be removed based on other flags.
  // crbug.com/370010
  child->SetShouldCheckForPaintInvalidation();

  // Place the child.
  child->SetLocation(location);
}

LayoutUnit LayoutDeprecatedFlexibleBox::AllowedChildFlex(LayoutBox* child,
                                                         bool expanding) {
  if (child->IsOutOfFlowPositioned() || child->StyleRef().BoxFlex() == 0.0f)
    return LayoutUnit();

  if (expanding) {
    if (IsHorizontal()) {
      // FIXME: For now just handle fixed values.
      LayoutUnit max_width = LayoutUnit::Max();
      LayoutUnit width = ContentWidthForChild(child);
      if (child->StyleRef().MaxWidth().IsFixed())
        max_width = LayoutUnit(child->StyleRef().MaxWidth().Value());
      if (max_width == LayoutUnit::Max())
        return max_width;
      return (max_width - width).ClampNegativeToZero();
    }
    // FIXME: For now just handle fixed values.
    LayoutUnit max_height = LayoutUnit::Max();
    LayoutUnit height = ContentHeightForChild(child);
    if (child->StyleRef().MaxHeight().IsFixed())
      max_height = LayoutUnit(child->StyleRef().MaxHeight().Value());
    if (max_height == LayoutUnit::Max())
      return max_height;
    return (max_height - height).ClampNegativeToZero();
  }

  // FIXME: For now just handle fixed values.
  if (IsHorizontal()) {
    LayoutUnit min_width = child->MinPreferredLogicalWidth();
    LayoutUnit width = ContentWidthForChild(child);
    const Length& min_width_length = child->StyleRef().MinWidth();
    if (min_width_length.IsFixed())
      min_width = LayoutUnit(min_width_length.Value());
    else if (min_width_length.IsAuto())
      min_width = LayoutUnit();

    LayoutUnit allowed_shrinkage = (min_width - width).ClampPositiveToZero();
    return allowed_shrinkage;
  }
  const Length& min_height_length = child->StyleRef().MinHeight();
  if (min_height_length.IsFixed() || min_height_length.IsAuto()) {
    LayoutUnit min_height(min_height_length.Value());
    LayoutUnit height = ContentHeightForChild(child);
    LayoutUnit allowed_shrinkage = (min_height - height).ClampPositiveToZero();
    return allowed_shrinkage;
  }
  return LayoutUnit();
}

}  // namespace blink
