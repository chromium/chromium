// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_SHORTHAND_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_SHORTHAND_H_

#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSParserContext;
class CSSParserLocalContext;
class CSSParserTokenRange;
class CSSPropertyValue;

class Shorthand : public CSSProperty {
 public:
  // Parses and consumes entire shorthand value from the token range and adds
  // all constituent parsed longhand properties to the 'properties' set.
  // Returns false if the input is invalid.
  virtual bool ParseShorthand(
      bool important,
      CSSParserTokenRange&,
      const CSSParserContext&,
      const CSSParserLocalContext&,
      HeapVector<CSSPropertyValue, 256>& properties) const {
    NOTREACHED();
    return false;
  }

 protected:
  constexpr Shorthand(CSSPropertyID id,
                      uint16_t flags,
                      char repetition_separator)
      : CSSProperty(id, flags | kShorthand, repetition_separator) {}
};

template <>
struct DowncastTraits<Shorthand> {
  static bool AllowFrom(const CSSProperty& property) {
    return property.IsShorthand();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_SHORTHAND_H_
