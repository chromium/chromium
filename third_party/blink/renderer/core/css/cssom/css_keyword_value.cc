// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_revert_value.h"
#include "third_party/blink/renderer/core/css/css_scoped_keyword_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

CSSKeywordValue* CSSKeywordValue::Create(const String& keyword,
                                         ExceptionState& exception_state) {
  if (keyword.empty()) {
    exception_state.ThrowTypeError(
        "CSSKeywordValue does not support empty strings");
    return nullptr;
  }
  return MakeGarbageCollected<CSSKeywordValue>(keyword);
}

CSSKeywordValue* CSSKeywordValue::FromCSSValue(const CSSValue& value) {
  if (value.IsInheritedValue()) {
    return MakeGarbageCollected<CSSKeywordValue>(
        getValueName(CSSValueID::kInherit));
  }
  if (value.IsInitialValue()) {
    return MakeGarbageCollected<CSSKeywordValue>(
        getValueName(CSSValueID::kInitial));
  }
  if (value.IsUnsetValue()) {
    return MakeGarbageCollected<CSSKeywordValue>(
        getValueName(CSSValueID::kUnset));
  }
  if (value.IsRevertValue()) {
    return MakeGarbageCollected<CSSKeywordValue>(
        getValueName(CSSValueID::kRevert));
  }
  if (value.IsRevertLayerValue()) {
    return MakeGarbageCollected<CSSKeywordValue>(
        getValueName(CSSValueID::kRevertLayer));
  }
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    return MakeGarbageCollected<CSSKeywordValue>(
        getValueName(identifier_value->GetValueID()));
  }
  if (const auto* ident_value = DynamicTo<CSSCustomIdentValue>(value)) {
    if (ident_value->IsKnownPropertyID()) {
      // CSSPropertyID represents the LHS of a CSS declaration, and
      // CSSKeywordValue represents a RHS.
      return nullptr;
    }
    return MakeGarbageCollected<CSSKeywordValue>(ident_value->Value());
  }
  if (auto* scoped_keyword_value =
          DynamicTo<cssvalue::CSSScopedKeywordValue>(value)) {
    return MakeGarbageCollected<CSSKeywordValue>(
        getValueName(scoped_keyword_value->GetValueID()));
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

CSSKeywordValue* CSSKeywordValue::Create(const String& keyword) {
  DCHECK(!keyword.empty());
  return MakeGarbageCollected<CSSKeywordValue>(keyword);
}

const String& CSSKeywordValue::value() const {
  return keyword_value_;
}

void CSSKeywordValue::setValue(const String& keyword,
                               ExceptionState& exception_state) {
  if (keyword.empty()) {
    exception_state.ThrowTypeError(
        "CSSKeywordValue does not support empty strings");
    return;
  }
  keyword_value_ = keyword;
}

CSSValueID CSSKeywordValue::KeywordValueID() const {
  return CssValueKeywordID(keyword_value_);
}

const CSSValue* CSSKeywordValue::ToCSSValue() const {
  CSSValueID keyword_id = KeywordValueID();
  switch (keyword_id) {
    case (CSSValueID::kInherit):
      return CSSInheritedValue::Create();
    case (CSSValueID::kInitial):
      return CSSInitialValue::Create();
    case (CSSValueID::kUnset):
      return cssvalue::CSSUnsetValue::Create();
    case (CSSValueID::kRevert):
      return cssvalue::CSSRevertValue::Create();
    case (CSSValueID::kInvalid):
      return MakeGarbageCollected<CSSCustomIdentValue>(
          AtomicString(keyword_value_));
    default:
      return CSSIdentifierValue::Create(keyword_id);
  }
}

}  // namespace blink
