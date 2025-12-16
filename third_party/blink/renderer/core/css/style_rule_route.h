// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_ROUTE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_ROUTE_H_

#include "third_party/blink/renderer/core/css/style_rule.h"

namespace blink {

class CSSPropertyValueSet;
class CSSStringValue;
class CSSURLPatternValue;
class Document;

// https://drafts.csswg.org/css-navigation-1/#at-route
class CORE_EXPORT StyleRuleRoute : public StyleRuleBase {
 public:
  StyleRuleRoute(const String& name, CSSPropertyValueSet*);
  StyleRuleRoute(const StyleRuleRoute&);

  void TraceAfterDispatch(Visitor*) const;

  const String& GetName() const { return name_; }

  void CreateRouteIfNeeded(Document*) const;

 private:
  String name_;

  Member<const CSSURLPatternValue> pattern_;
  Member<const CSSStringValue> protocol_;
  Member<const CSSStringValue> hostname_;
  Member<const CSSStringValue> port_;
  Member<const CSSStringValue> pathname_;
  Member<const CSSStringValue> search_;
  Member<const CSSStringValue> hash_;
  Member<const CSSStringValue> base_url_;
};

template <>
struct DowncastTraits<StyleRuleRoute> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsRouteRule();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_ROUTE_H_
