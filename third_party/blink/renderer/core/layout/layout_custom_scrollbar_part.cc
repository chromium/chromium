/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_custom_scrollbar_part.h"

#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/custom_scrollbar.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/custom_scrollbar_theme.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

LayoutCustomScrollbarPart::LayoutCustomScrollbarPart(
    ScrollableArea* scrollable_area,
    CustomScrollbar* scrollbar,
    ScrollbarPart part)
    : LayoutBlock(nullptr),
      scrollable_area_(scrollable_area),
      scrollbar_(scrollbar),
      part_(part) {
  DCHECK(scrollable_area_);
}

static void RecordScrollbarPartStats(Document& document, ScrollbarPart part) {
  switch (part) {
    case kBackButtonEndPart:
    case kForwardButtonStartPart:
      UseCounter::Count(
          document,
          WebFeature::kCSSSelectorPseudoScrollbarButtonReversedDirection);
      U_FALLTHROUGH;
    case kBackButtonStartPart:
    case kForwardButtonEndPart:
      UseCounter::Count(document,
                        WebFeature::kCSSSelectorPseudoScrollbarButton);
      break;
    case kBackTrackPart:
    case kForwardTrackPart:
      UseCounter::Count(document,
                        WebFeature::kCSSSelectorPseudoScrollbarTrackPiece);
      break;
    case kThumbPart:
      UseCounter::Count(document, WebFeature::kCSSSelectorPseudoScrollbarThumb);
      break;
    case kTrackBGPart:
      UseCounter::Count(document, WebFeature::kCSSSelectorPseudoScrollbarTrack);
      break;
    case kScrollbarBGPart:
      UseCounter::Count(document, WebFeature::kCSSSelectorPseudoScrollbar);
      break;
    case kNoPart:
    case kAllParts:
      break;
  }
}

LayoutCustomScrollbarPart* LayoutCustomScrollbarPart::CreateAnonymous(
    Document* document,
    ScrollableArea* scrollable_area,
    CustomScrollbar* scrollbar,
    ScrollbarPart part) {
  LayoutCustomScrollbarPart* layout_object =
      new LayoutCustomScrollbarPart(scrollable_area, scrollbar, part);
  RecordScrollbarPartStats(*document, part);
  layout_object->SetDocumentForAnonymous(document);
  return layout_object;
}

void LayoutCustomScrollbarPart::UpdateLayout() {
  // We don't worry about positioning ourselves. We're just determining our
  // minimum width/height.
  SetLocation(LayoutPoint());
  if (scrollbar_->Orientation() == kHorizontalScrollbar)
    LayoutHorizontalPart();
  else
    LayoutVerticalPart();

  ClearNeedsLayout();
}

void LayoutCustomScrollbarPart::LayoutHorizontalPart() {
  if (part_ == kScrollbarBGPart) {
    SetWidth(LayoutUnit(scrollbar_->Width()));
    UpdateScrollbarHeight();
  } else {
    UpdateScrollbarWidth();
    SetHeight(LayoutUnit(scrollbar_->Height()));
  }
}

void LayoutCustomScrollbarPart::LayoutVerticalPart() {
  if (part_ == kScrollbarBGPart) {
    UpdateScrollbarWidth();
    SetHeight(LayoutUnit(scrollbar_->Height()));
  } else {
    SetWidth(LayoutUnit(scrollbar_->Width()));
    UpdateScrollbarHeight();
  }
}

static int CalcScrollbarThicknessUsing(SizeType size_type,
                                       const Length& length,
                                       int containing_length,
                                       ScrollbarTheme* theme) {
  if (!length.IsIntrinsicOrAuto() || (size_type == kMinSize && length.IsAuto()))
    return MinimumValueForLength(length, LayoutUnit(containing_length)).ToInt();
  return theme->ScrollbarThickness();
}

int LayoutCustomScrollbarPart::ComputeScrollbarWidth(
    int visible_size,
    const ComputedStyle* style) {
  CustomScrollbarTheme* theme = CustomScrollbarTheme::GetCustomScrollbarTheme();
  int w = CalcScrollbarThicknessUsing(kMainOrPreferredSize, style->Width(),
                                      visible_size, theme);
  int min_width = CalcScrollbarThicknessUsing(kMinSize, style->MinWidth(),
                                              visible_size, theme);
  int max_width = w;
  if (!style->MaxWidth().IsMaxSizeNone()) {
    max_width = CalcScrollbarThicknessUsing(kMaxSize, style->MaxWidth(),
                                            visible_size, theme);
  }

  return std::max(min_width, std::min(max_width, w));
}

