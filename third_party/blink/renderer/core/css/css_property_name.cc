// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_property_name.h"

#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

namespace {

// TODO(andruud): Reduce this to sizeof(void*).
struct SameSizeAsCSSPropertyName {
  CSSPropertyID property_id_;
  AtomicString custom_property_name_;
};

static_assert(sizeof(CSSPropertyName) == sizeof(SameSizeAsCSSPropertyName),
              "CSSPropertyName should stay small");

}  // namespace

bool CSSPropertyName::operator==(const CSSPropertyName& other) const {
  if (value_ != other.value_)
    return false;
  if (value_ != static_cast<int>(CSSPropertyID::kVariable))
    return true;
  return custom_property_name_ == other.custom_property_name_;
}

AtomicString CSSPropertyName::ToAtomicString() const {
  if (IsCustomProperty())
    return custom_property_name_;
  return CSSProperty::Get(Id()).GetPropertyNameAtomicString();
}

unsigned CSSPropertyName::GetHash() const {
  if (IsCustomProperty())
    return AtomicStringHash::GetHash(custom_property_name_);
  return value_;
}

}  // namespace blink
