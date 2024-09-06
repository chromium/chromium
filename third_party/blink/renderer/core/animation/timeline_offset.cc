// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/timeline_offset.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range_offset.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_value.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/resolver/element_resolve_context.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

namespace {

void ThrowExceptionForInvalidTimelineOffset(ExceptionState& exception_state) {
  exception_state.ThrowTypeError(
      "Animation range must be a name <length-percent> pair");
}

}  // anonymous namespace

/* static */
String TimelineOffset::TimelineRangeNameToString(
    TimelineOffset::NamedRange range_name) {
  switch (range_name) {
    case NamedRange::kNone:
      return "none";

    case NamedRange::kCover:
      return "cover";

    case NamedRange::kContain:
      return "contain";

    case NamedRange::kEntry:
      return "entry";

    case NamedRange::kEntryCrossing:
      return "entry-crossing";

    case NamedRange::kExit:
      return "exit";

    case NamedRange::kExitCrossing:
      return "exit-crossing";
  }
}

String TimelineOffset::ToString() const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (name != NamedRange::kNone) {
    list->Append(*MakeGarbageCollected<CSSIdentifierValue>(name));
  }
  list->Append(*CSSValue::Create(offset, 1));
  return list->CssText();
}

bool TimelineOffset::UpdateOffset(Element* element, CSSValue* value) {
  Length new_offset = ResolveLength(element, value);
  if (new_offset != offset) {
    offset = new_offset;
    return true;
  }
  return false;
}

/* static */
std::optional<TimelineOffset> TimelineOffset::Create(
    Element* element,
    String css_text,
    double default_percent,
    ExceptionState& exception_state) {
  if (!element) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Unable to parse TimelineOffset from CSS text with a null effect or "
        "target");
    return std::nullopt;
  }

  Document& document = element->GetDocument();

  CSSParserTokenStream stream(css_text);
  stream.ConsumeWhitespace();

  const CSSValue* value = css_parsing_utils::ConsumeAnimationRange(
      stream, *document.ElementSheet().Contents()->ParserContext(),
      /* default_offset_percent */ default_percent);

  if (!value || !stream.AtEnd()) {
    ThrowExceptionForInvalidTimelineOffset(exception_state);
    return std::nullopt;
  }

  if (IsA<CSSIdentifierValue>(value)) {
    DCHECK_EQ(CSSValueID::kNormal, To<CSSIdentifierValue>(*value).GetValueID());
    return std::nullopt;
  }

  const auto& list = To<CSSValueList>(*value);

  DCHECK(list.length());
  NamedRange range_name = NamedRange::kNone;
  Length offset = Length::Percent(default_percent);
  std::optional<String> style_dependent_offset_str;
  if (list.Item(0).IsIdentifierValue()) {
    range_name = To<CSSIdentifierValue>(list.Item(0)).ConvertTo<NamedRange>();
    if (list.length() == 2u) {
      const CSSValue* css_offset_value = &list.Item(1);
      offset = ResolveLength(element, css_offset_value);
      if (IsStyleDependent(css_offset_value)) {
        style_dependent_offset_str = css_offset_value->CssText();
      }
    }
  } else {
    const CSSValue* css_offset_value = &list.Item(0);
    offset = ResolveLength(element, css_offset_value);
    if (IsStyleDependent(css_offset_value)) {
      style_dependent_offset_str = css_offset_value->CssText();
    }
  }

  return TimelineOffset(range_name, offset, style_dependent_offset_str);
}

/* static */
std::optional<TimelineOffset> TimelineOffset::Create(
    Element* element,
    const V8UnionStringOrTimelineRangeOffset* range_offset,
    double default_percent,
    ExceptionState& exception_state) {
  if (range_offset->IsString()) {
    return Create(element, range_offset->GetAsString(), default_percent,
                  exception_state);
  }

  TimelineRangeOffset* value = range_offset->GetAsTimelineRangeOffset();
  NamedRange name =
      value->hasRangeName() ? value->rangeName().AsEnum() : NamedRange::kNone;

  Length parsed_offset;
  std::optional<String> style_dependent_offset_str;
  if (value->hasOffset()) {
    CSSNumericValue* offset = value->offset();
    const CSSPrimitiveValue* css_value =
        DynamicTo<CSSPrimitiveValue>(offset->ToCSSValue());

    if (!css_value || (!css_value->IsPx() && !css_value->IsPercentage() &&
                       !css_value->IsCalculatedPercentageWithLength())) {
      exception_state.ThrowTypeError(
          "CSSNumericValue must be a length or percentage for animation "
          "range.");
      return std::nullopt;
    }

    if (css_value->IsPx()) {
      parsed_offset = Length::Fixed(css_value->GetDoubleValue());
    } else if (css_value->IsPercentage()) {
      parsed_offset = Length::Percent(css_value->GetDoubleValue());
    } else {
      DCHECK(css_value->IsCalculatedPercentageWithLength());
      parsed_offset = TimelineOffset::ResolveLength(element, css_value);
      style_dependent_offset_str = css_value->CssText();
    }
  } else {
    parsed_offset = Length::Percent(default_percent);
  }
  return TimelineOffset(name, parsed_offset, style_dependent_offset_str);
}

/* static */
bool TimelineOffset::IsStyleDependent(const CSSValue* value) {
  const CSSPrimitiveValue* primitive_value =
      DynamicTo<CSSPrimitiveValue>(value);
  if (!primitive_value) {
    return true;
  }

  if (primitive_value->IsPercentage()) {
    return false;
  }

  if (primitive_value->IsPx()) {
    return false;
  }

  return true;
}

/* static */
Length TimelineOffset::ResolveLength(Element* element, const CSSValue* value) {
  if (auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value)) {
    if (primitive_value->IsPercentage()) {
      return Length::Percent(primitive_value->GetDoubleValue());
    }
    if (primitive_value->IsPx()) {
      return Length::Fixed(primitive_value->GetDoubleValue());
    }
  }

  // Elements without the computed style don't have a layout box,
  // so the timeline will be inactive.
  // See ScrollTimeline::IsResolved.
  if (!element->GetComputedStyle()) {
    return Length::Fixed();
  }
  ElementResolveContext element_resolve_context(*element);
  Document& document = element->GetDocument();
  CSSToLengthConversionData::Flags ignored_flags = 0;
  CSSToLengthConversionData length_conversion_data(
      element->ComputedStyleRef(), element_resolve_context.ParentStyle(),
      element_resolve_context.RootElementStyle(),
      CSSToLengthConversionData::ViewportSize(document.GetLayoutView()),
      CSSToLengthConversionData::ContainerSizes(element),
      CSSToLengthConversionData::AnchorData(),
      element->GetComputedStyle()->EffectiveZoom(), ignored_flags);

  return DynamicTo<CSSPrimitiveValue>(value)->ConvertToLength(
      length_conversion_data);
}

/* static */
CSSValue* TimelineOffset::ParseOffset(Document* document, String css_text) {
  if (!document) {
    return nullptr;
  }

  CSSParserTokenStream stream(css_text);
  stream.ConsumeWhitespace();

  CSSValue* value = css_parsing_utils::ConsumeLengthOrPercent(
      stream, *document->ElementSheet().Contents()->ParserContext(),
      CSSPrimitiveValue::ValueRange::kAll);

  if (!stream.AtEnd()) {
    return nullptr;
  }

  return value;
}

}  // namespace blink
