// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CUSTOM_IDENT_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CUSTOM_IDENT_VALUE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CSSFunctionValue;
class CSSLengthResolver;
class ScopedCSSName;
class TreeScope;

class CORE_EXPORT CSSCustomIdentValue : public CSSValue {
 public:
  explicit CSSCustomIdentValue(const AtomicString&);
  explicit CSSCustomIdentValue(CSSPropertyID);
  explicit CSSCustomIdentValue(const ScopedCSSName&);
  explicit CSSCustomIdentValue(const CSSFunctionValue& ident_function);

  const TreeScope* GetTreeScope() const { return tree_scope_.Get(); }
  const AtomicString& Value() const {
    DCHECK(!IsKnownPropertyID());
    return string_;
  }
  AtomicString ComputeIdent(const CSSLengthResolver&) const;
  static AtomicString ComputeIdent(const CSSFunctionValue&,
                                   const CSSLengthResolver&);
  // If `this` contains any ident() functions, resolves those functions
  // a returns a new literal CSSCustomIdentValue with the result.
  // Otherwise, returns `this`.
  const CSSCustomIdentValue* Resolve(const CSSLengthResolver&) const;
  bool IsKnownPropertyID() const {
    return property_id_ != CSSPropertyID::kInvalid;
  }
  CSSPropertyID ValueAsPropertyID() const {
    DCHECK(IsKnownPropertyID());
    return property_id_;
  }

  String CustomCSSText() const;
  unsigned CustomHash() const;

  const CSSCustomIdentValue& PopulateWithTreeScope(const TreeScope*) const;

  bool Equals(const CSSCustomIdentValue& other) const;

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  WeakMember<const TreeScope> tree_scope_;
  Member<const CSSFunctionValue> ident_function_;
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