int LayoutCustomScrollbarPart::ComputeScrollbarHeight(
    int visible_size,
    const ComputedStyle* style) {
  CustomScrollbarTheme* theme = CustomScrollbarTheme::GetCustomScrollbarTheme();
  int h = CalcScrollbarThicknessUsing(kMainOrPreferredSize, style->Height(),
                                      visible_size, theme);
  int min_height = CalcScrollbarThicknessUsing(kMinSize, style->MinHeight(),
                                               visible_size, theme);
  int max_height = h;
  if (!style->MaxHeight().IsMaxSizeNone()) {
    max_height = CalcScrollbarThicknessUsing(kMaxSize, style->MaxHeight(),
                                             visible_size, theme);
  }
  return std::max(min_height, std::min(max_height, h));
}

void LayoutCustomScrollbarPart::UpdateScrollbarWidth() {
  LayoutBox* box = scrollbar_->GetScrollableArea()->GetLayoutBox();
  if (!box)
    return;
  // FIXME: We are querying layout information but nothing guarantees that it's
  // up to date, especially since we are called at style change.
  // FIXME: Querying the style's border information doesn't work on table cells
  // with collapsing borders.
  int visible_size = box->Size().Width() - box->StyleRef().BorderLeftWidth() -
                     box->StyleRef().BorderRightWidth();
  SetWidth(LayoutUnit(ComputeScrollbarWidth(visible_size, Style())));

  // Buttons and track pieces can all have margins along the axis of the
  // scrollbar. Values are rounded because scrollbar parts need to be rendered
  // at device pixel boundaries.
  SetMarginLeft(LayoutUnit(
      MinimumValueForLength(StyleRef().MarginLeft(), LayoutUnit(visible_size))
          .Round()));
  SetMarginRight(LayoutUnit(
      MinimumValueForLength(StyleRef().MarginRight(), LayoutUnit(visible_size))
          .Round()));
}

void LayoutCustomScrollbarPart::UpdateScrollbarHeight() {
  LayoutBox* box = scrollbar_->GetScrollableArea()->GetLayoutBox();
  if (!box)
    return;
  // FIXME: We are querying layout information but nothing guarantees that it's
  // up to date, especially since we are called at style change.
  // FIXME: Querying the style's border information doesn't work on table cells
  // with collapsing borders.
  int visible_size = box->Size().Height() - box->StyleRef().BorderTopWidth() -
                     box->StyleRef().BorderBottomWidth();
  SetHeight(LayoutUnit(ComputeScrollbarHeight(visible_size, Style())));

  // Buttons and track pieces can all have margins along the axis of the
  // scrollbar. Values are rounded because scrollbar parts need to be rendered
  // at device pixel boundaries.
  SetMarginTop(LayoutUnit(
      MinimumValueForLength(StyleRef().MarginTop(), LayoutUnit(visible_size))
          .Round()));
  SetMarginBottom(LayoutUnit(
      MinimumValueForLength(StyleRef().MarginBottom(), LayoutUnit(visible_size))
          .Round()));
}

void LayoutCustomScrollbarPart::ComputePreferredLogicalWidths() {
  if (!PreferredLogicalWidthsDirty())
    return;

  min_preferred_logical_width_ = max_preferred_logical_width_ = LayoutUnit();

  ClearPreferredLogicalWidthsDirty();
}

void LayoutCustomScrollbarPart::StyleWillChange(
    StyleDifference diff,
    const ComputedStyle& new_style) {
  LayoutBlock::StyleWillChange(diff, new_style);
  SetInline(false);
}

void LayoutCustomScrollbarPart::StyleDidChange(StyleDifference diff,
                                               const ComputedStyle* old_style) {
  LayoutBlock::StyleDidChange(diff, old_style);
  // See adjustStyleBeforeSet() above.
  DCHECK(!IsOrthogonalWritingModeRoot());
  SetInline(false);
  ClearPositionedState();
  SetFloating(false);
  if (old_style && (diff.NeedsFullPaintInvalidation() || diff.NeedsLayout()))
    SetNeedsPaintInvalidation();
}

void LayoutCustomScrollbarPart::ImageChanged(WrappedImagePtr image,
                                             CanDeferInvalidation defer) {
  SetNeedsPaintInvalidation();
  LayoutBlock::ImageChanged(image, defer);
}

void LayoutCustomScrollbarPart::SetNeedsPaintInvalidation() {
  if (scrollbar_) {
    scrollbar_->SetNeedsPaintInvalidation(kAllParts);
    return;
  }

  // This LayoutCustomScrollbarPart is a scroll corner or a resizer.
  DCHECK_EQ(part_, kNoPart);
  scrollable_area_->SetScrollCornerNeedsPaintInvalidation();
}

}  // namespace blink
