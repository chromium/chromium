// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/list_marker.h"

#include "third_party/blink/renderer/core/css/counter_style.h"
#include "third_party/blink/renderer/core/layout/layout_image_resource_style_image.h"
#include "third_party/blink/renderer/core/layout/layout_inside_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/layout_list_marker_image.h"
#include "third_party/blink/renderer/core/layout/layout_outside_list_marker.h"
#include "third_party/blink/renderer/core/layout/list_marker_text.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_inside_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_outside_list_marker.h"
#include "third_party/blink/renderer/core/style/list_style_type_data.h"

namespace blink {

const int kCMarkerPaddingPx = 7;

// TODO(glebl): Move to core/html/resources/html.css after
// Blink starts to support ::marker crbug.com/457718
// Recommended UA margin for list markers.
const int kCUAMarkerMarginEm = 1;

// 'closure-*' have 0.4em margin for compatibility with
// ::-webkit-details-marker.
const float kClosureMarkerMarginEm = 0.4f;

namespace {

LayoutUnit DisclosureSymbolSize(const ComputedStyle& style) {
  return LayoutUnit(style.SpecifiedFontSize() * style.EffectiveZoom() * 0.66);
}

}  // namespace

ListMarker::ListMarker() : marker_text_type_(kNotText) {}

const ListMarker* ListMarker::Get(const LayoutObject* marker) {
  if (auto* outside_marker = DynamicTo<LayoutOutsideListMarker>(marker))
    return &outside_marker->Marker();
  if (auto* inside_marker = DynamicTo<LayoutInsideListMarker>(marker))
    return &inside_marker->Marker();
  if (auto* ng_outside_marker = DynamicTo<LayoutNGOutsideListMarker>(marker))
    return &ng_outside_marker->Marker();
  if (auto* ng_inside_marker = DynamicTo<LayoutNGInsideListMarker>(marker))
    return &ng_inside_marker->Marker();
  return nullptr;
}

ListMarker* ListMarker::Get(LayoutObject* marker) {
  return const_cast<ListMarker*>(
      ListMarker::Get(static_cast<const LayoutObject*>(marker)));
}

LayoutObject* ListMarker::MarkerFromListItem(const LayoutObject* list_item) {
  if (auto* legacy_list_item = DynamicTo<LayoutListItem>(list_item))
    return legacy_list_item->Marker();
  if (auto* ng_list_item = DynamicTo<LayoutNGListItem>(list_item))
    return ng_list_item->Marker();
  return nullptr;
}

LayoutObject* ListMarker::ListItem(const LayoutObject& marker) const {
  DCHECK_EQ(Get(&marker), this);
  LayoutObject* list_item = marker.GetNode()->parentNode()->GetLayoutObject();
  DCHECK(list_item);
  DCHECK(list_item->IsListItemIncludingNG());
  return list_item;
}

LayoutBlockFlow* ListMarker::ListItemBlockFlow(
    const LayoutObject& marker) const {
  DCHECK_EQ(Get(&marker), this);
  LayoutObject* list_item = ListItem(marker);
  if (auto* legacy_list_item = DynamicTo<LayoutListItem>(list_item))
    return legacy_list_item;
  if (auto* ng_list_item = DynamicTo<LayoutNGListItem>(list_item))
    return ng_list_item;
  NOTREACHED();
  return nullptr;
}

int ListMarker::ListItemValue(const LayoutObject& list_item) const {
  if (auto* legacy_list_item = DynamicTo<LayoutListItem>(list_item))
    return legacy_list_item->Value();
  if (auto* ng_list_item = DynamicTo<LayoutNGListItem>(list_item))
    return ng_list_item->Value();
  NOTREACHED();
  return 0;
}

// If the value of ListStyleType changed, we need to update the marker text.
void ListMarker::ListStyleTypeChanged(LayoutObject& marker) {
  DCHECK_EQ(Get(&marker), this);
  if (marker_text_type_ == kNotText || marker_text_type_ == kUnresolved)
    return;

  marker_text_type_ = kUnresolved;
  marker.SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kListStyleTypeChange);
}

