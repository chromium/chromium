// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_syntax_component.h"

#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/cssom_types.h"

namespace blink {

bool CSSSyntaxComponent::CanTake(const CSSStyleValue& value) const {
  switch (type_) {
    case CSSSyntaxType::kTokenStream:
      return value.GetType() == CSSStyleValue::kUnparsedType;
    case CSSSyntaxType::kIdent:
      return value.GetType() == CSSStyleValue::kKeywordType &&
             static_cast<const CSSKeywordValue&>(value).value() == string_;
    case CSSSyntaxType::kLength:
      return CSSOMTypes::IsCSSStyleValueLength(value);
    case CSSSyntaxType::kInteger:
      // TODO(andruud): Support rounding.
      // https://drafts.css-houdini.org/css-typed-om-1/#numeric-objects
      FALLTHROUGH;
    case CSSSyntaxType::kNumber:
      return CSSOMTypes::IsCSSStyleValueNumber(value);
    case CSSSyntaxType::kPercentage:
      return CSSOMTypes::IsCSSStyleValuePercentage(value);
    case CSSSyntaxType::kLengthPercentage:
      // TODO(andruud): Support calc(X% + Ypx).
      return CSSOMTypes::IsCSSStyleValueLength(value) ||
             CSSOMTypes::IsCSSStyleValuePercentage(value);
    case CSSSyntaxType::kColor:
      // TODO(andruud): Support custom properties in CSSUnsupportedStyleValue.
      return false;
    case CSSSyntaxType::kImage:
    case CSSSyntaxType::kUrl:
      return value.GetType() == CSSStyleValue::kURLImageType;
    case CSSSyntaxType::kAngle:
      return CSSOMTypes::IsCSSStyleValueAngle(value);
    case CSSSyntaxType::kTime:
      return CSSOMTypes::IsCSSStyleValueTime(value);
    case CSSSyntaxType::kResolution:
      return CSSOMTypes::IsCSSStyleValueResolution(value);
    case CSSSyntaxType::kTransformFunction:
      // TODO(andruud): Currently not supported by Typed OM.
      // https://github.com/w3c/css-houdini-drafts/issues/290
      // For now, this should accept a CSSUnsupportedStyleValue, such that
      // <transform-function> values can be moved from one registered property
      // to another.
      // TODO(andruud): Support custom properties in CSSUnsupportedStyleValue.
      return false;
    case CSSSyntaxType::kTransformList:
      return value.GetType() == CSSStyleValue::kTransformType;
    case CSSSyntaxType::kCustomIdent:
      return value.GetType() == CSSStyleValue::kKeywordType;
    default:
      return false;
  }
}

}  // namespace blink
