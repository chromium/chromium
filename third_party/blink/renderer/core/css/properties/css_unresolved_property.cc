// Copyright 2019 The Chromium Authors. All rights reserved.
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

const CSSUnresolvedProperty& CSSUnresolvedProperty::GetNonAliasProperty(
    CSSPropertyID id) {
  if (id == CSSPropertyID::kVariable)
    return GetCSSPropertyVariableInternal();
  return GetNonAliasPropertyInternal(id);
}

const CSSUnresolvedProperty& CSSUnresolvedProperty::Get(CSSPropertyID id) {
  DCHECK_NE(id, CSSPropertyID::kInvalid);
  DCHECK_LE(id, lastUnresolvedCSSProperty);
  if (id <= lastCSSProperty)
    return GetNonAliasProperty(id);
  return *GetAliasProperty(id);
}

const CSSUnresolvedProperty& GetCSSPropertyVariableInternal() {
  return property_csspropertyvariable;
}

}  // namespace blink
