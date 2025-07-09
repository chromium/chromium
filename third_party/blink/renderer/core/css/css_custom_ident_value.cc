// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/properties/css_unresolved_property.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/style/scoped_css_name.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

CSSCustomIdentValue::CSSCustomIdentValue(const AtomicString& str)
    : CSSValue(kCustomIdentClass),
      string_(str),
      property_id_(CSSPropertyID::kInvalid) {
  needs_tree_scope_population_ = true;
}

CSSCustomIdentValue::CSSCustomIdentValue(CSSPropertyID id)
    : CSSValue(kCustomIdentClass), string_(), property_id_(id) {
  DCHECK(IsKnownPropertyID());
}

CSSCustomIdentValue::CSSCustomIdentValue(const ScopedCSSName& name)
    : CSSCustomIdentValue(name.GetName()) {
  tree_scope_ = name.GetTreeScope();
  needs_tree_scope_population_ = false;
}

CSSCustomIdentValue::CSSCustomIdentValue(const CSSFunctionValue& ident_function)
    : CSSValue(kCustomIdentClass),
      ident_function_(&ident_function),
      property_id_(CSSPropertyID::kInvalid) {
  needs_tree_scope_population_ = true;
}

AtomicString CSSCustomIdentValue::ComputeIdent(
    const CSSLengthResolver& length_resolver) const {
  if (!ident_function_) {
    return string_;
  }
  return ComputeIdent(*ident_function_, length_resolver);
}

AtomicString CSSCustomIdentValue::ComputeIdent(
    const CSSFunctionValue& ident_function,
    const CSSLengthResolver& length_resolver) {
  StringBuilder builder;

  for (const Member<const CSSValue>& item : ident_function) {
    if (auto* string_value = DynamicTo<CSSStringValue>(*item)) {
      builder.Append(string_value->Value());
    } else if (auto* custom_ident = DynamicTo<CSSCustomIdentValue>(*item)) {
      builder.Append(custom_ident->ComputeIdent(length_resolver));
    } else if (auto* primitive = DynamicTo<CSSPrimitiveValue>(*item)) {
      builder.AppendNumber(primitive->ComputeInteger(length_resolver));
    } else {
      NOTREACHED();
    }
  }

  return AtomicString(builder.ReleaseString());
}

const CSSCustomIdentValue* CSSCustomIdentValue::Resolve(
    const CSSLengthResolver& length_resolver) const {
  if (!ident_function_) {
    return this;
  }
  auto* custom_ident =
      MakeGarbageCollected<CSSCustomIdentValue>(ComputeIdent(length_resolver));
  custom_ident->tree_scope_ = tree_scope_;
  custom_ident->needs_tree_scope_population_ = needs_tree_scope_population_;
  return custom_ident;
}

String CSSCustomIdentValue::CustomCSSText() const {
  if (IsKnownPropertyID()) {
    return CSSUnresolvedProperty::Get(property_id_)
        .GetPropertyNameAtomicString();
  }
  if (ident_function_) {
    return ident_function_->CustomCSSText();
  }
  StringBuilder builder;
  SerializeIdentifier(string_, builder);
  return builder.ReleaseString();
}

unsigned CSSCustomIdentValue::CustomHash() const {
  if (IsKnownPropertyID()) {
    return HashInt(property_id_);
  } else if (ident_function_) {
    return HashPointer(ident_function_.Get());
  } else {
    return string_.Hash();
  }
}

const CSSCustomIdentValue& CSSCustomIdentValue::PopulateWithTreeScope(
    const TreeScope* tree_scope) const {
  DCHECK(this->needs_tree_scope_population_);
  CSSCustomIdentValue* populated =
      MakeGarbageCollected<CSSCustomIdentValue>(*this);
  populated->tree_scope_ = tree_scope;
  populated->needs_tree_scope_population_ = false;
  return *populated;
}

bool CSSCustomIdentValue::Equals(const CSSCustomIdentValue& other) const {
  if (IsKnownPropertyID()) {
    return property_id_ == other.property_id_;
  }
  return IsScopedValue() == other.IsScopedValue() &&
         tree_scope_ == other.tree_scope_ &&
         base::ValuesEquivalent(ident_function_, other.ident_function_) &&
         string_ == other.string_;
}

void CSSCustomIdentValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(tree_scope_);
  visitor->Trace(ident_function_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
