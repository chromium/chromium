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

#include "third_party/blink/renderer/core/css/css_keyframe_rule.h"

#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/keyframe_style_rule_css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

CSSKeyframeRule::CSSKeyframeRule(StyleRuleKeyframe* keyframe,
                                 CSSKeyframesRule* parent)
    : CSSRule(nullptr), keyframe_(keyframe) {
  SetParentRule(parent);
}

CSSKeyframeRule::~CSSKeyframeRule() = default;

void CSSKeyframeRule::setKeyText(const String& key_text,
                                 ExceptionState& exception_state) {
  CSSStyleSheet::RuleMutationScope(this);

  if (!keyframe_->SetKeyText(key_text))
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The key '" + key_text + "' is invalid and cannot be parsed");

  if (auto* parent = To<CSSKeyframesRule>(parentRule()))
    parent->StyleChanged();
}

CSSStyleDeclaration* CSSKeyframeRule::style() const {
  if (!properties_cssom_wrapper_) {
    properties_cssom_wrapper_ =
        MakeGarbageCollected<KeyframeStyleRuleCSSStyleDeclaration>(
            keyframe_->MutableProperties(), const_cast<CSSKeyframeRule*>(this));
  }
  return properties_cssom_wrapper_.Get();
}

void CSSKeyframeRule::Reattach(StyleRuleBase*) {
  // No need to reattach, the underlying data is shareable on mutation.
  NOTREACHED();
}

void CSSKeyframeRule::Trace(blink::Visitor* visitor) {
  visitor->Trace(keyframe_);
  visitor->Trace(properties_cssom_wrapper_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
