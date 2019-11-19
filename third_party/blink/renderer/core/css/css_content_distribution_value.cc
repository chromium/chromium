// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_content_distribution_value.h"

#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

CSSContentDistributionValue::CSSContentDistributionValue(
    CSSValueID distribution,
    CSSValueID position,
    CSSValueID overflow)
    : CSSValue(kCSSContentDistributionClass),
      distribution_(distribution),
      position_(position),
      overflow_(overflow) {}

CSSContentDistributionValue::~CSSContentDistributionValue() = default;

String CSSContentDistributionValue::CustomCSSText() const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  if (IsValidCSSValueID(distribution_))
    list->Append(*CSSIdentifierValue::Create(distribution_));
  if (IsValidCSSValueID(position_)) {
    if (position_ == CSSValueID::kFirstBaseline ||
        position_ == CSSValueID::kLastBaseline) {
      CSSValueID preference = position_ == CSSValueID::kFirstBaseline
                                  ? CSSValueID::kFirst
                                  : CSSValueID::kLast;
      list->Append(*CSSIdentifierValue::Create(preference));
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kBaseline));
    } else {
      if (IsValidCSSValueID(overflow_))
        list->Append(*CSSIdentifierValue::Create(overflow_));
      list->Append(*CSSIdentifierValue::Create(position_));
    }
  }
  return list->CustomCSSText();
}

bool CSSContentDistributionValue::Equals(
    const CSSContentDistributionValue& other) const {
  return distribution_ == other.distribution_ && position_ == other.position_ &&
         overflow_ == other.overflow_;
}

}  // namespace cssvalue
}  // namespace blink