// If the @counter-style in use has changed, we need to update the marker text.
void ListMarker::CounterStyleChanged(LayoutObject& marker) {
  DCHECK_EQ(Get(&marker), this);
  if (marker_text_type_ == kNotText || marker_text_type_ == kUnresolved)
    return;

  marker_text_type_ = kUnresolved;
  marker.SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kCounterStyleChange);
}

void ListMarker::OrdinalValueChanged(LayoutObject& marker) {
  DCHECK_EQ(Get(&marker), this);
  if (marker_text_type_ == kOrdinalValue) {
    marker_text_type_ = kUnresolved;
    marker.SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kListValueChange);
  }
}

void ListMarker::UpdateMarkerText(LayoutObject& marker, LayoutText* text) {
  DCHECK_EQ(Get(&marker), this);
  DCHECK(text);
  DCHECK_EQ(marker_text_type_, kUnresolved);
  StringBuilder marker_text_builder;
  marker_text_type_ =
      MarkerText(marker, &marker_text_builder, kWithPrefixSuffix);
  text->SetTextIfNeeded(marker_text_builder.ToString().ReleaseImpl());
  DCHECK_NE(marker_text_type_, kNotText);
  DCHECK_NE(marker_text_type_, kUnresolved);
}

void ListMarker::UpdateMarkerText(LayoutObject& marker) {
  DCHECK_EQ(Get(&marker), this);
  UpdateMarkerText(marker, To<LayoutText>(marker.SlowFirstChild()));
}

ListMarker::MarkerTextType ListMarker::MarkerText(
    const LayoutObject& marker,
    StringBuilder* text,
    MarkerTextFormat format) const {
  DCHECK_EQ(Get(&marker), this);
  if (!marker.StyleRef().ContentBehavesAsNormal())
    return kNotText;
  if (IsMarkerImage(marker)) {
    if (format == kWithPrefixSuffix)
      text->Append(' ');
    return kNotText;
  }

  LayoutObject* list_item = ListItem(marker);
  const ComputedStyle& style = list_item->StyleRef();
  switch (GetListStyleCategory(marker.GetDocument(), style)) {
    case ListStyleCategory::kNone:
      return kNotText;
    case ListStyleCategory::kStaticString:
      text->Append(style.ListStyleStringValue());
      return kStatic;
    case ListStyleCategory::kSymbol:
      if (RuntimeEnabledFeatures::CSSAtRuleCounterStyleEnabled()) {
        const CounterStyle& counter_style =
            GetCounterStyle(marker.GetDocument(), style);
        if (format == kWithPrefixSuffix)
          text->Append(counter_style.GetPrefix());
        text->Append(counter_style.GenerateRepresentation(0));
        if (format == kWithPrefixSuffix)
          text->Append(counter_style.GetSuffix());
      } else {
        text->Append(list_marker_text::GetText(style.ListStyleType(), 0));
        if (format == kWithPrefixSuffix)
          text->Append(' ');
      }
      return kSymbolValue;
    case ListStyleCategory::kLanguage: {
      int value = ListItemValue(*list_item);
      if (RuntimeEnabledFeatures::CSSAtRuleCounterStyleEnabled()) {
        const CounterStyle& counter_style =
            GetCounterStyle(marker.GetDocument(), style);
        if (format == kWithPrefixSuffix)
          text->Append(counter_style.GetPrefix());
        text->Append(counter_style.GenerateRepresentation(value));
        if (format == kWithPrefixSuffix)
          text->Append(counter_style.GetSuffix());
      } else {
        text->Append(list_marker_text::GetText(style.ListStyleType(), value));
        if (format == kWithPrefixSuffix) {
          text->Append(list_marker_text::Suffix(style.ListStyleType(), value));
          text->Append(' ');
        }
      }
      return kOrdinalValue;
    }
  }
  NOTREACHED();
  return kStatic;
}

String ListMarker::MarkerTextWithSuffix(const LayoutObject& marker) const {
  DCHECK_EQ(Get(&marker), this);
  StringBuilder text;
  MarkerText(marker, &text, kWithPrefixSuffix);
  return text.ToString();
}

