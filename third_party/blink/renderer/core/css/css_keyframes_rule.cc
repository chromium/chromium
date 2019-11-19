/*
 * Copyright (C) 2007, 2008, 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"

#include <memory>

#include "third_party/blink/renderer/core/css/css_keyframe_rule.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

StyleRuleKeyframes::StyleRuleKeyframes()
    : StyleRuleBase(kKeyframes), version_(0) {}

StyleRuleKeyframes::StyleRuleKeyframes(const StyleRuleKeyframes& o) = default;

StyleRuleKeyframes::~StyleRuleKeyframes() = default;

void StyleRuleKeyframes::ParserAppendKeyframe(StyleRuleKeyframe* keyframe) {
  if (!keyframe)
    return;
  keyframes_.push_back(keyframe);
}

void StyleRuleKeyframes::WrapperAppendKeyframe(StyleRuleKeyframe* keyframe) {
  keyframes_.push_back(keyframe);
  StyleChanged();
}

void StyleRuleKeyframes::WrapperRemoveKeyframe(unsigned index) {
  keyframes_.EraseAt(index);
  StyleChanged();
}

int StyleRuleKeyframes::FindKeyframeIndex(const String& key) const {
  std::unique_ptr<Vector<double>> keys = CSSParser::ParseKeyframeKeyList(key);
  if (!keys)
    return -1;
  for (wtf_size_t i = keyframes_.size(); i--;) {
    if (keyframes_[i]->Keys() == *keys)
      return static_cast<int>(i);
  }
  return -1;
}

void StyleRuleKeyframes::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(keyframes_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

CSSKeyframesRule::CSSKeyframesRule(StyleRuleKeyframes* keyframes_rule,
                                   CSSStyleSheet* parent)
    : CSSRule(parent),
      keyframes_rule_(keyframes_rule),
      child_rule_cssom_wrappers_(keyframes_rule->Keyframes().size()),
      is_prefixed_(keyframes_rule->IsVendorPrefixed()) {}

CSSKeyframesRule::~CSSKeyframesRule() = default;

void CSSKeyframesRule::setName(const String& name) {
  CSSStyleSheet::RuleMutationScope mutation_scope(this);

  keyframes_rule_->SetName(name);
}

void CSSKeyframesRule::appendRule(const ExecutionContext* execution_context,
                                  const String& rule_text) {
  DCHECK_EQ(child_rule_cssom_wrappers_.size(),
            keyframes_rule_->Keyframes().size());

  CSSStyleSheet* style_sheet = parentStyleSheet();
  auto* context = MakeGarbageCollected<CSSParserContext>(
      ParserContext(execution_context->GetSecureContextMode()), style_sheet);
  StyleRuleKeyframe* keyframe =
      CSSParser::ParseKeyframeRule(context, rule_text);
  if (!keyframe)
    return;

  CSSStyleSheet::RuleMutationScope mutation_scope(this);

  keyframes_rule_->WrapperAppendKeyframe(keyframe);

  child_rule_cssom_wrappers_.Grow(length());
}

void CSSKeyframesRule::deleteRule(const String& s) {
  DCHECK_EQ(child_rule_cssom_wrappers_.size(),
            keyframes_rule_->Keyframes().size());

  int i = keyframes_rule_->FindKeyframeIndex(s);
  if (i < 0)
    return;

  CSSStyleSheet::RuleMutationScope mutation_scope(this);

  keyframes_rule_->WrapperRemoveKeyframe(i);

  if (child_rule_cssom_wrappers_[i])
    child_rule_cssom_wrappers_[i]->SetParentRule(nullptr);
  child_rule_cssom_wrappers_.EraseAt(i);
}

CSSKeyframeRule* CSSKeyframesRule::findRule(const String& s) {
  int i = keyframes_rule_->FindKeyframeIndex(s);
  return (i >= 0) ? Item(i) : nullptr;
}

String CSSKeyframesRule::cssText() const {
  StringBuilder result;
  if (IsVendorPrefixed())
    result.Append("@-webkit-keyframes ");
  else
    result.Append("@keyframes ");
  result.Append(name());
  result.Append(" { \n");

  unsigned size = length();
  for (unsigned i = 0; i < size; ++i) {
    result.Append("  ");
    result.Append(keyframes_rule_->Keyframes()[i]->CssText());
    result.Append('\n');
  }
  result.Append('}');
  return result.ToString();
}

unsigned CSSKeyframesRule::length() const {
  return keyframes_rule_->Keyframes().size();
}

CSSKeyframeRule* CSSKeyframesRule::Item(unsigned index) const {
  if (index >= length())
    return nullptr;

  DCHECK_EQ(child_rule_cssom_wrappers_.size(),
            keyframes_rule_->Keyframes().size());
  Member<CSSKeyframeRule>& rule = child_rule_cssom_wrappers_[index];
  if (!rule) {
    rule = MakeGarbageCollected<CSSKeyframeRule>(
        keyframes_rule_->Keyframes()[index].Get(),
        const_cast<CSSKeyframesRule*>(this));
  }

  return rule.Get();
}

CSSKeyframeRule* CSSKeyframesRule::AnonymousIndexedGetter(
    unsigned index) const {
  const Document* parent_document =
      CSSStyleSheet::SingleOwnerDocument(parentStyleSheet());
  if (parent_document) {
    parent_document->CountUse(
        WebFeature::kCSSKeyframesRuleAnonymousIndexedGetter);
  }
  return Item(index);
}

CSSRuleList* CSSKeyframesRule::cssRules() const {
  if (!rule_list_cssom_wrapper_) {
    rule_list_cssom_wrapper_ =
        MakeGarbageCollected<LiveCSSRuleList<CSSKeyframesRule>>(
            const_cast<CSSKeyframesRule*>(this));
  }
  return rule_list_cssom_wrapper_.Get();
}

void CSSKeyframesRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  keyframes_rule_ = To<StyleRuleKeyframes>(rule);
}

void CSSKeyframesRule::Trace(blink::Visitor* visitor) {
  CSSRule::Trace(visitor);
  visitor->Trace(child_rule_cssom_wrappers_);
  visitor->Trace(keyframes_rule_);
  visitor->Trace(rule_list_cssom_wrapper_);
}

}  // namespace blink
