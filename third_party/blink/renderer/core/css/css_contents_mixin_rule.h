// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CONTENTS_MIXIN_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CONTENTS_MIXIN_RULE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class StyleRuleContentsStatement;

// CSSOM wrapper for @contents (used within @mixin). It is completely inert;
// the only thing you can really do is serialize it.
//
// The base type is called StyleRuleContentsStatement for less confusion,
// but for CSSOM, where names are exposed to JavaScript, we need to follow
// what the standard says about naming, which is “@contents rule”.
class CORE_EXPORT CSSContentsMixinRule final : public CSSRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSContentsMixinRule(StyleRuleContentsStatement*, CSSStyleSheet* parent);
  String name() const;
  String cssText() const override;
  void Reattach(StyleRuleBase*) override;
  void Trace(Visitor*) const override;

 private:
  CSSRule::Type GetType() const override { return kContentsMixinRule; }
  Member<StyleRuleContentsStatement> contents_statement_;
};

template <>
struct DowncastTraits<CSSContentsMixinRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.GetType() == CSSRule::kContentsMixinRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CONTENTS_MIXIN_RULE_H_
