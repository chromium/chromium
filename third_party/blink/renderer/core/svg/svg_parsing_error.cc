// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_parsing_error.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

#include <utility>

namespace blink {

namespace {

void AppendErrorContextInfo(StringBuilder& builder,
                            const String& tag_name,
                            const QualifiedName& name) {
  builder.Append('<');
  builder.Append(tag_name);
  builder.Append("> attribute ");
  builder.Append(name.ToString());
}

std::pair<const char*, const char*> MessageForStatus(SVGParseStatus status) {
  switch (status) {
    case SVGParseStatus::kTrailingGarbage:
      return std::make_pair("Trailing garbage, ", ".");
    case SVGParseStatus::kExpectedAngle:
      return std::make_pair("Expected angle, ", ".");
    case SVGParseStatus::kExpectedArcFlag:
      return std::make_pair("Expected arc flag ('0' or '1'), ", ".");
    case SVGParseStatus::kExpectedBoolean:
      return std::make_pair("Expected 'true' or 'false', ", ".");
    case SVGParseStatus::kExpectedEndOfArguments:
      return std::make_pair("Expected ')', ", ".");
    case SVGParseStatus::kExpectedEnumeration:
      return std::make_pair("Unrecognized enumerated value, ", ".");
    case SVGParseStatus::kExpectedInteger:
      return std::make_pair("Expected integer, ", ".");
    case SVGParseStatus::kExpectedLength:
      return std::make_pair("Expected length, ", ".");
    case SVGParseStatus::kExpectedMoveToCommand:
      return std::make_pair("Expected moveto path command ('M' or 'm'), ", ".");
    case SVGParseStatus::kExpectedNumber:
      return std::make_pair("Expected number, ", ".");
    case SVGParseStatus::kExpectedNumberOrPercentage:
      return std::make_pair("Expected number or percentage, ", ".");
    case SVGParseStatus::kExpectedPathCommand:
      return std::make_pair("Expected path command, ", ".");
    case SVGParseStatus::kExpectedStartOfArguments:
      return std::make_pair("Expected '(', ", ".");
    case SVGParseStatus::kExpectedTransformFunction:
      return std::make_pair("Expected transform function, ", ".");
    case SVGParseStatus::kNegativeValue:
      return std::make_pair("A negative value is not valid. (", ")");
    case SVGParseStatus::kZeroValue:
      return std::make_pair("A value of zero is not valid. (", ")");
    case SVGParseStatus::kParsingFailed:
      return std::make_pair("Invalid value, ", ".");
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return std::make_pair("", "");
}

bool DisableLocus(SVGParseStatus status) {
  // Disable locus for semantic errors and generic errors (see TODO below).
  return status == SVGParseStatus::kNegativeValue ||
         status == SVGParseStatus::kZeroValue ||
         status == SVGParseStatus::kParsingFailed;
}

void AppendValue(StringBuilder& builder,
                 SVGParsingError error,
                 const AtomicString& value) {
  builder.Append('"');
  if (!error.HasLocus() || DisableLocus(error.Status())) {
    EscapeStringForJSON(value.GetString(), &builder);
  } else {
    // Emit a string on the form: '"[...]<before><after>[...]"'
    unsigned locus = error.Locus();
    DCHECK_LE(locus, value.length());

    // Amount of context to show before/after the error.
    const unsigned kContext = 16;

    unsigned context_start = std::max(locus, kContext) - kContext;
    unsigned context_end = std::min(locus + kContext, value.length());
    DCHECK_LE(context_start, context_end);
    DCHECK_LE(context_end, value.length());
    if (context_start != 0)
      builder.Append(kHorizontalEllipsisCharacter);
    EscapeStringForJSON(
        value.GetString().Substring(context_start, context_end - context_start),
        &builder);
    if (context_end != value.length())
      builder.Append(kHorizontalEllipsisCharacter);
  }
  builder.Append('"');
}

}  // namespace

String SVGParsingError::Format(const String& tag_name,
                               const QualifiedName& name,
                               const AtomicString& value) const {
  StringBuilder builder;

  AppendErrorContextInfo(builder, tag_name, name);
  builder.Append(": ");

  if (HasLocus() && Locus() == value.length())
    builder.Append("Unexpected end of attribute. ");

  auto message = MessageForStatus(Status());
  builder.Append(message.first);
  AppendValue(builder, *this, value);
  builder.Append(message.second);
  return builder.ToString();
}

}  // namespace blink
