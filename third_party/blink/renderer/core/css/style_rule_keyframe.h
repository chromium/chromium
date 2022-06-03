// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_KEYFRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_KEYFRAME_H_

#include <memory>
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class MutableCSSPropertyValueSet;
class CSSPropertyValueSet;

class StyleRuleKeyframe final : public StyleRuleBase {
 public:
  StyleRuleKeyframe(std::unique_ptr<Vector<double>>, CSSPropertyValueSet*);

  // Exposed to JavaScript.
  String KeyText() const;
  bool SetKeyText(const String&);

  // Used by StyleResolver.
  const Vector<double>& Keys() const;

  const CSSPropertyValueSet& Properties() const { return *properties_; }
  MutableCSSPropertyValueSet& MutableProperties();

  String CssText() const;

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  Member<CSSPropertyValueSet> properties_;
  Vector<double> keys_;
};

template <>
struct DowncastTraits<StyleRuleKeyframe> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsKeyframeRule();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_KEYFRAME_H_
