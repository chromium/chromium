/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 * Copyright (C) 2010 Daniel Bates (dbates@intudata.com)
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

#include "third_party/blink/renderer/core/layout/layout_list_marker.h"

#include "third_party/blink/renderer/core/css/counter_style.h"
#include "third_party/blink/renderer/core/html/html_li_element.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/list_marker.h"
#include "third_party/blink/renderer/core/paint/list_marker_painter.h"
#include "third_party/blink/renderer/core/style/list_style_type_data.h"
#include "third_party/blink/renderer/platform/fonts/font.h"

namespace blink {
class HTMLLIElement;

LayoutListMarker::LayoutListMarker(Element* element) : LayoutBox(element) {
  DCHECK(ListItem());
  SetInline(true);
  SetIsAtomicInlineLevel(true);
}

LayoutListMarker::~LayoutListMarker() = default;

void LayoutListMarker::Trace(Visitor* visitor) const {
  visitor->Trace(image_);
  LayoutBox::Trace(visitor);
}

void LayoutListMarker::WillBeDestroyed() {
  NOT_DESTROYED();
  if (image_)
    image_->RemoveClient(this);
  LayoutBox::WillBeDestroyed();
}

const LayoutListItem* LayoutListMarker::ListItem() const {
  NOT_DESTROYED();
  LayoutObject* list_item = GetNode()->parentNode()->GetLayoutObject();
  DCHECK(list_item);
  return To<LayoutListItem>(list_item);
}

LayoutSize LayoutListMarker::ImageBulletSize() const {
  NOT_DESTROYED();
  DCHECK(IsImage());
  const SimpleFontData* font_data = StyleRef().GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return LayoutSize();

  // FIXME: This is a somewhat arbitrary default width. Generated images for
  // markers really won't become particularly useful until we support the CSS3
  // marker pseudoclass to allow control over the width and height of the
  // marker box.
  float bullet_width = font_data->GetFontMetrics().Ascent() / 2.0f;
  return RoundedLayoutSize(image_->ImageSize(
      StyleRef().EffectiveZoom(), gfx::SizeF(bullet_width, bullet_width),
      LayoutObject::ShouldRespectImageOrientation(this)));
}

void LayoutListMarker::ListStyleTypeChanged() {
  NOT_DESTROYED();
  if (IsImage())
    return;
  SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kListStyleTypeChange);
}

void LayoutListMarker::CounterStyleChanged() {
  NOT_DESTROYED();
  if (IsImage())
    return;
  SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kCounterStyleChange);
}

void LayoutListMarker::UpdateMarkerImageIfNeeded(StyleImage* image) {
  NOT_DESTROYED();
  if (image_ != image) {
    if (image_)
      image_->RemoveClient(this);
    image_ = image;
    if (image_)
      image_->AddClient(this);
  }
}

InlineBox* LayoutListMarker::CreateInlineBox() {
  NOT_DESTROYED();
  InlineBox* result = LayoutBox::CreateInlineBox();
  result->SetIsText(IsText());
  return result;
}

bool LayoutListMarker::IsImage() const {
  NOT_DESTROYED();
  return image_ && !image_->ErrorOccurred();
}

void LayoutListMarker::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  ListMarkerPainter(*this).Paint(paint_info);
}

void LayoutListMarker::UpdateLayout() {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());

  LayoutUnit block_offset = LogicalTop();
  const LayoutListItem* list_item = ListItem();
  for (LayoutBox* o = ParentBox(); o && o != list_item; o = o->ParentBox()) {
    block_offset += o->LogicalTop();
  }
  if (list_item->StyleRef().IsLeftToRightDirection()) {
    list_item_inline_start_offset_ = list_item->LogicalLeftOffsetForLine(
        block_offset, kDoNotIndentText, LayoutUnit());
  } else {
    list_item_inline_start_offset_ = list_item->LogicalRightOffsetForLine(
        block_offset, kDoNotIndentText, LayoutUnit());
  }
  if (IsImage()) {
    UpdateMargins();
    LayoutSize image_size(ImageBulletSize());
    SetWidth(image_size.Width());
    SetHeight(image_size.Height());
  } else {
    const SimpleFontData* font_data = StyleRef().GetFont().PrimaryFont();
    DCHECK(font_data);
    SetLogicalWidth(PreferredLogicalWidths().min_size);
    SetLogicalHeight(
        LayoutUnit(font_data ? font_data->GetFontMetrics().Height() : 0));
  }

  ClearNeedsLayout();
}

