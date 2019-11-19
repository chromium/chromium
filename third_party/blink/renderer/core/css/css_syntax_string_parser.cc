// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_syntax_string_parser.h"

#include <utility>
#include "third_party/blink/renderer/core/css/css_syntax_component.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

// https://drafts.css-houdini.org/css-properties-values-api-1/#supported-names
base::Optional<CSSSyntaxType> ParseSyntaxType(StringView type) {
  if (type == "length")
    return CSSSyntaxType::kLength;
  if (type == "number")
    return CSSSyntaxType::kNumber;
  if (type == "percentage")
    return CSSSyntaxType::kPercentage;
  if (type == "length-percentage")
    return CSSSyntaxType::kLengthPercentage;
  if (type == "color")
    return CSSSyntaxType::kColor;
  if (RuntimeEnabledFeatures::CSSVariables2ImageValuesEnabled()) {
    if (type == "image")
      return CSSSyntaxType::kImage;
  }
  if (type == "url")
    return CSSSyntaxType::kUrl;
  if (type == "integer")
    return CSSSyntaxType::kInteger;
  if (type == "angle")
    return CSSSyntaxType::kAngle;
  if (type == "time")
    return CSSSyntaxType::kTime;
  if (type == "resolution")
    return CSSSyntaxType::kResolution;
  if (RuntimeEnabledFeatures::CSSVariables2TransformValuesEnabled()) {
    if (type == "transform-function")
      return CSSSyntaxType::kTransformFunction;
    if (type == "transform-list")
      return CSSSyntaxType::kTransformList;
  }
  if (type == "custom-ident")
    return CSSSyntaxType::kCustomIdent;
  return base::nullopt;
}

bool IsPreMultiplied(CSSSyntaxType type) {
  return type == CSSSyntaxType::kTransformList;
}

}  // namespace

CSSSyntaxStringParser::CSSSyntaxStringParser(const String& string)
    : string_(string.StripWhiteSpace()), input_(string_) {}

base::Optional<CSSSyntaxDefinition> CSSSyntaxStringParser::Parse() {
  if (string_.IsEmpty())
    return base::nullopt;
  if (string_.length() == 1 && string_[0] == '*')
    return CSSSyntaxDefinition::CreateUniversal();

  Vector<CSSSyntaxComponent> components;

  while (true) {
    if (!ConsumeSyntaxComponent(components))
      return base::nullopt;
    input_.AdvanceUntilNonWhitespace();
    UChar cc = input_.NextInputChar();
    input_.Advance();
    if (cc == '\0')
      break;
    if (cc == '|')
      continue;
    return base::nullopt;
  }

  return CSSSyntaxDefinition(std::move(components));
}

bool CSSSyntaxStringParser::ConsumeSyntaxComponent(
    Vector<CSSSyntaxComponent>& components) {
  input_.AdvanceUntilNonWhitespace();

  CSSSyntaxType type = CSSSyntaxType::kTokenStream;
  String ident;

  UChar cc = input_.NextInputChar();
  input_.Advance();

  if (cc == '<') {
    if (!ConsumeDataTypeName(type))
      return false;
  } else if (IsNameStartCodePoint(cc) || cc == '\\') {
    if (NextCharsAreIdentifier(cc, input_)) {
      input_.PushBack(cc);
      type = CSSSyntaxType::kIdent;
      if (!ConsumeIdent(ident))
        return false;
    }
  } else {
    return false;
  }

  DCHECK_NE(type, CSSSyntaxType::kTokenStream);

  CSSSyntaxRepeat repeat =
      IsPreMultiplied(type) ? CSSSyntaxRepeat::kNone : ConsumeRepeatIfPresent();
  components.emplace_back(type, ident, repeat);
  return true;
}

CSSSyntaxRepeat CSSSyntaxStringParser::ConsumeRepeatIfPresent() {
  UChar cc = input_.NextInputChar();
  if (cc == '+') {
    input_.Advance();
    return CSSSyntaxRepeat::kSpaceSeparated;
  }
  if (cc == '#') {
    input_.Advance();
    return CSSSyntaxRepeat::kCommaSeparated;
  }
  return CSSSyntaxRepeat::kNone;
}

bool CSSSyntaxStringParser::ConsumeDataTypeName(CSSSyntaxType& type) {
  for (unsigned size = 0;; ++size) {
    UChar cc = input_.PeekWithoutReplacement(size);
    if (IsNameCodePoint(cc))
      continue;
    if (cc == '>') {
      unsigned start = input_.Offset();
      input_.Advance(size + 1);
      if (auto syntax_type = ParseSyntaxType(input_.RangeAt(start, size))) {
        type = *syntax_type;
        return true;
      }
      return false;
    }
    return false;
  }
}

bool CSSSyntaxStringParser::ConsumeIdent(String& ident) {
  ident = ConsumeName(input_);
  // TODO(crbug.com/579788): Implement 'revert'.
  // TODO(crbug.com/882285): Make 'default' invalid as <custom-ident>.
  return !css_property_parser_helpers::IsCSSWideKeyword(ident) &&
         !css_property_parser_helpers::IsRevertKeyword(ident) &&
         !css_property_parser_helpers::IsDefaultKeyword(ident);
}

}  // namespace blink
