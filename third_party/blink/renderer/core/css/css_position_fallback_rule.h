// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_POSITION_FALLBACK_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_POSITION_FALLBACK_RULE_H_

#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/style_rule.h"

namespace blink {

class CSSTryRule;
class StyleRuleTry;

class StyleRulePositionFallback final : public StyleRuleBase {
 public:
  explicit StyleRulePositionFallback(const AtomicString&);
  StyleRulePositionFallback(const StyleRulePositionFallback&);
  ~StyleRulePositionFallback();

  const AtomicString& Name() const { return name_; }
  const HeapVector<Member<StyleRuleTry>>& TryRules() const {
    return try_rules_;
  }

  void ParserAppendTryRule(StyleRuleTry*);

  StyleRulePositionFallback* Copy() const {
    return MakeGarbageCollected<StyleRulePositionFallback>(*this);
  }

  void TraceAfterDispatch(Visitor*) const;

 private:
  AtomicString name_;
  HeapVector<Member<StyleRuleTry>> try_rules_;
};

template <>
struct DowncastTraits<StyleRulePositionFallback> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsPositionFallbackRule();
  }
};

class CSSPositionFallbackRule final : public CSSRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSPositionFallbackRule(StyleRulePositionFallback*, CSSStyleSheet* parent);
  ~CSSPositionFallbackRule() final;

  String name() const { return position_fallback_rule_->Name(); }

  Type GetType() const final { return kPositionFallbackRule; }
  CSSRuleList* cssRules() const final { return rule_list_cssom_wrapper_; }

  // For CSSRuleList.
  unsigned length() const { return child_rule_cssom_wrappers_.size(); }
  CSSTryRule* Item(unsigned index) const {
    return index < length() ? child_rule_cssom_wrappers_[index] : nullptr;
  }

  String cssText() const final;
  void Reattach(StyleRuleBase*) final;

  void Trace(Visitor*) const final;

 private:
  void CreateChildRuleWrappers();

  Member<StyleRulePositionFallback> position_fallback_rule_;
  HeapVector<Member<CSSTryRule>> child_rule_cssom_wrappers_;
  Member<CSSRuleList> rule_list_cssom_wrapper_;
};

template <>
struct DowncastTraits<CSSPositionFallbackRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.GetType() == CSSRule::kPositionFallbackRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_POSITION_FALLBACK_RULE_H_
