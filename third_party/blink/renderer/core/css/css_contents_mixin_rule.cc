// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_contents_mixin_rule.h"

#include "base/check.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

CSSContentsMixinRule::CSSContentsMixinRule(
    StyleRuleContentsStatement* contents_statement,
    CSSStyleSheet* sheet)
    : CSSRule(sheet), contents_statement_(contents_statement) {}

String CSSContentsMixinRule::cssText() const {
  return "@contents;";
}

void CSSContentsMixinRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  contents_statement_ = To<StyleRuleContentsStatement>(rule);
}

void CSSContentsMixinRule::Trace(Visitor* visitor) const {
  visitor->Trace(contents_statement_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
