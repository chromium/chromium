// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CUSTOM_PROPERTY_DECLARATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CUSTOM_PROPERTY_DECLARATION_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CORE_EXPORT CSSCustomPropertyDeclaration : public CSSValue {
 public:
  explicit CSSCustomPropertyDeclaration(CSSValueID id)
      : CSSValue(kCustomPropertyDeclarationClass),
        value_(nullptr),
        value_id_(id) {
    DCHECK(css_parsing_utils::IsCSSWideKeyword(id));
  }

  explicit CSSCustomPropertyDeclaration(scoped_refptr<CSSVariableData> value)
      : CSSValue(kCustomPropertyDeclarationClass),
        value_(std::move(value)),
        value_id_(CSSValueID::kInvalid) {}

  CSSVariableData* Value() const { return value_.get(); }

  bool IsInherit(bool is_inherited_property) const {
    return value_id_ == CSSValueID::kInherit ||
           (is_inherited_property && value_id_ == CSSValueID::kUnset);
  }
  bool IsInitial(bool is_inherited_property) const {
    return value_id_ == CSSValueID::kInitial ||
           (!is_inherited_property && value_id_ == CSSValueID::kUnset);
  }
  bool IsRevert() const { return value_id_ == CSSValueID::kRevert; }
  bool IsRevertLayer() const { return value_id_ == CSSValueID::kRevertLayer; }

  String CustomCSSText() const;

  bool Equals(const CSSCustomPropertyDeclaration& other) const {
    return this == &other;
  }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
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