void LayoutListMarker::ImageChanged(WrappedImagePtr o, CanDeferInvalidation) {
  NOT_DESTROYED();
  // A list marker can't have a background or border image, so no need to call
  // the base class method.
  if (!image_ || o != image_->Data())
    return;

  LayoutSize image_size = IsImage() ? ImageBulletSize() : LayoutSize();
  if (Size() != image_size || image_->ErrorOccurred()) {
    SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kImageChanged);
  } else {
    SetShouldDoFullPaintInvalidation();
  }
}

void LayoutListMarker::UpdateContent() {
  NOT_DESTROYED();
  DCHECK(IntrinsicLogicalWidthsDirty());

  text_ = "";

  if (IsImage())
    return;

  switch (GetListStyleCategory()) {
    case ListMarker::ListStyleCategory::kNone:
      break;
    case ListMarker::ListStyleCategory::kSymbol:
      // value is ignored for these types
      text_ = GetCounterStyle().GenerateRepresentation(0);
      break;
    case ListMarker::ListStyleCategory::kLanguage:
      text_ = GetCounterStyle().GenerateRepresentation(ListItem()->Value());
      break;
    case ListMarker::ListStyleCategory::kStaticString:
      text_ = StyleRef().ListStyleStringValue();
      break;
  }
}

String LayoutListMarker::TextAlternative() const {
  NOT_DESTROYED();
  if (GetListStyleCategory() == ListMarker::ListStyleCategory::kStaticString)
    return text_;

  // Return prefix, marker text and then suffix even in RTL, reflecting speech
  // order.
  if (GetListStyleCategory() == ListMarker::ListStyleCategory::kNone)
    return "";

  const CounterStyle& counter_style = GetCounterStyle();
  if (RuntimeEnabledFeatures::CSSAtRuleCounterStyleSpeakAsDescriptorEnabled())
    return counter_style.GenerateTextAlternative(ListItem()->Value());
  return counter_style.GetPrefix() + text_ + counter_style.GetSuffix();
}

LayoutUnit LayoutListMarker::GetWidthOfText(
    ListMarker::ListStyleCategory category) const {
  NOT_DESTROYED();
  // TODO(crbug.com/1012289): this code doesn't support bidi algorithm.
  if (text_.empty())
    return LayoutUnit();
  const Font& font = StyleRef().GetFont();
  LayoutUnit item_width =
      LayoutUnit(font.Width(TextRun(text_))).ClampNegativeToZero();
  if (category == ListMarker::ListStyleCategory::kStaticString) {
    // Don't add a suffix.
    return item_width;
  }

  // This doesn't seem correct, e.g., ligatures. We don't fix it since it's
  // legacy layout.
  const CounterStyle& counter_style = GetCounterStyle();
  if (counter_style.GetPrefix()) {
    item_width += LayoutUnit(font.Width(TextRun(counter_style.GetPrefix())))
                      .ClampNegativeToZero();
  }
  if (counter_style.GetSuffix()) {
    item_width += LayoutUnit(font.Width(TextRun(counter_style.GetSuffix())))
                      .ClampNegativeToZero();
  }
  return item_width;
}

MinMaxSizes LayoutListMarker::ComputeIntrinsicLogicalWidths() const {
  NOT_DESTROYED();
  DCHECK(IntrinsicLogicalWidthsDirty());
  const_cast<LayoutListMarker*>(this)->UpdateContent();

  MinMaxSizes sizes;
  if (IsImage()) {
    LayoutSize image_size(ImageBulletSize());
    sizes = StyleRef().IsHorizontalWritingMode() ? image_size.Width()
                                                 : image_size.Height();
  } else {
    ListMarker::ListStyleCategory category = GetListStyleCategory();
    switch (category) {
      case ListMarker::ListStyleCategory::kNone:
        break;
      case ListMarker::ListStyleCategory::kSymbol:
        sizes = ListMarker::WidthOfSymbol(
            StyleRef(), StyleRef().ListStyleType()->GetCounterStyleName());
        break;
      case ListMarker::ListStyleCategory::kLanguage:
      case ListMarker::ListStyleCategory::kStaticString:
        sizes = GetWidthOfText(category);
        break;
    }
  }

  const_cast<LayoutListMarker*>(this)->UpdateMargins(sizes.min_size);
  return sizes;
}

