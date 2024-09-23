// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ATTR_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ATTR_TYPE_H_

#include <optional>

#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"

namespace blink {

struct CSSAttrType {
  STACK_ALLOCATED();

 public:
  static CSSAttrType Parse(StringView attr_type);

  enum class Category {
    kUnknown,
    kString,
    kIdent,
    kColor,
    kNumber,
    kPercentage,
    kLength,
    kAngle,
    kTime,
    kFrequency,
    kFlex,
    kDimensionUnit
  };

  CSSAttrType() : category(Category::kUnknown) {}

  explicit CSSAttrType(Category cat) : category(cat) {
    DCHECK_NE(cat, Category::kUnknown);
    DCHECK_NE(cat, Category::kDimensionUnit);
  }

  explicit CSSAttrType(CSSPrimitiveValue::UnitType unit)
      : category(Category::kDimensionUnit), dimension_unit(unit) {
    DCHECK_NE(unit, CSSPrimitiveValue::UnitType::kUnknown);
  }

  bool IsValid() const { return category != Category::kUnknown; }

  std::optional<CSSSyntaxDefinition> ConvertToCSSSyntaxDefinition() const;

  Category category;
  // Used when |category| is |kDimensionUnit|
  CSSPrimitiveValue::UnitType dimension_unit;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ATTR_TYPE_H_
