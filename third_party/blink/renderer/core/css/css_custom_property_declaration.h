// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CUSTOM_PROPERTY_DECLARATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CUSTOM_PROPERTY_DECLARATION_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CORE_EXPORT CSSCustomPropertyDeclaration : public CSSValue {
 public:
  CSSCustomPropertyDeclaration(const AtomicString& name, CSSValueID id)
      : CSSValue(kCustomPropertyDeclarationClass),
        name_(name),
        value_(nullptr),
        value_id_(id) {
    DCHECK(id == CSSValueID::kInherit || id == CSSValueID::kInitial ||
           id == CSSValueID::kUnset);
  }

  CSSCustomPropertyDeclaration(const AtomicString& name,
                               scoped_refptr<CSSVariableData> value)
      : CSSValue(kCustomPropertyDeclarationClass),
        name_(name),
        value_(std::move(value)),
        value_id_(CSSValueID::kInvalid) {}

  const AtomicString& GetName() const { return name_; }
  CSSVariableData* Value() const { return value_.get(); }

  bool IsInherit(bool is_inherited_property) const {
    return value_id_ == CSSValueID::kInherit ||
           (is_inherited_property && value_id_ == CSSValueID::kUnset);
  }
  bool IsInitial(bool is_inherited_property) const {
    return value_id_ == CSSValueID::kInitial ||
           (!is_inherited_property && value_id_ == CSSValueID::kUnset);
  }

  String CustomCSSText() const;

  bool Equals(const CSSCustomPropertyDeclaration& other) const {
    return this == &other;
  }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  const AtomicString name_;
  scoped_refptr<CSSVariableData> value_;
  CSSValueID value_id_;
};

template <>
struct DowncastTraits<CSSCustomPropertyDeclaration> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsCustomPropertyDeclaration();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CUSTOM_PROPERTY_DECLARATION_H_
