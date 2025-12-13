// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CUSTOM_MEDIA_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CUSTOM_MEDIA_RULE_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_boolean_medialist.h"
#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/media_query_set_owner.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class StyleRuleBase;
class StyleRuleCustomMedia;

class CORE_EXPORT CSSCustomMediaRule final : public CSSRule,
                                             public MediaQuerySetOwner {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSCustomMediaRule(StyleRuleCustomMedia*, CSSStyleSheet* parent);
  ~CSSCustomMediaRule() override;

  String name() const;

  V8CustomMediaQuery* query();

  void Reattach(StyleRuleBase*) override;

  String cssText() const override;

  void Trace(Visitor*) const override;

 private:
  CSSRule::Type GetType() const override { return kCustomMediaRule; }

  MediaQuerySetOwner* GetMediaQuerySetOwner() override { return this; }
  const MediaQuerySet* MediaQueries() const override;
  void SetMediaQueries(const MediaQuerySet*) override;

  Member<StyleRuleCustomMedia> custom_media_rule_;
};

template <>
struct DowncastTraits<CSSCustomMediaRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.GetType() == CSSRule::kCustomMediaRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CUSTOM_MEDIA_RULE_H_