String ListMarker::MarkerTextWithoutSuffix(const LayoutObject& marker) const {
  DCHECK_EQ(Get(&marker), this);
  StringBuilder text;
  MarkerText(marker, &text, kWithoutPrefixSuffix);
  return text.ToString();
}

String ListMarker::TextAlternative(const LayoutObject& marker) const {
  DCHECK_EQ(Get(&marker), this);
  DCHECK_NE(marker_text_type_, kUnresolved);
  if (marker_text_type_ == kNotText || marker_text_type_ == kUnresolved) {
    // For accessibility, return the marker string in the logical order even in
    // RTL, reflecting speech order.
    return MarkerTextWithSuffix(marker);
  }

  LayoutObject* child = marker.SlowFirstChild();

  // There should be a single text child
  DCHECK(child);
  DCHECK(!child->NextSibling());

  return To<LayoutText>(child)->PlainText();
}

void ListMarker::UpdateMarkerContentIfNeeded(LayoutObject& marker) {
  DCHECK_EQ(Get(&marker), this);
  if (!marker.StyleRef().ContentBehavesAsNormal()) {
    marker_text_type_ = kNotText;
    return;
  }

  // There should be at most one child.
  LayoutObject* child = marker.SlowFirstChild();
  DCHECK(!child || !child->NextSibling());

  const ComputedStyle& style = ListItem(marker)->StyleRef();
  if (IsMarkerImage(marker)) {
    StyleImage* list_style_image = style.ListStyleImage();
    if (child) {
      // If the url of `list-style-image` changed, create a new LayoutImage.
      if (!child->IsLayoutImage() ||
          To<LayoutImage>(child)->ImageResource()->ImagePtr() !=
              list_style_image->Data()) {
        child->Destroy();
        child = nullptr;
      }
    }
    if (!child) {
      LayoutListMarkerImage* image =
          LayoutListMarkerImage::CreateAnonymous(&marker.GetDocument());
      if (marker.IsLayoutNGListMarker())
        image->SetIsLayoutNGObjectForListMarkerImage(true);
      ComputedStyle* image_style =
          ComputedStyle::CreateAnonymousStyleWithDisplay(marker.StyleRef(),
                                                         EDisplay::kInline);
      image->SetStyle(image_style);
      image->SetImageResource(
          MakeGarbageCollected<LayoutImageResourceStyleImage>(
              list_style_image));
      image->SetIsGeneratedContent();
      marker.AddChild(image);
    }
    marker_text_type_ = kNotText;
    return;
  }

  if (!style.GetListStyleType()) {
    marker_text_type_ = kNotText;
    return;
  }

  // Create a LayoutText in it.
  LayoutText* text = nullptr;
  // |text_style| should be as same as style propagated in
  // |LayoutObject::PropagateStyleToAnonymousChildren()| to avoid unexpected
  // full layout due by style difference. See http://crbug.com/980399
  ComputedStyle* text_style = ComputedStyle::CreateAnonymousStyleWithDisplay(
      marker.StyleRef(), marker.StyleRef().Display());
  if (child) {
    if (child->IsText()) {
      text = To<LayoutText>(child);
      text->SetStyle(text_style);
    } else {
      child->Destroy();
      child = nullptr;
    }
  }
  if (!child) {
    text = LayoutText::CreateEmptyAnonymous(marker.GetDocument(), text_style,
                                            LegacyLayout::kAuto);
    marker.AddChild(text);
    marker_text_type_ = kUnresolved;
  }
}

LayoutObject* ListMarker::SymbolMarkerLayoutText(
    const LayoutObject& marker) const {
  DCHECK_EQ(Get(&marker), this);
  if (marker_text_type_ != kSymbolValue)
    return nullptr;
  return marker.SlowFirstChild();
}

bool ListMarker::IsMarkerImage(const LayoutObject& marker) const {
  DCHECK_EQ(Get(&marker), this);
  return marker.StyleRef().ContentBehavesAsNormal() &&
         ListItem(marker)->StyleRef().GeneratesMarkerImage();
}

