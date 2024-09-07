// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/list/list_marker.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/counter_style.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_image_resource_style_image.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/list/layout_inline_list_item.h"
#include "third_party/blink/renderer/core/layout/list/layout_inside_list_marker.h"
#include "third_party/blink/renderer/core/layout/list/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/list/layout_list_marker_image.h"
#include "third_party/blink/renderer/core/layout/list/layout_outside_list_marker.h"
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

void DestroyLayoutObject(LayoutObject* layout_object) {
  // AXObjects are normally removed from destroyed layout objects in
  // Node::DetachLayoutTree(), but as the list marker implementation manually
  // destroys the layout objects, it must manually remove the accessibility
  // objects for them as well.
  if (auto* cache = layout_object->GetDocument().ExistingAXObjectCache()) {
    cache->RemoveAXObjectsInLayoutSubtree(layout_object);
  }
  layout_object->Destroy();
}

}  // namespace

ListMarker::ListMarker() : marker_text_type_(kNotText) {}

const ListMarker* ListMarker::Get(const LayoutObject* marker) {
  if (auto* ng_outside_marker = DynamicTo<LayoutOutsideListMarker>(marker)) {
    return &ng_outside_marker->Marker();
  }
  if (auto* ng_inside_marker = DynamicTo<LayoutInsideListMarker>(marker)) {
    return &ng_inside_marker->Marker();
  }
  return nullptr;
}

ListMarker* ListMarker::Get(LayoutObject* marker) {
  return const_cast<ListMarker*>(
      ListMarker::Get(static_cast<const LayoutObject*>(marker)));
}

LayoutObject* ListMarker::MarkerFromListItem(const LayoutObject* list_item) {
  if (auto* ng_list_item = DynamicTo<LayoutListItem>(list_item)) {
    return ng_list_item->Marker();
  }
  if (auto* inline_list_item = DynamicTo<LayoutInlineListItem>(list_item)) {
    return inline_list_item->Marker();
  }
  return nullptr;
}

LayoutObject* ListMarker::ListItem(const LayoutObject& marker) const {
  DCHECK_EQ(Get(&marker), this);
  LayoutObject* list_item = marker.GetNode()->parentNode()->GetLayoutObject();
  DCHECK(list_item);
  DCHECK(list_item->IsListItem());
  return list_item;
}

int ListMarker::ListItemValue(const LayoutObject& list_item) const {
  if (auto* ng_list_item = DynamicTo<LayoutListItem>(list_item)) {
    return ng_list_item->Value();
  }
  if (auto* inline_list_item = DynamicTo<LayoutInlineListItem>(list_item)) {
    return inline_list_item->Value();
  }
  NOTREACHED_IN_MIGRATION();
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

LayoutObject* ListMarker::GetContentChild(const LayoutObject& marker) const {
  DCHECK_EQ(Get(&marker), this);
  LayoutObject* const first_child = marker.SlowFirstChild();
  if (IsA<LayoutTextCombine>(first_child)) {
    return first_child->SlowFirstChild();
  }
  return first_child;
}

LayoutTextFragment& ListMarker::GetTextChild(const LayoutObject& marker) const {
  auto& text = *To<LayoutTextFragment>(GetContentChild(marker));
  // There should be a single text child
  DCHECK(!text.NextSibling());
  return text;
}

void ListMarker::UpdateMarkerText(LayoutObject& marker) {
  DCHECK_EQ(Get(&marker), this);
  auto& text = GetTextChild(marker);
  DCHECK_EQ(marker_text_type_, kUnresolved);
  StringBuilder marker_text_builder;
  marker_text_type_ =
      MarkerText(marker, &marker_text_builder, kWithPrefixSuffix);
  text.SetContentString(marker_text_builder.ToString());
  DCHECK_NE(marker_text_type_, kNotText);
  DCHECK_NE(marker_text_type_, kUnresolved);
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
    case ListStyleCategory::kSymbol: {
      const CounterStyle& counter_style =
          GetCounterStyle(marker.GetDocument(), style);
      switch (format) {
        case kWithPrefixSuffix:
          text->Append(
              counter_style.GenerateRepresentationWithPrefixAndSuffix(0));
          break;
        case kWithoutPrefixSuffix:
          text->Append(counter_style.GenerateRepresentation(0));
          break;
        case kAlternativeText:
          text->Append(counter_style.GenerateTextAlternative(0));
      }
      return kSymbolValue;
    }
    case ListStyleCategory::kLanguage: {
      int value = ListItemValue(*list_item);
      const CounterStyle& counter_style =
          GetCounterStyle(marker.GetDocument(), style);
      switch (format) {
        case kWithPrefixSuffix:
          text->Append(
              counter_style.GenerateRepresentationWithPrefixAndSuffix(value));
          break;
        case kWithoutPrefixSuffix:
          text->Append(counter_style.GenerateRepresentation(value));
          break;
        case kAlternativeText:
          text->Append(counter_style.GenerateTextAlternative(value));
      }
      return kOrdinalValue;
    }
  }
  NOTREACHED_IN_MIGRATION();
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
  // For accessibility, return the marker string in the logical order even in
  // RTL, reflecting speech order.
  if (marker_text_type_ == kNotText) {
    String text = MarkerTextWithSuffix(marker);
    if (!text.empty()) {
      return text;
    }

    // Pseudo element list markers may return empty text as their text
    // alternative, so obtain the text from its child as a fallback mechanism.
    auto* text_child = GetContentChild(marker);
    if (text_child && !text_child->NextSibling() &&
        IsA<LayoutTextFragment>(text_child)) {
      return GetTextChild(marker).PlainText();
    }

    // The fallback is not present, so return the original empty text.
    return text;
  }

  if (RuntimeEnabledFeatures::CSSAtRuleCounterStyleSpeakAsDescriptorEnabled()) {
    StringBuilder text;
    MarkerText(marker, &text, kAlternativeText);
    return text.ToString();
  }

  if (marker_text_type_ == kUnresolved) {
    return MarkerTextWithSuffix(marker);
  }

  return GetTextChild(marker).PlainText();
}

