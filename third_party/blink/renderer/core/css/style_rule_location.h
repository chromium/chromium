// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_LOCATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_LOCATION_H_

#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CSSPropertyValueSet;
class CSSStringValue;
class CSSURLPatternValue;
class Document;

// https://drafts.csswg.org/css-navigation-1/#at-location
class CORE_EXPORT StyleRuleLocation : public StyleRuleBase {
 public:
  StyleRuleLocation(const AtomicString& name, CSSPropertyValueSet*);
  StyleRuleLocation(const StyleRuleLocation&) = default;

  void TraceAfterDispatch(Visitor*) const;

  const AtomicString& GetName() const { return name_; }

  const CSSURLPatternValue* GetPattern() const { return pattern_.Get(); }
  const CSSStringValue* GetProtocol() const { return protocol_.Get(); }
  const CSSStringValue* GetHostname() const { return hostname_.Get(); }
  const CSSStringValue* GetPort() const { return port_.Get(); }
  const CSSStringValue* GetPathname() const { return pathname_.Get(); }
  const CSSStringValue* GetSearch() const { return search_.Get(); }
  const CSSStringValue* GetHash() const { return hash_.Get(); }
  const CSSStringValue* GetBaseUrl() const { return base_url_.Get(); }

  void CreateRouteIfNeeded(Document*) const;

 private:
  AtomicString name_;

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
struct DowncastTraits<StyleRuleLocation> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsLocationRule();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_LOCATION_H_