LayoutUnit ListMarker::WidthOfSymbol(const ComputedStyle& style) {
  const Font& font = style.GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return LayoutUnit();
  const AtomicString name = style.GetListStyleType()->GetCounterStyleName();
  if (name == "disclosure-open" || name == "disclosure-closed")
    return DisclosureSymbolSize(style);
  return LayoutUnit((font_data->GetFontMetrics().Ascent() * 2 / 3 + 1) / 2 + 2);
}

std::pair<LayoutUnit, LayoutUnit> ListMarker::InlineMarginsForInside(
    Document& document,
    const ComputedStyle& marker_style,
    const ComputedStyle& list_item_style) {
  if (!marker_style.ContentBehavesAsNormal())
    return {};
  if (list_item_style.GeneratesMarkerImage())
    return {LayoutUnit(), LayoutUnit(kCMarkerPaddingPx)};
  switch (GetListStyleCategory(document, list_item_style)) {
    case ListStyleCategory::kSymbol: {
      const AtomicString name =
          list_item_style.GetListStyleType()->GetCounterStyleName();
      if (name == "disclosure-open" || name == "disclosure-closed") {
        return {LayoutUnit(), LayoutUnit(kClosureMarkerMarginEm *
                                         marker_style.SpecifiedFontSize())};
      }
      return {LayoutUnit(-1),
              LayoutUnit(kCUAMarkerMarginEm * marker_style.ComputedFontSize())};
    }
    default:
      break;
  }
  return {};
}

std::pair<LayoutUnit, LayoutUnit> ListMarker::InlineMarginsForOutside(
    Document& document,
    const ComputedStyle& marker_style,
    const ComputedStyle& list_item_style,
    LayoutUnit marker_inline_size) {
  LayoutUnit margin_start;
  LayoutUnit margin_end;
  if (!marker_style.ContentBehavesAsNormal()) {
    margin_start = -marker_inline_size;
  } else if (list_item_style.GeneratesMarkerImage()) {
    margin_start = -marker_inline_size - kCMarkerPaddingPx;
    margin_end = LayoutUnit(kCMarkerPaddingPx);
  } else {
    switch (GetListStyleCategory(document, list_item_style)) {
      case ListStyleCategory::kNone:
        break;
      case ListStyleCategory::kSymbol: {
        const SimpleFontData* font_data = marker_style.GetFont().PrimaryFont();
        DCHECK(font_data);
        if (!font_data)
          return {};
        const FontMetrics& font_metrics = font_data->GetFontMetrics();
        const AtomicString name =
            list_item_style.GetListStyleType()->GetCounterStyleName();
        LayoutUnit offset =
            (name == "disclosure-open" || name == "disclosure-closed")
                ? DisclosureSymbolSize(marker_style)
                : LayoutUnit(font_metrics.Ascent() * 2 / 3);
        margin_start = -offset - kCMarkerPaddingPx - 1;
        margin_end = offset + kCMarkerPaddingPx + 1 - marker_inline_size;
        break;
      }
      default:
        margin_start = -marker_inline_size;
    }
  }
  DCHECK_EQ(margin_start + margin_end, -marker_inline_size);
  return {margin_start, margin_end};
}

LayoutRect ListMarker::RelativeSymbolMarkerRect(const ComputedStyle& style,
                                                LayoutUnit width) {
  LayoutRect relative_rect;
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return LayoutRect();

  // TODO(wkorman): Review and clean up/document the calculations below.
  // http://crbug.com/543193
  const FontMetrics& font_metrics = font_data->GetFontMetrics();
  const int ascent = font_metrics.Ascent();
  const AtomicString name = style.GetListStyleType()->GetCounterStyleName();
  if (name == "disclosure-open" || name == "disclosure-closed") {
    LayoutUnit marker_size = DisclosureSymbolSize(style);
    relative_rect = LayoutRect(LayoutUnit(), ascent - marker_size, marker_size,
                               marker_size);
  } else {
    int bullet_width = (ascent * 2 / 3 + 1) / 2;
    relative_rect = LayoutRect(1, 3 * (ascent - ascent * 2 / 3) / 2,
                               bullet_width, bullet_width);
  }
  if (!style.IsHorizontalWritingMode()) {
    relative_rect = relative_rect.TransposedRect();
    relative_rect.SetX(width - relative_rect.X() - relative_rect.Width());
  }
  return relative_rect;
}