void ListMarker::UpdateMarkerContentIfNeeded(LayoutObject& marker) {
  DCHECK_EQ(Get(&marker), this);
  if (!marker.StyleRef().ContentBehavesAsNormal()) {
    marker_text_type_ = kNotText;
    return;
  }

  // There should be at most one child.
  LayoutObject* child = GetContentChild(marker);

  const ComputedStyle& style = ListItem(marker)->StyleRef();
  if (IsMarkerImage(marker)) {
    StyleImage* list_style_image = style.ListStyleImage();
    if (child) {
      // If the url of `list-style-image` changed, create a new LayoutImage.
      if (!child->IsLayoutImage() ||
          To<LayoutImage>(child)->ImageResource()->ImagePtr() !=
              list_style_image->Data()) {
        if (IsA<LayoutTextCombine>(child->Parent())) [[unlikely]] {
          DestroyLayoutObject(child->Parent());
        } else {
          DestroyLayoutObject(child);
        }
        child = nullptr;
      }
    }
    if (!child) {
      LayoutListMarkerImage* image =
          LayoutListMarkerImage::CreateAnonymous(&marker.GetDocument());
      const ComputedStyle* image_style =
          marker.GetDocument()
              .GetStyleResolver()
              .CreateAnonymousStyleWithDisplay(marker.StyleRef(),
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

  if (!style.ListStyleType()) {
    marker_text_type_ = kNotText;
    return;
  }

  // |text_style| should be as same as style propagated in
  // |LayoutObject::PropagateStyleToAnonymousChildren()| to avoid unexpected
  // full layout due by style difference. See http://crbug.com/980399
  const auto& style_parent = child ? *child->Parent() : marker;
  const ComputedStyle* text_style =
      marker.GetDocument().GetStyleResolver().CreateAnonymousStyleWithDisplay(
          style_parent.StyleRef(), marker.StyleRef().Display());
  if (IsA<LayoutTextFragment>(child))
    return child->SetStyle(text_style);
  if (child) {
    DestroyLayoutObject(child);
  }

  auto* const new_text = LayoutTextFragment::CreateAnonymous(
      marker.GetDocument(), StringImpl::empty_, 0, 0);
  new_text->SetStyle(std::move(text_style));
  marker.AddChild(new_text);
  marker_text_type_ = kUnresolved;
}

LayoutObject* ListMarker::SymbolMarkerLayoutText(
    const LayoutObject& marker) const {
  DCHECK_EQ(Get(&marker), this);
  if (marker_text_type_ != kSymbolValue)
    return nullptr;
  return GetContentChild(marker);
}

bool ListMarker::IsMarkerImage(const LayoutObject& marker) const {
  DCHECK_EQ(Get(&marker), this);
  return marker.StyleRef().ContentBehavesAsNormal() &&
         ListItem(marker)->StyleRef().GeneratesMarkerImage();
}

LayoutUnit ListMarker::WidthOfSymbol(const ComputedStyle& style,
                                     const AtomicString& list_style) {
  const Font& font = style.GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return LayoutUnit();
  if (style.SpecifiedFontSize() == 0) [[unlikely]] {
    // See http://crbug.com/1228157
    return LayoutUnit();
  }
  if (list_style == keywords::kDisclosureOpen ||
      list_style == keywords::kDisclosureClosed) {
    return DisclosureSymbolSize(style);
  }
  return LayoutUnit((font_data->GetFontMetrics().Ascent() * 2 / 3 + 1) / 2 + 2);
}

std::pair<LayoutUnit, LayoutUnit> ListMarker::InlineMarginsForInside(
    Document& document,
    const ComputedStyleBuilder& marker_style_builder,
    const ComputedStyle& list_item_style) {
  if (!marker_style_builder.GetDisplayStyle().ContentBehavesAsNormal()) {
    return {};
  }
  if (list_item_style.GeneratesMarkerImage())
    return {LayoutUnit(), LayoutUnit(kCMarkerPaddingPx)};
  switch (GetListStyleCategory(document, list_item_style)) {
    case ListStyleCategory::kSymbol: {
      const AtomicString& name =
          list_item_style.ListStyleType()->GetCounterStyleName();
      if (name == keywords::kDisclosureOpen ||
          name == keywords::kDisclosureClosed) {
        return {LayoutUnit(),
                LayoutUnit(
                    kClosureMarkerMarginEm *
                    marker_style_builder.GetFontDescription().SpecifiedSize())};
      }
      return {
          LayoutUnit(-1),
          LayoutUnit(kCUAMarkerMarginEm *
                     marker_style_builder.GetFontDescription().ComputedSize())};
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
        const AtomicString& name =
            list_item_style.ListStyleType()->GetCounterStyleName();
        LayoutUnit offset = (name == keywords::kDisclosureOpen ||
                             name == keywords::kDisclosureClosed)
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
  DCHECK_EQ(-margin_start - margin_end, marker_inline_size);
  return {margin_start, margin_end};
}

PhysicalRect ListMarker::RelativeSymbolMarkerRect(
    const ComputedStyle& style,
    const AtomicString& list_style,
    LayoutUnit width) {
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return PhysicalRect();

  LogicalRect relative_rect;
  // TODO(wkorman): Review and clean up/document the calculations below.
  // http://crbug.com/543193
  const FontMetrics& font_metrics = font_data->GetFontMetrics();
  const int ascent = font_metrics.Ascent();
  if (list_style == keywords::kDisclosureOpen ||
      list_style == keywords::kDisclosureClosed) {
    LayoutUnit marker_size = DisclosureSymbolSize(style);
    relative_rect = LogicalRect(LayoutUnit(), ascent - marker_size, marker_size,
                                marker_size);
  } else {
    LayoutUnit bullet_width = LayoutUnit((ascent * 2 / 3 + 1) / 2);
    relative_rect = LogicalRect(LayoutUnit(1),
                                LayoutUnit(3 * (ascent - ascent * 2 / 3) / 2),
                                bullet_width, bullet_width);
  }
  // TextDirection doesn't matter here.  Passing
  // `relative_rect.size.inline_size` to get a correct result in sideways-lr.
  WritingModeConverter converter(
      {ToLineWritingMode(style.GetWritingMode()), TextDirection::kLtr},
      PhysicalSize(width, relative_rect.size.inline_size));
  return converter.ToPhysical(relative_rect);
}

const CounterStyle& ListMarker::GetCounterStyle(Document& document,
                                                const ComputedStyle& style) {
  DCHECK(style.ListStyleType());
  DCHECK(style.ListStyleType()->IsCounterStyle());
  return style.ListStyleType()->GetCounterStyle(document);
}

ListMarker::ListStyleCategory ListMarker::GetListStyleCategory(
    Document& document,
    const ComputedStyle& style) {
  const ListStyleTypeData* list_style = style.ListStyleType();
  if (!list_style)
    return ListStyleCategory::kNone;
  if (list_style->IsString())
    return ListStyleCategory::kStaticString;
  DCHECK(list_style->IsCounterStyle());
  return GetCounterStyle(document, style).IsPredefinedSymbolMarker()
             ? ListStyleCategory::kSymbol
             : ListStyleCategory::kLanguage;
}

}  // namespace blink