MinMaxSizes LayoutListMarker::PreferredLogicalWidths() const {
  NOT_DESTROYED();
  return IntrinsicLogicalWidths();
}

void LayoutListMarker::UpdateMargins(LayoutUnit marker_inline_size) {
  NOT_DESTROYED();
  LayoutUnit margin_start;
  LayoutUnit margin_end;
  const ComputedStyle& style = StyleRef();
  const ComputedStyle& list_item_style = ListItem()->StyleRef();
  if (IsInside()) {
    std::tie(margin_start, margin_end) = ListMarker::InlineMarginsForInside(
        GetDocument(), ComputedStyleBuilder(style), list_item_style);
  } else {
    std::tie(margin_start, margin_end) = ListMarker::InlineMarginsForOutside(
        GetDocument(), style, list_item_style, marker_inline_size);
  }

  SetMarginStart(margin_start);
  SetMarginEnd(margin_end);
}

void LayoutListMarker::UpdateMargins() {
  NOT_DESTROYED();
  UpdateMargins(PreferredLogicalWidths().min_size);
}

LayoutUnit LayoutListMarker::LineHeight(
    bool first_line,
    LineDirectionMode direction,
    LinePositionMode line_position_mode) const {
  NOT_DESTROYED();
  if (!IsImage())
    return ListItem()->LineHeight(first_line, direction,
                                  kPositionOfInteriorLineBoxes);
  return LayoutBox::LineHeight(first_line, direction, line_position_mode);
}

LayoutUnit LayoutListMarker::BaselinePosition(
    FontBaseline baseline_type,
    bool first_line,
    LineDirectionMode direction,
    LinePositionMode line_position_mode) const {
  NOT_DESTROYED();
  DCHECK_EQ(line_position_mode, kPositionOnContainingLine);
  if (!IsImage())
    return ListItem()->BaselinePosition(baseline_type, first_line, direction,
                                        kPositionOfInteriorLineBoxes);
  return LayoutBox::BaselinePosition(baseline_type, first_line, direction,
                                     line_position_mode);
}

ListMarker::ListStyleCategory LayoutListMarker::GetListStyleCategory() const {
  NOT_DESTROYED();
  return ListMarker::GetListStyleCategory(GetDocument(), StyleRef());
}

const CounterStyle& LayoutListMarker::GetCounterStyle() const {
  NOT_DESTROYED();
  const ListStyleTypeData* list_style_data = StyleRef().ListStyleType();
  DCHECK(list_style_data);
  DCHECK(list_style_data->IsCounterStyle());
  return list_style_data->GetCounterStyle(GetDocument());
}

bool LayoutListMarker::IsInside() const {
  NOT_DESTROYED();
  const LayoutListItem* list_item = ListItem();
  const ComputedStyle& parent_style = list_item->StyleRef();
  return parent_style.ListStylePosition() == EListStylePosition::kInside ||
         (IsA<HTMLLIElement>(list_item->GetNode()) &&
          !parent_style.IsInsideListElement());
}

LayoutRect LayoutListMarker::GetRelativeMarkerRect() const {
  NOT_DESTROYED();
  if (IsImage())
    return LayoutRect(LayoutPoint(), ImageBulletSize());

  LayoutRect relative_rect;
  ListMarker::ListStyleCategory category = GetListStyleCategory();
  switch (category) {
    case ListMarker::ListStyleCategory::kNone:
      return LayoutRect();
    case ListMarker::ListStyleCategory::kSymbol:
      return ListMarker::RelativeSymbolMarkerRect(
          StyleRef(), StyleRef().ListStyleType()->GetCounterStyleName(),
          Size().Width());
    case ListMarker::ListStyleCategory::kLanguage:
    case ListMarker::ListStyleCategory::kStaticString: {
      const SimpleFontData* font_data = StyleRef().GetFont().PrimaryFont();
      DCHECK(font_data);
      if (!font_data)
        return relative_rect;
      relative_rect =
          LayoutRect(LayoutUnit(), LayoutUnit(), GetWidthOfText(category),
                     LayoutUnit(font_data->GetFontMetrics().Height()));
      break;
    }
  }

  if (!StyleRef().IsHorizontalWritingMode()) {
    relative_rect = relative_rect.TransposedRect();
    relative_rect.SetX(Size().Width() - relative_rect.X() -
                       relative_rect.Width());
  }
  return relative_rect;
}

}  // namespace blink