const CounterStyle& ListMarker::GetCounterStyle(Document& document,
                                                const ComputedStyle& style) {
  DCHECK(RuntimeEnabledFeatures::CSSAtRuleCounterStyleEnabled());
  DCHECK(style.GetListStyleType());
  DCHECK(style.GetListStyleType()->IsCounterStyle());
  return style.GetListStyleType()->GetCounterStyle(document);
}

ListMarker::ListStyleCategory ListMarker::GetListStyleCategory(
    Document& document,
    const ComputedStyle& style) {
  if (RuntimeEnabledFeatures::CSSAtRuleCounterStyleEnabled()) {
    const ListStyleTypeData* list_style = style.GetListStyleType();
    if (!list_style)
      return ListStyleCategory::kNone;
    if (list_style->IsString())
      return ListStyleCategory::kStaticString;
    DCHECK(list_style->IsCounterStyle());
    return GetCounterStyle(document, style).IsPredefinedSymbolMarker()
               ? ListStyleCategory::kSymbol
               : ListStyleCategory::kLanguage;
  }

  EListStyleType type = style.ListStyleType();
  switch (type) {
    case EListStyleType::kNone:
      return ListStyleCategory::kNone;
    case EListStyleType::kString:
      return ListStyleCategory::kStaticString;
    case EListStyleType::kDisc:
    case EListStyleType::kCircle:
    case EListStyleType::kSquare:
    case EListStyleType::kDisclosureOpen:
    case EListStyleType::kDisclosureClosed:
      return ListStyleCategory::kSymbol;
    case EListStyleType::kArabicIndic:
    case EListStyleType::kArmenian:
    case EListStyleType::kBengali:
    case EListStyleType::kCambodian:
    case EListStyleType::kCjkIdeographic:
    case EListStyleType::kCjkEarthlyBranch:
    case EListStyleType::kCjkHeavenlyStem:
    case EListStyleType::kDecimalLeadingZero:
    case EListStyleType::kDecimal:
    case EListStyleType::kDevanagari:
    case EListStyleType::kEthiopicHalehame:
    case EListStyleType::kEthiopicHalehameAm:
    case EListStyleType::kEthiopicHalehameTiEr:
    case EListStyleType::kEthiopicHalehameTiEt:
    case EListStyleType::kGeorgian:
    case EListStyleType::kGujarati:
    case EListStyleType::kGurmukhi:
    case EListStyleType::kHangul:
    case EListStyleType::kHangulConsonant:
    case EListStyleType::kHebrew:
    case EListStyleType::kHiragana:
    case EListStyleType::kHiraganaIroha:
    case EListStyleType::kKannada:
    case EListStyleType::kKatakana:
    case EListStyleType::kKatakanaIroha:
    case EListStyleType::kKhmer:
    case EListStyleType::kKoreanHangulFormal:
    case EListStyleType::kKoreanHanjaFormal:
    case EListStyleType::kKoreanHanjaInformal:
    case EListStyleType::kLao:
    case EListStyleType::kLowerAlpha:
    case EListStyleType::kLowerArmenian:
    case EListStyleType::kLowerGreek:
    case EListStyleType::kLowerLatin:
    case EListStyleType::kLowerRoman:
    case EListStyleType::kMalayalam:
    case EListStyleType::kMongolian:
    case EListStyleType::kMyanmar:
    case EListStyleType::kOriya:
    case EListStyleType::kPersian:
    case EListStyleType::kSimpChineseFormal:
    case EListStyleType::kSimpChineseInformal:
    case EListStyleType::kTelugu:
    case EListStyleType::kThai:
    case EListStyleType::kTibetan:
    case EListStyleType::kTradChineseFormal:
    case EListStyleType::kTradChineseInformal:
    case EListStyleType::kUpperAlpha:
    case EListStyleType::kUpperArmenian:
    case EListStyleType::kUpperLatin:
    case EListStyleType::kUpperRoman:
    case EListStyleType::kUrdu:
      return ListStyleCategory::kLanguage;
    default:
      NOTREACHED();
      return ListStyleCategory::kLanguage;
  }
}

}  // namespace blink
