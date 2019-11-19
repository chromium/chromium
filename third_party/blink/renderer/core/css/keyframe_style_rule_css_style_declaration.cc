// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/keyframe_style_rule_css_style_declaration.h"

#include "third_party/blink/renderer/core/css/css_keyframe_rule.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"

namespace blink {

KeyframeStyleRuleCSSStyleDeclaration::KeyframeStyleRuleCSSStyleDeclaration(
    MutableCSSPropertyValueSet& property_set_arg,
    CSSKeyframeRule* parent_rule)
    : StyleRuleCSSStyleDeclaration(property_set_arg, parent_rule) {}

void KeyframeStyleRuleCSSStyleDeclaration::DidMutate(MutationType type) {
  StyleRuleCSSStyleDeclaration::DidMutate(type);
  if (auto* parent = To<CSSKeyframesRule>(parent_rule_->parentRule()))
    parent->StyleChanged();
}

}  // namespace blink
