// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_attr_type.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/css/css_syntax_string_parser.h"

namespace blink {

// static
CSSAttrType CSSAttrType::Parse(StringView attr_type) {
  using Category = CSSAttrType::Category;
  if (attr_type == "string") {
    return CSSAttrType(Category::kString);
  }
  if (attr_type == "ident") {
    return CSSAttrType(Category::kIdent);
  }
  if (attr_type == "color") {
    return CSSAttrType(Category::kColor);
  }
  if (attr_type == "number") {
    return CSSAttrType(Category::kNumber);
  }
  if (attr_type == "percentage") {
    return CSSAttrType(Category::kPercentage);
  }
  if (attr_type == "length") {
    return CSSAttrType(Category::kLength);
  }
  if (attr_type == "angle") {
    return CSSAttrType(Category::kAngle);
  }
  if (attr_type == "time") {
    return CSSAttrType(Category::kTime);
  }
  if (attr_type == "frequency") {
    return CSSAttrType(Category::kFrequency);
  }
  if (attr_type == "flex") {
    return CSSAttrType(Category::kFlex);
  }

  CSSPrimitiveValue::UnitType unit =
      CSSPrimitiveValue::StringToUnitType(attr_type);
  // The <dimension-unit> production matches a literal "%"
  // character (that is, a <delim-token> with a value of "%")
  // or an ident whose value is any of the CSS units for
  // <length>, <angle>, <time>, <frequency>, or <flex> values.
  if (CSSPrimitiveValue::IsLength(unit) || CSSPrimitiveValue::IsAngle(unit) ||
      CSSPrimitiveValue::IsTime(unit) || CSSPrimitiveValue::IsFrequency(unit) ||
      CSSPrimitiveValue::IsFlex(unit) ||
      CSSPrimitiveValue::IsPercentage(unit)) {
    return CSSAttrType(unit);
  }

  return CSSAttrType();
}

std::optional<CSSSyntaxDefinition> CSSAttrType::ConvertToCSSSyntaxDefinition()
    const {
  String syntax;
  switch (category) {
    case Category::kUnknown:
      return std::nullopt;
    case Category::kString:
      syntax = "<string>";
      break;
    case Category::kIdent:
      syntax = "<ident>";
      break;
    case Category::kColor:
      syntax = "<color>";
      break;
    case Category::kNumber:
      syntax = "<number>";
      break;
    case Category::kPercentage:
      syntax = "<percentage>";
      break;
    case Category::kLength:
      syntax = "<length>";
      break;
    case Category::kAngle:
      syntax = "<angle>";
      break;
    case Category::kTime:
      syntax = "<time>";
      break;
    case Category::kFrequency:
      syntax = "<frequency>";
      break;
    case Category::kFlex:
      // TODO(crbug.com/40320391): Support parsing from <flex>.
      syntax = "<flex>";
      break;
    case Category::kDimensionUnit:
      syntax = "<number>";
      break;
  }
  return CSSSyntaxStringParser(syntax).Parse();
}

}  // namespace blink
