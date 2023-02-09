// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_rule_keyframe.h"

#include <memory>

#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

StyleRuleKeyframe::StyleRuleKeyframe(
    std::unique_ptr<Vector<KeyframeOffset>> keys,
    CSSPropertyValueSet* properties)
    : StyleRuleBase(kKeyframe), properties_(properties), keys_(*keys) {}

String StyleRuleKeyframe::KeyText() const {
  DCHECK(!keys_.empty());

  StringBuilder key_text;
  for (unsigned i = 0; i < keys_.size(); ++i) {
    if (i) {
      key_text.Append(", ");
    }
    if (keys_.at(i).name != TimelineOffset::NamedRange::kNone) {
      key_text.Append(
          TimelineOffset::TimelineRangeNameToString(keys_.at(i).name));
      key_text.Append(" ");
    }
    key_text.AppendNumber(keys_.at(i).percent * 100);
    key_text.Append('%');
  }

  return key_text.ReleaseString();
}

bool StyleRuleKeyframe::SetKeyText(const ExecutionContext* execution_context,
                                   const String& key_text) {
  DCHECK(!key_text.IsNull());

  auto* context = MakeGarbageCollected<CSSParserContext>(*execution_context);

  std::unique_ptr<Vector<KeyframeOffset>> keys =
      CSSParser::ParseKeyframeKeyList(context, key_text);
  if (!keys || keys->empty()) {
    return false;
  }

  keys_ = *keys;
  return true;
}

const Vector<KeyframeOffset>& StyleRuleKeyframe::Keys() const {
  return keys_;
}

MutableCSSPropertyValueSet& StyleRuleKeyframe::MutableProperties() {
  if (!properties_->IsMutable()) {
    properties_ = properties_->MutableCopy();
  }
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

String StyleRuleKeyframe::CssText() const {
  StringBuilder result;
  result.Append(KeyText());
  result.Append(" { ");
  String decls = properties_->AsText();
  result.Append(decls);
  if (!decls.empty()) {
    result.Append(' ');
  }
  result.Append('}');
  return result.ReleaseString();
}

void StyleRuleKeyframe::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(properties_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

}  // namespace blink
