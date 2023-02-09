// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/timeline_offset.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/resolver/element_resolve_context.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

namespace {

void ThrowExcpetionForInvalidTimelineOffset(ExceptionState& exception_state) {
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
  if (name == NamedRange::kNone) {
    return "auto";
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*MakeGarbageCollected<CSSIdentifierValue>(name));
  list->Append(*CSSValue::Create(offset, 1));
  return list->CssText();
}

/* static */
absl::optional<TimelineOffset> TimelineOffset::Create(
    Element* element,
    String css_text,
    ExceptionState& exception_state) {
  Document& document = element->GetDocument();
  const CSSValue* value_list = CSSParser::ParseSingleValue(
      CSSPropertyID::kAnimationRangeStart, css_text,
      document.ElementSheet().Contents()->ParserContext());

  if (!value_list) {
    ThrowExcpetionForInvalidTimelineOffset(exception_state);
    return absl::nullopt;
  }

  if (To<CSSValueList>(value_list)->length() != 1) {
    ThrowExcpetionForInvalidTimelineOffset(exception_state);
    return absl::nullopt;
  }

  const CSSValue& value = To<CSSValueList>(value_list)->Item(0);

  if (value.IsIdentifierValue() &&
      To<CSSIdentifierValue>(value).GetValueID() == CSSValueID::kAuto) {
    return absl::nullopt;
  }

  if (!value.IsValueList()) {
    ThrowExcpetionForInvalidTimelineOffset(exception_state);
    return absl::nullopt;
  }

  const auto& list = To<CSSValueList>(value);
  if (list.length() != 2) {
    ThrowExcpetionForInvalidTimelineOffset(exception_state);
    return absl::nullopt;
  }

  // TODO(kevers): Keep track of style dependent lengths in order
  // to re-resolve on a style update.
  const auto& range_name = To<CSSIdentifierValue>(list.Item(0));
  return TimelineOffset(range_name.ConvertTo<NamedRange>(),
                        ResolveLength(element, &list.Item(1)));
}

/* static */
Length TimelineOffset::ResolveLength(Element* element, const CSSValue* value) {
  ElementResolveContext element_resolve_context(*element);
  Document& document = element->GetDocument();
  // TODO(kevers): Re-resolve any value that is not px or % on a style change.
  CSSToLengthConversionData::Flags ignored_flags = 0;
  CSSToLengthConversionData length_conversion_data(
      element->ComputedStyleRef(), element_resolve_context.ParentStyle(),
      element_resolve_context.RootElementStyle(), document.GetLayoutView(),
      CSSToLengthConversionData::ContainerSizes(element),
      element->GetComputedStyle()->EffectiveZoom(), ignored_flags);

  return DynamicTo<CSSPrimitiveValue>(value)->ConvertToLength(
      length_conversion_data);
}

}  // namespace blink
