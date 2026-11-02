// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIVATE_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIVATE_RULE_H_

#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSPrivateRule final : public CSSStyleRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSPrivateRule(StyleRule*, CSSStyleSheet* parent);

  String cssText() const override;
  void Reattach(StyleRuleBase*) override;

  void Trace(Visitor*) const override;

  AtomicString HygienicToAuthoredCustomPropertyName(const AtomicString&) const;
  AtomicString AuthoredToHygienicCustomPropertyName(const AtomicString&) const;
  AtomicString EnsureCustomPropertyNameForHygienic(const AtomicString&);
  PropertyRegistry& GetPropertyRegistry() { return *property_registry_; }
  PrivateNameMap& GetAuthoredToHygienicNameMap() {
    return authored_to_hygienic_names_;
  }

 private:
  void UpdatePrivateNames();

  CSSRule::Type GetType() const override { return kPrivateRule; }

  Member<PropertyRegistry> property_registry_;
  PrivateNameMap authored_to_hygienic_names_;
};

template <>
struct DowncastTraits<CSSPrivateRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.GetType() == CSSRule::kPrivateRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIVATE_RULE_H_
