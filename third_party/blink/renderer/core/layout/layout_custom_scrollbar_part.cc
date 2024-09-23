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

#include "base/notreached.h"
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
    ScrollbarPart part,
    bool suppress_use_counters)
    : LayoutReplaced(nullptr, PhysicalSize()),
      scrollable_area_(scrollable_area),
      scrollbar_(scrollbar),
      part_(part),
      suppress_use_counters_(suppress_use_counters) {
  DCHECK(scrollable_area_);
}

static void RecordScrollbarPartStats(Document& document, ScrollbarPart part) {
  switch (part) {
    case kBackButtonEndPart:
    case kForwardButtonStartPart:
      UseCounter::Count(
          document,
          WebFeature::kCSSSelectorPseudoScrollbarButtonReversedDirection);
      [[fallthrough]];
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
    ScrollbarPart part,
    bool suppress_use_counters) {
  LayoutCustomScrollbarPart* layout_object =
      MakeGarbageCollected<LayoutCustomScrollbarPart>(
          scrollable_area, scrollbar, part, suppress_use_counters);
  if (!suppress_use_counters) {
    RecordScrollbarPartStats(*document, part);
  }
  layout_object->SetDocumentForAnonymous(document);
  return layout_object;
}

void LayoutCustomScrollbarPart::Trace(Visitor* visitor) const {
  visitor->Trace(scrollable_area_);
  visitor->Trace(scrollbar_);
  LayoutReplaced::Trace(visitor);
}

// TODO(crbug.com/1020913): Support subpixel layout of scrollbars and remove
// ToInt() in the following functions.
// TODO(crbug.com/40339056): This could handle intrinsic sizing keywords
// and calc-size() a bit better than it does.
int LayoutCustomScrollbarPart::ComputeSize(const Length& length,
                                           int container_size) const {
  NOT_DESTROYED();
  if (!length.HasAutoOrContentOrIntrinsic() && !length.HasStretch()) {
    CHECK(length.IsSpecified());
    return MinimumValueForLength(length, LayoutUnit(container_size)).ToInt();
  }
  return CustomScrollbarTheme::GetCustomScrollbarTheme()->ScrollbarThickness(
      scrollbar_->ScaleFromDIP(), StyleRef().UsedScrollbarWidth());
}

int LayoutCustomScrollbarPart::ComputeWidth(int container_width) const {
  NOT_DESTROYED();
  const auto& style = StyleRef();
  if (style.Display() == EDisplay::kNone) {
    return 0;
  }

  int width = ComputeSize(style.Width(), container_width);
  int min_width = style.MinWidth().IsAuto()
                      ? 0
                      : ComputeSize(style.MinWidth(), container_width);
  int max_width = style.MaxWidth().IsNone()
                      ? width
                      : ComputeSize(style.MaxWidth(), container_width);
  return std::max(min_width, std::min(max_width, width));
}

int LayoutCustomScrollbarPart::ComputeHeight(int container_height) const {
  NOT_DESTROYED();
  const auto& style = StyleRef();
  if (style.Display() == EDisplay::kNone) {
    return 0;
  }

  int height = ComputeSize(style.Height(), container_height);
  int min_height = style.MinHeight().IsAuto()
                       ? 0
                       : ComputeSize(style.MinHeight(), container_height);
  int max_height = style.MaxHeight().IsNone()
                       ? height
                       : ComputeSize(style.MaxHeight(), container_height);
  return std::max(min_height, std::min(max_height, height));
}

int LayoutCustomScrollbarPart::ComputeThickness() const {
  NOT_DESTROYED();
  DCHECK_EQ(kScrollbarBGPart, part_);

  // Use 0 for container width/height, so percentage size will be ignored.
  // We have never supported that.
  if (scrollbar_->Orientation() == kHorizontalScrollbar)
    return ComputeHeight(0);
  return ComputeWidth(0);
}

int LayoutCustomScrollbarPart::ComputeLength() const {
  NOT_DESTROYED();
  DCHECK_NE(kScrollbarBGPart, part_);

  if (scrollbar_->Orientation() == kHorizontalScrollbar) {
    return ComputeWidth(scrollbar_->FrameRect().width());
  }
  return ComputeHeight(scrollbar_->FrameRect().height());
}

void LayoutCustomScrollbarPart::SetOverriddenSize(const PhysicalSize& size) {
  NOT_DESTROYED();
  overridden_size_ = size;
}

LayoutPoint LayoutCustomScrollbarPart::LocationInternal() const {
  NOT_DESTROYED();
  NOTREACHED();
}

PhysicalSize LayoutCustomScrollbarPart::Size() const {
  NOT_DESTROYED();
  return overridden_size_;
}

static LayoutUnit ComputeMargin(const Length& style_margin) {
  // TODO(crbug.com/1020913): Support subpixel layout of scrollbars and remove
  // Round() below.
  return LayoutUnit(MinimumValueForLength(style_margin, LayoutUnit()).Round());
}

LayoutUnit LayoutCustomScrollbarPart::MarginTop() const {
  NOT_DESTROYED();
  if (scrollbar_ && scrollbar_->Orientation() == kHorizontalScrollbar) {
    return LayoutUnit();
  }
  return ComputeMargin(StyleRef().MarginTop());
}

LayoutUnit LayoutCustomScrollbarPart::MarginBottom() const {
  NOT_DESTROYED();
  if (scrollbar_ && scrollbar_->Orientation() == kHorizontalScrollbar) {
    return LayoutUnit();
  }
  return ComputeMargin(StyleRef().MarginBottom());
}

LayoutUnit LayoutCustomScrollbarPart::MarginLeft() const {
  NOT_DESTROYED();
  if (scrollbar_ && scrollbar_->Orientation() == kVerticalScrollbar) {
    return LayoutUnit();
  }
  return ComputeMargin(StyleRef().MarginLeft());
}

LayoutUnit LayoutCustomScrollbarPart::MarginRight() const {
  NOT_DESTROYED();
  if (scrollbar_ && scrollbar_->Orientation() == kVerticalScrollbar) {
    return LayoutUnit();
  }
  return ComputeMargin(StyleRef().MarginRight());
}

void LayoutCustomScrollbarPart::UpdateFromStyle() {
  NOT_DESTROYED();
  LayoutReplaced::UpdateFromStyle();
  SetInline(false);
  ClearPositionedState();
  SetFloating(false);
}

void LayoutCustomScrollbarPart::StyleDidChange(StyleDifference diff,
                                               const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutReplaced::StyleDidChange(diff, old_style);
  if (old_style &&
      (diff.NeedsNormalPaintInvalidation() || diff.NeedsLayout())) {
    SetNeedsPaintInvalidation();
  }
  RecordPercentLengthStats();
}

void LayoutCustomScrollbarPart::RecordPercentLengthStats() const {
  NOT_DESTROYED();
  if (!scrollbar_ || suppress_use_counters_) {
    return;
  }

  auto feature = part_ == kScrollbarBGPart
                     ? WebFeature::kCustomScrollbarPercentThickness
                     : WebFeature::kCustomScrollbarPartPercentLength;
  // The orientation that the width css property has effect for the part.
  auto width_orientation =
      part_ == kScrollbarBGPart ? kVerticalScrollbar : kHorizontalScrollbar;

  // "==" below tests both direct percent length and percent used in calculated
  // length.
  if (scrollbar_->Orientation() == width_orientation) {
    if (ComputeWidth(0) == ComputeWidth(LayoutUnit::NearlyMax().ToInt()))
      return;
  } else if (ComputeHeight(0) ==
             ComputeHeight(LayoutUnit::NearlyMax().ToInt())) {
    return;
  }

  UseCounter::Count(GetDocument(), feature);
}

void LayoutCustomScrollbarPart::ImageChanged(WrappedImagePtr image,
                                             CanDeferInvalidation defer) {
  NOT_DESTROYED();
  SetNeedsPaintInvalidation();
  LayoutReplaced::ImageChanged(image, defer);
}

void LayoutCustomScrollbarPart::SetNeedsPaintInvalidation() {
  NOT_DESTROYED();
  if (scrollbar_) {
    scrollbar_->SetNeedsPaintInvalidation(kAllParts);
    return;
  }

  // This LayoutCustomScrollbarPart is a scroll corner or a resizer.
  DCHECK_EQ(part_, kNoPart);
  scrollable_area_->SetScrollCornerNeedsPaintInvalidation();
}

}  // namespace blink
