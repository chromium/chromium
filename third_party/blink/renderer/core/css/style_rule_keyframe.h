// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_KEYFRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_KEYFRAME_H_

#include <memory>
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class MutableCSSPropertyValueSet;
class CSSPropertyValueSet;
class ExecutionContext;

struct KeyframeOffset {
  explicit KeyframeOffset(
      Timing::TimelineNamedPhase phase = Timing::TimelineNamedPhase::kNone,
      double percent = 0)
      : phase(phase), percent(percent) {}

  bool operator==(const KeyframeOffset& b) const {
    return percent == b.percent && phase == b.phase;
  }

  bool operator!=(const KeyframeOffset& b) const { return !(*this == b); }

  Timing::TimelineNamedPhase phase;
  double percent;
};

class StyleRuleKeyframe final : public StyleRuleBase {
 public:
  StyleRuleKeyframe(std::unique_ptr<Vector<KeyframeOffset>>,
                    CSSPropertyValueSet*);

  // Exposed to JavaScript.
  String KeyText() const;
  bool SetKeyText(const ExecutionContext*, const String&);

  // Used by StyleResolver.
  const Vector<KeyframeOffset>& Keys() const;

  const CSSPropertyValueSet& Properties() const { return *properties_; }
  MutableCSSPropertyValueSet& MutableProperties();

  String CssText() const;

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  Member<CSSPropertyValueSet> properties_;
  Vector<KeyframeOffset> keys_;
};

template <>
struct DowncastTraits<StyleRuleKeyframe> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsKeyframeRule();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_KEYFRAME_H_
