/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/flex/flexible_box_algorithm.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {
namespace {

ItemPosition BoxAlignmentToItemPosition(EBoxAlignment alignment) {
  switch (alignment) {
    case EBoxAlignment::kBaseline:
      return ItemPosition::kBaseline;
    case EBoxAlignment::kCenter:
      return ItemPosition::kCenter;
    case EBoxAlignment::kStretch:
      return ItemPosition::kStretch;
    case EBoxAlignment::kStart:
      return ItemPosition::kFlexStart;
    case EBoxAlignment::kEnd:
      return ItemPosition::kFlexEnd;
  }
}

ContentPosition BoxPackToContentPosition(EBoxPack box_pack) {
  switch (box_pack) {
    case EBoxPack::kCenter:
      return ContentPosition::kCenter;
    case EBoxPack::kJustify:
      return ContentPosition::kFlexStart;
    case EBoxPack::kStart:
      return ContentPosition::kFlexStart;
    case EBoxPack::kEnd:
      return ContentPosition::kFlexEnd;
  }
}

ContentDistributionType BoxPackToContentDistribution(EBoxPack box_pack) {
  return box_pack == EBoxPack::kJustify ? ContentDistributionType::kSpaceBetween
                                        : ContentDistributionType::kDefault;
}

}  // namespace

// static
bool FlexibleBoxAlgorithm::IsColumnFlow(const ComputedStyle& style) {
  return style.ResolvedIsColumnFlexDirection();
}

// static
bool FlexibleBoxAlgorithm::IsHorizontalFlow(const ComputedStyle& style) {
  if (style.IsHorizontalWritingMode())
    return !style.ResolvedIsColumnFlexDirection();
  return style.ResolvedIsColumnFlexDirection();
}

// static
const StyleContentAlignmentData&
FlexibleBoxAlgorithm::ContentAlignmentNormalBehavior() {
  // The justify-content property applies along the main axis, but since
  // flexing in the main axis is controlled by flex, stretch behaves as
  // flex-start (ignoring the specified fallback alignment, if any).
  // https://drafts.csswg.org/css-align/#distribution-flex
  static const StyleContentAlignmentData kNormalBehavior = {
      ContentPosition::kNormal, ContentDistributionType::kStretch};
  return kNormalBehavior;
}

// static
StyleContentAlignmentData FlexibleBoxAlgorithm::ResolvedJustifyContent(
    const ComputedStyle& style) {
  const bool is_webkit_box = style.IsDeprecatedWebkitBox();
  ContentPosition position;
  if (is_webkit_box) {
    position = BoxPackToContentPosition(style.BoxPack());
    // As row-reverse does layout in reverse, it effectively swaps end & start.
    // -webkit-box didn't do this (-webkit-box always did layout starting at 0,
    // and increasing).
    if (style.ResolvedIsRowReverseFlexDirection()) {
      if (position == ContentPosition::kFlexEnd)
        position = ContentPosition::kFlexStart;
      else if (position == ContentPosition::kFlexStart)
        position = ContentPosition::kFlexEnd;
    }
  } else {
    position =
        style.ResolvedJustifyContentPosition(ContentAlignmentNormalBehavior());
  }
  if (position == ContentPosition::kLeft ||
      position == ContentPosition::kRight) {
    if (IsColumnFlow(style)) {
      if (style.IsHorizontalWritingMode()) {
        // Main axis is perpendicular to both the physical left<->right and
        // inline start<->end axes, so kLeft and kRight behave as kStart.
        position = ContentPosition::kStart;
      } else if ((position == ContentPosition::kLeft &&
                  style.IsFlippedBlocksWritingMode()) ||
                 (position == ContentPosition::kRight &&
                  style.GetWritingDirection().BlockEnd() ==
                      PhysicalDirection::kRight)) {
        position = ContentPosition::kEnd;
      } else {
        position = ContentPosition::kStart;
      }
    } else if ((position == ContentPosition::kLeft &&
                !style.IsLeftToRightDirection()) ||
               (position == ContentPosition::kRight &&
                style.IsLeftToRightDirection())) {
      DCHECK(!FlexibleBoxAlgorithm::IsColumnFlow(style));
      position = ContentPosition::kEnd;
    } else {
      position = ContentPosition::kStart;
    }
  }
  DCHECK_NE(position, ContentPosition::kLeft);
  DCHECK_NE(position, ContentPosition::kRight);

  ContentDistributionType distribution =
      is_webkit_box ? BoxPackToContentDistribution(style.BoxPack())
                    : style.ResolvedJustifyContentDistribution(
                          ContentAlignmentNormalBehavior());
  OverflowAlignment overflow = style.JustifyContent().Overflow();
  if (is_webkit_box) {
    overflow = OverflowAlignment::kSafe;
  } else if (distribution == ContentDistributionType::kStretch) {
    // For flex, justify-content: stretch behaves as flex-start:
    // https://drafts.csswg.org/css-align/#distribution-flex
    position = ContentPosition::kFlexStart;
    distribution = ContentDistributionType::kDefault;
  }
  return StyleContentAlignmentData(position, distribution, overflow);
}

// static
StyleContentAlignmentData FlexibleBoxAlgorithm::ResolvedAlignContent(
    const ComputedStyle& style) {
  ContentPosition position =
      style.ResolvedAlignContentPosition(ContentAlignmentNormalBehavior());
  ContentDistributionType distribution =
      style.ResolvedAlignContentDistribution(ContentAlignmentNormalBehavior());
  OverflowAlignment overflow = style.AlignContent().Overflow();
  return StyleContentAlignmentData(position, distribution, overflow);
}

// static
ItemPosition FlexibleBoxAlgorithm::AlignmentForChild(
    const ComputedStyle& flexbox_style,
    const ComputedStyle& child_style) {
  ItemPosition align =
      flexbox_style.IsDeprecatedWebkitBox()
          ? BoxAlignmentToItemPosition(flexbox_style.BoxAlign())
          : child_style
                .ResolvedAlignSelf(
                    {ItemPosition::kStretch, OverflowAlignment::kDefault},
                    &flexbox_style)
                .GetPosition();
  DCHECK_NE(align, ItemPosition::kAuto);
  DCHECK_NE(align, ItemPosition::kNormal);
  DCHECK_NE(align, ItemPosition::kLeft) << "left, right are only for justify";
  DCHECK_NE(align, ItemPosition::kRight) << "left, right are only for justify";

  if (align == ItemPosition::kStart)
    return ItemPosition::kFlexStart;
  if (align == ItemPosition::kEnd)
    return ItemPosition::kFlexEnd;

  if (align == ItemPosition::kSelfStart || align == ItemPosition::kSelfEnd) {
    LogicalToPhysical<ItemPosition> physical(
        child_style.GetWritingDirection(), ItemPosition::kFlexStart,
        ItemPosition::kFlexEnd, ItemPosition::kFlexStart,
        ItemPosition::kFlexEnd);

    PhysicalToLogical<ItemPosition> logical(flexbox_style.GetWritingDirection(),
                                            physical.Top(), physical.Right(),
                                            physical.Bottom(), physical.Left());

    if (flexbox_style.ResolvedIsColumnFlexDirection()) {
      return align == ItemPosition::kSelfStart ? logical.InlineStart()
                                               : logical.InlineEnd();
    }
    return align == ItemPosition::kSelfStart ? logical.BlockStart()
                                             : logical.BlockEnd();
  }

  if (flexbox_style.FlexWrap() == EFlexWrap::kWrapReverse) {
    if (align == ItemPosition::kFlexStart) {
      align = ItemPosition::kFlexEnd;
    } else if (align == ItemPosition::kFlexEnd) {
      align = ItemPosition::kFlexStart;
    }
  }

  if (!child_style.HasOutOfFlowPosition()) {
    if (IsHorizontalFlow(flexbox_style)) {
      if (child_style.MarginTop().IsAuto() ||
          child_style.MarginBottom().IsAuto()) {
        align = ItemPosition::kFlexStart;
      }
    } else {
      if (child_style.MarginLeft().IsAuto() ||
          child_style.MarginRight().IsAuto()) {
        align = ItemPosition::kFlexStart;
      }
    }
  }

  return align;
}

}  // namespace blink
