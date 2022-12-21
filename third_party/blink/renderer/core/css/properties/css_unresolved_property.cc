// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_unresolved_property.h"

#include "third_party/blink/renderer/core/css/properties/longhands/variable.h"

namespace blink {
namespace {

static constexpr Variable property_csspropertyvariable;

}  // namespace

const CSSUnresolvedProperty* CSSUnresolvedProperty::GetAliasProperty(
    CSSPropertyID id) {
  return GetAliasPropertyInternal(id);
}

const CSSUnresolvedProperty& CSSUnresolvedProperty::Get(CSSPropertyID id) {
  DCHECK_NE(id, CSSPropertyID::kInvalid);
  DCHECK_LE(id, kLastUnresolvedCSSProperty);
  if (id <= kLastCSSProperty) {
    return GetNonAliasProperty(id);
  }
  return *GetAliasProperty(id);
}

const CSSUnresolvedProperty& GetCSSPropertyVariableInternal() {
  return property_csspropertyvariable;
}

}  // namespace blink
