// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CUSTOM_IDENT_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CUSTOM_IDENT_VALUE_H_

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CORE_EXPORT CSSCustomIdentValue : public CSSValue {
 public:
  explicit CSSCustomIdentValue(const AtomicString&);
  explicit CSSCustomIdentValue(CSSPropertyID);

  AtomicString Value() const {
    DCHECK(!IsKnownPropertyID());
    return string_;
  }
  bool IsKnownPropertyID() const {
    return property_id_ != CSSPropertyID::kInvalid;
  }
  CSSPropertyID ValueAsPropertyID() const {
    DCHECK(IsKnownPropertyID());
    return property_id_;
  }

  String CustomCSSText() const;

  bool Equals(const CSSCustomIdentValue& other) const {
    return IsKnownPropertyID() ? property_id_ == other.property_id_
                               : string_ == other.string_;
  }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  AtomicString string_;
  CSSPropertyID property_id_;
};

template <>
struct DowncastTraits<CSSCustomIdentValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsCustomIdentValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CUSTOM_IDENT_VALUE_H_
