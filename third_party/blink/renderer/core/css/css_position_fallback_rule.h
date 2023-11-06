// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_POSITION_FALLBACK_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_POSITION_FALLBACK_RULE_H_

#include "third_party/blink/renderer/core/css/css_grouping_rule.h"
#include "third_party/blink/renderer/core/css/style_rule.h"

namespace blink {

class CascadeLayer;

class StyleRulePositionFallback final : public StyleRuleGroup {
 public:
  StyleRulePositionFallback(const AtomicString&,
                            HeapVector<Member<StyleRuleBase>> rules);
  StyleRulePositionFallback(const StyleRulePositionFallback&);
  ~StyleRulePositionFallback();

  const AtomicString& Name() const { return name_; }

  StyleRulePositionFallback* Copy() const {
    return MakeGarbageCollected<StyleRulePositionFallback>(*this);
  }

  void SetCascadeLayer(const CascadeLayer* layer) { layer_ = layer; }
  const CascadeLayer* GetCascadeLayer() const { return layer_.Get(); }

  void TraceAfterDispatch(Visitor*) const;

 private:
  AtomicString name_;
  Member<const CascadeLayer> layer_;
};

template <>
struct DowncastTraits<StyleRulePositionFallback> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsPositionFallbackRule();
  }
};

class CSSPositionFallbackRule final : public CSSGroupingRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSPositionFallbackRule(StyleRulePositionFallback*, CSSStyleSheet* parent);
  ~CSSPositionFallbackRule() final;

  StyleRulePositionFallback* PositionFallback() const {
    return To<StyleRulePositionFallback>(group_rule_.Get());
  }

  String name() const { return PositionFallback()->Name(); }
  Type GetType() const final { return kPositionFallbackRule; }
  String cssText() const final;
};

template <>
struct DowncastTraits<CSSPositionFallbackRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.GetType() == CSSRule::kPositionFallbackRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_POSITION_FALLBACK_RULE_H_
