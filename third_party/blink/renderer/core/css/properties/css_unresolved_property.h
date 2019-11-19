// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_UNRESOLVED_PROPERTY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_UNRESOLVED_PROPERTY_H_

#include "third_party/blink/renderer/core/css/properties/css_exposure.h"
#include "third_party/blink/renderer/core/css/properties/css_property_instances.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// TODO(crbug.com/793288): audit and consider redesigning how aliases are
// handled once more of project Ribbon is done and all use of aliases can be
// found and (hopefully) constrained.
class CORE_EXPORT CSSUnresolvedProperty {
 public:
  static const CSSUnresolvedProperty& Get(CSSPropertyID);
  static const CSSUnresolvedProperty* GetAliasProperty(CSSPropertyID);

  bool IsWebExposed() const { return blink::IsWebExposed(Exposure()); }
  bool IsUAExposed() const { return blink::IsUAExposed(Exposure()); }
  virtual CSSExposure Exposure() const { return CSSExposure::kWeb; }
  virtual bool IsResolvedProperty() const { return false; }
  virtual const char* GetPropertyName() const {
    NOTREACHED();
    return nullptr;
  }
  virtual const WTF::AtomicString& GetPropertyNameAtomicString() const {
    NOTREACHED();
    return g_empty_atom;
  }
  virtual const char* GetJSPropertyName() const {
    NOTREACHED();
    return "";
  }
  WTF::String GetPropertyNameString() const {
    // We share the StringImpl with the AtomicStrings.
    return GetPropertyNameAtomicString().GetString();
  }

 protected:
  static const CSSUnresolvedProperty& GetNonAliasProperty(CSSPropertyID);

  constexpr CSSUnresolvedProperty() {}
};

const CSSUnresolvedProperty& GetCSSPropertyVariableInternal();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_UNRESOLVED_PROPERTY_H_
